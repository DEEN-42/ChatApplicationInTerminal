#include "NetworkClient.h"
#include "ClientState.h" // Include the full definition for use
#include "UI.h"
#include "Utils.h"

#include <iostream>
#include <WS2tcpip.h>
#include <thread>
#include <string>
#include <atomic>
#include <sstream>
#include <chrono>
#include <iomanip>

using namespace std;

// ============================================================================
// MESSAGE SENDING
// ============================================================================

void sendMessageToServer(SOCKET serverSocket, ClientState& state) {
    // Get username
    bool nameAccepted = false;

    while (!nameAccepted && !state.shouldExit()) {
        string username;
        cout << "Enter your chat name: ";
        getline(cin, username);
        username = trim(username);

        if (username.empty()) {
            username = "Anonymous";
            cout << "Using default name: Anonymous\n" << endl;
        }

        state.setUsername(username);

        // Send username to server
        string nameCmd = "/SETNAME " + username;
        int result = send(serverSocket, nameCmd.c_str(), static_cast<int>(nameCmd.length()), 0);

        if (result == SOCKET_ERROR) {
            cout << "[ERROR] Failed to set username" << endl;
            state.setShouldExit(true);
            return;
        }

        state.setWaitingForNameValidation(true);

        // Wait for server response
        auto startTime = chrono::steady_clock::now();
        while (state.isWaitingForNameValidation() && !state.shouldExit()) {
            this_thread::sleep_for(chrono::milliseconds(50));

            // Timeout after 5 seconds
            auto elapsed = chrono::duration_cast<chrono::seconds>(
                chrono::steady_clock::now() - startTime).count();
            if (elapsed > 5) {
                cout << "[ERROR] Server response timeout" << endl;
                state.setShouldExit(true);
                return;
            }
        }

        if (!state.isWaitingForNameValidation() && !state.getUsername().empty()) {
            nameAccepted = true;
        }
        else {
            // Name was rejected, reset username and loop again
            state.setUsername("");
        }
    }

    if (state.shouldExit()) return;

    // Display menu
    displayMenu();

    string message;
    while (!state.shouldExit()) {
        getline(cin, message);
        message = trim(message);

        if (message.empty()) {
            continue;
        }

        // Handle quit command
        if (message == "quit" || message == "exit") {
            cout << "\nExiting chat..." << endl;
            state.setShouldExit(true);
            break;
        }

        // Check if it's a private message
        if (message[0] == '@') {
            if (!state.isInRoom()) {
                cout << "[ERROR] You must join or create a room first to send private messages!" << endl;
                continue;
            }

            size_t spacePos = message.find(' ');
            if (spacePos == string::npos || spacePos == 1) {
                cout << "[ERROR] Invalid private message format. Use: @username message" << endl;
                continue;
            }

            string recipientName = message.substr(1, spacePos - 1);
            recipientName = trim(recipientName);

            if (recipientName.empty()) {
                cout << "[ERROR] Invalid private message format. Use: @username message" << endl;
                continue;
            }
        }
        // Check if it's a command
        else if (message[0] == '/') {
            stringstream ss(message.substr(1));
            string cmd;
            ss >> cmd;

            // Convert to uppercase for comparison
            for (auto& c : cmd) c = toupper(c);

            if (cmd == "CREATE") {
                string type;
                ss >> type;
                for (auto& c : type) c = toupper(c);

                if (type != "PUBLIC" && type != "PRIVATE") {
                    cout << "[ERROR] Invalid room type. Use PUBLIC or PRIVATE" << endl;
                    cout << "Examples:" << endl;
                    cout << "  /CREATE PUBLIC" << endl;
                    cout << "  /CREATE PRIVATE mypassword123\n" << endl;
                    continue;
                }

                if (type == "PRIVATE") {
                    string password;
                    getline(ss, password);
                    password = trim(password);

                    if (password.empty()) {
                        cout << "[ERROR] Private rooms require a password" << endl;
                        cout << "Example: /CREATE PRIVATE mypassword123\n" << endl;
                        continue;
                    }
                }
            }
            else if (cmd == "JOIN") {
                string roomId;
                ss >> roomId;

                if (roomId.empty()) {
                    cout << "[ERROR] Please specify a room ID" << endl;
                    cout << "Example: /JOIN 123456\n" << endl;
                    continue;
                }

                state.setPendingRoomJoin(roomId);
            }
            else if (cmd == "LEAVE") {
                if (!state.isInRoom()) {
                    cout << "[ERROR] You are not in a room" << endl;
                    continue;
                }

                if (state.hasOwnerLeaveWarning()) {
                    // Owner wants to force leave
                    message = "/FORCELEAVE";
                    state.setOwnerLeaveWarning(false);
                }
            }
            else if (cmd == "KICK") {
                if (!state.isInRoom()) {
                    cout << "[ERROR] You must be in a room to kick users" << endl;
                    continue;
                }
                if (!state.isRoomOwner()) {
                    cout << "[ERROR] Only room owner can kick users" << endl;
                    continue;
                }
                string targetUser;
                getline(ss, targetUser);
                targetUser = trim(targetUser);
                if (targetUser.empty()) {
                    cout << "[ERROR] Please specify a username to kick" << endl;
                    cout << "Example: /KICK username\n" << endl;
                    continue;
                }
            }
            else if (cmd == "BAN") {
                if (!state.isInRoom()) {
                    cout << "[ERROR] You must be in a room to ban users" << endl;
                    continue;
                }
                if (!state.isRoomOwner()) {
                    cout << "[ERROR] Only room owner can ban users" << endl;
                    continue;
                }
                string targetUser;
                getline(ss, targetUser);
                targetUser = trim(targetUser);
                if (targetUser.empty()) {
                    cout << "[ERROR] Please specify a username to ban" << endl;
                    cout << "Example: /BAN username\n" << endl;
                    continue;
                }
            }
            else if (cmd == "TRANSFER") {
                if (!state.isInRoom()) {
                    cout << "[ERROR] You must be in a room to transfer ownership" << endl;
                    continue;
                }
                if (!state.isRoomOwner()) {
                    cout << "[ERROR] Only room owner can transfer ownership" << endl;
                    continue;
                }
                string targetUser;
                getline(ss, targetUser);
                targetUser = trim(targetUser);
                if (targetUser.empty()) {
                    cout << "[ERROR] Please specify a username to transfer ownership to" << endl;
                    cout << "Example: /TRANSFER username\n" << endl;
                    continue;
                }
            }
            else if (cmd == "CHANGEPASSWORD") {
                if (!state.isInRoom()) {
                    cout << "[ERROR] You must be in a room to change password" << endl;
                    continue;
                }
                if (!state.isRoomOwner()) {
                    cout << "[ERROR] Only room owner can change password" << endl;
                    continue;
                }
                string newPass;
                getline(ss, newPass);
                newPass = trim(newPass);
                if (newPass.empty()) {
                    cout << "[ERROR] Please specify a new password" << endl;
                    cout << "Example: /CHANGEPASSWORD newpassword123\n" << endl;
                    continue;
                }
            }
            else if (cmd == "HELP") {
                displayMenu();
                continue;
            }
            else if (cmd == "LIST") {
                // Just pass through to server
            }
            else if (cmd == "USERS") {
                // Just pass through to server
            }
            else if (cmd == "GETPASSWORD") {
                // Pass through to server
            }
            else {
                cout << "[ERROR] Unknown command. Type /HELP for available commands." << endl;
                continue;
            }
        }
        else {
            // Regular message - check if in a room
            if (!state.isInRoom()) {
                cout << "[ERROR] You must join or create a room first!" << endl;
                cout << "Use /CREATE PUBLIC or /CREATE PRIVATE <password> to create a room" << endl;
                cout << "Use /JOIN <room_id> to join a public room" << endl;
                cout << "Use /JOIN <room_id> <password> to join a private room" << endl;
                cout << "Type /LIST to see available rooms.\n" << endl;
                continue;
            }
        }

        // Send message to server
        int bytesSent = send(serverSocket, message.c_str(), static_cast<int>(message.length()), 0);

        if (bytesSent == SOCKET_ERROR) {
            cout << "[ERROR] Failed to send message: " << WSAGetLastError() << endl;
            state.setShouldExit(true);
            break;
        }
    }
}


