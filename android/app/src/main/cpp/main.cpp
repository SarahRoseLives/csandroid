//-------------------------------------------------------------------------------------
//
// CSPSP Android entry point.
//
// Uses android_native_app_glue: creates an EGL 1.1 context, extracts the game
// assets from the APK into the app's internal storage, chdir()s there (so all
// of the game's relative fopen()/opendir() paths work unchanged), then drives
// the JGE update/render loop and forwards gamepad input.
//
//-------------------------------------------------------------------------------------

#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES/gl.h>

#include <jni.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "JGE.h"
#include "JApp.h"
#include "JRenderer.h"
#include "JGameLauncher.h"
#include "JGEAndroid.h"
#include "JGEInputAndroid.h"

#define LOG_TAG "CSPSP"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static JavaVM* gJavaVM = NULL;
static JNIEnv* gJNIEnv = NULL;
static char gAssetPath[1024];
static jobject gClassLoader = NULL;
static jmethodID gLoadClass = NULL;

//------------------------------------------------------------------------------------------------
// JGEAndroid platform bridge
//------------------------------------------------------------------------------------------------
void JGEAndroid_SetJavaVM(JavaVM* vm) { gJavaVM = vm; }
JavaVM* JGEAndroid_GetJavaVM(void) { return gJavaVM; }

JNIEnv* JGEAndroid_GetJNIEnv(void)
{
	if (gJNIEnv != NULL) return gJNIEnv;
	if (gJavaVM == NULL) return NULL;
	gJavaVM->AttachCurrentThread(&gJNIEnv, NULL);
	return gJNIEnv;
}

void JGEAndroid_SetClassLoader(JNIEnv* env, jobject classLoader, jobject activityClassObject)
{
	gClassLoader = env->NewGlobalRef(classLoader);

	jclass classLoaderClass = env->FindClass("java/lang/ClassLoader");
	gLoadClass = env->GetMethodID(classLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
	env->DeleteLocalRef(classLoaderClass);
}

jclass JGEAndroid_FindClass(const char* name)
{
	if (gClassLoader == NULL || gLoadClass == NULL) return NULL;

	JNIEnv* env = JGEAndroid_GetJNIEnv();
	if (env == NULL) return NULL;

	jstring className = env->NewStringUTF(name);
	jclass clazz = (jclass)env->CallObjectMethod(gClassLoader, gLoadClass, className);
	env->DeleteLocalRef(className);

	if (env->ExceptionCheck()) {
		env->ExceptionClear();
		return NULL;
	}
	return clazz;
}

const char* JGEAndroid_AssetPath(void) { return gAssetPath; }
void JGEAndroid_SetAssetPath(const char* path) { strncpy(gAssetPath, path, sizeof(gAssetPath) - 1); }

//------------------------------------------------------------------------------------------------
// EGL
//------------------------------------------------------------------------------------------------
static EGLDisplay gDisplay = EGL_NO_DISPLAY;
static EGLSurface gSurface = EGL_NO_SURFACE;
static EGLContext gContext = EGL_NO_CONTEXT;

static int initEGL(ANativeWindow* window)
{
	gDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (gDisplay == EGL_NO_DISPLAY) return -1;

	EGLint major, minor;
	if (!eglInitialize(gDisplay, &major, &minor)) return -1;

	const EGLint attribs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 16,
		EGL_NONE
	};

	EGLConfig config;
	EGLint numConfigs;
	if (!eglChooseConfig(gDisplay, attribs, &config, 1, &numConfigs) || numConfigs < 1)
		return -1;

	const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 1, EGL_NONE };
	gContext = eglCreateContext(gDisplay, config, EGL_NO_CONTEXT, ctxAttribs);
	if (gContext == EGL_NO_CONTEXT) return -1;

	const EGLint surfAttribs[] = { EGL_NONE };
	gSurface = eglCreateWindowSurface(gDisplay, config, window, surfAttribs);
	if (gSurface == EGL_NO_SURFACE) return -1;

	if (!eglMakeCurrent(gDisplay, gSurface, gSurface, gContext)) return -1;

	eglQuerySurface(gDisplay, gSurface, EGL_WIDTH, &major);
	eglQuerySurface(gDisplay, gSurface, EGL_HEIGHT, &minor);
	LOGI("EGL surface %dx%d (GLES %s)", major, minor, glGetString(GL_VERSION));

	return 0;
}

