// Include Winsock2 BEFORE any Windows headers to avoid conflicts
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include "NetworkManager.h"
#include "../Board.h"
#include "../../LawnApp.h"
#include <cstring>

#ifdef _WIN32
#include <string.h>
#define SOCKET_ERROR_CODE WSAGetLastError()
#define CLOSE_SOCKET closesocket
#define SOCKET_TYPE SOCKET
// INVALID_SOCKET and SOCKET_ERROR are already defined in winsock2.h
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#define SOCKET_ERROR_CODE errno
#define CLOSE_SOCKET close
#define SOCKET_TYPE int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

NetworkManager::NetworkManager()
{
    mIsHost = false;
    mIsConnected = false;
    mPlayerCount = 1;
    mMyRole = NETWORK_ROLE_HOST_PLANTS;
    mMyPlayerID = 1;
    mListenSocket = nullptr;
    mClientSocket = nullptr;
    mClientSockets[0] = nullptr;
    mClientSockets[1] = nullptr;
    mHammerCooldown = 0;
    mZombieSelectionUpdated = false;
    mPendingGameWon = false;
    mPendingGameLost = false;
    mPendingGameRestart = false;
    mPendingZombieSyncCount = 0;
    ClearZombieSelection();
    
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

NetworkManager::~NetworkManager()
{
    Disconnect();
    StopHost();
    
#ifdef _WIN32
    WSACleanup();
#endif
}

bool NetworkManager::StartHost(int thePort)
{
    if (mIsHost || mIsConnected)
        return false;
        
    mIsHost = true;
    mIsConnected = true;
    mMyRole = NETWORK_ROLE_HOST_PLANTS;
    mMyPlayerID = 1;
    mPlayerCount = 1;
    
    // Automatically select the best I Zombie zombies for Player 2
    ClearZombieSelection();
    // Best zombies (based on I Zombie Endless mode selection)
    SetZombieSelected(ZOMBIE_IMP, true);
    SetZombieSelected(ZOMBIE_TRAFFIC_CONE, true);
    SetZombieSelected(ZOMBIE_POLEVAULTER, true);
    SetZombieSelected(ZOMBIE_PAIL, true);
    SetZombieSelected(ZOMBIE_BUNGEE, true);
    SetZombieSelected(ZOMBIE_DIGGER, true);
    SetZombieSelected(ZOMBIE_LADDER, true);
    SetZombieSelected(ZOMBIE_FOOTBALL, true);
    SetZombieSelected(ZOMBIE_DANCER, true);
    
    // Create listening socket
    SOCKET_TYPE listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET)
        return false;
    
    // Set socket options
    int reuse = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
    
    // Bind socket - INADDR_ANY allows connections from localhost and network
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  // Accept connections from localhost (127.0.0.1) and network
    addr.sin_port = htons(thePort);
    
    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        CLOSE_SOCKET(listenSock);
        return false;
    }
    
    // Listen for connections
    if (listen(listenSock, 2) == SOCKET_ERROR)
    {
        CLOSE_SOCKET(listenSock);
        return false;
    }
    
    // Set socket to non-blocking
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(listenSock, FIONBIO, &mode);
#else
    int flags = fcntl(listenSock, F_GETFL, 0);
    fcntl(listenSock, F_SETFL, flags | O_NONBLOCK);
#endif
    
    mListenSocket = (void*)listenSock;
    return true;
}

void NetworkManager::StopHost()
{
    if (mListenSocket)
    {
        CLOSE_SOCKET((SOCKET_TYPE)mListenSocket);
        mListenSocket = nullptr;
    }
    
    for (int i = 0; i < 2; i++)
    {
        if (mClientSockets[i])
        {
            CLOSE_SOCKET((SOCKET_TYPE)mClientSockets[i]);
            mClientSockets[i] = nullptr;
        }
    }
    
    mIsHost = false;
    mIsConnected = false;
    mPlayerCount = 1;
}

