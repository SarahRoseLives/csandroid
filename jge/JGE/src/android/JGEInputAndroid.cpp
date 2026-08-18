//-------------------------------------------------------------------------------------
//
// JGE++ Android input backend.
//
// Maps Android gamepad (DualShock 4 and similar) input events onto the PSP
// button/analog interface used throughout the game.
//
//-------------------------------------------------------------------------------------

#include "../../include/JGE.h"
#include "JGEInputAndroid.h"

#include <android/input.h>
#include <android/keycodes.h>

static u32 gButtons = 0;
static u32 gOldButtons = 0;
static u8 gAnalogX = 0x80;
static u8 gAnalogY = 0x80;

static u32 mapKeyCode(int keyCode)
{
	switch (keyCode)
	{
		case AKEYCODE_BUTTON_A:      return PSP_CTRL_CROSS;
		case AKEYCODE_BUTTON_B:      return PSP_CTRL_CIRCLE;
		case AKEYCODE_BUTTON_X:      return PSP_CTRL_SQUARE;
		case AKEYCODE_BUTTON_Y:      return PSP_CTRL_TRIANGLE;
		case AKEYCODE_BUTTON_L1:     return PSP_CTRL_LTRIGGER;
		case AKEYCODE_BUTTON_R1:     return PSP_CTRL_RTRIGGER;
		case AKEYCODE_BUTTON_START:  return PSP_CTRL_START;
		case AKEYCODE_BUTTON_SELECT: return PSP_CTRL_SELECT;
		case AKEYCODE_DPAD_UP:       return PSP_CTRL_UP;
		case AKEYCODE_DPAD_DOWN:     return PSP_CTRL_DOWN;
		case AKEYCODE_DPAD_LEFT:     return PSP_CTRL_LEFT;
		case AKEYCODE_DPAD_RIGHT:    return PSP_CTRL_RIGHT;
		default:                     return 0;
	}
}

static void setButton(u32 bit, bool down)
{
	if (down)
		gButtons |= bit;
	else
		gButtons &= ~bit;
}

void JGEAndroid_ProcessInput(AInputEvent* event)
{
	int type = AInputEvent_getType(event);
	int source = AInputEvent_getSource(event);

	if (type == AINPUT_EVENT_TYPE_KEY)
	{
		int32_t action = AKeyEvent_getAction(event);
		int32_t keyCode = AKeyEvent_getKeyCode(event);
		u32 bit = mapKeyCode(keyCode);
		if (bit == 0) return;

		if (action == AKEY_EVENT_ACTION_DOWN)
			setButton(bit, true);
		else if (action == AKEY_EVENT_ACTION_UP)
			setButton(bit, false);
	}
	else if (type == AINPUT_EVENT_TYPE_MOTION)
	{
		// Left analog stick -> PSP analog (movement).
		if ((source & AINPUT_SOURCE_JOYSTICK) || (source & AINPUT_SOURCE_GAMEPAD))
		{
			float x = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_X, 0);
			float y = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_Y, 0);

			// Clamp to [-1, 1]
			if (x < -1.0f) x = -1.0f;
			if (x >  1.0f) x =  1.0f;
			if (y < -1.0f) y = -1.0f;
			if (y >  1.0f) y =  1.0f;

			gAnalogX = (u8)(128 + (int)(x * 127.0f));
			gAnalogY = (u8)(128 + (int)(y * 127.0f));

			// D-pad can also be reported as a hat axis.
			float hatX = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_X, 0);
			float hatY = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_Y, 0);
			setButton(PSP_CTRL_LEFT,  hatX < -0.5f);
			setButton(PSP_CTRL_RIGHT, hatX >  0.5f);
			setButton(PSP_CTRL_UP,    hatY < -0.5f);
			setButton(PSP_CTRL_DOWN,  hatY >  0.5f);
		}
	}
}

void JGEAndroidLatchInput(void)
{
	gOldButtons = gButtons;
}

bool JGEGetButtonState(u32 button)
{
	return (gButtons & button) == button;
}

bool JGEGetButtonClick(u32 button)
{
	return ((gButtons & button) == button) && ((gOldButtons & button) != button);
}

u8 JGEGetAnalogX()
{
	return gAnalogX;
}

u8 JGEGetAnalogY()
{
	return gAnalogY;
}
