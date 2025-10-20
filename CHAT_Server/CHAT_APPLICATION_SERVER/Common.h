#pragma once

// Standard C++ Libraries
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <csignal>
#include <map>
#include <set>
#include <sstream>
#include <random>
#include <memory>
#include <chrono>
#include <ctime>

// Winsock
#include <winsock2.h>
#include <WS2tcpip.h>
#include <tchar.h>

// Link Winsock library
#pragma comment(lib, "ws2_32.lib")

// Global Definitions
#define PORT 12345
#define BUFFER_SIZE 4096
#define MAX_MESSAGE_HISTORY 100

// Using namespace
using namespace std;

// Forward Declarations
class ClientInfo;
class Message;
class ChatRoom;