void NetworkManager::UpdateHost()
{
    if (!mIsHost || !mListenSocket)
        return;
    
    // Accept new connections
    if (mPlayerCount < 3)
    {
        sockaddr_in clientAddr;
#ifdef _WIN32
        int addrLen = sizeof(clientAddr);
#else
        socklen_t addrLen = sizeof(clientAddr);
#endif
        SOCKET_TYPE clientSock = accept((SOCKET_TYPE)mListenSocket, (sockaddr*)&clientAddr, &addrLen);
        
        if (clientSock != INVALID_SOCKET)
        {
            // Set to non-blocking
#ifdef _WIN32
            u_long mode = 1;
            ioctlsocket(clientSock, FIONBIO, &mode);
#else
            int flags = fcntl(clientSock, F_GETFL, 0);
            fcntl(clientSock, F_SETFL, flags | O_NONBLOCK);
#endif
            
            // Assign player ID and role
            int playerSlot = mPlayerCount - 1;
            mClientSockets[playerSlot] = (void*)clientSock;
            mPlayerCount++;
            
            // Send connection accepted message with player ID and role
            NetworkPlayerRole role = (playerSlot == 0) ? NETWORK_ROLE_CLIENT_ZOMBIES : NETWORK_ROLE_CLIENT_HAMMER;
            int playerID = mPlayerCount;
            ConnectResponse response;
            response.mPlayerID = playerID;
            response.mRole = role;
            SendMessage(mClientSockets[playerSlot], NETMSG_CONNECT, &response, sizeof(response));
            
            // Send zombie selection to zombie player (slot 0) when they connect
            if (playerSlot == 0)
            {
                ZombieType selectedZombies[NUM_ZOMBIE_TYPES];
                int count = 0;
                for (int i = 0; i < NUM_ZOMBIE_TYPES && count < 20; i++)
                {
                    ZombieType zType = (ZombieType)i;
                    if (IsZombieSelected(zType))
                    {
                        selectedZombies[count++] = zType;
                    }
                }
                if (count > 0)
                {
                    SendZombieSelection(selectedZombies, count);
                }
            }
        }
    }
    
    // Process messages from clients
    ProcessNetworkMessages();
}

bool NetworkManager::GetHostIPAddress(char* theBuffer, int theBufferSize)
{
    if (!theBuffer || theBufferSize < 16)
        return false;
    
    // Try to get local IP address
#ifdef _WIN32
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        struct hostent* host = gethostbyname(hostname);
        if (host && host->h_addr_list[0])
        {
            struct in_addr* addr = (struct in_addr*)host->h_addr_list[0];
            const char* ip = inet_ntoa(*addr);
            if (ip)
            {
                strncpy(theBuffer, ip, theBufferSize - 1);
                theBuffer[theBufferSize - 1] = '\0';
                return true;
            }
        }
    }
    // Fallback to localhost
    strncpy(theBuffer, "127.0.0.1", theBufferSize - 1);
    theBuffer[theBufferSize - 1] = '\0';
    return true;
#else
    // On Unix/Linux, try to get local IP
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        struct hostent* host = gethostbyname(hostname);
        if (host && host->h_addr_list[0])
        {
            struct in_addr* addr = (struct in_addr*)host->h_addr_list[0];
            const char* ip = inet_ntoa(*addr);
            if (ip)
            {
                strncpy(theBuffer, ip, theBufferSize - 1);
                theBuffer[theBufferSize - 1] = '\0';
                return true;
            }
        }
    }
    // Fallback to localhost
    strncpy(theBuffer, "127.0.0.1", theBufferSize - 1);
    theBuffer[theBufferSize - 1] = '\0';
    return true;
#endif
}

bool NetworkManager::ConnectToHost(const char* theHostIP, int thePort)
{
    if (mIsConnected || mIsHost)
        return false;
    
    SOCKET_TYPE sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
        return false;
    
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(thePort);
    
    // Support both IP addresses and hostnames (including "localhost")
    if (strcmp(theHostIP, "localhost") == 0)
    {
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    }
    else
    {
        // Try to convert IP address directly first
        addr.sin_addr.s_addr = inet_addr(theHostIP);
        if (addr.sin_addr.s_addr == INADDR_NONE)
        {
            // If that failed, try hostname resolution
            struct hostent* host = gethostbyname(theHostIP);
            if (host == nullptr)
            {
                CLOSE_SOCKET(sock);
                return false;
            }
            memcpy(&addr.sin_addr, host->h_addr_list[0], host->h_length);
        }
    }
    
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
#ifdef _WIN32
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS)
        {
            CLOSE_SOCKET(sock);
            return false;
        }
