#include "GamepadManager.h"
#include "../../Lawn/GamepadPlayer.h"
#include "../../GameConstants.h"
#include "../../LawnApp.h"
#include "../../Lawn/Board.h"
#include "../../Sexy.TodLib/TodCommon.h"

const int cMouseMaxSpeed = 7;

Gamepad::Gamepad()
{
    mApp = nullptr;
	mIndex = -1;
	mSDLGamepad = nullptr;
	mThreshold = 0.0f;
	memset(mButtons, 0, sizeof(mButtons));
	memset(mLastButtons, 0, sizeof(mLastButtons));
}

Gamepad::~Gamepad()
{
	SDL_CloseGamepad(mSDLGamepad);
}

bool Gamepad::GetButton(SDL_GamepadButton theButton)
{
	if (!mApp->mHasFocus)
		return false;
    return mButtons[theButton];
}

bool Gamepad::GetButtonDown(SDL_GamepadButton theButton)
{
	if (!mApp->mHasFocus)
		return false;
    return GetButton(theButton) && !mLastButtons[theButton];
}

bool Gamepad::GetButtonUp(SDL_GamepadButton theButton)
{
	if (!mApp->mHasFocus)
		return false;
    return !GetButton(theButton) && mLastButtons[theButton];
}

int Gamepad::GetAxisRawValue(SDL_GamepadAxis theAxis)
{
    if (!mApp->mHasFocus)
        return 0.0f;
    return (int)SDL_GetGamepadAxis(mSDLGamepad, theAxis);
}

float Gamepad::GetAxisValue(SDL_GamepadAxis theAxis)
{
	if (!mApp->mHasFocus || theAxis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || theAxis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
		return 0.0f;
	float aValue = (float)GetAxisRawValue(theAxis) / (float)INT16_MAX;
	if (abs(aValue) < mThreshold)
		return 0.0f;
	//adding more settings into the advanced options later (including deadzone, anti deadzone, sensitivity)
	//but first redesigning the advanced options would be good and then these
	return ClampFloat(aValue, -1.0f, 1.0f);
}

float Gamepad::GetTriggerAxisValue(SDL_GamepadAxis theAxis)
{
	if (!mApp->mHasFocus || (theAxis != SDL_GAMEPAD_AXIS_LEFT_TRIGGER && theAxis != SDL_GAMEPAD_AXIS_RIGHT_TRIGGER))
		return 0.0f;
	//this will have the same settings but in a different category
	return ClampFloat((float)GetAxisRawValue(theAxis) / (float)INT16_MAX, 0.0f, 1.0f);
}

void Gamepad::Rumble(float theLow, float theHigh, int theDuration)
{
	SDL_RumbleGamepad(mSDLGamepad, ClampFloat(theLow, 0.0f, 1.0f) * UINT16_MAX, ClampFloat(theHigh, 0.0f, 1.0f) * UINT16_MAX, theDuration);
}

GamepadManager::GamepadManager(LawnApp* theApp)
{
	mApp = theApp;
	mInit = SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK);
	mCurrentMouse = -1;
	mLastUsedType = SDL_GAMEPAD_TYPE_UNKNOWN;
	if (!mInit)
		MessageBox(mApp->mHWnd, StrFormat("Couldn't initialize SDL for the gamepads. Error: %s", SDL_GetError()).c_str(), "SDL Error", MB_OK | MB_ICONERROR);
}

GamepadManager::~GamepadManager()
{
	for (const auto& aPair : mGamepads)
		delete aPair.second;
	SDL_Quit();
}

