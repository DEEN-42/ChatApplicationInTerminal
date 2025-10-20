#pragma once
#include "Common.h"

class Message {
private:
    SOCKET m_senderSocket;
    string m_content;
    string m_roomId;
    string m_senderName;
    bool m_isPrivate;
    string m_recipientName;

public:
    Message();

    // Getters
    SOCKET getSenderSocket() const;
    string getContent() const;
    string getRoomId() const;
    string getSenderName() const;
    bool isPrivate() const;
    string getRecipientName() const;

    // Setters
    void setSenderSocket(SOCKET socket);
    void setContent(const string& content);
    void setRoomId(const string& roomId);
    void setSenderName(const string& name);
    void setIsPrivate(bool isPrivate);
    void setRecipientName(const string& name);
};