#else
        if (errno != EINPROGRESS)
        {
            CLOSE_SOCKET(sock);
            return false;
        }
#endif
    }
    
    // Set to non-blocking
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    
    mClientSocket = (void*)sock;
    mIsConnected = true;
    
    // Wait for connection response
    ProcessNetworkMessages();
    
    return true;
}

bool NetworkManager::ConnectToLocalhost(int thePort)
{
    return ConnectToHost("127.0.0.1", thePort);
}

void NetworkManager::Disconnect()
{
    if (mClientSocket)
    {
        CLOSE_SOCKET((SOCKET_TYPE)mClientSocket);
        mClientSocket = nullptr;
    }
    
    mIsConnected = false;
    mMyRole = NETWORK_ROLE_NONE;
    mMyPlayerID = 0;
    mPlayerCount = 0;
}

void NetworkManager::UpdateClient()
{
    if (!mIsConnected || !mClientSocket)
        return;
    
    ProcessNetworkMessages();
}

void NetworkManager::Update()
{
    if (mIsHost)
        UpdateHost();
    else if (mIsConnected)
        UpdateClient();
    
    // Update hammer cooldown
    if (mHammerCooldown > 0)
        mHammerCooldown--;
}

void NetworkManager::SendPlantPlaced(int theGridX, int theGridY, SeedType theSeedType)
{
    PlantPlaceData data;
    data.mGridX = theGridX;
    data.mGridY = theGridY;
    data.mSeedType = theSeedType;
    
    if (mIsHost)
    {
        // Broadcast to all clients
        for (int i = 0; i < 2; i++)
        {
            if (mClientSockets[i])
                SendMessage(mClientSockets[i], NETMSG_PLANT_PLACED, &data, sizeof(data));
        }
    }
    else if (mClientSocket)
    {
        SendMessage(mClientSocket, NETMSG_PLANT_PLACED, &data, sizeof(data));
    }
}

void NetworkManager::SendZombiePlaced(int theGridX, int theGridY, ZombieType theZombieType)
{
    ZombiePlaceData data;
    data.mGridX = theGridX;
    data.mGridY = theGridY;
    data.mZombieType = theZombieType;
    
    if (mIsHost)
    {
        // Host places locally and broadcasts to all clients
        mPendingZombiePlace.mGridX = theGridX;
        mPendingZombiePlace.mGridY = theGridY;
        mPendingZombiePlace.mZombieType = theZombieType;
        
        // Broadcast to all clients
        for (int i = 0; i < 2; i++)
        {
            if (mClientSockets[i])
                SendMessage(mClientSockets[i], NETMSG_ZOMBIE_PLACED, &data, sizeof(data));
        }
    }
    else if (mClientSocket)
    {
        // Client sends to host - host will broadcast back to all (including this client)
        SendMessage(mClientSocket, NETMSG_ZOMBIE_PLACED, &data, sizeof(data));
    }
}

void NetworkManager::SendHammerStrike(int theX, int theY)
{
    if (mHammerCooldown > 0)
        return;  // Still on cooldown
        
    HammerStrikeData data;
    data.mX = theX;
    data.mY = theY;
    data.mPlayerID = mMyPlayerID;
    
    mHammerCooldown = HAMMER_COOLDOWN_TIME;
    
    if (mIsHost)
    {
        // Host processes locally and broadcasts to all clients
        mPendingHammerStrike.mX = theX;
        mPendingHammerStrike.mY = theY;
        mPendingHammerStrike.mPlayerID = mMyPlayerID;
        
        // Broadcast to all clients
        for (int i = 0; i < 2; i++)
        {
            if (mClientSockets[i])
                SendMessage(mClientSockets[i], NETMSG_HAMMER_STRIKE, &data, sizeof(data));
        }
    }
    else if (mClientSocket)
    {
        // Client sends to host, host will broadcast to all and process
        SendMessage(mClientSocket, NETMSG_HAMMER_STRIKE, &data, sizeof(data));
    }
}

