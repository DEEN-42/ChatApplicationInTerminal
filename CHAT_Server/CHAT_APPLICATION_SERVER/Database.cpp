#include "Database.h"
#include <iomanip>
#include <algorithm>

// Global database instance
unique_ptr<Database> g_database = nullptr;

Database::Database(const string& dbPath) : m_db(nullptr) {
    int rc = sqlite3_open(dbPath.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        cerr << "[DB ERROR] Cannot open database: " << sqlite3_errmsg(m_db) << endl;
        m_db = nullptr;
    }
    else {
        // Enable WAL mode for better concurrent access
        char* errMsg = nullptr;
        sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errMsg);
        if (errMsg) sqlite3_free(errMsg);

        // Enable foreign keys
        sqlite3_exec(m_db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, &errMsg);
        if (errMsg) sqlite3_free(errMsg);

        cout << "[DB] Database opened successfully: " << dbPath << endl;
    }
}

Database::~Database() {
    if (m_db) {
        sqlite3_close(m_db);
        cout << "[DB] Database closed" << endl;
    }
}

bool Database::isConnected() const {
    return m_db != nullptr;
}

bool Database::executeQuery(const string& query) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, query.c_str(), nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        cerr << "[DB ERROR] Query failed: " << errMsg << endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool Database::initialize() {
    if (!m_db) {
        cerr << "[DB ERROR] Database not connected" << endl;
        return false;
    }

    lock_guard<mutex> lock(m_dbMutex);

    // Create users table
    const char* createUsersTable = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            last_seen DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    // Create rooms table
    const char* createRoomsTable = R"(
        CREATE TABLE IF NOT EXISTS rooms (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            room_id TEXT UNIQUE NOT NULL,
            is_private INTEGER NOT NULL DEFAULT 0,
            owner_username TEXT NOT NULL,
            password_hash TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    // Create messages table
    const char* createMessagesTable = R"(
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            room_id TEXT NOT NULL,
            sender_username TEXT NOT NULL,
            content TEXT NOT NULL,
            is_private INTEGER NOT NULL DEFAULT 0,
            recipient_username TEXT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

    // Create bans table
    const char* createBansTable = R"(
        CREATE TABLE IF NOT EXISTS bans (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            room_id TEXT NOT NULL,
            username TEXT NOT NULL,
            banned_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(room_id, username)
        );
    )";

    // Create indexes for performance
    const char* createIndexes = R"(
        CREATE INDEX IF NOT EXISTS idx_messages_room ON messages(room_id);
        CREATE INDEX IF NOT EXISTS idx_messages_timestamp ON messages(timestamp);
        CREATE INDEX IF NOT EXISTS idx_messages_sender ON messages(sender_username);
        CREATE INDEX IF NOT EXISTS idx_bans_room ON bans(room_id);
        CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
    )";

    char* errMsg = nullptr;

    if (sqlite3_exec(m_db, createUsersTable, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        cerr << "[DB ERROR] Users table: " << errMsg << endl;
        sqlite3_free(errMsg);
        return false;
    }

    if (sqlite3_exec(m_db, createRoomsTable, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        cerr << "[DB ERROR] Rooms table: " << errMsg << endl;
        sqlite3_free(errMsg);
        return false;
    }

    if (sqlite3_exec(m_db, createMessagesTable, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        cerr << "[DB ERROR] Messages table: " << errMsg << endl;
        sqlite3_free(errMsg);
        return false;
    }

    if (sqlite3_exec(m_db, createBansTable, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        cerr << "[DB ERROR] Bans table: " << errMsg << endl;
        sqlite3_free(errMsg);
        return false;
    }

    if (sqlite3_exec(m_db, createIndexes, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        cerr << "[DB ERROR] Indexes: " << errMsg << endl;
        sqlite3_free(errMsg);
        return false;
    }

    cout << "[DB] Database schema initialized successfully" << endl;
    return true;
}

// ============================================================================
// USER OPERATIONS
// ============================================================================

bool Database::createUser(const string& username, const string& passwordHash) {
    if (!m_db) return false;

    lock_guard<mutex> lock(m_dbMutex);

    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO users (username, password_hash) VALUES (?, ?);";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "[DB ERROR] Prepare createUser: " << sqlite3_errmsg(m_db) << endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, passwordHash.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        cerr << "[DB ERROR] Execute createUser: " << sqlite3_errmsg(m_db) << endl;
        return false;
    }

    cout << "[DB] User created: " << username << endl;
    return true;
}

bool Database::authenticateUser(const string& username, const string& passwordHash) {
    if (!m_db) return false;

    lock_guard<mutex> lock(m_dbMutex);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id FROM users WHERE username = ? AND password_hash = ?;";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, passwordHash.c_str(), -1, SQLITE_TRANSIENT);

    bool authenticated = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);

    return authenticated;
}

bool Database::userExists(const string& username) {
    if (!m_db) return false;

    lock_guard<mutex> lock(m_dbMutex);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id FROM users WHERE username = ?;";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);

    return exists;
}

bool Database::updateLastSeen(const string& username) {
    if (!m_db) return false;

    lock_guard<mutex> lock(m_dbMutex);

    sqlite3_stmt* stmt;
    const char* sql = "UPDATE users SET last_seen = CURRENT_TIMESTAMP WHERE username = ?;";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

// ============================================================================
// MESSAGE OPERATIONS
// ============================================================================

void Database::saveMessage(const string& roomId, const string& sender,
                           const string& content, bool isPrivate,
                           const string& recipient) {
    if (!m_db) return;

    lock_guard<mutex> lock(m_dbMutex);

    sqlite3_stmt* stmt;
    const char* sql = R"(
        INSERT INTO messages (room_id, sender_username, content, is_private, recipient_username) 
        VALUES (?, ?, ?, ?, ?);
    )";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "[DB ERROR] Prepare saveMessage: " << sqlite3_errmsg(m_db) << endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, roomId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, sender.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, isPrivate ? 1 : 0);

    if (recipient.empty()) {
        sqlite3_bind_null(stmt, 5);
    }
    else {
        sqlite3_bind_text(stmt, 5, recipient.c_str(), -1, SQLITE_TRANSIENT);
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        cerr << "[DB ERROR] Execute saveMessage: " << sqlite3_errmsg(m_db) << endl;
    }

    sqlite3_finalize(stmt);
}

vector<string> Database::getMessageHistory(const string& roomId, int limit) {
    vector<string> history;
    if (!m_db) return history;

    lock_guard<mutex> lock(m_dbMutex);

    sqlite3_stmt* stmt;
    const char* sql = R"(
        SELECT sender_username, content, timestamp 
        FROM messages 
        WHERE room_id = ? AND is_private = 0
        ORDER BY timestamp DESC 
        LIMIT ?;
    )";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "[DB ERROR] Prepare getMessageHistory: " << sqlite3_errmsg(m_db) << endl;
        return history;
    }

    sqlite3_bind_text(stmt, 1, roomId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* senderPtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* contentPtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* timestampPtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        string sender = senderPtr ? senderPtr : "";
        string content = contentPtr ? contentPtr : "";
        string timestamp = timestampPtr ? timestampPtr : "";

        stringstream ss;
        ss << "[" << timestamp << "] " << sender << ": " << content << "\n";
        history.push_back(ss.str());
    }

    sqlite3_finalize(stmt);

    // Reverse to chronological order
    reverse(history.begin(), history.end());
    return history;
}

