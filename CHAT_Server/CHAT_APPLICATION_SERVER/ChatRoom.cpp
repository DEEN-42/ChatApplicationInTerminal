#include "ChatRoom.h"

ChatRoom::ChatRoom(const string& id, bool isPrivate, const string& password, SOCKET owner)
    : m_roomId(id)
    , m_password(password)
    , m_isPrivate(isPrivate)
    , m_ownerSocket(owner) {
    cout << "[ROOM] Created " << (isPrivate ? "private" : "public")
        << " room: " << m_roomId << endl;
}

ChatRoom::~ChatRoom() {
    cout << "[ROOM] Destroyed room: " << m_roomId << endl;
}

// Room information getters
string ChatRoom::getRoomId() const {
    lock_guard<mutex> lock(m_roomMutex);
    return m_roomId;
}

bool ChatRoom::getIsPrivate() const {
    lock_guard<mutex> lock(m_roomMutex);
    return m_isPrivate;
}

string ChatRoom::getPassword() const {
    lock_guard<mutex> lock(m_roomMutex);
    return m_password;
}

SOCKET ChatRoom::getOwner() const {
    lock_guard<mutex> lock(m_roomMutex);
    return m_ownerSocket;
}

int ChatRoom::getClientCount() const {
    lock_guard<mutex> lock(m_roomMutex);
    return static_cast<int>(m_clients.size());
}

bool ChatRoom::isEmpty() const {
    return getClientCount() == 0;
}

// Password management
bool ChatRoom::verifyPassword(const string& password) const {
    lock_guard<mutex> lock(m_roomMutex);
    return m_password == password;
}

void ChatRoom::setPassword(const string& newPassword) {
    lock_guard<mutex> lock(m_roomMutex);
    m_password = newPassword;
    cout << "[ROOM:" << m_roomId << "] Password changed" << endl;
}

// Ownership management
void ChatRoom::setOwner(SOCKET newOwner) {
    lock_guard<mutex> lock(m_roomMutex);
    m_ownerSocket = newOwner;
    cout << "[ROOM:" << m_roomId << "] Ownership transferred to client "
        << newOwner << endl;
}

// Ban management
bool ChatRoom::isUserBanned(const string& username) const {
    lock_guard<mutex> lock(m_roomMutex);
    return m_bannedUsers.find(username) != m_bannedUsers.end();
}

void ChatRoom::banUser(const string& username) {
    lock_guard<mutex> lock(m_roomMutex);
    m_bannedUsers.insert(username);
    cout << "[ROOM:" << m_roomId << "] User banned: " << username << endl;
}

// Client management
void ChatRoom::addClient(SOCKET clientSocket) {
    lock_guard<mutex> lock(m_roomMutex);
    m_clients.insert(clientSocket);
    m_clientJoinTimes[clientSocket] = chrono::steady_clock::now();
    cout << "[ROOM:" << m_roomId << "] Client " << clientSocket
        << " added. Total: " << m_clients.size() << endl;
}

void ChatRoom::removeClient(SOCKET clientSocket) {
    lock_guard<mutex> lock(m_roomMutex);
    m_clients.erase(clientSocket);
    m_clientJoinTimes.erase(clientSocket);
    cout << "[ROOM:" << m_roomId << "] Client " << clientSocket
        << " removed. Remaining: " << m_clients.size() << endl;
}

bool ChatRoom::hasClient(SOCKET clientSocket) const {
    lock_guard<mutex> lock(m_roomMutex);
    return m_clients.find(clientSocket) != m_clients.end();
}

set<SOCKET> ChatRoom::getClients() const {
    lock_guard<mutex> lock(m_roomMutex);
    return m_clients;
}

SOCKET ChatRoom::getLongestMember() const {
    lock_guard<mutex> lock(m_roomMutex);

    if (m_clients.empty()) {
        return INVALID_SOCKET;
    }

    SOCKET longestMember = INVALID_SOCKET;
    chrono::steady_clock::time_point earliestTime = chrono::steady_clock::now();

    for (SOCKET sock : m_clients) {
        auto it = m_clientJoinTimes.find(sock);
        if (it != m_clientJoinTimes.end()) {
            if (longestMember == INVALID_SOCKET || it->second < earliestTime) {
                earliestTime = it->second;
                longestMember = sock;
            }
        }
    }

    return longestMember;
}

// Message history management
void ChatRoom::addMessageToHistory(const string& message) {
    lock_guard<mutex> lock(m_roomMutex);
    m_messageHistory.push_back(message);

    if (m_messageHistory.size() > MAX_MESSAGE_HISTORY) {
        m_messageHistory.erase(m_messageHistory.begin());
    }
}

vector<string> ChatRoom::getMessageHistory() const {
    lock_guard<mutex> lock(m_roomMutex);
    return m_messageHistory;
}

// Broadcasting methods
void ChatRoom::broadcast(const string& message, SOCKET senderSocket) {
    lock_guard<mutex> lock(m_roomMutex);

    for (SOCKET clientSocket : m_clients) {
        if (clientSocket != senderSocket && clientSocket != INVALID_SOCKET) {
            int result = send(clientSocket, message.c_str(),
                static_cast<int>(message.length()), 0);
            if (result == SOCKET_ERROR) {
                cout << "[ERROR] Failed to send to client " << clientSocket << endl;
            }
        }
    }
}

void ChatRoom::broadcastToAll(const string& message) {
    lock_guard<mutex> lock(m_roomMutex);

    for (SOCKET clientSocket : m_clients) {
        if (clientSocket != INVALID_SOCKET) {
            int result = send(clientSocket, message.c_str(),
                static_cast<int>(message.length()), 0);
            if (result == SOCKET_ERROR) {
                cout << "[ERROR] Failed to send to client " << clientSocket << endl;
            }
        }
    }
}