void NetworkManager::SendZombieSelection(const ZombieType* theZombieTypes, int theCount)
{
    // Host sends zombie selection to zombie player (client slot 0)
    if (mIsHost && mClientSockets[0])
    {
        struct ZombieSelectionData {
            int mCount;
            ZombieType mTypes[20];  // Increased to 20 to support more zombie types
        } data;
        data.mCount = (theCount > 20) ? 20 : theCount;
        for (int i = 0; i < data.mCount; i++)
            data.mTypes[i] = theZombieTypes[i];
        
        SendMessage(mClientSockets[0], NETMSG_ZOMBIE_SELECTION, &data, sizeof(data));
    }
}

void NetworkManager::SendPlantDied(int theGridX, int theGridY)
{
    // Only host sends death messages (authoritative)
    if (!mIsHost)
        return;
        
    PlantDeathData data;
    data.mGridX = theGridX;
    data.mGridY = theGridY;
    
    // Broadcast to all clients
    for (int i = 0; i < 2; i++)
    {
        if (mClientSockets[i])
            SendMessage(mClientSockets[i], NETMSG_PLANT_DIED, &data, sizeof(data));
    }
}

void NetworkManager::SendZombieDied(int theGridX, int theGridY)
{
    // Only host sends death messages (authoritative)
    if (!mIsHost)
        return;
        
    ZombieDeathData data;
    data.mGridX = theGridX;
    data.mGridY = theGridY;
    
    // Broadcast to all clients
    for (int i = 0; i < 2; i++)
    {
        if (mClientSockets[i])
            SendMessage(mClientSockets[i], NETMSG_ZOMBIE_DIED, &data, sizeof(data));
    }
}

void NetworkManager::SendGameState()
{
    // Placeholder for game state synchronization
    // This would sync positions, health, etc.
}

void NetworkManager::SendGameWon()
{
    // Only host sends win/loss messages (authoritative)
    if (!mIsHost)
        return;
    
    // Broadcast to all clients
    for (int i = 0; i < 2; i++)
    {
        if (mClientSockets[i])
            SendMessage(mClientSockets[i], NETMSG_GAME_WON, nullptr, 0);
    }
}

void NetworkManager::SendGameLost()
{
    // Only host sends win/loss messages (authoritative)
    if (!mIsHost)
        return;
    
    // Broadcast to all clients
    for (int i = 0; i < 2; i++)
    {
        if (mClientSockets[i])
            SendMessage(mClientSockets[i], NETMSG_GAME_LOST, nullptr, 0);
    }
}

void NetworkManager::SendGameRestart()
{
    // Any player can request restart, but host is authoritative
    if (mIsHost)
    {
        // Host broadcasts to all clients
        for (int i = 0; i < 2; i++)
        {
            if (mClientSockets[i])
                SendMessage(mClientSockets[i], NETMSG_GAME_RESTART, nullptr, 0);
        }
        // Host processes locally
        mPendingGameRestart = true;
    }
    else if (mClientSocket)
    {
        // Client sends to host, host will broadcast
        SendMessage(mClientSocket, NETMSG_GAME_RESTART, nullptr, 0);
    }
}

void NetworkManager::SendZombiePosition(unsigned int theZombieID, float thePosX, float thePosY, int theRow)
{
    // Only host sends position updates (authoritative)
    if (!mIsHost)
        return;
    
    ZombiePositionData data;
    data.mZombieID = theZombieID;
    data.mPosX = thePosX;
    data.mPosY = thePosY;
    data.mRow = theRow;
    
    // Broadcast to all clients
    for (int i = 0; i < 2; i++)
    {
        if (mClientSockets[i])
            SendMessage(mClientSockets[i], NETMSG_ZOMBIE_POSITION, &data, sizeof(data));
    }
}