vector<string> Database::getPrivateMessages(const string& user1, const string& user2, int limit) {
    vector<string> history;
    if (!m_db) return history;

    lock_guard<mutex> lock(m_dbMutex);

    sqlite3_stmt* stmt;
    const char* sql = R"(
        SELECT sender_username, recipient_username, content, timestamp 
        FROM messages 
        WHERE is_private = 1 AND (
            (sender_username = ? AND recipient_username = ?) OR
            (sender_username = ? AND recipient_username = ?)
        )
        ORDER BY timestamp DESC 
        LIMIT ?;
    )";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return history;
    }

    sqlite3_bind_text(stmt, 1, user1.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, user2.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, user2.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, user1.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* senderPtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* contentPtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const char* timestampPtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

        string sender = senderPtr ? senderPtr : "";
        string content = contentPtr ? contentPtr : "";
        string timestamp = timestampPtr ? timestampPtr : "";

        stringstream ss;
        ss << "[" << timestamp << "] " << sender << ": " << content << "\n";
        history.push_back(ss.str());
    }

    sqlite3_finalize(stmt);

    reverse(history.begin(), history.end());
    return history;
}

// ============================================================================
// ROOM OPERATIONS
// ============================================================================

bool Database::createRoom(const string& roomId, bool isPrivate,
                          const string& owner, const string& passwordHash) {
    if (!m_db) return false;

    lock_guard<mutex> lock(m_dbMutex);

    sqlite3_stmt* stmt;
    const char* sql = R"(
        INSERT INTO rooms (room_id, is_private, owner_username, password_hash) 
        VALUES (?, ?, ?, ?);
    )";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, roomId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, isPrivate ? 1 : 0);
    sqlite3_bind_text(stmt, 3, owner.c_str(), -1, SQLITE_TRANSIENT);

    if (passwordHash.empty()) {
        sqlite3_bind_null(stmt, 4);
    }
    else {
        sqlite3_bind_text(stmt, 4, passwordHash.c_str(), -1, SQLITE_TRANSIENT);
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        cout << "[DB] Room created: " << roomId << endl;
        return true;
    }
    return false;
}

