// Include Winsock2 BEFORE any Windows headers to avoid conflicts
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
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
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
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
    
    // Following GeeksforGeeks socket programming guide:
    // https://www.geeksforgeeks.org/cpp/socket-programming-in-cpp/
    
    // Step 1: Create server socket (TCP stream socket)
    SOCKET_TYPE serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET)
        return false;
    
    // Set socket options for reliability
    int reuse = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
    int keepAlive = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_KEEPALIVE, (const char*)&keepAlive, sizeof(keepAlive));
    int nodelay = 1;
    setsockopt(serverSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
    
    // Step 2: Define server address structure
    sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(thePort);
    serverAddress.sin_addr.s_addr = INADDR_ANY;  // Accept connections on any IP (0.0.0.0)
    
    // Explicitly allow connections from any interface (important for network connections)
    // INADDR_ANY (0.0.0.0) means bind to all available interfaces
    int optval = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval));
    
    // Step 3: Bind socket to address - this binds to ALL network interfaces
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
    {
#ifdef _WIN32
        int error = WSAGetLastError();
        // Common errors:
        // WSAEADDRINUSE (10048) = Port already in use
        // WSAEACCES (10013) = Permission denied (firewall/antivirus blocking)
        // WSAENOTSOCK (10038) = Invalid socket
#endif
        CLOSE_SOCKET(serverSocket);
        mIsHost = false;
        mIsConnected = false;
        return false;
    }
    
    // Verify binding worked by checking the bound address
    sockaddr_in boundAddr;
    memset(&boundAddr, 0, sizeof(boundAddr));
#ifdef _WIN32
    int addrLen = sizeof(boundAddr);
#else
    socklen_t addrLen = sizeof(boundAddr);
#endif
    if (getsockname(serverSocket, (struct sockaddr*)&boundAddr, &addrLen) == 0)
    {
        // Socket is bound - should be 0.0.0.0 (INADDR_ANY) which accepts all connections
    }
    
    // Step 4: Listen for incoming connections
    if (listen(serverSocket, 5) == SOCKET_ERROR)  // Backlog of 5 like guide
    {
        CLOSE_SOCKET(serverSocket);
        return false;
    }
    
    // Set socket to non-blocking for game responsiveness
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(serverSocket, FIONBIO, &mode);
#else
    int flags = fcntl(serverSocket, F_GETFL, 0);
    fcntl(serverSocket, F_SETFL, flags | O_NONBLOCK);
#endif
    
    mListenSocket = (void*)serverSocket;
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
        // Step 5: Accept client connection (following guide pattern)
        sockaddr_in clientAddr;
#ifdef _WIN32
        int addrLen = sizeof(clientAddr);
#else
        socklen_t addrLen = sizeof(clientAddr);
#endif
        SOCKET_TYPE clientSocket = accept((SOCKET_TYPE)mListenSocket, (struct sockaddr*)&clientAddr, &addrLen);
        
        if (clientSocket != INVALID_SOCKET)
        {
            // Log client connection info for debugging
            char clientIP[INET_ADDRSTRLEN];
            const char* clientIPStr = inet_ntoa(clientAddr.sin_addr);
            if (clientIPStr)
            {
                strncpy(clientIP, clientIPStr, sizeof(clientIP) - 1);
                clientIP[sizeof(clientIP) - 1] = '\0';
                // Connection accepted from: clientIP (can be localhost or network IP)
            }
            
            // Set TCP socket options for accepted connections
            int keepAlive = 1;
            setsockopt(clientSocket, SOL_SOCKET, SO_KEEPALIVE, (const char*)&keepAlive, sizeof(keepAlive));
            int nodelay = 1;
            setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
            
            // Set to non-blocking for game responsiveness
#ifdef _WIN32
            u_long mode = 1;
            ioctlsocket(clientSocket, FIONBIO, &mode);
#else
            int flags = fcntl(clientSocket, F_GETFL, 0);
            fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);
#endif
            
            // Assign player ID and role
            int playerSlot = mPlayerCount - 1;
            mClientSockets[playerSlot] = (void*)clientSocket;
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
    
    // Try to get actual network IP address (not localhost)