void NetworkManager::SendZombieHealth(unsigned int theZombieID, int theBodyHealth, int theHelmHealth, int theShieldHealth)
{
    // Only host sends health updates (authoritative)
    if (!mIsHost)
        return;
    
    ZombieHealthData data;
    data.mZombieID = theZombieID;
    data.mBodyHealth = theBodyHealth;
    data.mHelmHealth = theHelmHealth;
    data.mShieldHealth = theShieldHealth;
    
    // Broadcast to all clients
    for (int i = 0; i < 2; i++)
    {
        if (mClientSockets[i])
            SendMessage(mClientSockets[i], NETMSG_ZOMBIE_HEALTH, &data, sizeof(data));
    }
}

NetworkManager::PendingZombieSync* NetworkManager::GetOrCreatePendingZombieSync(unsigned int theZombieID)
{
    // Try to find existing entry
    for (int i = 0; i < mPendingZombieSyncCount; i++)
    {
        if (mPendingZombieSyncs[i].mZombieID == theZombieID)
            return &mPendingZombieSyncs[i];
    }
    
    // Create new entry if space available
    if (mPendingZombieSyncCount < MAX_PENDING_ZOMBIE_SYNC)
    {
        PendingZombieSync* sync = &mPendingZombieSyncs[mPendingZombieSyncCount++];
        sync->mZombieID = theZombieID;
        sync->mHasPosition = false;
        sync->mHasHealth = false;
        sync->mHasState = false;
        return sync;
    }
    
    return nullptr;  // No space
}

void NetworkManager::SendZombieState(unsigned int theZombieID, ZombiePhase thePhase, int theFrame, bool theIsEating, float theVelX, int thePhaseCounter)
{
    // Only host sends state updates (authoritative)
    if (!mIsHost)
        return;
    
    ZombieStateData data;
    data.mZombieID = theZombieID;
    data.mZombiePhase = thePhase;
    data.mFrame = theFrame;
    data.mIsEating = theIsEating;
    data.mVelX = theVelX;
    data.mPhaseCounter = thePhaseCounter;
    
    // Broadcast to all clients
    for (int i = 0; i < 2; i++)
    {
        if (mClientSockets[i])
            SendMessage(mClientSockets[i], NETMSG_ZOMBIE_STATE, &data, sizeof(data));
    }
}

void NetworkManager::SendZombieFullSyncData(const NetworkManager::ZombieFullSyncData& theData)
{
    // Only host sends full sync (authoritative)
    if (!mIsHost)
        return;
    
    // Broadcast to all clients
    for (int i = 0; i < 2; i++)
    {
        if (mClientSockets[i])
            SendMessage(mClientSockets[i], NETMSG_ZOMBIE_FULL_SYNC, &theData, sizeof(theData));
    }
}

void NetworkManager::SendMessage(void* theSocket, NetworkMessageType theType, const void* theData, int theDataSize)
{
    if (!theSocket)
        return;
    
    // Send message header
    MessageHeader header;
    header.mType = theType;
    header.mDataSize = theDataSize;
    
    send((SOCKET_TYPE)theSocket, (const char*)&header, sizeof(header), 0);
    if (theData && theDataSize > 0)
        send((SOCKET_TYPE)theSocket, (const char*)theData, theDataSize, 0);
}

void NetworkManager::ProcessNetworkMessages()
{
    if (mIsHost)
    {
        // Check messages from all client sockets
        for (int i = 0; i < 2; i++)
        {
            if (mClientSockets[i])
            {
                ReadMessagesFromSocket(mClientSockets[i], i + 2);  // Player IDs: 2 and 3
            }
        }
    }
    else if (mClientSocket)
    {
        // Read messages from host
        ReadMessagesFromSocket(mClientSocket, 1);  // Host is player ID 1
    }
}

