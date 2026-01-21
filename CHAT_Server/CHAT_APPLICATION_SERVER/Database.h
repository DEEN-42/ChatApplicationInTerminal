#pragma once
#include "Common.h"
#include <sqlite3.h>

class Database {
private:
    sqlite3* m_db;
    mutex m_dbMutex;

    bool executeQuery(const string& query);

public:
    Database(const string& dbPath = "chatserver.db");
    ~Database();

    bool initialize();
    bool isConnected() const;

    // User operations
    bool createUser(const string& username, const string& passwordHash);
    bool authenticateUser(const string& username, const string& passwordHash);
    bool userExists(const string& username);
    bool updateLastSeen(const string& username);

    // Message operations
    void saveMessage(const string& roomId, const string& sender,
                     const string& content, bool isPrivate = false,
                     const string& recipient = "");
    vector<string> getMessageHistory(const string& roomId, int limit = MAX_MESSAGE_HISTORY);
    vector<string> getPrivateMessages(const string& user1, const string& user2, int limit = 50);

    // Room operations
    bool createRoom(const string& roomId, bool isPrivate,
                    const string& owner, const string& passwordHash = "");
    bool deleteRoom(const string& roomId);
    bool roomExists(const string& roomId);
    bool updateRoomOwner(const string& roomId, const string& newOwner);
    bool updateRoomPassword(const string& roomId, const string& newPasswordHash);

    // Ban operations
    bool addBan(const string& roomId, const string& username);
    bool removeBan(const string& roomId, const string& username);
    bool isUserBanned(const string& roomId, const string& username);
    vector<string> getBannedUsers(const string& roomId);
};

// Global database instance
extern unique_ptr<Database> g_database;
