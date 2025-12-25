#ifndef __GAMEPADPLAYER_H__
#define __GAMEPADPLAYER_H__

#include "GameObject.h"

using namespace Sexy;

class CursorObject;
class CursorPreview;
class ToolTipWidget;

class GamepadPlayer : public GameObject
{
public:
	int					mIndex;
	Color				mColor;
	SeedType			mSeedChooserSeed;
	SeedType			mSeedChooserPrevSeed;
	ToolTipWidget*		mSeedChooserToolTip;
	int					mSeedChooserTimeMoveMotion;
	int					mSeedChooserTimeButtonMotion;
	int					mSeedBankIndex;
	int					mSeedBankPrevIndex;
	CursorObject*		mCursorObject;
	CursorPreview*		mCursorPreview;

public:
	void				GamepadPlayerInitialize(int theIndex);
	void				Update();
	void				UpdateColor();
};

#endif