void NetworkManager::ReadMessagesFromSocket(void* theSocket, int thePlayerID)
{
    if (!theSocket)
        return;
    
    SOCKET_TYPE sock = (SOCKET_TYPE)theSocket;
    char buffer[4096];
    
    // Try to peek at available data first
    int bytesAvailable = 0;
#ifdef _WIN32
    u_long bytesToRead = 0;
    if (ioctlsocket(sock, FIONREAD, &bytesToRead) == 0)
        bytesAvailable = (int)bytesToRead;
#else
    if (ioctl(sock, FIONREAD, &bytesAvailable) == 0)
        ;  // bytesAvailable already set
#endif
    
    // Read messages while data is available
    while (bytesAvailable >= (int)sizeof(MessageHeader))
    {
        // Read message header (peek first to check size)
        MessageHeader header;
        int received = recv(sock, (char*)&header, sizeof(header), MSG_PEEK);
        if (received != sizeof(header))
            break;
        
        // Check if we have the full message
        int totalSize = sizeof(header) + header.mDataSize;
        if (bytesAvailable < totalSize)
            break;  // Need more data
        
        // Read the full header
        received = recv(sock, (char*)&header, sizeof(header), 0);
        if (received != sizeof(header))
            break;
        
        // Read message data if present
        if (header.mDataSize > 0 && header.mDataSize < (int)sizeof(buffer))
        {
            received = recv(sock, buffer, header.mDataSize, 0);
            if (received == header.mDataSize)
            {
                HandleMessage(header.mType, buffer, header.mDataSize, thePlayerID);
            }
            else
            {
                break;  // Error reading data
            }
        }
        else if (header.mDataSize == 0)
        {
            HandleMessage(header.mType, nullptr, 0, thePlayerID);
        }
        else
        {
            break;  // Invalid message size
        }
        
        // Check for more messages
#ifdef _WIN32
        bytesToRead = 0;
        if (ioctlsocket(sock, FIONREAD, &bytesToRead) == 0)
            bytesAvailable = (int)bytesToRead;
        else
            break;
#else
        if (ioctl(sock, FIONREAD, &bytesAvailable) != 0)
            break;
#endif
    }
}

