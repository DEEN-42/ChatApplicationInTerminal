#pragma once
#include "Common.h"
#include "ClientInfo.h"
#include "Message.h"

// Global map of all chat rooms, keyed by Room ID
extern map<string, shared_ptr<ChatRoom>> g_chatRooms;
extern mutex g_chatRoomsMutex;

// Global map of all connected clients, keyed by Socket
extern map<SOCKET, ClientInfo> g_clients;
extern mutex g_clientsMutex;

// Global message queue for the broadcaster thread
extern queue<Message> g_messageQueue;
extern mutex g_queueMutex;
extern condition_variable g_messageCV;

// Global shutdown flag
extern atomic<bool> g_shutdownRequested;