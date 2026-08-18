//-------------------------------------------------------------------------------------
//
// JGE++ Android input handling (gamepad / DS4 focused).
//
//-------------------------------------------------------------------------------------

#ifndef _JGE_INPUT_ANDROID_H_
#define _JGE_INPUT_ANDROID_H_

#include <android/input.h>

#ifdef __cplusplus
extern "C" {
#endif

void JGEAndroid_ProcessInput(AInputEvent* event);
void JGEAndroidLatchInput(void);

#ifdef __cplusplus
}
#endif

#endif
