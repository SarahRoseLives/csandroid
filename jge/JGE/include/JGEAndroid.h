//-------------------------------------------------------------------------------------
//
// JGE++ Android platform bridge.
//
//-------------------------------------------------------------------------------------

#ifndef _JGE_ANDROID_H_
#define _JGE_ANDROID_H_

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

// Called once by the Android entry point after the native activity is ready.
void JGEAndroid_SetJavaVM(JavaVM* vm);

// Cached JavaVM.
JavaVM* JGEAndroid_GetJavaVM(void);

// JNI environment for the calling (main/game) thread.
JNIEnv* JGEAndroid_GetJNIEnv(void);

// Find a class using the application's class loader (the game thread is not
// attached with the app class loader, so env->FindClass() alone would fail).
jclass JGEAndroid_FindClass(const char* name);

// Set the application class loader (called once by the Android entry point).
void JGEAndroid_SetClassLoader(JNIEnv* env, jobject classLoader, jobject activityClassObject);

// Absolute path to the extracted asset root directory (used by the Java audio
// bridge and any code that needs on-disk paths).
const char* JGEAndroid_AssetPath(void);

// Set the absolute asset root path (called by the Android entry point).
void JGEAndroid_SetAssetPath(const char* path);

#ifdef __cplusplus
}
#endif

#endif