#ifdef _WIN32
    // Use GetAdaptersAddresses for better network interface detection
    ULONG bufferSize = 0;
    GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr, &bufferSize);
    if (bufferSize > 0)
    {
        PIP_ADAPTER_ADDRESSES adapterAddresses = (PIP_ADAPTER_ADDRESSES)malloc(bufferSize);
        if (adapterAddresses)
        {
            if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapterAddresses, &bufferSize) == ERROR_SUCCESS)
            {
                PIP_ADAPTER_ADDRESSES adapter = adapterAddresses;
                while (adapter)
                {
                    // Skip loopback and non-operational adapters
                    if (adapter->IfType != IF_TYPE_SOFTWARE_LOOPBACK && 
                        adapter->OperStatus == IfOperStatusUp &&
                        adapter->FirstUnicastAddress)
                    {
                        // Get the first IPv4 address
                        PIP_ADAPTER_UNICAST_ADDRESS unicast = adapter->FirstUnicastAddress;
                        while (unicast)
                        {
                            if (unicast->Address.lpSockaddr->sa_family == AF_INET)
                            {
                                sockaddr_in* sin = (sockaddr_in*)unicast->Address.lpSockaddr;
                                const char* ip = inet_ntoa(sin->sin_addr);
                                if (ip && strcmp(ip, "127.0.0.1") != 0)
                                {
                                    strncpy(theBuffer, ip, theBufferSize - 1);
                                    theBuffer[theBufferSize - 1] = '\0';
                                    free(adapterAddresses);
                                    return true;
                                }
                            }
                            unicast = unicast->Next;
                        }
                    }
                    adapter = adapter->Next;
                }
            }
            free(adapterAddresses);
        }
    }
    
    // Fallback: try hostname resolution
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        struct hostent* host = gethostbyname(hostname);
        if (host && host->h_addr_list)
        {
            // Try all addresses, skip localhost
            for (int i = 0; host->h_addr_list[i] != nullptr; i++)
            {
                struct in_addr* addr = (struct in_addr*)host->h_addr_list[i];
                const char* ip = inet_ntoa(*addr);
                if (ip && strcmp(ip, "127.0.0.1") != 0)
                {
                    strncpy(theBuffer, ip, theBufferSize - 1);
                    theBuffer[theBufferSize - 1] = '\0';
                    return true;
                }
            }
        }
    }
    
    // Last resort: return localhost
    strncpy(theBuffer, "127.0.0.1", theBufferSize - 1);
    theBuffer[theBufferSize - 1] = '\0';
    return true;
#else
    // On Unix/Linux, use getifaddrs for better interface detection
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == 0)
    {
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == nullptr)
                continue;
            
            // Only look at IPv4 addresses
            if (ifa->ifa_addr->sa_family == AF_INET)
            {
                // Skip loopback interface
                if (strcmp(ifa->ifa_name, "lo") == 0)
                    continue;
                
                sockaddr_in* sin = (sockaddr_in*)ifa->ifa_addr;
                const char* ip = inet_ntoa(sin->sin_addr);
                if (ip && strcmp(ip, "127.0.0.1") != 0)
                {
                    strncpy(theBuffer, ip, theBufferSize - 1);
                    theBuffer[theBufferSize - 1] = '\0';
                    freeifaddrs(ifaddr);
                    return true;
                }
            }
        }
        freeifaddrs(ifaddr);
    }
    
    // Fallback: try hostname resolution
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        struct hostent* host = gethostbyname(hostname);
        if (host && host->h_addr_list)
        {
            // Try all addresses, skip localhost
            for (int i = 0; host->h_addr_list[i] != nullptr; i++)
            {
                struct in_addr* addr = (struct in_addr*)host->h_addr_list[i];
                const char* ip = inet_ntoa(*addr);
                if (ip && strcmp(ip, "127.0.0.1") != 0)
                {
                    strncpy(theBuffer, ip, theBufferSize - 1);
                    theBuffer[theBufferSize - 1] = '\0';
                    return true;
                }
            }
        }
    }
    
    // Last resort: return localhost
    strncpy(theBuffer, "127.0.0.1", theBufferSize - 1);
    theBuffer[theBufferSize - 1] = '\0';
    return true;
#endif
}

bool NetworkManager::GetExternalIPAddress(char* theBuffer, int theBufferSize)
{
    if (!theBuffer || theBufferSize < 16)
        return false;
    
    // Try to get external IP by connecting to a public service
    // This is a simple approach - in production you might want to use a dedicated service
    SOCKET_TYPE sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        strncpy(theBuffer, "Unknown", theBufferSize - 1);
        theBuffer[theBufferSize - 1] = '\0';
        return false;
    }
    
    // Connect to a public DNS server to determine external IP
    // Using Google's DNS (8.8.8.8) - we don't actually need to connect, just get local socket address
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("8.8.8.8");
    addr.sin_port = htons(53);
    
    // Set to non-blocking
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    
    connect(sock, (sockaddr*)&addr, sizeof(addr));
    
    // Get the local address that would be used for this connection
    sockaddr_in localAddr;
    memset(&localAddr, 0, sizeof(localAddr));
