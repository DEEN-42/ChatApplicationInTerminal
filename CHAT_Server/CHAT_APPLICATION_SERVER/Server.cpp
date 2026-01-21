#include "Server.h"
#include "Globals.h"
#include "Utilities.h"
#include "ChatRoom.h"
#include "ClientInfo.h"
#include "Message.h"
#include "Database.h"

// ============================================================================
// UTILITY FUNCTIONS (Server-Specific)
// ============================================================================

bool isUsernameAvailable(const string& username, SOCKET excludeSocket) {
    lock_guard<mutex> lock(g_clientsMutex);
    for (const auto& pair : g_clients) {
        if (pair.first != excludeSocket && pair.second.getUsername() == username) {
            return false;
        }
    }
    return true;
}

SOCKET findClientByUsername(const string& username, const string& roomId) {
    lock_guard<mutex> lock(g_clientsMutex);
    for (const auto& pair : g_clients) {
        if (pair.second.getUsername() == username &&
            pair.second.getRoomId() == roomId) {
            return pair.first;
        }
    }
    return INVALID_SOCKET;
}

// ============================================================================
// ROOM MANAGEMENT
// ============================================================================

void cleanupEmptyRoom(const string& roomId) {
    lock_guard<mutex> lock(g_chatRoomsMutex);
    auto it = g_chatRooms.find(roomId);

    if (it != g_chatRooms.end() && it->second->isEmpty()) {
        cout << "[CLEANUP] Deleting empty room: " << roomId << endl;
        g_chatRooms.erase(it);
        
        // Delete room from database
        if (g_database) {
            g_database->deleteRoom(roomId);
        }
    }
}

void removeClientFromRoom(SOCKET clientSocket) {
    string roomId;
    string username;

    {
        lock_guard<mutex> clientLock(g_clientsMutex);
        auto clientIt = g_clients.find(clientSocket);
        if (clientIt == g_clients.end()) {
            return;
        }

        roomId = clientIt->second.getRoomId();
        username = clientIt->second.getUsername();

        if (roomId.empty()) {
            return;
        }
    }

    // Notify others that user left
    {
        lock_guard<mutex> roomLock(g_chatRoomsMutex);
        auto roomIt = g_chatRooms.find(roomId);
        if (roomIt != g_chatRooms.end()) {
            string leaveMsg = getCurrentTimestamp() + " SYSTEM: " +
                username + " has left the room\n";
            roomIt->second->broadcast(leaveMsg, clientSocket);
            roomIt->second->removeClient(clientSocket);
        }
    }

    {
        lock_guard<mutex> clientLock(g_clientsMutex);
        g_clients[clientSocket].setRoomId("");
        g_clients[clientSocket].setIsRoomOwner(false);
    }

    // Cleanup empty room
    cleanupEmptyRoom(roomId);
}

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

void handleCreateCommand(SOCKET clientSocket, const string& params) {
    stringstream ss(params);
    string typeStr, password;
    ss >> typeStr;

    bool isPrivate = (typeStr == "PRIVATE");

    if (isPrivate) {
        getline(ss, password);
        password = trim(password);
        if (password.empty()) {
            sendToClient(clientSocket, "ERROR: Private rooms require a password\n");
            return;
        }
    }

    string roomId = generateRoomId();
    string ownerUsername;

    // Check if client is already in a room
    {
        lock_guard<mutex> clientLock(g_clientsMutex);
        ClientInfo& client = g_clients[clientSocket];
        ownerUsername = client.getUsername();

        if (!client.getRoomId().empty()) {
            // Remove from old room first
            lock_guard<mutex> roomLock(g_chatRoomsMutex);
            auto oldRoomIt = g_chatRooms.find(client.getRoomId());
            if (oldRoomIt != g_chatRooms.end()) {
                string leaveMsg = getCurrentTimestamp() + " SYSTEM: " +
                    client.getUsername() + " has left the room\n";
                oldRoomIt->second->broadcast(leaveMsg, clientSocket);
                oldRoomIt->second->removeClient(clientSocket);

                if (oldRoomIt->second->isEmpty()) {
                    string oldRoomId = client.getRoomId();
                    thread([oldRoomId]() {
                        this_thread::sleep_for(chrono::milliseconds(100));
                        cleanupEmptyRoom(oldRoomId);
                        }).detach();
                }
            }
        }
    }

    {
        lock_guard<mutex> roomLock(g_chatRoomsMutex);
        g_chatRooms[roomId] = make_shared<ChatRoom>(roomId, isPrivate, password, clientSocket);
        g_chatRooms[roomId]->addClient(clientSocket);
    }

    {
        lock_guard<mutex> clientLock(g_clientsMutex);
        g_clients[clientSocket].setRoomId(roomId);
        g_clients[clientSocket].setIsRoomOwner(true);
    }

    // Save room to database
    if (g_database) {
        g_database->createRoom(roomId, isPrivate, ownerUsername, password);
    }

    string response = "ROOM_CREATED:" + roomId + ":" +
        (isPrivate ? "PRIVATE" : "PUBLIC") + "\n";
    sendToClient(clientSocket, response);

    cout << "[CMD] Client " << clientSocket << " created "
        << (isPrivate ? "private" : "public") << " room: " << roomId << endl;
}

