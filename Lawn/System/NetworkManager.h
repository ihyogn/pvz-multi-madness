#ifndef __NETWORKMANAGER_H__
#define __NETWORKMANAGER_H__

#include "../../ConstEnums.h"

enum NetworkPlayerRole
{
    NETWORK_ROLE_NONE = 0,
    NETWORK_ROLE_HOST_PLANTS = 1,  // Player 1 - Host plays as plants
    NETWORK_ROLE_CLIENT_ZOMBIES = 2,  // Player 2 - Client plays as zombies
    NETWORK_ROLE_CLIENT_HAMMER = 3  // Player 3 - Hammer player
};

enum NetworkMessageType
{
    NETMSG_CONNECT = 1,
    NETMSG_DISCONNECT = 2,
    NETMSG_PLANT_PLACED = 3,
    NETMSG_ZOMBIE_PLACED = 4,
    NETMSG_HAMMER_STRIKE = 5,
    NETMSG_GAME_STATE = 6,
    NETMSG_ZOMBIE_SELECTION = 7,
    NETMSG_SYNC = 8,
    NETMSG_PLANT_DIED = 9,
    NETMSG_ZOMBIE_DIED = 10,
    NETMSG_GAME_WON = 11,
    NETMSG_GAME_LOST = 12,
    NETMSG_GAME_RESTART = 13,
    NETMSG_ZOMBIE_POSITION = 14,
    NETMSG_ZOMBIE_HEALTH = 15,
    NETMSG_ZOMBIE_STATE = 16,
    NETMSG_PLANT_STATE = 17,
    NETMSG_ZOMBIE_FULL_SYNC = 18  // Complete zombie data sync (all properties in one message)
};

class NetworkManager
{
public:
    // Complete zombie sync data - all zombie properties in one message for reliable sync
    struct ZombieFullSyncData
    {
        unsigned int mZombieID;
        ZombieType mZombieType;
        float mPosX;
        float mPosY;
        int mRow;
        int mBodyHealth;
        int mHelmHealth;
        int mShieldHealth;
        ZombiePhase mZombiePhase;
        int mFrame;
        bool mIsEating;
        float mVelX;
        int mPhaseCounter;
        bool mDead;
    };

    NetworkManager();
    ~NetworkManager();

    bool IsHost() const { return mIsHost; }
    bool IsConnected() const { return mIsConnected; }
    int GetPlayerCount() const { return mPlayerCount; }
    NetworkPlayerRole GetMyRole() const { return mMyRole; }
    int GetMyPlayerID() const { return mMyPlayerID; }

    // Host functions
    bool StartHost(int thePort = 7777);
    void StopHost();
    void UpdateHost();
    bool GetHostIPAddress(char* theBuffer, int theBufferSize);  // Get local IP for display
    bool GetExternalIPAddress(char* theBuffer, int theBufferSize);  // Get external IP for internet play
    
    // Client functions
    bool ConnectToHost(const char* theHostIP, int thePort = 7777);
    bool ConnectToLocalhost(int thePort = 7777);  // Convenience function for testing
    void Disconnect();
    void UpdateClient();

    // Common functions
    void Update();
    void SendPlantPlaced(int theGridX, int theGridY, SeedType theSeedType);
    void SendZombiePlaced(int theGridX, int theGridY, ZombieType theZombieType);
    void SendHammerStrike(int theX, int theY);
    void SendZombieSelection(const ZombieType* theZombieTypes, int theCount);
    void SendGameState();
    void SendPlantDied(int theGridX, int theGridY);
    void SendZombieDied(int theGridX, int theGridY);
    void SendGameWon();
    void SendGameLost();
    void SendGameRestart();
    void SendZombiePosition(unsigned int theZombieID, float thePosX, float thePosY, int theRow);
    void SendZombieHealth(unsigned int theZombieID, int theBodyHealth, int theHelmHealth, int theShieldHealth);
    void SendZombieState(unsigned int theZombieID, ZombiePhase thePhase, int theFrame, bool theIsEating, float theVelX, int thePhaseCounter);
    void SendZombieFullSyncData(const ZombieFullSyncData& theData);  // Send complete zombie data

    bool HasPendingPlantPlace() const { return mPendingPlantPlace.mGridX != -1; }
    bool HasPendingZombiePlace() const { return mPendingZombiePlace.mGridX != -1; }
    bool HasPendingHammerStrike() const { return mPendingHammerStrike.mX != -1; }
    bool HasPendingPlantDeath() const { return mPendingPlantDeath.mGridX != -1; }
    bool HasPendingZombieDeath() const { return mPendingZombieDeath.mGridX != -1; }
    bool HasPendingGameWon() const { return mPendingGameWon; }
    bool HasPendingGameLost() const { return mPendingGameLost; }
    bool HasPendingGameRestart() const { return mPendingGameRestart; }
    
