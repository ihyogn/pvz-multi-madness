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
	mShowIPButton = MakeButton(102, this, _S("Show My IP"));
	
	mIPEditWidget = CreateEditWidget(0, this, this);
	mIPEditWidget->mMaxChars = 64;
	mIPEditWidget->AddWidthCheckFont(FONT_BRIANNETOD16, 400);
	// Don't default to localhost - user must enter network IP for multi-PC play
	mIPEditWidget->SetText(_S(""), true);  // Empty - user must enter host IP
	
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
	delete mShowIPButton;
}

void NetworkConnectionDialog::AddedToManager(Sexy::WidgetManager* theWidgetManager)
{
	LawnDialog::AddedToManager(theWidgetManager);
	AddWidget(mIPEditWidget);
	AddWidget(mPortEditWidget);
	AddWidget(mHostButton);
	AddWidget(mConnectButton);
	AddWidget(mShowIPButton);
	theWidgetManager->SetFocus(mIPEditWidget);
}

void NetworkConnectionDialog::RemovedFromManager(Sexy::WidgetManager* theWidgetManager)
{
	LawnDialog::RemovedFromManager(theWidgetManager);
	RemoveWidget(mIPEditWidget);
	RemoveWidget(mPortEditWidget);
	RemoveWidget(mHostButton);
	RemoveWidget(mConnectButton);
	RemoveWidget(mShowIPButton);
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
	
	// Show IP button below the other buttons
	int showIPY = buttonY + 50;
	mShowIPButton->Resize((mWidth - buttonWidth) / 2, showIPY, buttonWidth, 30);
	
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
		
		// IMPORTANT: You don't need to enter an IP when hosting!
		// The host automatically binds to all network interfaces.
		// Your IP (192.168.1.23) is only needed for CLIENTS to connect TO you.
		
		// Start hosting
		if (mApp->mNetworkManager->StartHost(port))
		{
			// Get and show the host's IP address
			char localIP[256];
			mApp->mNetworkManager->GetHostIPAddress(localIP, sizeof(localIP));
			
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
			
			// Show success message with IP
			SexyString successMsg = _S("Host started successfully!\n\nYour IP: ") + SexyString(localIP) + 
			                        _S("\nPort: ") + portStr + 
			                        _S("\n\nGive this IP to other players to connect.\n\nMake sure Windows Firewall allows the connection!");
			
			mApp->DoDialog(
				Dialogs::DIALOG_MESSAGE,
				true,
				_S("Hosting Started"),
				successMsg,
				_S("OK"),
				Dialog::BUTTONS_FOOTER
			);
			
			// Close connection dialog and start game
			mApp->KillDialog(Dialogs::DIALOG_NETWORK_CONNECT);
			mApp->mGameMode = GameMode::GAMEMODE_MULTIPLAYER_ONLINE;
			mApp->PreNewGame(GameMode::GAMEMODE_MULTIPLAYER_ONLINE, true);
		}
		else
		{
			// More detailed error message
			SexyString errorMsg = _S("Could not start hosting on port ") + portStr + _S(".\n\nPossible causes:\n");
			errorMsg += _S("1. Port is already in use (close other instances)\n");
			errorMsg += _S("2. Windows Firewall is blocking\n");
			errorMsg += _S("3. Antivirus is blocking\n");
			errorMsg += _S("4. Insufficient permissions\n\n");
			errorMsg += _S("Try:\n- Closing and restarting the game\n");
			errorMsg += _S("- Temporarily disabling firewall to test\n");
			errorMsg += _S("- Using a different port");
			
			mApp->DoDialog(
				Dialogs::DIALOG_MESSAGE,
				true,
				_S("Host Failed"),
				errorMsg,
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
		// Don't default to localhost - user must enter network IP
		if (ipStr.empty())
		{
			mApp->DoDialog(
				Dialogs::DIALOG_MESSAGE,
				true,
				_S("IP Address Required"),
				_S("Please enter the host's IP address.\n\nFor same PC testing: 127.0.0.1\nFor network play: Use the host's network IP (e.g., 192.168.1.100)\n\nLocalhost (127.0.0.1) ONLY works on the same PC!"),
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
		
		// Warn if using localhost for network connection
		if (ipStr == _S("localhost") || ipStr == _S("127.0.0.1"))
		{
			mApp->DoDialog(
				Dialogs::DIALOG_MESSAGE,
				true,
				_S("Localhost Warning"),
				_S("You're connecting to localhost (127.0.0.1).\n\nThis ONLY works if the host is on the SAME PC!\n\nFor network play, use the host's network IP address\n(e.g., 192.168.1.100) shown in the host dialog."),
				_S("Continue Anyway"),
				Dialog::BUTTONS_FOOTER
			);
		}
		
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
				_S("Could not connect to host.\n\nFor LOCAL network (same router):\n- Use the host's LOCAL IP address\n- NO port forwarding needed\n- Make sure both PCs are on the same network\n\nFor INTERNET play:\n- Use the host's EXTERNAL IP address\n- Host MUST forward port 7777 in their router\n\nCheck:\n- Host is running and waiting for players\n- IP address and port (7777) are correct\n- Windows Firewall allows the connection"),
				_S("OK"),
				Dialog::BUTTONS_FOOTER
			);
		}
	}
	else if (theId == 102)  // Show My IP button
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
		
		char localIP[256];
		char externalIP[256];
		bool hasLocal = mApp->mNetworkManager->GetHostIPAddress(localIP, sizeof(localIP));
		bool hasExternal = mApp->mNetworkManager->GetExternalIPAddress(externalIP, sizeof(externalIP));
		
		SexyString message;
		if (hasLocal && hasExternal)
		{
			bool isLocalhost = (strcmp(localIP, "127.0.0.1") == 0);
			if (isLocalhost)
			{
				message = _S("Your Network IP: NOT FOUND\n(Only localhost detected)\n\nExternal IP: ") + SexyString(externalIP) + 
				          _S("\n\nTo find your network IP:\n1. Open Command Prompt\n2. Type: ipconfig\n3. Look for 'IPv4 Address'\n4. Use the address that's NOT 127.0.0.1");
			}
			else
			{
				message = _S("Your Network IP: ") + SexyString(localIP) + 
				          _S("\n(Use this for local network play)\n\nExternal IP: ") + SexyString(externalIP) + 
				          _S("\n(Use this for internet play)\n\nPort: 7777");
			}
		}
		else if (hasLocal)
		{
			bool isLocalhost = (strcmp(localIP, "127.0.0.1") == 0);
			if (isLocalhost)
			{
				message = _S("Your Network IP: NOT FOUND\n(Only localhost detected)\n\nTo find your network IP:\n1. Open Command Prompt (Win+R, type 'cmd')\n2. Type: ipconfig\n3. Look for 'IPv4 Address' under your network adapter\n4. Use the address that's NOT 127.0.0.1\n\nExample: 192.168.1.100");
			}
			else
			{
				message = _S("Your Network IP: ") + SexyString(localIP) + 
				          _S("\n\nUse this IP address for local network play.\n\nPort: 7777");
			}
		}
		else
		{
			message = _S("Could not detect your IP address.\n\nTo find it manually:\n1. Press Win+R, type 'cmd', press Enter\n2. Type: ipconfig\n3. Look for 'IPv4 Address'\n4. Use the address that's NOT 127.0.0.1");
		}
		
		mApp->DoDialog(
			Dialogs::DIALOG_MESSAGE,
			true,
			_S("Your IP Address"),
			message,
			_S("OK"),
			Dialog::BUTTONS_FOOTER
		);
	}
	else
	{
		LawnDialog::ButtonDepress(theId);
	}
}

