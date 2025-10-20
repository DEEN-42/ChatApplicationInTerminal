#include <iostream>
#include <WinSock2.h>
#include <thread>
#include <string>
#include <chrono>

#include "ClientState.h"
#include "Utils.h"
#include "UI.h"
#include "NetworkClient.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main() {
    if (!initializeWinsock()) {
        return 1;
    }

    displayWelcome();

    const string SERVER_ADDRESS = "127.0.0.1";
    const int SERVER_PORT = 12345;

    SOCKET serverSocket = connectToServer(SERVER_ADDRESS, SERVER_PORT);

    if (serverSocket == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    // Create client state object
    ClientState clientState;

    // Wait for welcome message before prompting for username
    this_thread::sleep_for(chrono::milliseconds(200));

    // Start sender and receiver threads
    thread senderThread(sendMessageToServer, serverSocket, ref(clientState));
    thread receiverThread(receiveMessages, serverSocket, ref(clientState));

    // Wait for sender thread (user initiated exit)
    if (senderThread.joinable()) {
        senderThread.join();
    }

    // Signal receiver thread to stop
    clientState.setShouldExit(true);

    // Wait for receiver thread with timeout
    if (receiverThread.joinable()) {
        this_thread::sleep_for(chrono::milliseconds(500));
        receiverThread.join();
    }

    // Cleanup
    cout << "\nClosing connection and cleaning up..." << endl;
    closesocket(serverSocket);
    WSACleanup();

    cout << "Goodbye!" << endl;
    return 0;
}