    // Zombie sync data (map from zombie ID to position/health)
    struct PendingZombieSync
    {
        unsigned int mZombieID;
        ZombieType mZombieType;  // For creating zombies that don't exist
        float mPosX;
        float mPosY;
        int mRow;
        int mBodyHealth;
        int mHelmHealth;
        int mShieldHealth;
        ZombiePhase mZombiePhase;
        int mFrame;
        bool mIsEating;
        float mVelX;
        int mPhaseCounter;
        bool mHasPosition;
        bool mHasHealth;
        bool mHasState;
        bool mDead;  // For syncing death state
        bool mNeedsCreation;  // Flag to create zombie if it doesn't exist
        PendingZombieSync() : mZombieID(0), mZombieType(ZombieType::ZOMBIE_NORMAL), mPosX(0), mPosY(0), mRow(0), 
            mBodyHealth(0), mHelmHealth(0), mShieldHealth(0), 
            mZombiePhase(ZombiePhase::PHASE_ZOMBIE_NORMAL), mFrame(0), mIsEating(false), mVelX(0), mPhaseCounter(0),
            mHasPosition(false), mHasHealth(false), mHasState(false), mDead(false), mNeedsCreation(false) {}
    };
    
    // Simple map for zombie sync (limited size for simplicity)
    static const int MAX_PENDING_ZOMBIE_SYNC = 100;
    PendingZombieSync mPendingZombieSyncs[MAX_PENDING_ZOMBIE_SYNC];
    int mPendingZombieSyncCount;
    
    PendingZombieSync* GetOrCreatePendingZombieSync(unsigned int theZombieID);
    
    struct PendingPlantPlace
    {
        int mGridX;
        int mGridY;
        SeedType mSeedType;
        PendingPlantPlace() : mGridX(-1), mGridY(-1), mSeedType(SEED_NONE) {}
    };
    
    struct PendingZombiePlace
    {
        int mGridX;
        int mGridY;
        ZombieType mZombieType;
        PendingZombiePlace() : mGridX(-1), mGridY(-1), mZombieType(ZOMBIE_NORMAL) {}
    };
    
    struct PendingHammerStrike
    {
        int mX;
        int mY;
        int mPlayerID;
        PendingHammerStrike() : mX(-1), mY(-1), mPlayerID(-1) {}
    };
    
    struct PendingPlantDeath
    {
        int mGridX;
        int mGridY;
        PendingPlantDeath() : mGridX(-1), mGridY(-1) {}
    };
    
    struct PendingZombieDeath
    {
        int mGridX;
        int mGridY;
        PendingZombieDeath() : mGridX(-1), mGridY(-1) {}
    };

    struct PlantPlaceData
    {
        int mGridX;
        int mGridY;
        SeedType mSeedType;
    };
    
    struct ZombiePlaceData
    {
        int mGridX;
        int mGridY;
        ZombieType mZombieType;
    };
    
    struct HammerStrikeData
    {
        int mX;
        int mY;
        int mPlayerID;
    };
    
    struct PlantDeathData
    {
        int mGridX;
        int mGridY;
    };
    
    struct ZombieDeathData
    {
        int mGridX;
        int mGridY;
    };
    
    struct ZombiePositionData
    {
        unsigned int mZombieID;  // DataArray ID for the zombie
        float mPosX;
        float mPosY;
        int mRow;
    };
    
    struct ZombieHealthData
    {
        unsigned int mZombieID;  // DataArray ID for the zombie
        int mBodyHealth;
        int mHelmHealth;
        int mShieldHealth;
    };
    
    struct ZombieStateData
    {
        unsigned int mZombieID;  // DataArray ID for the zombie
        ZombiePhase mZombiePhase;
        int mFrame;
        bool mIsEating;
        float mVelX;
        int mPhaseCounter;
    };
    
    struct ConnectResponse
    {
        int mPlayerID;
        NetworkPlayerRole mRole;
    };

    PendingPlantPlace mPendingPlantPlace;
    PendingZombiePlace mPendingZombiePlace;
    PendingHammerStrike mPendingHammerStrike;
    PendingPlantDeath mPendingPlantDeath;
    PendingZombieDeath mPendingZombieDeath;
    bool mPendingGameWon;
    bool mPendingGameLost;
    bool mPendingGameRestart;

    // Hammer cooldown (in game ticks)
    int mHammerCooldown;
    static const int HAMMER_COOLDOWN_TIME = 120;  // 2 seconds at 60 FPS
    
    // Zombie selection for Player 2 (host selects which zombies Player 2 can use)
    bool mZombieSelected[NUM_ZOMBIE_TYPES];
    int mSelectedZombieCount;
    bool mZombieSelectionUpdated;  // Flag to signal seed bank needs updating
    void ClearZombieSelection();
    void SetZombieSelected(ZombieType theZombieType, bool theSelected);
    bool IsZombieSelected(ZombieType theZombieType) const;
    bool HasZombieSelectionUpdated() const { return mZombieSelectionUpdated; }
    void ClearZombieSelectionUpdated() { mZombieSelectionUpdated = false; }

private:
    bool mIsHost;
    bool mIsConnected;
    int mPlayerCount;
    NetworkPlayerRole mMyRole;
    int mMyPlayerID;
    
    // Network sockets (WinSock on Windows)
    void* mListenSocket;  // Host only
    void* mClientSocket;  // Client connection
    void* mClientSockets[2];  // Host: connected clients (max 2 additional players)
    
    void ProcessNetworkMessages();
    void ReadMessagesFromSocket(void* theSocket, int thePlayerID);
    void SendMessage(void* theSocket, NetworkMessageType theType, const void* theData, int theDataSize);
    void HandleMessage(NetworkMessageType theType, const void* theData, int theDataSize, int thePlayerID);
    
    struct MessageHeader
    {
        NetworkMessageType mType;
        int mDataSize;
    };
};

#endif

