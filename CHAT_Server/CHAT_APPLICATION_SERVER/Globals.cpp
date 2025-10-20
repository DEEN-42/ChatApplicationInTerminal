#include "Globals.h"

// Define global variables
map<string, shared_ptr<ChatRoom>> g_chatRooms;
mutex g_chatRoomsMutex;

map<SOCKET, ClientInfo> g_clients;
mutex g_clientsMutex;

queue<Message> g_messageQueue;
mutex g_queueMutex;
condition_variable g_messageCV;

atomic<bool> g_shutdownRequested(false);