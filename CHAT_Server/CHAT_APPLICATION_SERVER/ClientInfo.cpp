#include "ClientInfo.h"

ClientInfo::ClientInfo()
    : m_socket(INVALID_SOCKET)
    , m_username("")
    , m_roomId("")
    , m_isRoomOwner(false)
    , m_joinTime(chrono::steady_clock::now()) {
}

ClientInfo::ClientInfo(SOCKET socket)
    : m_socket(socket)
    , m_username("")
    , m_roomId("")
    , m_isRoomOwner(false)
    , m_joinTime(chrono::steady_clock::now()) {
}

// Getters
SOCKET ClientInfo::getSocket() const { return m_socket; }
string ClientInfo::getUsername() const { return m_username; }
string ClientInfo::getRoomId() const { return m_roomId; }
bool ClientInfo::isRoomOwner() const { return m_isRoomOwner; }
chrono::steady_clock::time_point ClientInfo::getJoinTime() const { return m_joinTime; }

// Setters
void ClientInfo::setSocket(SOCKET socket) { m_socket = socket; }
void ClientInfo::setUsername(const string& username) { m_username = username; }
void ClientInfo::setRoomId(const string& roomId) { m_roomId = roomId; }
void ClientInfo::setIsRoomOwner(bool isOwner) { m_isRoomOwner = isOwner; }
void ClientInfo::setJoinTime(const chrono::steady_clock::time_point& time) { m_joinTime = time; }