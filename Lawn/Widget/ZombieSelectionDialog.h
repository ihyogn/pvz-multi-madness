#ifndef __ZOMBIESELECTIONDIALOG_H__
#define __ZOMBIESELECTIONDIALOG_H__

#include "LawnDialog.h"

class LawnStoneButton;

class ZombieSelectionDialog : public LawnDialog
{
public:
	ZombieSelectionDialog(LawnApp* theApp);
	virtual ~ZombieSelectionDialog();

	virtual void		Resize(int theX, int theY, int theWidth, int theHeight);
	virtual int			GetPreferredHeight(int theWidth);
	virtual void		AddedToManager(Sexy::WidgetManager* theWidgetManager);
	virtual void		RemovedFromManager(Sexy::WidgetManager* theWidgetManager);
	virtual void		Draw(Sexy::Graphics* g);
	virtual void		ButtonDepress(int theId);
	virtual void		MouseDown(int x, int y, int theClickCount);
	
private:
	static const int MAX_ZOMBIE_BUTTONS = 20;
	LawnStoneButton* mZombieButtons[MAX_ZOMBIE_BUTTONS];
	int mZombieButtonCount;
	
	void SetupZombieButtons();
	void UpdateButtonStates();
	ZombieType GetZombieTypeForButton(int theButtonId);
};

#endif

