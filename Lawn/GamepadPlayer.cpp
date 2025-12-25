#include "GamepadPlayer.h"
#include "../Resources.h"
#include "../../LawnApp.h"
#include "../../Lawn/Board.h"
#include "Cutscene.h"
#include "../../Lawn/Widget/SeedChooserScreen.h"
#include "../../Lawn/SeedPacket.h"
#include "../../Lawn/Widget/GameButton.h"
#include "../../Lawn/CursorObject.h"
#include "../../Lawn/ToolTipWidget.h"
#include "../../SexyAppFramework/Dialog.h"

void GamepadPlayer::GamepadPlayerInitialize(int theIndex)
{
	mIndex = theIndex;
	UpdateColor();
	mSeedChooserSeed = SEED_NONE;
	mSeedChooserPrevSeed = SEED_NONE;
	mSeedChooserToolTip = new ToolTipWidget();
	mSeedChooserTimeMoveMotion = -1;
	mSeedChooserTimeButtonMotion = -1;
	mSeedBankIndex = -1;
	mSeedBankPrevIndex = -1;
	mCursorObject = new CursorObject();
	mCursorPreview = new CursorPreview();
}

void GamepadPlayer::Update()
{
	Gamepad* aController = mApp->mGamepadManager->GetGamepad(mIndex);
	if (aController != nullptr)
	{
		bool aIsSeedChoosing = mBoard->mCutScene->mSeedChoosing;

		if (mSeedChooserSeed == SEED_NONE)
		{
			mSeedChooserSeed = mSeedChooserPrevSeed != SEED_NONE ? mSeedChooserPrevSeed : SEED_PEASHOOTER;
			mSeedChooserPrevSeed = SEED_NONE;
		}

		if (mSeedBankIndex == -1 && !aIsSeedChoosing)
		{
			mSeedBankIndex = mSeedBankPrevIndex != -1 ? mSeedBankPrevIndex : 0;
			mSeedBankPrevIndex = -1;
		}

		if (aController->GetButtonDown(SDL_GAMEPAD_BUTTON_START))
		{
			if (!aIsSeedChoosing)
			{
				if (mApp->GetDialogCount() == 0)
				{
					mBoard->UpdateCursor();
					mBoard->ClearCursor();
					mApp->PlaySample(SOUND_PAUSE);
					mApp->DoNewOptions(false);
				}
				else
				{
					mApp->KillNewOptionsDialog();
					mApp->KillDialog(Dialogs::DIALOG_PAUSED);
				}
			}
			else
			{
				//still crashes on this part
				mApp->LawnMessageBox(
					Dialogs::DIALOG_CHOOSER_WARNING,
					_S("[DIALOG_WARNING]"),
					"test",
					_S("[DIALOG_BUTTON_YES]"),
					_S("[REPICK_BUTTON]"),
					Dialog::BUTTONS_YES_NO
				);
					/*
				if (!mApp->mSeedChooserScreen->mStartButton->mDisabled)
					mApp->mSeedChooserScreen->ButtonDepress(mApp->mSeedChooserScreen->mStartButton->mId);
					*/
			}
		}

		if (mBoard->mPaused)
			return;

		if (aController->GetButtonDown(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER))
		{
			mSeedBankIndex--;
			if (mSeedBankIndex < 0)
				mSeedBankIndex = mBoard->mSeedBank->mNumPackets - 1;
		}
		if (aController->GetButtonDown(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))
		{
			mSeedBankIndex++;
			if (mSeedBankIndex >= mBoard->mSeedBank->mNumPackets)
				mSeedBankIndex = 0;
		}
		if (aIsSeedChoosing)
		{
			int aRows = mApp->mSeedChooserScreen->GetRows();
			bool aButtonDPadLeft = aController->GetButton(SDL_GAMEPAD_BUTTON_DPAD_LEFT);
			bool aButtonDPadRight = aController->GetButton(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
			bool aButtonDPadUp = aController->GetButton(SDL_GAMEPAD_BUTTON_DPAD_UP);
			bool aButtonDPadDown = aController->GetButton(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
			float aAxisLeftX = aController->GetAxisValue(SDL_GAMEPAD_AXIS_LEFTX);
			float aAxisLeftY = aController->GetAxisValue(SDL_GAMEPAD_AXIS_LEFTY);
			if ((mSeedChooserTimeMoveMotion == -1 || mApp->mSeedChooserScreen->mSeedChooserAge >= mSeedChooserTimeMoveMotion) && (aButtonDPadLeft || aButtonDPadRight || aButtonDPadUp || aButtonDPadDown || aAxisLeftX != 0 || aAxisLeftY != 0))
			{
				if (aButtonDPadLeft || aAxisLeftX < 0)
				{
					if (!(mSeedChooserSeed % aRows == 0 && mSeedChooserSeed != SEED_IMITATER))
						mSeedChooserSeed = (SeedType)(mSeedChooserSeed - 1);
				}
				if (aButtonDPadRight || aAxisLeftX > 0)
				{
					int aIsLastRow = (mSeedChooserSeed + 1) % aRows == 0;
					if (aIsLastRow && mApp->SeedTypeAvailable(SEED_IMITATER))
						mSeedChooserSeed = SEED_IMITATER;
					else if (!(aIsLastRow && !mApp->SeedTypeAvailable(SEED_IMITATER)))
						mSeedChooserSeed = (SeedType)(mSeedChooserSeed + 1);
				}
				if (aButtonDPadUp || aAxisLeftY < 0)
				{
					if (!(mSeedChooserSeed == SEED_IMITATER || mSeedChooserSeed < aRows))
						mSeedChooserSeed = (SeedType)(mSeedChooserSeed - aRows);
				}
				if (aButtonDPadDown || aAxisLeftY > 0)
				{
					if (!(mSeedChooserSeed >= SEED_IMITATER - aRows))
						mSeedChooserSeed = (SeedType)(mSeedChooserSeed + aRows);
				}
				mSeedChooserSeed = (SeedType)ClampInt(mSeedChooserSeed, SEED_PEASHOOTER, SEED_IMITATER);
				mSeedChooserTimeMoveMotion = mApp->mSeedChooserScreen->mSeedChooserAge + 15;
			}
			else if (!aButtonDPadLeft && !aButtonDPadRight && !aButtonDPadUp && !aButtonDPadDown && aAxisLeftX == 0 && aAxisLeftY == 0)
				mSeedChooserTimeMoveMotion = -1;

			bool aButtonSouth = aController->GetButton(SDL_GAMEPAD_BUTTON_SOUTH);
			bool aButtonEast = aController->GetButton(SDL_GAMEPAD_BUTTON_EAST);
			if ((mSeedChooserTimeButtonMotion == -1 || mApp->mSeedChooserScreen->mSeedChooserAge >= mSeedChooserTimeButtonMotion) && (aButtonSouth || aButtonEast))
			{
				if (aButtonSouth)
					mApp->mSeedChooserScreen->SelectSeedType(mSeedChooserSeed);
				if (aButtonEast)
				{
					int aHighestIndex = 0;
					SeedType aHighestSeedType = SEED_NONE;
					for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
					{
						if (mApp->SeedTypeAvailable(aSeedType))
						{
							ChosenSeed& aChosenSeed = mApp->mSeedChooserScreen->mChosenSeeds[aSeedType];
							if (!aChosenSeed.mCrazyDavePicked && aHighestIndex <= aChosenSeed.mSeedIndexInBank && (aChosenSeed.mSeedState == SEED_IN_BANK || aChosenSeed.mSeedState == SEED_FLYING_TO_BANK))
							{
								aHighestIndex = aChosenSeed.mSeedIndexInBank;
								aHighestSeedType = aSeedType;
							}
						}
					}
					if (aHighestSeedType == SEED_NONE)
					{
						mApp->LeaveBoard(mIndex);
						return;
					}
					mApp->mSeedChooserScreen->SelectSeedType(aHighestSeedType);
				}
				mSeedChooserTimeButtonMotion = mApp->mSeedChooserScreen->mSeedChooserAge + 27;
			}
			else if (!aButtonSouth && !aButtonEast)
				mSeedChooserTimeButtonMotion = -1;
		}
	}
	else if (mSeedBankIndex != -1 || mSeedChooserSeed != SEED_NONE)
	{
		mSeedChooserPrevSeed = mSeedChooserSeed;
		mSeedChooserSeed = SEED_NONE;
		mSeedChooserTimeMoveMotion = -1;
		mSeedChooserTimeButtonMotion = -1;
		mSeedBankPrevIndex = mSeedBankIndex;
		mSeedBankIndex = -1;
	}
}

void GamepadPlayer::UpdateColor()
{
	switch (mIndex)
	{
	case 0:
		mColor = Color(255, 242, 0);
		break;
	case 1:
		mColor = Color(71, 198, 255);
		break;
	case 2:
		mColor = Color(255, 71, 87);
		break;
	case 3:
		mColor = Color(153, 102, 255);
		break;
	case 4:
		mColor = Color(0, 255, 128);
		break;
	case 5:
		mColor = Color(255, 128, 0);
		break;
	case 6:
		mColor = Color(255, 0, 255);
		break;
	case 7:
		mColor = Color(128, 255, 255);
		break;
	default:
		mColor = Color(Rand(255), Rand(255), Rand(255));
		break;
	}
}