void GamepadManager::Update()
{
	if (!mInit)
		return;

	static int aMouseX = -1;
	static int aMouseY = -1;
	static int aSpeedX = 0;
	static int aSpeedY = 0;

	if (aSpeedX != 0 || aSpeedY != 0)
	{
		if (aMouseX == -1 && aMouseY == -1)
		{
			aMouseX = mApp->mWidgetManager->mLastMouseX;
			aMouseY = mApp->mWidgetManager->mLastMouseY;
		}
		aMouseX = ClampInt(aMouseX + aSpeedX, 0, BOARD_WIDTH - 1);
		aMouseY = ClampInt(aMouseY + aSpeedY, 0, BOARD_HEIGHT - 1);
		POINT aPoint = { aMouseX, aMouseY };
		::ClientToScreen(mApp->mHWnd, &aPoint);
		::SetCursorPos(aPoint.x, aPoint.y);
	}
	else
	{
		aMouseX = -1;
		aMouseY = -1;
	}

	for (const auto& aPair : mGamepads)
	{
		if (!mApp->mHasFocus)
			break;

		Gamepad* aGamepad = aPair.second;
		int aIndex = aGamepad->mIndex;
		SDL_Gamepad* aSDLGamepad = aGamepad->mSDLGamepad;

		float aAxisLeftX = aGamepad->GetAxisValue(SDL_GAMEPAD_AXIS_LEFTX);
		float aAxisLeftY = aGamepad->GetAxisValue(SDL_GAMEPAD_AXIS_LEFTY);
		float aAxisRightX = aGamepad->GetAxisValue(SDL_GAMEPAD_AXIS_RIGHTX);
		float aAxisRightY = aGamepad->GetAxisValue(SDL_GAMEPAD_AXIS_RIGHTY);
		float aAxisLeftTrigger = aGamepad->GetTriggerAxisValue(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
		float aAxisRightTrigger = aGamepad->GetTriggerAxisValue(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

		bool aHasInput = false;
		for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++)
		{
			aGamepad->mLastButtons[i] = aGamepad->mButtons[i];
			aGamepad->mButtons[i] = SDL_GetGamepadButton(aSDLGamepad, (SDL_GamepadButton)i);
			if (!aHasInput)
				aHasInput = aGamepad->mButtons[i];
		}

		if (aHasInput || aAxisLeftX != 0 || aAxisLeftY != 0 || aAxisRightX != 0 || aAxisRightY != 0 || aAxisLeftTrigger != 0 || aAxisRightTrigger != 0)
			mLastUsedType = SDL_GetGamepadType(aSDLGamepad);

		if (aGamepad->GetButtonDown(SDL_GAMEPAD_BUTTON_LEFT_STICK))
		{
			if (mCurrentMouse == -1)
				mCurrentMouse = aIndex;
			else if (mCurrentMouse == aIndex)
			{
				mCurrentMouse = -1;
				aSpeedX = 0;
				aSpeedY = 0;
			}
		}

		if (mCurrentMouse == aIndex)
		{
			if (aAxisLeftX != 0)
				aSpeedX = cMouseMaxSpeed * aAxisLeftX;
			else
			{
				if (aGamepad->GetButton(SDL_GAMEPAD_BUTTON_DPAD_LEFT))
					aSpeedX = -cMouseMaxSpeed;
				else if (aGamepad->GetButton(SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
					aSpeedX = cMouseMaxSpeed;
				else
					aSpeedX = 0;
			}

			if (aAxisLeftY != 0)
				aSpeedY = cMouseMaxSpeed * aAxisLeftY;
			else
			{
				if (aGamepad->GetButton(SDL_GAMEPAD_BUTTON_DPAD_UP))
					aSpeedY = -cMouseMaxSpeed;
				else if (aGamepad->GetButton(SDL_GAMEPAD_BUTTON_DPAD_DOWN))
					aSpeedY = cMouseMaxSpeed;
				else
					aSpeedY = 0;
			}

			if (aGamepad->GetButtonDown(SDL_GAMEPAD_BUTTON_SOUTH))
				SendMessage(mApp->mHWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(mApp->mWidgetManager->mLastMouseX, mApp->mWidgetManager->mLastMouseY));
			else if (aGamepad->GetButtonUp(SDL_GAMEPAD_BUTTON_SOUTH))
				SendMessage(mApp->mHWnd, WM_LBUTTONUP, 0, MAKELPARAM(mApp->mWidgetManager->mLastMouseX, mApp->mWidgetManager->mLastMouseY));

			if (aGamepad->GetButtonDown(SDL_GAMEPAD_BUTTON_EAST))
				SendMessage(mApp->mHWnd, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(mApp->mWidgetManager->mLastMouseX, mApp->mWidgetManager->mLastMouseY));
			else if (aGamepad->GetButtonUp(SDL_GAMEPAD_BUTTON_EAST))
				SendMessage(mApp->mHWnd, WM_RBUTTONUP, 0, MAKELPARAM(mApp->mWidgetManager->mLastMouseX, mApp->mWidgetManager->mLastMouseY));
		}
	}

	SDL_Event aEvent;
	while (SDL_PollEvent(&aEvent))
	{
		int aIndex = -1;
		SDL_JoystickID aID = aEvent.gdevice.which;
		auto aIt = mGamepads.find(aID);
		if (aIt != mGamepads.end())
			aIndex = aIt->second->mIndex;
		switch (aEvent.type)
		{
		case SDL_EVENT_GAMEPAD_ADDED:
			if (SDL_Gamepad* aSDLGamepad = SDL_OpenGamepad(aID))
			{
				if (mGamepads.find(aID) == mGamepads.end())
				{
					for (int i = 0; i < MAX_GAMEPADS; i++)
					{
						bool aHasIndex = false;
						for (const auto& aPair : mGamepads)
						{
							if (aPair.second->mIndex == i) 
							{
								aHasIndex = true;
								break;
							}
						}
						if (!aHasIndex)
						{
							aIndex = i;
							Gamepad* aGamepad = new Gamepad();
							aGamepad->mApp = mApp;
							aGamepad->mIndex = aIndex;
							aGamepad->mSDLGamepad = aSDLGamepad;
							aGamepad->mThreshold = 0.4f;
							mGamepads[aID] = aGamepad;
							break;
						}
					}
				}
				if (aIndex == -1)
					SDL_CloseGamepad(aSDLGamepad);
			}
			break;
		case SDL_EVENT_GAMEPAD_REMOVED:
			if (aIndex != -1)
			{
				delete aIt->second;
				mGamepads.erase(aIt);
				if (mCurrentMouse == aIndex)
				{
					mCurrentMouse = -1;
					aSpeedX = 0;
					aSpeedY = 0;
				}
				//reassigning is planned
			}
			break;
		}
	}
}

Gamepad* GamepadManager::GetGamepad(int theIndex)
{
	if (!mInit)
		return nullptr;
	for (const auto& aPair : mGamepads)
	{
		Gamepad* aGamepad = aPair.second;
		int aIndex = aGamepad->mIndex;
		if (aIndex == theIndex && mCurrentMouse != aIndex)
			return aGamepad;
	}
	return nullptr;
}

//actually add a different method where it returns the sprite set name for Resources instead of this
SDL_GamepadType GamepadManager::GetLastUsedType()
{
	return mLastUsedType;
}

void GamepadManager::RumbleAll(float theLow, float theHigh, int theDuration)
{
	if (!mInit)
		return;
	for (const auto& aPair : mGamepads)
		aPair.second->Rumble(theLow, theHigh, theDuration);
}