// ============================================================================
// MESSAGE RECEIVING
// ============================================================================

void receiveMessages(SOCKET serverSocket, ClientState& state) {
    const int BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    string incompleteMessage = "";
    bool inMessageHistory = false;

    while (!state.shouldExit()) {
        int bytesReceived = recv(serverSocket, buffer, BUFFER_SIZE - 1, 0);

        if (bytesReceived <= 0) {
            if (bytesReceived == 0) {
                cout << "\n[INFO] Server closed the connection" << endl;
            }
            else if (!state.shouldExit()) {
                int error = WSAGetLastError();
                if (error != WSAEWOULDBLOCK) {
                    cout << "\n[ERROR] Connection error: " << error << endl;
                }
            }
            if (bytesReceived == 0) {
                state.setShouldExit(true);
            }
            break;
        }

        buffer[bytesReceived] = '\0';
        string receivedData = incompleteMessage + string(buffer, bytesReceived);

        size_t pos = 0;
        size_t newlinePos;

        while ((newlinePos = receivedData.find('\n', pos)) != string::npos) {
            string message = receivedData.substr(pos, newlinePos - pos);
            message = trim(message);

            if (!message.empty()) {
                // Parse server responses
                if (message.find("WELCOME:") == 0) {
                    cout << "[+] Connected to server successfully!\n" << endl;
                }
                else if (message.find("ROOM_CREATED:") == 0) {
                    size_t firstColon = message.find(':');
                    size_t secondColon = message.find(':', firstColon + 1);

                    string currentRoomId = message.substr(firstColon + 1, secondColon - firstColon - 1);
                    string roomType = message.substr(secondColon + 1);

                    currentRoomId = trim(currentRoomId);
                    roomType = trim(roomType);

                    state.setCurrentRoomId(currentRoomId);
                    state.setInRoom(true);
                    state.setRoomOwner(true);
                    displayRoomCreated(currentRoomId, roomType);
                }
                else if (message.find("ROOM_JOINED:") == 0) {
                    string currentRoomId = message.substr(12);
                    currentRoomId = trim(currentRoomId);
                    state.setCurrentRoomId(currentRoomId);
                    state.setInRoom(true);
                    state.setRoomOwner(false);
                    state.clearPendingJoin();
                    displayRoomJoined(currentRoomId);
                }
                else if (message.find("MESSAGE_HISTORY_START") == 0) {
                    inMessageHistory = true;
                    cout << "\n--- Previous Messages ---" << endl;
                }
                else if (message.find("MESSAGE_HISTORY_END") == 0) {
                    inMessageHistory = false;
                    cout << "--- End of History ---\n" << endl;
                }
                else if (message.find("ROOM_NOT_FOUND") == 0) {
                    cout << "\n[X] Error: Room not found! Please check the room ID." << endl;
                    cout << "Use /LIST to see available rooms or /CREATE to make a new one.\n" << endl;
                    state.setInRoom(false);
                    state.clearPendingJoin();
                }
                else if (message.find("PASSWORD_REQUIRED") == 0) {
                    cout << "\n[*] This is a PRIVATE room. Password required." << endl;
                    cout << "Use: /JOIN " << state.getPendingRoomJoin() << " <password>\n" << endl;
                    state.clearPendingJoin();
                }
                else if (message.find("WRONG_PASSWORD") == 0) {
                    cout << "\n[X] Error: Incorrect password!" << endl;
                    cout << "Please try again with the correct password.\n" << endl;
                    state.clearPendingJoin();
                }
                else if (message.find("NAME_SET") == 0) {
                    state.setWaitingForNameValidation(false);
                    cout << "[+] Username set successfully: " << state.getUsername() << "\n" << endl;
                }
                else if (message.find("NAME_TAKEN") == 0) {
                    cout << "\n[X] Error: Username '" << state.getUsername() << "' is already taken!" << endl;
                    cout << "Please choose a different name.\n" << endl;
                    state.setUsername("");
                    state.setWaitingForNameValidation(false);
                }
                else if (message.find("ROOMS_LIST:") == 0) {
                    string roomsList = message.substr(11);
                    displayRoomsList(roomsList);
                }
                else if (message.find("USERS_LIST:") == 0) {
                    string usersList = message.substr(11);
                    displayUsersList(usersList, state.getUsername(), state.isRoomOwner());
                }
                else if (message.find("ROOM_PASSWORD:") == 0) {
                    string password = message.substr(14);
                    password = trim(password);
                    cout << "\n+========================================+" << endl;
                    cout << "|         ROOM PASSWORD                  |" << endl;
                    cout << "+========================================+" << endl;
                    cout << "Password: " << password << endl;
                    cout << "Keep this safe and share only with trusted users!\n" << endl;
                }
                else if (message.find("PASSWORD_CHANGED:") == 0) {
                    string newPassword = message.substr(17);
                    newPassword = trim(newPassword);
                    cout << "\n+========================================+" << endl;
                    cout << "|    PASSWORD CHANGED SUCCESSFULLY!      |" << endl;
                    cout << "+========================================+" << endl;
                    cout << "New Password: " << newPassword << endl;
                    cout << "Share this with users you want to join!\n" << endl;
                }
                else if (message.find("KICKED_FROM_ROOM") == 0) {
                    state.resetRoomState();
                    cout << "\n[INFO] You have been removed from the room." << endl;
                    cout << "Use /LIST to find other rooms or /CREATE to make your own.\n" << endl;
                }
                else if (message.find("LEFT_ROOM") == 0) {
                    state.resetRoomState();
                    cout << "\n[+] You have left the room." << endl;
                    cout << "Use /LIST to find other rooms or /CREATE to make your own.\n" << endl;
                }
                else if (message.find("OWNER_LEAVE_WARNING") == 0) {
                    state.setOwnerLeaveWarning(true);
                    cout << "\n[!] WARNING: You are the room owner!" << endl;
                    cout << "Options:" << endl;
                    cout << "  1. Transfer ownership first: /TRANSFER <username>" << endl;
                    cout << "  2. Force leave (ownership goes to longest member): /LEAVE again\n" << endl;
                }
                else if (message.find("OWNERSHIP_RECEIVED") == 0) {
                    state.setRoomOwner(true);
                    cout << "\n+========================================+" << endl;
                    cout << "|   YOU ARE NOW THE ROOM OWNER!          |" << endl;
                    cout << "+========================================+" << endl;
                    cout << "You now have access to owner commands." << endl;
                    cout << "Type /HELP to see all available commands.\n" << endl;
                }
                else if (message.find("SUCCESS:") == 0) {
                    cout << "\n[+] " << message.substr(8) << "\n" << endl;
                }
                else if (message.find("PM_FROM:") == 0) {
                    // Private message received: PM_FROM:sender:message
                    size_t firstColon = message.find(':');
                    size_t secondColon = message.find(':', firstColon + 1);

                    string sender = message.substr(firstColon + 1, secondColon - firstColon - 1);
                    string pmContent = message.substr(secondColon + 1);

                    string formattedPM = "[PM from " + sender + "]: " + pmContent;
                    printAlignedMessage(formattedPM, false);
                }
                else if (message.find("PM_SENT:") == 0) {
                    // Confirmation that PM was sent: PM_SENT:recipient:message
                    size_t firstColon = message.find(':');
                    size_t secondColon = message.find(':', firstColon + 1);

                    string recipient = message.substr(firstColon + 1, secondColon - firstColon - 1);
                    string pmContent = message.substr(secondColon + 1);

                    string formattedPM = "[PM to " + recipient + "]: " + pmContent;
                    printAlignedMessage(formattedPM, true);
                }
                else if (message.find("ERROR:") == 0) {
                    cout << "\n" << message << "\n" << endl;
                }
                else if (message.find("SYSTEM:") == 0) {
                    // System messages (user joined/left)
                    if (state.isInRoom() || inMessageHistory) {
                        cout << "\n" << message << endl;
                    }
                }
                else {
                    // Regular chat message from other users
                    if (state.isInRoom() || inMessageHistory) {
                        printAlignedMessage(message, false);
                    }
                }
            }

            pos = newlinePos + 1;
        }

        // Save incomplete message for next iteration
        if (pos < receivedData.length()) {
            incompleteMessage = receivedData.substr(pos);
        }
        else {
            incompleteMessage = "";
        }
    }
}


// ============================================================================
// CONNECTION MANAGEMENT
// ============================================================================

SOCKET connectToServer(const string& serverAddress, int port) {
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket == INVALID_SOCKET) {
        cout << "[ERROR] Socket creation failed: " << WSAGetLastError() << endl;
        return INVALID_SOCKET;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, serverAddress.c_str(), &serverAddr.sin_addr) <= 0) {
        cout << "[ERROR] Invalid address: " << serverAddress << endl;
        closesocket(clientSocket);
        return INVALID_SOCKET;
    }

    cout << "Connecting to server at " << serverAddress << ":" << port << "..." << endl;

    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "[ERROR] Could not connect to server: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        return INVALID_SOCKET;
    }

    return clientSocket;
}