void handleJoinCommand(SOCKET clientSocket, const string& params) {
    stringstream ss(params);
    string roomId, password;
    ss >> roomId;

    roomId = trim(roomId);

    if (roomId.empty()) {
        sendToClient(clientSocket, "ERROR: Room ID cannot be empty\n");
        return;
    }

    lock_guard<mutex> clientLock(g_clientsMutex);
    ClientInfo& client = g_clients[clientSocket];

    if (roomId == client.getRoomId()) {
        sendToClient(clientSocket, "ERROR: You are already in this room\n");
        return;
    }

    lock_guard<mutex> roomLock(g_chatRoomsMutex);
    auto targetRoomIt = g_chatRooms.find(roomId);

    if (targetRoomIt == g_chatRooms.end()) {
        sendToClient(clientSocket, "ROOM_NOT_FOUND\n");
        return;
    }

    // Check if user is banned (check both in-memory and database)
    if (targetRoomIt->second->isUserBanned(client.getUsername()) ||
        (g_database && g_database->isUserBanned(roomId, client.getUsername()))) {
        sendToClient(clientSocket, "ERROR: You are banned from this room\n");
        return;
    }

    // Check if room is private
    if (targetRoomIt->second->getIsPrivate()) {
        getline(ss, password);
        password = trim(password);

        if (password.empty()) {
            sendToClient(clientSocket, "PASSWORD_REQUIRED\n");
            return;
        }

        if (!targetRoomIt->second->verifyPassword(password)) {
            sendToClient(clientSocket, "WRONG_PASSWORD\n");
            return;
        }
    }

    // Remove from old room
    if (!client.getRoomId().empty()) {
        auto oldRoomIt = g_chatRooms.find(client.getRoomId());
        if (oldRoomIt != g_chatRooms.end()) {
            string leaveMsg = getCurrentTimestamp() + " SYSTEM: " +
                client.getUsername() + " has left the room\n";
            oldRoomIt->second->broadcast(leaveMsg, clientSocket);
            oldRoomIt->second->removeClient(clientSocket);

            if (oldRoomIt->second->isEmpty()) {
                string oldRoomId = client.getRoomId();
                thread([oldRoomId]() {
                    this_thread::sleep_for(chrono::milliseconds(100));
                    cleanupEmptyRoom(oldRoomId);
                    }).detach();
            }
        }
    }

    // Join new room
    targetRoomIt->second->addClient(clientSocket);
    client.setRoomId(roomId);
    client.setIsRoomOwner(false);

    string response = "ROOM_JOINED:" + roomId + "\n";
    sendToClient(clientSocket, response);

    // Send message history - first from database, then from memory
    vector<string> history;
    if (g_database) {
        history = g_database->getMessageHistory(roomId, MAX_MESSAGE_HISTORY);
    }
    
    // If database history is empty, use in-memory history
    if (history.empty()) {
        history = targetRoomIt->second->getMessageHistory();
    }

    if (!history.empty()) {
        sendToClient(clientSocket, "MESSAGE_HISTORY_START\n");
        for (const string& msg : history) {
            sendToClient(clientSocket, msg);
        }
        sendToClient(clientSocket, "MESSAGE_HISTORY_END\n");
    }

    // Notify others
    string joinMsg = getCurrentTimestamp() + " SYSTEM: " +
        client.getUsername() + " has joined the room\n";
    targetRoomIt->second->broadcast(joinMsg, clientSocket);

    cout << "[CMD] Client " << clientSocket << " (" << client.getUsername()
        << ") joined room: " << roomId << endl;
}