bool Database::deleteRoom(const string& roomId) {
    if (!m_db) return false;

    lock_guard<mutex> lock(m_dbMutex);

    // Delete room messages
    sqlite3_stmt* stmt;
    const char* sqlMessages = "DELETE FROM messages WHERE room_id = ?;";

    if (sqlite3_prepare_v2(m_db, sqlMessages, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, roomId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Delete room bans
    const char* sqlBans = "DELETE FROM bans WHERE room_id = ?;";
    if (sqlite3_prepare_v2(m_db, sqlBans, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, roomId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Delete room
    const char* sqlRoom = "DELETE FROM rooms WHERE room_id = ?;";

    if (sqlite3_prepare_v2(m_db, sqlRoom, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, roomId.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        cout << "[DB] Room deleted: " << roomId << endl;
        return true;
    }
    return false;
}

bool Database::roomExists(const string& roomId) {
    if (!m_db) return false;

    lock_guard<mutex> lock(m_dbMutex);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id FROM rooms WHERE room_id = ?;";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, roomId.c_str(), -1, SQLITE_TRANSIENT);

    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);

    return exists;
}

bool Database::updateRoomOwner(const string& roomId, const string& newOwner) {
    if (!m_db) return false;

    lock_guard<mutex> lock(m_dbMutex);

    sqlite3_stmt* stmt;
    const char* sql = "UPDATE rooms SET owner_username = ? WHERE room_id = ?;";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, newOwner.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, roomId.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

bool Database::updateRoomPassword(const string& roomId, const string& newPasswordHash) {
    if (!m_db) return false;

    lock_guard<mutex> lock(m_dbMutex);

    sqlite3_stmt* stmt;
    const char* sql = "UPDATE rooms SET password_hash = ? WHERE room_id = ?;";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, newPasswordHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, roomId.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

// ============================================================================
// BAN OPERATIONS
// ============================================================================

bool Database::addBan(const string& roomId, const string& username) {
    if (!m_db) return false;

    lock_guard<mutex> lock(m_dbMutex);

    sqlite3_stmt* stmt;
    const char* sql = "INSERT OR IGNORE INTO bans (room_id, username) VALUES (?, ?);";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, roomId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

bool Database::removeBan(const string& roomId, const string& username) {
    if (!m_db) return false;

    lock_guard<mutex> lock(m_dbMutex);

    sqlite3_stmt* stmt;
    const char* sql = "DELETE FROM bans WHERE room_id = ? AND username = ?;";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, roomId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

bool Database::isUserBanned(const string& roomId, const string& username) {
    if (!m_db) return false;

    lock_guard<mutex> lock(m_dbMutex);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT id FROM bans WHERE room_id = ? AND username = ?;";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, roomId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);

    bool banned = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);

    return banned;
}

vector<string> Database::getBannedUsers(const string& roomId) {
    vector<string> bannedUsers;
    if (!m_db) return bannedUsers;

    lock_guard<mutex> lock(m_dbMutex);

    sqlite3_stmt* stmt;
    const char* sql = "SELECT username FROM bans WHERE room_id = ?;";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return bannedUsers;
    }

    sqlite3_bind_text(stmt, 1, roomId.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* usernamePtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (usernamePtr) {
            bannedUsers.push_back(usernamePtr);
        }
    }

    sqlite3_finalize(stmt);
    return bannedUsers;
}
