#include "Common.h"
#include "Globals.h"
#include "Utilities.h"
#include "Server.h"
#include "ClientInfo.h"

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    if (!initializeWinsock()) {
        return 1;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET) {
        cout << "[ERROR] Socket creation failed" << endl;
        WSACleanup();
        return 1;
    }

    u_long mode = 1;
    ioctlsocket(listenSocket, FIONBIO, &mode);

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "[ERROR] Bind failed: " << WSAGetLastError() << endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        cout << "[ERROR] Listen failed: " << WSAGetLastError() << endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    cout << "========================================" << endl;
    cout << "  CHAT SERVER WITH PRIVATE ROOMS" << endl;
    cout << "========================================" << endl;
    cout << "Port: " << PORT << endl;
    cout << "Press Ctrl+C to shutdown gracefully" << endl;
    cout << "========================================\n" << endl;

    vector<WSAPOLLFD> pollFds;
    mutex pollFdsMutex;

    WSAPOLLFD listenPollFd = {};
    listenPollFd.fd = listenSocket;
    listenPollFd.events = POLLRDNORM;
    pollFds.push_back(listenPollFd);

    thread broadcasterThread(broadcastMessages);

    char buffer[BUFFER_SIZE];

    while (!g_shutdownRequested) {
        vector<WSAPOLLFD> currentPollFds;

        {
            lock_guard<mutex> lock(pollFdsMutex);
            currentPollFds = pollFds;
        }

        int pollResult = WSAPoll(currentPollFds.data(),
            static_cast<ULONG>(currentPollFds.size()), 100);

        if (pollResult == SOCKET_ERROR) {
            cout << "[ERROR] WSAPoll failed: " << WSAGetLastError() << endl;
            break;
        }

        if (pollResult == 0) {
            continue;
        }

        for (size_t i = 0; i < currentPollFds.size(); i++) {
            if (currentPollFds[i].revents == 0) {
                continue;
            }

            if (i == 0 && (currentPollFds[i].revents & POLLRDNORM)) {
                SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);

                if (clientSocket != INVALID_SOCKET) {
                    u_long clientMode = 1;
                    ioctlsocket(clientSocket, FIONBIO, &clientMode);

                    cout << "[CONNECT] New client connected: " << clientSocket << endl;

                    {
                        lock_guard<mutex> clientLock(g_clientsMutex);
                        g_clients[clientSocket] = ClientInfo(clientSocket);
                    }

                    WSAPOLLFD clientPollFd = {};
                    clientPollFd.fd = clientSocket;
                    clientPollFd.events = POLLRDNORM;

                    {
                        lock_guard<mutex> lock(pollFdsMutex);
                        pollFds.push_back(clientPollFd);
                    }

                    string welcome = "WELCOME:Chat Server\n";
                    sendToClient(clientSocket, welcome);
                }
            }
            else {
                SOCKET clientSocket = currentPollFds[i].fd;

                if (currentPollFds[i].revents & (POLLRDNORM | POLLHUP | POLLERR)) {
                    int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);

                    if (bytesReceived <= 0) {
                        handleClientDisconnect(clientSocket, pollFds, pollFdsMutex);
                    }
                    else {
                        buffer[bytesReceived] = '\0';
                        handleClientMessage(clientSocket, buffer, bytesReceived);
                    }
                }
            }
        }
    }

    cout << "\n[SHUTDOWN] Initiating shutdown sequence..." << endl;

    closesocket(listenSocket);
    g_messageCV.notify_all();

    if (broadcasterThread.joinable()) {
        broadcasterThread.join();
        cout << "[SHUTDOWN] Broadcaster thread joined" << endl;
    }

    {
        lock_guard<mutex> lock(pollFdsMutex);
        for (size_t i = 1; i < pollFds.size(); i++) {
            if (pollFds[i].fd != INVALID_SOCKET) {
                closesocket(pollFds[i].fd);
            }
        }
        pollFds.clear();
    }

    {
        lock_guard<mutex> lock(g_chatRoomsMutex);
        g_chatRooms.clear();
    }

    {
        lock_guard<mutex> lock(g_clientsMutex);
        g_clients.clear();
    }

    WSACleanup();

    cout << "[SHUTDOWN] Server shutdown complete" << endl;
    return 0;
}