void handleSetNameCommand(SOCKET clientSocket, const string& name) {
    string trimmedName = trim(name);

    if (trimmedName.empty()) {
        sendToClient(clientSocket, "ERROR: Name cannot be empty\n");
        return;
    }

    if (!isUsernameAvailable(trimmedName, clientSocket)) {
        sendToClient(clientSocket, "NAME_TAKEN\n");
        return;
    }

    {
        lock_guard<mutex> clientLock(g_clientsMutex);
        g_clients[clientSocket].setUsername(trimmedName);
    }

    sendToClient(clientSocket, "NAME_SET\n");
    cout << "[CMD] Client " << clientSocket << " set name: " << trimmedName << endl;
}

void handleListCommand(SOCKET clientSocket) {
    lock_guard<mutex> roomLock(g_chatRoomsMutex);

    string response = "ROOMS_LIST:";
    for (const auto& pair : g_chatRooms) {
        response += pair.first + "(" + to_string(pair.second->getClientCount()) + ")" +
            (pair.second->getIsPrivate() ? "[PRIVATE]" : "[PUBLIC]") + ",";
    }
    response += "\n";

    sendToClient(clientSocket, response);
    cout << "[CMD] Client " << clientSocket << " requested room list" << endl;
}

void handleGetPasswordCommand(SOCKET clientSocket) {
    string roomId;
    bool isOwner = false;

    {
        lock_guard<mutex> clientLock(g_clientsMutex);
        auto it = g_clients.find(clientSocket);
        if (it != g_clients.end()) {
            roomId = it->second.getRoomId();
            isOwner = it->second.isRoomOwner();
        }
    }

    if (roomId.empty()) {
        sendToClient(clientSocket, "ERROR: You are not in a room\n");
        return;
    }

    if (!isOwner) {
        sendToClient(clientSocket, "ERROR: Only room owner can view password\n");
        return;
    }

    lock_guard<mutex> roomLock(g_chatRoomsMutex);
    auto roomIt = g_chatRooms.find(roomId);

    if (roomIt != g_chatRooms.end()) {
        if (roomIt->second->getIsPrivate()) {
            string response = "ROOM_PASSWORD:" + roomIt->second->getPassword() + "\n";
            sendToClient(clientSocket, response);
        }
        else {
            sendToClient(clientSocket, "ERROR: This is a public room\n");
        }
    }
}

void handleUsersCommand(SOCKET clientSocket) {
    string roomId;

    {
        lock_guard<mutex> clientLock(g_clientsMutex);
        auto it = g_clients.find(clientSocket);
        if (it != g_clients.end()) {
            roomId = it->second.getRoomId();
        }
    }

    if (roomId.empty()) {
        sendToClient(clientSocket, "ERROR: You are not in a room\n");
        return;
    }

    lock_guard<mutex> roomLock(g_chatRoomsMutex);
    auto roomIt = g_chatRooms.find(roomId);

    if (roomIt != g_chatRooms.end()) {
        set<SOCKET> roomClients = roomIt->second->getClients();

        string response = "USERS_LIST:";
        lock_guard<mutex> clientLock(g_clientsMutex);
        for (SOCKET sock : roomClients) {
            auto clientIt = g_clients.find(sock);
            if (clientIt != g_clients.end() && !clientIt->second.getUsername().empty()) {
                response += clientIt->second.getUsername() + ",";
            }
        }
        response += "\n";

        sendToClient(clientSocket, response);
        cout << "[CMD] Client " << clientSocket << " requested users list" << endl;
    }
}

