#include "ZombieSelectionDialog.h"
#include "GameButton.h"
#include "../LawnCommon.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../../SexyAppFramework/WidgetManager.h"
#include "../../SexyAppFramework/ImageFont.h"
#include "../System/NetworkManager.h"
#include "../../Sexy.TodLib/TodStringFile.h"
#include "../Zombie.h"
#include <cstring>

// Standard zombies available in I, Zombie mode (excluding boss and special ones)
static ZombieType gAvailableZombies[] = {
    ZOMBIE_NORMAL,
    ZOMBIE_TRAFFIC_CONE,
    ZOMBIE_POLEVAULTER,
    ZOMBIE_PAIL,
    ZOMBIE_LADDER,
    ZOMBIE_DIGGER,
    ZOMBIE_BUNGEE,
    ZOMBIE_FOOTBALL,
    ZOMBIE_BALLOON,
    ZOMBIE_DOOR,
    ZOMBIE_ZAMBONI,
    ZOMBIE_POGO,
    ZOMBIE_DANCER,
    ZOMBIE_GARGANTUAR,
    ZOMBIE_IMP
};

static const int NUM_AVAILABLE_ZOMBIES = sizeof(gAvailableZombies) / sizeof(gAvailableZombies[0]);

ZombieSelectionDialog::ZombieSelectionDialog(LawnApp* theApp) : LawnDialog(
    theApp,
    Dialogs::DIALOG_ZOMBIE_SELECTION,
    true,
    _S("Select Zombies for Player 2"),
    _S("Choose which zombies Player 2 can use:"),
    _S(""),
    Dialog::BUTTONS_NONE)
{
    mVerticalCenterText = false;
    mZombieButtonCount = 0;
    memset(mZombieButtons, 0, sizeof(mZombieButtons));
    
    SetupZombieButtons();
    CalcSize(600, 500);
}

ZombieSelectionDialog::~ZombieSelectionDialog()
{
    for (int i = 0; i < mZombieButtonCount; i++)
    {
        delete mZombieButtons[i];
    }
}

void ZombieSelectionDialog::SetupZombieButtons()
{
    if (!mApp->mNetworkManager)
        return;
    
    // Default: select all standard zombies
    mApp->mNetworkManager->ClearZombieSelection();
    for (int i = 0; i < NUM_AVAILABLE_ZOMBIES; i++)
    {
        mApp->mNetworkManager->SetZombieSelected(gAvailableZombies[i], true);
    }
    
    // Create buttons for each zombie type
    const int BUTTONS_PER_ROW = 3;
    const int BUTTON_WIDTH = 150;
    const int BUTTON_HEIGHT = 30;
    const int BUTTON_SPACING_X = 20;
    const int BUTTON_SPACING_Y = 10;
    const int START_X = mContentInsets.mLeft + 20;
    const int START_Y = 150;
    
    for (int i = 0; i < NUM_AVAILABLE_ZOMBIES && i < MAX_ZOMBIE_BUTTONS; i++)
    {
        ZombieType zType = gAvailableZombies[i];
        ZombieDefinition& zDef = GetZombieDefinition(zType);
        SexyString buttonLabel = TodStringTranslate(zDef.mZombieName);
        
        int row = i / BUTTONS_PER_ROW;
        int col = i % BUTTONS_PER_ROW;
        int x = START_X + col * (BUTTON_WIDTH + BUTTON_SPACING_X);
        int y = START_Y + row * (BUTTON_HEIGHT + BUTTON_SPACING_Y);
        
        LawnStoneButton* button = MakeButton(200 + i, this, buttonLabel);
        button->Resize(x, y, BUTTON_WIDTH, BUTTON_HEIGHT);
        mZombieButtons[mZombieButtonCount++] = button;
    }
    
    // Add OK button
    LawnStoneButton* okButton = MakeButton(100, this, _S("OK"));
    okButton->Resize(mWidth / 2 - 75, mHeight - 80, 150, 35);
    mZombieButtons[mZombieButtonCount++] = okButton;
    
    UpdateButtonStates();
}

void ZombieSelectionDialog::UpdateButtonStates()
{
    if (!mApp->mNetworkManager)
        return;
    
    for (int i = 0; i < NUM_AVAILABLE_ZOMBIES && i < mZombieButtonCount - 1; i++)
    {
        ZombieType zType = gAvailableZombies[i];
        bool selected = mApp->mNetworkManager->IsZombieSelected(zType);
        
        // Visual feedback: darken unselected buttons
        if (mZombieButtons[i])
        {
            // Button state is handled by toggling selection on click
        }
    }
}