void NetworkManager::HandleMessage(NetworkMessageType theType, const void* theData, int theDataSize, int thePlayerID)
{
    switch (theType)
    {
    case NETMSG_CONNECT:
    {
        if (theDataSize >= sizeof(ConnectResponse))
        {
            // Connection response
            ConnectResponse* response = (ConnectResponse*)theData;
            mMyPlayerID = response->mPlayerID;
            mMyRole = response->mRole;
        }
        break;
    }
    case NETMSG_PLANT_PLACED:
    {
        if (theDataSize >= sizeof(PlantPlaceData))
        {
            PlantPlaceData* data = (PlantPlaceData*)theData;
            mPendingPlantPlace.mGridX = data->mGridX;
            mPendingPlantPlace.mGridY = data->mGridY;
            mPendingPlantPlace.mSeedType = data->mSeedType;
        }
        break;
    }
    case NETMSG_ZOMBIE_PLACED:
    {
        if (theDataSize >= sizeof(ZombiePlaceData))
        {
            ZombiePlaceData* data = (ZombiePlaceData*)theData;
            
            if (mIsHost)
            {
                // Host receives from client - place locally and broadcast to ALL clients (including sender)
                mPendingZombiePlace.mGridX = data->mGridX;
                mPendingZombiePlace.mGridY = data->mGridY;
                mPendingZombiePlace.mZombieType = data->mZombieType;
                
                // Broadcast to all clients (including the one who sent it, so they get authoritative placement)
                for (int i = 0; i < 2; i++)
                {
                    if (mClientSockets[i])
                        SendMessage(mClientSockets[i], NETMSG_ZOMBIE_PLACED, data, sizeof(ZombiePlaceData));
                }
            }
            else
            {
                // Client receives broadcast from host - place zombie
                mPendingZombiePlace.mGridX = data->mGridX;
                mPendingZombiePlace.mGridY = data->mGridY;
                mPendingZombiePlace.mZombieType = data->mZombieType;
            }
        }
        break;
    }
    case NETMSG_HAMMER_STRIKE:
    {
        if (theDataSize >= sizeof(HammerStrikeData))
        {
            HammerStrikeData* data = (HammerStrikeData*)theData;
            mPendingHammerStrike.mX = data->mX;
            mPendingHammerStrike.mY = data->mY;
            mPendingHammerStrike.mPlayerID = data->mPlayerID;
            
            // Host broadcasts to all other clients
            if (mIsHost)
            {
                for (int i = 0; i < 2; i++)
                {
                    // Don't send back to the client who sent it
                    if (mClientSockets[i] && thePlayerID != (i + 2))
                    {
                        SendMessage(mClientSockets[i], NETMSG_HAMMER_STRIKE, data, sizeof(HammerStrikeData));
                    }
                }
            }
        }
        break;
    }
    case NETMSG_ZOMBIE_SELECTION:
    {
        struct ZombieSelectionData {
            int mCount;
            ZombieType mTypes[20];  // Increased to 20 to support more zombie types
        };
        if (theDataSize >= sizeof(ZombieSelectionData))
        {
            ZombieSelectionData* data = (ZombieSelectionData*)theData;
            ClearZombieSelection();
            for (int i = 0; i < data->mCount && i < 20; i++)
            {
                SetZombieSelected(data->mTypes[i], true);
            }
            // Signal that zombie selection was updated (so seed bank can be refreshed)
            mZombieSelectionUpdated = true;
        }
        break;
    }
    case NETMSG_PLANT_DIED:
    {
        if (theDataSize >= sizeof(PlantDeathData))
        {
            PlantDeathData* data = (PlantDeathData*)theData;
            mPendingPlantDeath.mGridX = data->mGridX;
            mPendingPlantDeath.mGridY = data->mGridY;
            
            // Host broadcasts to all other clients
            if (mIsHost)
            {
                for (int i = 0; i < 2; i++)
                {
                    // Don't send back to the client who sent it (though clients shouldn't send deaths)
                    if (mClientSockets[i] && thePlayerID != (i + 2))
                    {
                        SendMessage(mClientSockets[i], NETMSG_PLANT_DIED, data, sizeof(PlantDeathData));
                    }
                }
            }
        }
        break;
    }
    case NETMSG_ZOMBIE_DIED:
    {
        if (theDataSize >= sizeof(ZombieDeathData))
        {
            ZombieDeathData* data = (ZombieDeathData*)theData;
            mPendingZombieDeath.mGridX = data->mGridX;
            mPendingZombieDeath.mGridY = data->mGridY;
            
            // Host broadcasts to all other clients
            if (mIsHost)
            {
                for (int i = 0; i < 2; i++)
                {
                    // Don't send back to the client who sent it (though clients shouldn't send deaths)
                    if (mClientSockets[i] && thePlayerID != (i + 2))
                    {
                        SendMessage(mClientSockets[i], NETMSG_ZOMBIE_DIED, data, sizeof(ZombieDeathData));
                    }
                }
            }
        }
        break;
    }
    case NETMSG_GAME_WON:
    {
        mPendingGameWon = true;
        // Host broadcasts to all other clients
        if (mIsHost)
        {
            for (int i = 0; i < 2; i++)
            {
                if (mClientSockets[i] && thePlayerID != (i + 2))
                {
                    SendMessage(mClientSockets[i], NETMSG_GAME_WON, nullptr, 0);
                }
            }
        }
        break;
    }
    case NETMSG_GAME_LOST:
    {
        mPendingGameLost = true;
        // Host broadcasts to all other clients
        if (mIsHost)
        {
            for (int i = 0; i < 2; i++)
            {
                if (mClientSockets[i] && thePlayerID != (i + 2))
                {
                    SendMessage(mClientSockets[i], NETMSG_GAME_LOST, nullptr, 0);
                }
            }
        }
        break;
    }
    case NETMSG_GAME_RESTART:
    {
        mPendingGameRestart = true;
        // Host broadcasts to all other clients
        if (mIsHost)
        {
            for (int i = 0; i < 2; i++)
            {
                if (mClientSockets[i] && thePlayerID != (i + 2))
                {
                    SendMessage(mClientSockets[i], NETMSG_GAME_RESTART, nullptr, 0);
                }
            }
        }
        break;
    }
    case NETMSG_ZOMBIE_POSITION:
    {
        if (theDataSize >= sizeof(ZombiePositionData))
        {
            ZombiePositionData* data = (ZombiePositionData*)theData;
            PendingZombieSync* sync = GetOrCreatePendingZombieSync(data->mZombieID);
            if (sync)
            {
                sync->mPosX = data->mPosX;
                sync->mPosY = data->mPosY;
                sync->mRow = data->mRow;
                sync->mHasPosition = true;
            }
        }
        break;
    }
    case NETMSG_ZOMBIE_HEALTH:
    {
        if (theDataSize >= sizeof(ZombieHealthData))
        {
            ZombieHealthData* data = (ZombieHealthData*)theData;
            PendingZombieSync* sync = GetOrCreatePendingZombieSync(data->mZombieID);
            if (sync)
            {
                sync->mBodyHealth = data->mBodyHealth;
                sync->mHelmHealth = data->mHelmHealth;
                sync->mShieldHealth = data->mShieldHealth;
                sync->mHasHealth = true;
            }
        }
        break;
    }
    case NETMSG_ZOMBIE_STATE:
    {
        if (theDataSize >= sizeof(ZombieStateData))
        {
            ZombieStateData* data = (ZombieStateData*)theData;
            PendingZombieSync* sync = GetOrCreatePendingZombieSync(data->mZombieID);
            if (sync)
            {
                sync->mZombiePhase = data->mZombiePhase;
                sync->mFrame = data->mFrame;
                sync->mIsEating = data->mIsEating;
                sync->mVelX = data->mVelX;
                sync->mPhaseCounter = data->mPhaseCounter;
                sync->mHasState = true;
            }
        }
        break;
    }
    case NETMSG_ZOMBIE_FULL_SYNC:
    {
        if (theDataSize >= sizeof(ZombieFullSyncData))
        {
            ZombieFullSyncData* data = (ZombieFullSyncData*)theData;
            // Update or create pending sync with all data at once
            PendingZombieSync* sync = GetOrCreatePendingZombieSync(data->mZombieID);
            if (sync)
            {
                sync->mZombieType = data->mZombieType;
                sync->mPosX = data->mPosX;
                sync->mPosY = data->mPosY;
                sync->mRow = data->mRow;
                sync->mBodyHealth = data->mBodyHealth;
                sync->mHelmHealth = data->mHelmHealth;
                sync->mShieldHealth = data->mShieldHealth;
                sync->mZombiePhase = data->mZombiePhase;
                sync->mFrame = data->mFrame;
                sync->mIsEating = data->mIsEating;
                sync->mVelX = data->mVelX;
                sync->mPhaseCounter = data->mPhaseCounter;
                sync->mDead = data->mDead;
                sync->mHasPosition = true;
                sync->mHasHealth = true;
                sync->mHasState = true;
                sync->mNeedsCreation = true;  // Flag that this zombie needs to be created if missing
            }
        }
        break;
    }
    default:
        break;
    }
}

void NetworkManager::ClearZombieSelection()
{
    memset(mZombieSelected, 0, sizeof(mZombieSelected));
    mSelectedZombieCount = 0;
    mZombieSelectionUpdated = false;
}

void NetworkManager::SetZombieSelected(ZombieType theZombieType, bool theSelected)
{
    if (theZombieType < 0 || theZombieType >= NUM_ZOMBIE_TYPES)
        return;
    
    if (mZombieSelected[theZombieType] != theSelected)
    {
        mZombieSelected[theZombieType] = theSelected;
        if (theSelected)
            mSelectedZombieCount++;
        else
            mSelectedZombieCount--;
    }
}

bool NetworkManager::IsZombieSelected(ZombieType theZombieType) const
{
    if (theZombieType < 0 || theZombieType >= NUM_ZOMBIE_TYPES)
        return false;
    return mZombieSelected[theZombieType];
}