void handleKickCommand(SOCKET clientSocket, const string& params) {
    string targetUsername = trim(params);

    if (targetUsername.empty()) {
        sendToClient(clientSocket, "ERROR: Please specify a username to kick\n");
        return;
    }

    string roomId;
    bool isOwner = false;
    string ownerName;

    {
        lock_guard<mutex> clientLock(g_clientsMutex);
        auto it = g_clients.find(clientSocket);
        if (it != g_clients.end()) {
            roomId = it->second.getRoomId();
            isOwner = it->second.isRoomOwner();
            ownerName = it->second.getUsername();
        }
    }

    if (roomId.empty()) {
        sendToClient(clientSocket, "ERROR: You are not in a room\n");
        return;
    }

    if (!isOwner) {
        sendToClient(clientSocket, "ERROR: Only room owner can kick users\n");
        return;
    }

    if (targetUsername == ownerName) {
        sendToClient(clientSocket, "ERROR: You cannot kick yourself\n");
        return;
    }

    SOCKET targetSocket = findClientByUsername(targetUsername, roomId);

    if (targetSocket == INVALID_SOCKET) {
        sendToClient(clientSocket, "ERROR: User not found in this room\n");
        return;
    }

    // Notify the kicked user
    sendToClient(targetSocket, getCurrentTimestamp() +
        " SYSTEM: You have been kicked from the room by the owner\n");
    sendToClient(targetSocket, "KICKED_FROM_ROOM\n");

    // Remove from room
    {
        lock_guard<mutex> roomLock(g_chatRoomsMutex);
        auto roomIt = g_chatRooms.find(roomId);
        if (roomIt != g_chatRooms.end()) {
            string kickMsg = getCurrentTimestamp() + " SYSTEM: " +
                targetUsername + " has been kicked from the room\n";
            roomIt->second->broadcastToAll(kickMsg);
            roomIt->second->removeClient(targetSocket);
        }
    }

    {
        lock_guard<mutex> clientLock(g_clientsMutex);
        g_clients[targetSocket].setRoomId("");
        g_clients[targetSocket].setIsRoomOwner(false);
    }

    cout << "[CMD] Client " << clientSocket << " kicked " << targetUsername
        << " from room " << roomId << endl;
}

void handleBanCommand(SOCKET clientSocket, const string& params) {
    string targetUsername = trim(params);

    if (targetUsername.empty()) {
        sendToClient(clientSocket, "ERROR: Please specify a username to ban\n");
        return;
    }

    string roomId;
    bool isOwner = false;
    string ownerName;

    {
        lock_guard<mutex> clientLock(g_clientsMutex);
        auto it = g_clients.find(clientSocket);
        if (it != g_clients.end()) {
            roomId = it->second.getRoomId();
            isOwner = it->second.isRoomOwner();
            ownerName = it->second.getUsername();
        }
    }

    if (roomId.empty()) {
        sendToClient(clientSocket, "ERROR: You are not in a room\n");
        return;
    }

    if (!isOwner) {
        sendToClient(clientSocket, "ERROR: Only room owner can ban users\n");
        return;
    }

    if (targetUsername == ownerName) {
        sendToClient(clientSocket, "ERROR: You cannot ban yourself\n");
        return;
    }

    SOCKET targetSocket = findClientByUsername(targetUsername, roomId);

    {
        lock_guard<mutex> roomLock(g_chatRoomsMutex);
        auto roomIt = g_chatRooms.find(roomId);
        if (roomIt != g_chatRooms.end()) {
            roomIt->second->banUser(targetUsername);

            // Save ban to database
            if (g_database) {
                g_database->addBan(roomId, targetUsername);
            }

            if (targetSocket != INVALID_SOCKET) {
                // User is currently in the room, kick them
                sendToClient(targetSocket, getCurrentTimestamp() +
                    " SYSTEM: You have been banned from the room by the owner\n");
                sendToClient(targetSocket, "KICKED_FROM_ROOM\n");

                string banMsg = getCurrentTimestamp() + " SYSTEM: " +
                    targetUsername + " has been banned from the room\n";
                roomIt->second->broadcastToAll(banMsg);
                roomIt->second->removeClient(targetSocket);

                lock_guard<mutex> clientLock(g_clientsMutex);
                g_clients[targetSocket].setRoomId("");
                g_clients[targetSocket].setIsRoomOwner(false);
            }

            sendToClient(clientSocket, "SUCCESS: User " + targetUsername + " has been banned\n");
        }
    }

    cout << "[CMD] Client " << clientSocket << " banned " << targetUsername
        << " from room " << roomId << endl;
}