#ifdef _WIN32
    int addrLen = sizeof(localAddr);
#else
    socklen_t addrLen = sizeof(localAddr);
#endif
    getsockname(sock, (sockaddr*)&localAddr, &addrLen);
    
    CLOSE_SOCKET(sock);
    
    const char* ip = inet_ntoa(localAddr.sin_addr);
    if (ip && strcmp(ip, "0.0.0.0") != 0)
    {
        strncpy(theBuffer, ip, theBufferSize - 1);
        theBuffer[theBufferSize - 1] = '\0';
        return true;
    }
    
    // Fallback: return local IP
    return GetHostIPAddress(theBuffer, theBufferSize);
}

bool NetworkManager::ConnectToHost(const char* theHostIP, int thePort)
{
    if (mIsConnected || mIsHost)
        return false;
    
    // Following GeeksforGeeks socket programming guide:
    // https://www.geeksforgeeks.org/cpp/socket-programming-in-cpp/
    
    // Step 1: Create client socket (TCP stream socket)
    SOCKET_TYPE clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET)
        return false;
    
    // Step 2: Define server address structure
    sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(thePort);
    
    // Resolve IP address
    if (strcmp(theHostIP, "localhost") == 0 || strcmp(theHostIP, "127.0.0.1") == 0)
    {
        serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    }
    else
    {
        // Try direct IP conversion first
        serverAddress.sin_addr.s_addr = inet_addr(theHostIP);
        if (serverAddress.sin_addr.s_addr == INADDR_NONE)
        {
            // Fallback to hostname resolution
            struct hostent* host = gethostbyname(theHostIP);
            if (host == nullptr || host->h_addr_list[0] == nullptr)
            {
                CLOSE_SOCKET(clientSocket);
                return false;
            }
            memcpy(&serverAddress.sin_addr, host->h_addr_list[0], host->h_length);
        }
    }
    
    // Set TCP socket options for reliability
    int keepAlive = 1;
    setsockopt(clientSocket, SOL_SOCKET, SO_KEEPALIVE, (const char*)&keepAlive, sizeof(keepAlive));
    int nodelay = 1;
    setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
    
    // Set socket to non-blocking for game responsiveness
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(clientSocket, FIONBIO, &mode) != 0)
    {
        CLOSE_SOCKET(clientSocket);
        return false;
    }
#else
    int flags = fcntl(clientSocket, F_GETFL, 0);
    if (fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        CLOSE_SOCKET(clientSocket);
        return false;
    }
#endif
    
    // Step 3: Connect to server (following guide pattern)
    int connectResult = connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    
    // Check if connected immediately (can happen with localhost)
    if (connectResult == 0)
    {
        mClientSocket = (void*)clientSocket;
        mIsConnected = true;
        ProcessNetworkMessages();
        return true;
    }
    
    // Connection in progress - wait with select() (non-blocking approach for games)
#ifdef _WIN32
    int error = WSAGetLastError();
    if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS)
    {
        CLOSE_SOCKET(clientSocket);
        return false;
    }
#else
    if (errno != EINPROGRESS && errno != EAGAIN)
    {
        CLOSE_SOCKET(clientSocket);
        return false;
    }
#endif
    
    // Wait for connection with select() - 10 second timeout
    fd_set writeSet, exceptSet;
    FD_ZERO(&writeSet);
    FD_ZERO(&exceptSet);
    FD_SET(clientSocket, &writeSet);
    FD_SET(clientSocket, &exceptSet);
    
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    
    int nfds = (int)clientSocket + 1;
    int selectResult = select(nfds, nullptr, &writeSet, &exceptSet, &timeout);
    
    if (selectResult <= 0)
    {
        CLOSE_SOCKET(clientSocket);
        return false;
    }
    
    // Check for connection errors
    if (FD_ISSET(clientSocket, &exceptSet))
    {
        CLOSE_SOCKET(clientSocket);
        return false;
    }
    
    // Verify connection succeeded
    if (FD_ISSET(clientSocket, &writeSet))
    {
        int socketError = 0;
        socklen_t len = sizeof(socketError);
#ifdef _WIN32
        if (getsockopt(clientSocket, SOL_SOCKET, SO_ERROR, (char*)&socketError, &len) == 0 && socketError == 0)
#else
        if (getsockopt(clientSocket, SOL_SOCKET, SO_ERROR, &socketError, &len) == 0 && socketError == 0)
#endif
        {
            // Connection successful!
            mClientSocket = (void*)clientSocket;
            mIsConnected = true;
            ProcessNetworkMessages();
            return true;
        }
    }
    
    // Connection failed
    CLOSE_SOCKET(clientSocket);
    return false;
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

