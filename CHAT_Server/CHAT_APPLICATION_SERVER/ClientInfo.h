#pragma once
#include "Common.h"

class ClientInfo {
private:
    SOCKET m_socket;
    string m_username;
    string m_roomId;
    bool m_isRoomOwner;
    chrono::steady_clock::time_point m_joinTime;

public:
    ClientInfo();
    explicit ClientInfo(SOCKET socket);

    // Getters
    SOCKET getSocket() const;
    string getUsername() const;
    string getRoomId() const;
    bool isRoomOwner() const;
    chrono::steady_clock::time_point getJoinTime() const;

    // Setters
    void setSocket(SOCKET socket);
    void setUsername(const string& username);
    void setRoomId(const string& roomId);
    void setIsRoomOwner(bool isOwner);
    void setJoinTime(const chrono::steady_clock::time_point& time);
};