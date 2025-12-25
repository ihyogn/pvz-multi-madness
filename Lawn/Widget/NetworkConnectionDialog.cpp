#include "NetworkConnectionDialog.h"
#include "GameButton.h"
#include "../LawnCommon.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../SexyAppFramework/ImageFont.h"
#include "../System/NetworkManager.h"
#include "../../Sexy.TodLib/TodStringFile.h"
#include <string>

NetworkConnectionDialog::NetworkConnectionDialog(LawnApp* theApp) : LawnDialog(
	theApp, 
	Dialogs::DIALOG_NETWORK_CONNECT, 
	true, 
	_S("Multiplayer Connection"),
	_S("Connect to host or start as host?"),
	_S(""), 
	Dialog::BUTTONS_NONE)
{
	mVerticalCenterText = false;
	
	// Create host and connect buttons (using MakeButton from GameButton.h)
	mHostButton = MakeButton(100, this, _S("Start as Host"));
	mConnectButton = MakeButton(101, this, _S("Connect to Host"));
	
	mIPEditWidget = CreateEditWidget(0, this, this);
	mIPEditWidget->mMaxChars = 64;
	mIPEditWidget->AddWidthCheckFont(FONT_BRIANNETOD16, 400);
	mIPEditWidget->SetText(_S("127.0.0.1"), true);  // Default to localhost
	
	mPortEditWidget = CreateEditWidget(1, this, this);
	mPortEditWidget->mMaxChars = 5;
	mPortEditWidget->AddWidthCheckFont(FONT_BRIANNETOD16, 100);
	mPortEditWidget->SetText(_S("7777"), true);  // Default port
	
	CalcSize(450, 250);
}

NetworkConnectionDialog::~NetworkConnectionDialog()
{
	delete mIPEditWidget;
	delete mPortEditWidget;
	delete mHostButton;
	delete mConnectButton;
}

void NetworkConnectionDialog::AddedToManager(Sexy::WidgetManager* theWidgetManager)
{
	LawnDialog::AddedToManager(theWidgetManager);
	AddWidget(mIPEditWidget);
	AddWidget(mPortEditWidget);
	AddWidget(mHostButton);
	AddWidget(mConnectButton);
	theWidgetManager->SetFocus(mIPEditWidget);
}

void NetworkConnectionDialog::RemovedFromManager(Sexy::WidgetManager* theWidgetManager)
{
	LawnDialog::RemovedFromManager(theWidgetManager);
	RemoveWidget(mIPEditWidget);
	RemoveWidget(mPortEditWidget);
	RemoveWidget(mHostButton);
	RemoveWidget(mConnectButton);
}

int NetworkConnectionDialog::GetPreferredHeight(int theWidth)
{
	return LawnDialog::GetPreferredHeight(theWidth) + 100;
}

void NetworkConnectionDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
	LawnDialog::Resize(theX, theY, theWidth, theHeight);
	int buttonY = mHeight - 150;
	int buttonWidth = 150;
	int buttonSpacing = 20;
	int totalWidth = buttonWidth * 2 + buttonSpacing;
	int startX = (mWidth - totalWidth) / 2;
	
	mHostButton->Resize(startX, buttonY, buttonWidth, 35);
	mConnectButton->Resize(startX + buttonWidth + buttonSpacing, buttonY, buttonWidth, 35);
	
	int editY = mHeight - 280;
	mIPEditWidget->Resize(mContentInsets.mLeft + 12, editY, mWidth - mContentInsets.mLeft - mContentInsets.mRight - 24, 28);
	mPortEditWidget->Resize(mContentInsets.mLeft + 12, editY + 45, mWidth - mContentInsets.mLeft - mContentInsets.mRight - 24, 28);
}

void NetworkConnectionDialog::Draw(Sexy::Graphics* g)
{
	LawnDialog::Draw(g);
	DrawEditBox(g, mIPEditWidget);
	DrawEditBox(g, mPortEditWidget);
	
	// Draw labels
	EnsureFonts();
	if (mLinesFont)
	{
		int labelY = mHeight - 250;
		g->SetFont(mLinesFont);
		g->SetColor(Sexy::Color(0xE0, 0xBB, 0x62));
		g->DrawString(_S("IP Address:"), mContentInsets.mLeft + 12, labelY);
		g->DrawString(_S("Port:"), mContentInsets.mLeft + 12, labelY + 45);
	}
	
	// Draw connection status if connected
	if (mApp->mNetworkManager && mApp->mNetworkManager->IsConnected())
	{
		EnsureFonts();
		if (mLinesFont)
		{
			g->SetFont(mLinesFont);
			g->SetColor(Sexy::Color(0, 255, 0));
			SexyString statusText = _S("Connected!");
			int statusX = mWidth / 2 - mLinesFont->StringWidth(statusText) / 2;
			g->DrawString(statusText, statusX, mHeight - 80);
		}
	}
}

