#pragma once
#include "Common.h"

class ChatRoom {
private:
    string m_roomId;
    string m_password;
    bool m_isPrivate;
    SOCKET m_ownerSocket;
    set<SOCKET> m_clients;
    set<string> m_bannedUsers;
    vector<string> m_messageHistory;
    map<SOCKET, chrono::steady_clock::time_point> m_clientJoinTimes;
    mutable mutex m_roomMutex;

public:
    ChatRoom(const string& id, bool isPrivate, const string& password, SOCKET owner);
    ~ChatRoom();

    // Room information getters
    string getRoomId() const;
    bool getIsPrivate() const;
    string getPassword() const;
    SOCKET getOwner() const;
    int getClientCount() const;
    bool isEmpty() const;

    // Password management
    bool verifyPassword(const string& password) const;
    void setPassword(const string& newPassword);

    // Ownership management
    void setOwner(SOCKET newOwner);

    // Ban management
    bool isUserBanned(const string& username) const;
    void banUser(const string& username);

    // Client management
    void addClient(SOCKET clientSocket);
    void removeClient(SOCKET clientSocket);
    bool hasClient(SOCKET clientSocket) const;
    set<SOCKET> getClients() const;
    SOCKET getLongestMember() const;

    // Message history management
    void addMessageToHistory(const string& message);
    vector<string> getMessageHistory() const;

    // Broadcasting methods
    void broadcast(const string& message, SOCKET senderSocket);
    void broadcastToAll(const string& message);
};