void handleTransferCommand(SOCKET clientSocket, const string& params) {
    string targetUsername = trim(params);

    if (targetUsername.empty()) {
        sendToClient(clientSocket, "ERROR: Please specify a username to transfer ownership\n");
        return;
    }

    string roomId;
    bool isOwner = false;
    string ownerName;

    {
        lock_guard<mutex> clientLock(g_clientsMutex);
        auto it = g_clients.find(clientSocket);
        if (it != g_clients.end()) {
            roomId = it->second.getRoomId();
            isOwner = it->second.isRoomOwner();
            ownerName = it->second.getUsername();
        }
    }

    if (roomId.empty()) {
        sendToClient(clientSocket, "ERROR: You are not in a room\n");
        return;
    }

    if (!isOwner) {
        sendToClient(clientSocket, "ERROR: Only room owner can transfer ownership\n");
        return;
    }

    if (targetUsername == ownerName) {
        sendToClient(clientSocket, "ERROR: You are already the owner\n");
        return;
    }

    SOCKET targetSocket = findClientByUsername(targetUsername, roomId);

    if (targetSocket == INVALID_SOCKET) {
        sendToClient(clientSocket, "ERROR: User not found in this room\n");
        return;
    }

    // Transfer ownership
    {
        lock_guard<mutex> roomLock(g_chatRoomsMutex);
        auto roomIt = g_chatRooms.find(roomId);
        if (roomIt != g_chatRooms.end()) {
            roomIt->second->setOwner(targetSocket);

            lock_guard<mutex> clientLock(g_clientsMutex);
            g_clients[clientSocket].setIsRoomOwner(false);
            g_clients[targetSocket].setIsRoomOwner(true);

            // Update owner in database
            if (g_database) {
                g_database->updateRoomOwner(roomId, targetUsername);
            }

            string transferMsg = getCurrentTimestamp() +
                " SYSTEM: Room ownership transferred to " +
                targetUsername + "\n";
            roomIt->second->broadcastToAll(transferMsg);

            sendToClient(clientSocket, "SUCCESS: Ownership transferred to " +
                targetUsername + "\n");
            sendToClient(targetSocket, "OWNERSHIP_RECEIVED\n");
        }
    }

    cout << "[CMD] Ownership of room " << roomId << " transferred from "
        << ownerName << " to " << targetUsername << endl;
}

void handleLeaveCommand(SOCKET clientSocket) {
    string roomId;
    bool isOwner = false;
    string username;

    {
        lock_guard<mutex> clientLock(g_clientsMutex);
        auto it = g_clients.find(clientSocket);
        if (it != g_clients.end()) {
            roomId = it->second.getRoomId();
            isOwner = it->second.isRoomOwner();
            username = it->second.getUsername();
        }
    }

    if (roomId.empty()) {
        sendToClient(clientSocket, "ERROR: You are not in a room\n");
        return;
    }

    if (isOwner) {
        sendToClient(clientSocket, "OWNER_LEAVE_WARNING\n");
        return;
    }

    // Leave room
    {
        lock_guard<mutex> roomLock(g_chatRoomsMutex);
        auto roomIt = g_chatRooms.find(roomId);
        if (roomIt != g_chatRooms.end()) {
            string leaveMsg = getCurrentTimestamp() + " SYSTEM: " +
                username + " has left the room\n";
            roomIt->second->broadcast(leaveMsg, clientSocket);
            roomIt->second->removeClient(clientSocket);

            if (roomIt->second->isEmpty()) {
                thread([roomId]() {
                    this_thread::sleep_for(chrono::milliseconds(100));
                    cleanupEmptyRoom(roomId);
                    }).detach();
            }
        }
    }

    {
        lock_guard<mutex> clientLock(g_clientsMutex);
        g_clients[clientSocket].setRoomId("");
    }

    sendToClient(clientSocket, "LEFT_ROOM\n");
    cout << "[CMD] Client " << clientSocket << " (" << username
        << ") left room: " << roomId << endl;
}

