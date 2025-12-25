#ifndef __NETWORKCONNECTIONDIALOG_H__
#define __NETWORKCONNECTIONDIALOG_H__

#include "LawnDialog.h"
#include "../../SexyAppFramework/EditListener.h"

namespace Sexy
{
	class EditWidget;
	class DialogButton;
}

class NetworkConnectionDialog : public LawnDialog, public EditListener
{
public:
	Sexy::EditWidget*			mIPEditWidget;
	Sexy::EditWidget*			mPortEditWidget;
	Sexy::DialogButton*			mHostButton;
	Sexy::DialogButton*			mConnectButton;
	
public:
	NetworkConnectionDialog(LawnApp* theApp);
	virtual ~NetworkConnectionDialog();

	virtual void		Resize(int theX, int theY, int theWidth, int theHeight);
	virtual int			GetPreferredHeight(int theWidth);
	virtual void		AddedToManager(Sexy::WidgetManager* theWidgetManager);
	virtual void		RemovedFromManager(Sexy::WidgetManager* theWidgetManager);
	virtual void		Draw(Sexy::Graphics* g);
	virtual void		EditWidgetText(int theId, const SexyString& theString);
	virtual bool		AllowChar(int theId, SexyChar theChar);
	virtual void		ButtonDepress(int theId);
};

#endif

