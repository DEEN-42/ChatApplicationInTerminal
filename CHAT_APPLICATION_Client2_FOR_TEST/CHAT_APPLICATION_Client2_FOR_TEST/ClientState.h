#pragma once

#include <string>
#include <atomic>
#include <mutex>

class ClientState {
private:
    std::atomic<bool> m_inRoom;
    std::atomic<bool> m_shouldExit;
    std::atomic<bool> m_waitingForNameValidation;
    std::atomic<bool> m_waitingForPassword;
    std::atomic<bool> m_ownerLeaveWarning;
    std::atomic<bool> m_isRoomOwner;
    std::string m_currentRoomId;
    std::string m_username;
    std::string m_pendingRoomJoin;
    mutable std::mutex m_stringMutex;

public:
    ClientState()
        : m_inRoom(false)
        , m_shouldExit(false)
        , m_waitingForNameValidation(false)
        , m_waitingForPassword(false)
        , m_ownerLeaveWarning(false)
        , m_isRoomOwner(false)
        , m_currentRoomId("")
        , m_username("")
        , m_pendingRoomJoin("")
    {
    }

    // Getters
    bool isInRoom() const { return m_inRoom; }
    bool shouldExit() const { return m_shouldExit; }
    bool isWaitingForNameValidation() const { return m_waitingForNameValidation; }
    bool isWaitingForPassword() const { return m_waitingForPassword; }
    bool hasOwnerLeaveWarning() const { return m_ownerLeaveWarning; }
    bool isRoomOwner() const { return m_isRoomOwner; }

    std::string getCurrentRoomId() const {
        std::lock_guard<std::mutex> lock(m_stringMutex);
        return m_currentRoomId;
    }

    std::string getUsername() const {
        std::lock_guard<std::mutex> lock(m_stringMutex);
        return m_username;
    }

    std::string getPendingRoomJoin() const {
        std::lock_guard<std::mutex> lock(m_stringMutex);
        return m_pendingRoomJoin;
    }

    // Setters
    void setInRoom(bool value) { m_inRoom = value; }
    void setShouldExit(bool value) { m_shouldExit = value; }
    void setWaitingForNameValidation(bool value) { m_waitingForNameValidation = value; }
    void setWaitingForPassword(bool value) { m_waitingForPassword = value; }
    void setOwnerLeaveWarning(bool value) { m_ownerLeaveWarning = value; }
    void setRoomOwner(bool value) { m_isRoomOwner = value; }

    void setCurrentRoomId(const std::string& roomId) {
        std::lock_guard<std::mutex> lock(m_stringMutex);
        m_currentRoomId = roomId;
    }

    void setUsername(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_stringMutex);
        m_username = name;
    }

    void setPendingRoomJoin(const std::string& roomId) {
        std::lock_guard<std::mutex> lock(m_stringMutex);
        m_pendingRoomJoin = roomId;
    }

    // Utility methods
    void resetRoomState() {
        m_inRoom = false;
        m_isRoomOwner = false;
        m_ownerLeaveWarning = false;
        std::lock_guard<std::mutex> lock(m_stringMutex);
        m_currentRoomId = "";
    }

    void clearPendingJoin() {
        std::lock_guard<std::mutex> lock(m_stringMutex);
        m_pendingRoomJoin = "";
    }
};