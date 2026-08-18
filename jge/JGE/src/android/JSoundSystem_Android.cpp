//-------------------------------------------------------------------------------------
//
// JGE++ is a hardware accelerated 2D game SDK for PSP/Windows/Android.
//
// Licensed under the BSD license, see LICENSE in JGE root for details.
//
// Android backend: audio through the Java AudioBridge (SoundPool for SFX,
// MediaPlayer for music).
//
//-------------------------------------------------------------------------------------

#include "../../include/JSoundSystem.h"
#include "../../include/JFileSystem.h"
#include "../../include/JGEAndroid.h"

#include <string.h>
#include <stdio.h>

//------------------------------------------------------------------------------------------------
JMusic::JMusic()
{
	mTrack = -1;
}

JMusic::~JMusic()
{
	// The single MediaPlayer is stopped/released by StopMusic(); nothing to
	// free here.
}


//------------------------------------------------------------------------------------------------
JSample::JSample()
{
	mVoice = -1;
	mVolume = 255;
	mPanning = 127;
	mSample = -1;
}

JSample::~JSample()
{
}


//------------------------------------------------------------------------------------------------
static jclass gAudioBridgeClass = NULL;
static jmethodID gInit = NULL;
static jmethodID gLoadSample = NULL;
static jmethodID gPlaySample = NULL;
static jmethodID gStopSample = NULL;
static jmethodID gLoadMusic = NULL;
static jmethodID gPlayMusic = NULL;
static jmethodID gStopMusic = NULL;
static jmethodID gSetVolume = NULL;

static void resolveAudioBridge()
{
	if (gAudioBridgeClass != NULL) return;

	JNIEnv* env = JGEAndroid_GetJNIEnv();
	if (env == NULL) return;

	jclass clazz = JGEAndroid_FindClass("com/cspsp/android/AudioBridge");
	if (clazz == NULL) return;

	gAudioBridgeClass = (jclass)env->NewGlobalRef(clazz);

	gInit       = env->GetStaticMethodID(gAudioBridgeClass, "init", "()I");
	gLoadSample = env->GetStaticMethodID(gAudioBridgeClass, "loadSample", "(Ljava/lang/String;)I");
	gPlaySample = env->GetStaticMethodID(gAudioBridgeClass, "playSample", "(III)I");
	gStopSample = env->GetStaticMethodID(gAudioBridgeClass, "stopSample", "(I)I");
	gLoadMusic  = env->GetStaticMethodID(gAudioBridgeClass, "loadMusic", "(Ljava/lang/String;)I");
	gPlayMusic  = env->GetStaticMethodID(gAudioBridgeClass, "playMusic", "(Z)I");
	gStopMusic  = env->GetStaticMethodID(gAudioBridgeClass, "stopMusic", "()I");
	gSetVolume  = env->GetStaticMethodID(gAudioBridgeClass, "setVolume", "(I)I");
}

static jstring makeAbsolutePath(const char* fileName)
{
	JNIEnv* env = JGEAndroid_GetJNIEnv();
	if (env == NULL) return NULL;

	const char* base = JGEAndroid_AssetPath();
	char full[1024];
	if (base && base[0]) {
		snprintf(full, sizeof(full), "%s/%s", base, fileName);
	} else {
		snprintf(full, sizeof(full), "%s", fileName);
	}
	return env->NewStringUTF(full);
}

//------------------------------------------------------------------------------------------------
JSoundSystem* JSoundSystem::mInstance = NULL;

JSoundSystem* JSoundSystem::GetInstance()
{
	if (mInstance == NULL)
	{
		mInstance = new JSoundSystem();
		mInstance->InitSoundSystem();
	}

	return mInstance;
}

void JSoundSystem::Destroy()
{
	if (mInstance)
	{
		mInstance->DestroySoundSystem();
		delete mInstance;
		mInstance = NULL;
	}
}

JSoundSystem::JSoundSystem()
{
	mVolume = 255;
	mChannel = 0;
}

JSoundSystem::~JSoundSystem()
{
}

void JSoundSystem::InitSoundSystem()
{
	resolveAudioBridge();
	JNIEnv* env = JGEAndroid_GetJNIEnv();
	if (env && gInit)
		env->CallStaticIntMethod(gAudioBridgeClass, gInit);
}

void JSoundSystem::DestroySoundSystem()
{
}


JMusic *JSoundSystem::LoadMusic(const char *fileName)
{
	JMusic* music = new JMusic();

	resolveAudioBridge();
	JNIEnv* env = JGEAndroid_GetJNIEnv();
	if (env && gLoadMusic && music)
	{
		jstring path = makeAbsolutePath(fileName);
		if (path) {
			int r = env->CallStaticIntMethod(gAudioBridgeClass, gLoadMusic, path);
			env->DeleteLocalRef(path);
			music->mTrack = r;
		}
	}

	return music;
}


void JSoundSystem::PlayMusic(JMusic *music, bool looping)
{
	resolveAudioBridge();
	JNIEnv* env = JGEAndroid_GetJNIEnv();
	if (env && gPlayMusic)
		env->CallStaticIntMethod(gAudioBridgeClass, gPlayMusic, looping ? JNI_TRUE : JNI_FALSE);
}


void JSoundSystem::StopMusic(JMusic *music)
{
	resolveAudioBridge();
	JNIEnv* env = JGEAndroid_GetJNIEnv();
	if (env && gStopMusic)
		env->CallStaticIntMethod(gAudioBridgeClass, gStopMusic);
}


void JSoundSystem::SetVolume(int volume)
{
	mVolume = volume;

	resolveAudioBridge();
	JNIEnv* env = JGEAndroid_GetJNIEnv();
	if (env && gSetVolume)
		env->CallStaticIntMethod(gAudioBridgeClass, gSetVolume, volume);
}


JSample *JSoundSystem::LoadSample(const char *fileName)
{
	JSample* sample = new JSample();

	resolveAudioBridge();
	JNIEnv* env = JGEAndroid_GetJNIEnv();
	if (env && gLoadSample && sample)
	{
		jstring path = makeAbsolutePath(fileName);
		if (path) {
			int id = env->CallStaticIntMethod(gAudioBridgeClass, gLoadSample, path);
			env->DeleteLocalRef(path);
			sample->mSample = id;
		}
	}

	return sample;
}


void JSoundSystem::PlaySample(JSample *sample)
{
	if (sample == NULL || sample->mSample < 0) return;

	resolveAudioBridge();
	JNIEnv* env = JGEAndroid_GetJNIEnv();
	if (env && gPlaySample)
		sample->mVoice = env->CallStaticIntMethod(gAudioBridgeClass, gPlaySample,
				sample->mSample, sample->mVolume, sample->mPanning);
}


void JSoundSystem::StopSample(int voice)
{
	resolveAudioBridge();
	JNIEnv* env = JGEAndroid_GetJNIEnv();
	if (env && gStopSample)
		env->CallStaticIntMethod(gAudioBridgeClass, gStopSample, voice);
}