static void termEGL()
{
	if (gDisplay != EGL_NO_DISPLAY)
	{
		eglMakeCurrent(gDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (gContext != EGL_NO_CONTEXT) eglDestroyContext(gDisplay, gContext);
		if (gSurface != EGL_NO_SURFACE) eglDestroySurface(gDisplay, gSurface);
		eglTerminate(gDisplay);
	}
	gDisplay = EGL_NO_DISPLAY;
	gSurface = EGL_NO_SURFACE;
	gContext = EGL_NO_CONTEXT;
}

//------------------------------------------------------------------------------------------------
// android_main
//------------------------------------------------------------------------------------------------
static int32_t onInputEvent(struct android_app* app, AInputEvent* event)
{
	JGEAndroid_ProcessInput(event);
	return 1;
}

void android_main(struct android_app* app)
{
	app_dummy();

	app->onInputEvent = onInputEvent;

	JGEAndroid_SetJavaVM(app->activity->vm);
	JNIEnv* env = JGEAndroid_GetJNIEnv();
	JGEAndroid_SetAssetPath(app->activity->internalDataPath);

	// Cache the app class loader so native code can resolve app classes
	// (the game thread is not attached with the app class loader).
	if (env != NULL)
	{
		jobject activityClass = app->activity->clazz;
		jclass classClass = env->GetObjectClass(activityClass);
		jmethodID getClassLoader = env->GetMethodID(classClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
		jobject classLoader = env->CallObjectMethod(activityClass, getClassLoader);
		if (!env->ExceptionCheck() && classLoader != NULL)
		{
			JGEAndroid_SetClassLoader(env, classLoader, activityClass);
		}
		env->ExceptionClear();
		env->DeleteLocalRef(classLoader);
		env->DeleteLocalRef(classClass);
	}

	// Assets are extracted by the Java activity before android_main runs;
	// just chdir to the extracted location so relative fopen()/opendir() paths work.
	chdir(app->activity->internalDataPath);
	LOGI("Using asset dir %s", app->activity->internalDataPath);

	// Create the game + engine.
	JGameLauncher* launcher = new JGameLauncher();
	u32 flags = launcher->GetInitFlags();
	if ((flags & JINIT_FLAG_ENABLE3D) != 0)
		JRenderer::Set3DFlag(true);

	JGE* engine = JGE::GetInstance();
	JApp* game = launcher->GetGameApp();
	game->Create();
	engine->SetApp(game);

	bool initialized = false;
	struct timespec lastTick;
	clock_gettime(CLOCK_MONOTONIC, &lastTick);

	while (!engine->IsDone())
	{
		// Process events.
		int events;
		struct android_poll_source* source;
		while (ALooper_pollOnce(0, NULL, &events, (void**)&source) >= 0)
		{
			if (source != NULL)
				source->process(app, source);

			if (app->destroyRequested != 0)
			{
				engine->End();
				break;
			}
		}

		if (engine->IsDone())
			break;

		if (app->window == NULL)
			continue;

		if (!initialized)
		{
			if (initEGL(app->window) != 0)
			{
				LOGE("EGL init failed");
				break;
			}

			int w, h;
			eglQuerySurface(gDisplay, gSurface, EGL_WIDTH, &w);
			eglQuerySurface(gDisplay, gSurface, EGL_HEIGHT, &h);
			JRenderer::GetInstance()->SetDisplaySize(w, h);
			JRenderer::GetInstance()->Enable2D();

			initialized = true;
		}

		// Frame timing.
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		long dtMs = (now.tv_sec - lastTick.tv_sec) * 1000 + (now.tv_nsec - lastTick.tv_nsec) / 1000000;
		if (dtMs < 0) dtMs = 0;
		lastTick = now;

		engine->SetDelta((int)dtMs);
		engine->Update();
		engine->mClicked = false;
		engine->Render();

		JGEAndroidLatchInput();

		eglSwapBuffers(gDisplay, gSurface);
	}

	engine->SetApp(NULL);
	game->Destroy();
	delete game;
	delete launcher;

	engine->End();
	JGE::Destroy();

	termEGL();
}