void handleForceLeaveCommand(SOCKET clientSocket) {
    string roomId;
    bool isOwner = false;
    string username;

    {
        lock_guard<mutex> clientLock(g_clientsMutex);
        auto it = g_clients.find(clientSocket);
        if (it != g_clients.end()) {
            roomId = it->second.getRoomId();
            isOwner = it->second.isRoomOwner();
            username = it->second.getUsername();
        }
    }

    if (roomId.empty()) {
        sendToClient(clientSocket, "ERROR: You are not in a room\n");
        return;
    }

    if (!isOwner) {
        sendToClient(clientSocket, "ERROR: This command is for room owners\n");
        return;
    }

    // Owner forcing leave - transfer to longest member
    SOCKET newOwner = INVALID_SOCKET;
    string newOwnerName;

    {
        lock_guard<mutex> roomLock(g_chatRoomsMutex);
        auto roomIt = g_chatRooms.find(roomId);
        if (roomIt != g_chatRooms.end()) {
            if (roomIt->second->getClientCount() > 1) {
                newOwner = roomIt->second->getLongestMember();

                // If the longest member is the current owner, find the next one
                if (newOwner == clientSocket) {
                    set<SOCKET> roomClients = roomIt->second->getClients();
                    for (SOCKET sock : roomClients) {
                        if (sock != clientSocket) {
                            newOwner = sock;
                            break;
                        }
                    }
                }

                if (newOwner != INVALID_SOCKET && newOwner != clientSocket) {
                    lock_guard<mutex> clientLock(g_clientsMutex);
                    auto newOwnerIt = g_clients.find(newOwner);
                    if (newOwnerIt != g_clients.end()) {
                        newOwnerName = newOwnerIt->second.getUsername();
                        g_clients[newOwner].setIsRoomOwner(true);
                        roomIt->second->setOwner(newOwner);
                    }
                }
            }

            string leaveMsg = getCurrentTimestamp() + " SYSTEM: " +
                username + " has left the room\n";
            if (!newOwnerName.empty()) {
                string transferMsg = getCurrentTimestamp() +
                    " SYSTEM: Room ownership transferred to " +
                    newOwnerName + "\n";
                roomIt->second->broadcastToAll(transferMsg);
                sendToClient(newOwner, "OWNERSHIP_RECEIVED\n");
            }
            roomIt->second->broadcast(leaveMsg, clientSocket);
            roomIt->second->removeClient(clientSocket);

            if (roomIt->second->isEmpty()) {
                thread([roomId]() {
                    this_thread::sleep_for(chrono::milliseconds(100));
                    cleanupEmptyRoom(roomId);
                    }).detach();
            }
        }
    }

    {
        lock_guard<mutex> clientLock(g_clientsMutex);
        g_clients[clientSocket].setRoomId("");
        g_clients[clientSocket].setIsRoomOwner(false);
    }

    sendToClient(clientSocket, "LEFT_ROOM\n");
    cout << "[CMD] Owner " << clientSocket << " (" << username
        << ") force left room: " << roomId << endl;
}

void handleChangePasswordCommand(SOCKET clientSocket, const string& params) {
    string newPassword = trim(params);

    if (newPassword.empty()) {
        sendToClient(clientSocket, "ERROR: Password cannot be empty\n");
        return;
    }

    string roomId;
    bool isOwner = false;

    {
        lock_guard<mutex> clientLock(g_clientsMutex);
        auto it = g_clients.find(clientSocket);
        if (it != g_clients.end()) {
            roomId = it->second.getRoomId();
            isOwner = it->second.isRoomOwner();
        }
    }

    if (roomId.empty()) {
        sendToClient(clientSocket, "ERROR: You are not in a room\n");
        return;
    }

    if (!isOwner) {
        sendToClient(clientSocket, "ERROR: Only room owner can change password\n");
        return;
    }

    {
        lock_guard<mutex> roomLock(g_chatRoomsMutex);
        auto roomIt = g_chatRooms.find(roomId);
        if (roomIt != g_chatRooms.end()) {
            if (!roomIt->second->getIsPrivate()) {
                sendToClient(clientSocket, "ERROR: This is a public room\n");
                return;
            }

            roomIt->second->setPassword(newPassword);

            // Update password in database
            if (g_database) {
                g_database->updateRoomPassword(roomId, newPassword);
            }

            sendToClient(clientSocket, "PASSWORD_CHANGED:" + newPassword + "\n");

            string sysMsg = getCurrentTimestamp() +
                " SYSTEM: Room password has been changed by the owner\n";
            roomIt->second->broadcast(sysMsg, clientSocket);
        }
    }

    cout << "[CMD] Client " << clientSocket << " changed password for room: "
        << roomId << endl;
}