void NetworkConnectionDialog::EditWidgetText(int theId, const SexyString& theString)
{
	mApp->ButtonDepress(mId + 2000);
}

bool NetworkConnectionDialog::AllowChar(int theId, SexyChar theChar)
{
	if (theId == 0)  // IP address
	{
		return isalnum(theChar) || theChar == _S('.') || theChar == _S(':') || theChar == _S('-');
	}
	else if (theId == 1)  // Port
	{
		return isdigit(theChar);
	}
	return false;
}

void NetworkConnectionDialog::ButtonDepress(int theId)
{
	if (theId == 100)  // Host button
	{
		if (!mApp->mNetworkManager)
		{
			mApp->DoDialog(
				Dialogs::DIALOG_MESSAGE,
				true,
				_S("Error"),
				_S("Network manager not initialized."),
				_S("OK"),
				Dialog::BUTTONS_FOOTER
			);
			return;
		}
		
		SexyString portStr = mPortEditWidget->mString;
		if (portStr.empty())
			portStr = _S("7777");
		
		int port = atoi(portStr.c_str());
		if (port <= 0 || port > 65535)
			port = 7777;
		
		// Start hosting
		if (mApp->mNetworkManager->StartHost(port))
		{
			// Send zombie selection to clients automatically (best zombies are set in StartHost)
			if (mApp->mNetworkManager->IsHost())
			{
				ZombieType selectedZombies[NUM_ZOMBIE_TYPES];
				int count = 0;
				for (int i = 0; i < NUM_ZOMBIE_TYPES && count < 20; i++)
				{
					ZombieType zType = (ZombieType)i;
					if (mApp->mNetworkManager->IsZombieSelected(zType))
					{
						selectedZombies[count++] = zType;
					}
				}
				if (count > 0)
				{
					mApp->mNetworkManager->SendZombieSelection(selectedZombies, count);
				}
			}
			
			// Close dialog - host started successfully, start game directly
			mApp->KillDialog(Dialogs::DIALOG_NETWORK_CONNECT);
			mApp->mGameMode = GameMode::GAMEMODE_MULTIPLAYER_ONLINE;
			mApp->PreNewGame(GameMode::GAMEMODE_MULTIPLAYER_ONLINE, true);
		}
		else
		{
			mApp->DoDialog(
				Dialogs::DIALOG_MESSAGE,
				true,
				_S("Host Failed"),
				_S("Could not start hosting. Port may be in use."),
				_S("OK"),
				Dialog::BUTTONS_FOOTER
			);
		}
	}
	else if (theId == 101)  // Connect button
	{
		if (!mApp->mNetworkManager)
		{
			mApp->DoDialog(
				Dialogs::DIALOG_MESSAGE,
				true,
				_S("Error"),
				_S("Network manager not initialized."),
				_S("OK"),
				Dialog::BUTTONS_FOOTER
			);
			return;
		}
		
		SexyString ipStr = mIPEditWidget->mString;
		if (ipStr.empty())
			ipStr = _S("127.0.0.1");
		
		SexyString portStr = mPortEditWidget->mString;
		if (portStr.empty())
			portStr = _S("7777");
		
		int port = atoi(portStr.c_str());
		if (port <= 0 || port > 65535)
			port = 7777;
		
		// Try to connect
		bool connected = false;
		if (ipStr == _S("localhost") || ipStr == _S("127.0.0.1"))
		{
			connected = mApp->mNetworkManager->ConnectToLocalhost(port);
		}
		else
		{
			connected = mApp->mNetworkManager->ConnectToHost(ipStr.c_str(), port);
		}
		
		if (connected)
		{
			// Close dialog - connection successful
			mApp->KillDialog(Dialogs::DIALOG_NETWORK_CONNECT);
			// Start the game in multiplayer mode
			mApp->mGameMode = GameMode::GAMEMODE_MULTIPLAYER_ONLINE;
			mApp->PreNewGame(GameMode::GAMEMODE_MULTIPLAYER_ONLINE, true);
		}
		else
		{
			mApp->DoDialog(
				Dialogs::DIALOG_MESSAGE,
				true,
				_S("Connection Failed"),
				_S("Could not connect to host. Make sure the host is running and the IP/port is correct."),
				_S("OK"),
				Dialog::BUTTONS_FOOTER
			);
		}
	}
	else
	{
		LawnDialog::ButtonDepress(theId);
	}
}

