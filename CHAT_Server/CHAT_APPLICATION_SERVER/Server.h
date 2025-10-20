#pragma once
#include "Common.h"

// ============================================================================
// UTILITY FUNCTIONS (Server-Specific)
// ============================================================================
bool isUsernameAvailable(const string& username, SOCKET excludeSocket = INVALID_SOCKET);
SOCKET findClientByUsername(const string& username, const string& roomId);

// ============================================================================
// ROOM MANAGEMENT
// ============================================================================
void cleanupEmptyRoom(const string& roomId);
void removeClientFromRoom(SOCKET clientSocket);

// ============================================================================
// COMMAND HANDLERS
// ============================================================================
void handleCreateCommand(SOCKET clientSocket, const string& params);
void handleJoinCommand(SOCKET clientSocket, const string& params);
void handleSetNameCommand(SOCKET clientSocket, const string& name);
void handleListCommand(SOCKET clientSocket);
void handleGetPasswordCommand(SOCKET clientSocket);
void handleUsersCommand(SOCKET clientSocket);
void handleKickCommand(SOCKET clientSocket, const string& params);
void handleBanCommand(SOCKET clientSocket, const string& params);
void handleTransferCommand(SOCKET clientSocket, const string& params);
void handleLeaveCommand(SOCKET clientSocket);
void handleForceLeaveCommand(SOCKET clientSocket);
void handleChangePasswordCommand(SOCKET clientSocket, const string& params);
void handleClientCommand(SOCKET clientSocket, const string& command);

// ============================================================================
// MESSAGE BROADCASTING
// ============================================================================
void broadcastMessages();

// ============================================================================
// CLIENT HANDLING
// ============================================================================
void handleClientDisconnect(SOCKET clientSocket, vector<WSAPOLLFD>& pollFds,
    mutex& pollFdsMutex);
void handleClientMessage(SOCKET clientSocket, const char* buffer, int bytesReceived);