void handleClientCommand(SOCKET clientSocket, const string& command) {
    stringstream ss(command);
    string cmd;
    ss >> cmd;

    if (cmd == "CREATE") {
        string params;
        getline(ss, params);
        handleCreateCommand(clientSocket, params);
    }
    else if (cmd == "JOIN") {
        string params;
        getline(ss, params);
        handleJoinCommand(clientSocket, params);
    }
    else if (cmd == "SETNAME") {
        string name;
        getline(ss, name);
        handleSetNameCommand(clientSocket, name);
    }
    else if (cmd == "LIST") {
        handleListCommand(clientSocket);
    }
    else if (cmd == "GETPASSWORD") {
        handleGetPasswordCommand(clientSocket);
    }
    else if (cmd == "USERS") {
        handleUsersCommand(clientSocket);
    }
    else if (cmd == "KICK") {
        string params;
        getline(ss, params);
        handleKickCommand(clientSocket, params);
    }
    else if (cmd == "BAN") {
        string params;
        getline(ss, params);
        handleBanCommand(clientSocket, params);
    }
    else if (cmd == "TRANSFER") {
        string params;
        getline(ss, params);
        handleTransferCommand(clientSocket, params);
    }
    else if (cmd == "LEAVE") {
        handleLeaveCommand(clientSocket);
    }
    else if (cmd == "FORCELEAVE") {
        handleForceLeaveCommand(clientSocket);
    }
    else if (cmd == "CHANGEPASSWORD") {
        string params;
        getline(ss, params);
        handleChangePasswordCommand(clientSocket, params);
    }
    else {
        sendToClient(clientSocket, "ERROR: Unknown command\n");
    }
}

// ============================================================================
// MESSAGE BROADCASTING
// ============================================================================

void broadcastMessages() {
    cout << "[THREAD] Broadcaster thread started" << endl;

    while (!g_shutdownRequested) {
        Message message;

        {
            unique_lock<mutex> lock(g_queueMutex);
            g_messageCV.wait_for(lock, chrono::milliseconds(100), [] {
                return !g_messageQueue.empty() || g_shutdownRequested;
                });

            if (g_shutdownRequested && g_messageQueue.empty()) {
                break;
            }

            if (g_messageQueue.empty()) {
                continue;
            }

            message = g_messageQueue.front();
            g_messageQueue.pop();
        }

        {
            lock_guard<mutex> lock(g_chatRoomsMutex);
            auto it = g_chatRooms.find(message.getRoomId());
            if (it != g_chatRooms.end()) {
                if (message.isPrivate()) {
                    // Handle private message
                    SOCKET recipientSocket = findClientByUsername(
                        message.getRecipientName(),
                        message.getRoomId()
                    );

                    if (recipientSocket == INVALID_SOCKET) {
                        string errorMsg = "ERROR: User '" +
                            message.getRecipientName() +
                            "' not found in this room\n";
                        sendToClient(message.getSenderSocket(), errorMsg);
                        cout << "[PM] Failed - recipient not found: "
                            << message.getRecipientName() << endl;
                    }
                    else {
                        string timestamp = getCurrentTimestamp();
                        string formattedMessage = timestamp + " PM_FROM:" +
                            message.getSenderName() + ":" +
                            message.getContent() + "\n";
                        sendToClient(recipientSocket, formattedMessage);

                        string confirmMessage = timestamp + " PM_SENT:" +
                            message.getRecipientName() + ":" +
                            message.getContent() + "\n";
                        sendToClient(message.getSenderSocket(), confirmMessage);

                        // Save private message to database
                        if (g_database) {
                            g_database->saveMessage(
                                message.getRoomId(),
                                message.getSenderName(),
                                message.getContent(),
                                true,
                                message.getRecipientName()
                            );
                        }

                        cout << "[PM] " << timestamp << " "
                            << message.getSenderName() << " -> "
                            << message.getRecipientName() << ": "
                            << message.getContent() << endl;
                    }
                }
                else {
                    // Handle regular broadcast message
                    string timestamp = getCurrentTimestamp();
                    string formattedMessage = timestamp + " " +
                        message.getSenderName() + ": " +
                        message.getContent() + "\n";

                    it->second->addMessageToHistory(formattedMessage);
                    it->second->broadcast(formattedMessage, message.getSenderSocket());

                    // Save message to database
                    if (g_database) {
                        g_database->saveMessage(
                            message.getRoomId(),
                            message.getSenderName(),
                            message.getContent(),
                            false,
                            ""
                        );
                    }

                    cout << "[BROADCAST] Room " << message.getRoomId() << " - "
                        << timestamp << " " << message.getSenderName() << ": "
                        << message.getContent() << endl;
                }
            }
        }
    }

    cout << "[THREAD] Broadcaster thread exiting" << endl;
}

