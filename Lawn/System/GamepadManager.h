#ifndef __GAMEPADMANAGER_H__
#define __GAMEPADMANAGER_H__

#include "../../SDL/include/SDL3/SDL.h"
#include "../../SexyAppFramework/Common.h"

class LawnApp;

class Gamepad
{
	friend class			GamepadManager;

private:
	LawnApp*				mApp;
	int						mIndex;
	SDL_Gamepad*			mSDLGamepad;
	float                   mThreshold;
	bool					mButtons[SDL_GAMEPAD_BUTTON_COUNT];
	bool					mLastButtons[SDL_GAMEPAD_BUTTON_COUNT];

public:
	Gamepad();
	~Gamepad();

	bool                    GetButton(SDL_GamepadButton theButton);
	bool                    GetButtonDown(SDL_GamepadButton theButton);
	bool                    GetButtonUp(SDL_GamepadButton theButton);
	int                     GetAxisRawValue(SDL_GamepadAxis theAxis);
	float                   GetAxisValue(SDL_GamepadAxis theAxis);
	float                   GetTriggerAxisValue(SDL_GamepadAxis theAxis);
	void                    Rumble(float theLow, float theHigh, int theDuration);
};

class GamepadManager
{
private:
	LawnApp*				mApp;
	bool                    mInit;
	std::map<SDL_JoystickID, Gamepad*> mGamepads;
	int                     mCurrentMouse;
	SDL_GamepadType         mLastUsedType;

public:
	GamepadManager(LawnApp* theApp);
	~GamepadManager();

	void                    Update();
	Gamepad*				GetGamepad(int theIndex);
	SDL_GamepadType         GetLastUsedType();
	void                    RumbleAll(float theLow, float theHigh, int theDuration);
};

#endif