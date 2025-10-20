#include "Message.h"

Message::Message()
    : m_senderSocket(INVALID_SOCKET)
    , m_content("")
    , m_roomId("")
    , m_senderName("")
    , m_isPrivate(false)
    , m_recipientName("") {
}

// Getters
SOCKET Message::getSenderSocket() const { return m_senderSocket; }
string Message::getContent() const { return m_content; }
string Message::getRoomId() const { return m_roomId; }
string Message::getSenderName() const { return m_senderName; }
bool Message::isPrivate() const { return m_isPrivate; }
string Message::getRecipientName() const { return m_recipientName; }

// Setters
void Message::setSenderSocket(SOCKET socket) { m_senderSocket = socket; }
void Message::setContent(const string& content) { m_content = content; }
void Message::setRoomId(const string& roomId) { m_roomId = roomId; }
void Message::setSenderName(const string& name) { m_senderName = name; }
void Message::setIsPrivate(bool isPrivate) { m_isPrivate = isPrivate; }
void Message::setRecipientName(const string& name) { m_recipientName = name; }