// ============================================================================
// CLIENT HANDLING
// ============================================================================

void handleClientDisconnect(SOCKET clientSocket, vector<WSAPOLLFD>& pollFds,
    mutex& pollFdsMutex) {
    cout << "[DISCONNECT] Client " << clientSocket << " disconnected" << endl;

    removeClientFromRoom(clientSocket);

    {
        lock_guard<mutex> clientLock(g_clientsMutex);
        g_clients.erase(clientSocket);
    }

    closesocket(clientSocket);

    {
        lock_guard<mutex> lock(pollFdsMutex);
        for (auto it = pollFds.begin(); it != pollFds.end(); ++it) {
            if (it->fd == clientSocket) {
                pollFds.erase(it);
                break;
            }
        }
    }
}

void handleClientMessage(SOCKET clientSocket, const char* buffer, int bytesReceived) {
    string receivedText(buffer, bytesReceived);
    receivedText = trim(receivedText);

    if (receivedText.empty()) {
        return;
    }

    if (receivedText[0] == '/') {
        handleClientCommand(clientSocket, receivedText.substr(1));
    }
    else if (receivedText[0] == '@') {
        // Private message format: @username message text
        size_t spacePos = receivedText.find(' ');
        if (spacePos == string::npos || spacePos == 1) {
            sendToClient(clientSocket,
                "ERROR: Invalid private message format. Use: @username message\n");
            return;
        }

        string recipientName = receivedText.substr(1, spacePos - 1);
        string messageContent = receivedText.substr(spacePos + 1);

        recipientName = trim(recipientName);
        messageContent = trim(messageContent);

        if (recipientName.empty() || messageContent.empty()) {
            sendToClient(clientSocket,
                "ERROR: Invalid private message format. Use: @username message\n");
            return;
        }

        string roomId;
        string username;

        {
            lock_guard<mutex> lock(g_clientsMutex);
            auto it = g_clients.find(clientSocket);
            if (it != g_clients.end()) {
                roomId = it->second.getRoomId();
                username = it->second.getUsername();
            }
        }

        if (roomId.empty()) {
            sendToClient(clientSocket, "ERROR: You must join a room first\n");
            return;
        }

        if (recipientName == username) {
            sendToClient(clientSocket,
                "ERROR: You cannot send a private message to yourself\n");
            return;
        }

        Message msg;
        msg.setSenderSocket(clientSocket);
        msg.setContent(messageContent);
        msg.setRoomId(roomId);
        msg.setSenderName(username);
        msg.setIsPrivate(true);
        msg.setRecipientName(recipientName);

        {
            lock_guard<mutex> lock(g_queueMutex);
            g_messageQueue.push(msg);
        }
        g_messageCV.notify_one();
    }
    else {
        string roomId;
        string username;

        {
            lock_guard<mutex> lock(g_clientsMutex);
            auto it = g_clients.find(clientSocket);
            if (it != g_clients.end()) {
                roomId = it->second.getRoomId();
                username = it->second.getUsername();
            }
        }

        if (roomId.empty()) {
            sendToClient(clientSocket, "ERROR: You must join a room first\n");
            return;
        }

        Message msg;
        msg.setSenderSocket(clientSocket);
        msg.setContent(receivedText);
        msg.setRoomId(roomId);
        msg.setSenderName(username);
        msg.setIsPrivate(false);

        {
            lock_guard<mutex> lock(g_queueMutex);
            g_messageQueue.push(msg);
        }
        g_messageCV.notify_one();
    }
}