ZombieType ZombieSelectionDialog::GetZombieTypeForButton(int theButtonId)
{
    if (theButtonId >= 200 && theButtonId < 200 + NUM_AVAILABLE_ZOMBIES)
    {
        int index = theButtonId - 200;
        if (index >= 0 && index < NUM_AVAILABLE_ZOMBIES)
            return gAvailableZombies[index];
    }
    return ZOMBIE_INVALID;
}

void ZombieSelectionDialog::AddedToManager(Sexy::WidgetManager* theWidgetManager)
{
    LawnDialog::AddedToManager(theWidgetManager);
    for (int i = 0; i < mZombieButtonCount; i++)
    {
        if (mZombieButtons[i])
            AddWidget(mZombieButtons[i]);
    }
}

void ZombieSelectionDialog::RemovedFromManager(Sexy::WidgetManager* theWidgetManager)
{
    LawnDialog::RemovedFromManager(theWidgetManager);
    for (int i = 0; i < mZombieButtonCount; i++)
    {
        if (mZombieButtons[i])
            RemoveWidget(mZombieButtons[i]);
    }
}

void ZombieSelectionDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
    LawnDialog::Resize(theX, theY, theWidth, theHeight);
    
    // Reposition buttons
    const int BUTTONS_PER_ROW = 3;
    const int BUTTON_WIDTH = 150;
    const int BUTTON_HEIGHT = 30;
    const int BUTTON_SPACING_X = 20;
    const int BUTTON_SPACING_Y = 10;
    const int START_X = mContentInsets.mLeft + 20;
    const int START_Y = 150;
    
    for (int i = 0; i < NUM_AVAILABLE_ZOMBIES && i < mZombieButtonCount - 1; i++)
    {
        int row = i / BUTTONS_PER_ROW;
        int col = i % BUTTONS_PER_ROW;
        int x = START_X + col * (BUTTON_WIDTH + BUTTON_SPACING_X);
        int y = START_Y + row * (BUTTON_HEIGHT + BUTTON_SPACING_Y);
        
        if (mZombieButtons[i])
            mZombieButtons[i]->Resize(x, y, BUTTON_WIDTH, BUTTON_HEIGHT);
    }
    
    // Reposition OK button
    if (mZombieButtons[mZombieButtonCount - 1])
        mZombieButtons[mZombieButtonCount - 1]->Resize(mWidth / 2 - 75, mHeight - 80, 150, 35);
}

int ZombieSelectionDialog::GetPreferredHeight(int theWidth)
{
    return 500;
}

void ZombieSelectionDialog::Draw(Sexy::Graphics* g)
{
    LawnDialog::Draw(g);
    
    EnsureFonts();
    if (mLinesFont)
    {
        g->SetFont(mLinesFont);
        g->SetColor(Sexy::Color(0xE0, 0xBB, 0x62));
        
        // Draw selection status for each button
        for (int i = 0; i < NUM_AVAILABLE_ZOMBIES && i < mZombieButtonCount - 1; i++)
        {
            if (!mZombieButtons[i])
                continue;
                
            ZombieType zType = gAvailableZombies[i];
            bool selected = mApp->mNetworkManager && mApp->mNetworkManager->IsZombieSelected(zType);
            
            int x = mZombieButtons[i]->mX;
            int y = mZombieButtons[i]->mY - 2;
            SexyString status = selected ? _S("[X]") : _S("[ ]");
            g->DrawString(status, x - 20, y + mLinesFont->GetAscent());
        }
    }
}

void ZombieSelectionDialog::ButtonDepress(int theId)
{
    if (theId == 100)  // OK button
    {
        // Send zombie selection to clients before starting game
        if (mApp->mNetworkManager && mApp->mNetworkManager->IsHost())
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
        
        // Close dialog and start the game
        mApp->KillDialog(Dialogs::DIALOG_ZOMBIE_SELECTION);
        mApp->mGameMode = GameMode::GAMEMODE_MULTIPLAYER_ONLINE;
        mApp->PreNewGame(GameMode::GAMEMODE_MULTIPLAYER_ONLINE, true);
    }
    else if (theId >= 200 && theId < 200 + NUM_AVAILABLE_ZOMBIES)  // Zombie toggle button
    {
        ZombieType zType = GetZombieTypeForButton(theId);
        if (zType != ZOMBIE_INVALID && mApp->mNetworkManager)
        {
            bool currentState = mApp->mNetworkManager->IsZombieSelected(zType);
            mApp->mNetworkManager->SetZombieSelected(zType, !currentState);
            UpdateButtonStates();
            MarkDirty();
        }
    }
    else
    {
        LawnDialog::ButtonDepress(theId);
    }
}

void ZombieSelectionDialog::MouseDown(int x, int y, int theClickCount)
{
    Widget::MouseDown(x, y, theClickCount);
}

