#pragma once
#include "Common.h"

// Gets the current time as a formatted string
string getCurrentTimestamp();

// Generates a random 6-digit room ID
string generateRoomId();

// Trims whitespace from the beginning and end of a string
string trim(const string& str);

// Safely sends a message to a client socket
void sendToClient(SOCKET clientSocket, const string& message);

// Initializes the Winsock library
bool initializeWinsock();

// Handles server shutdown signals (Ctrl+C)
void signalHandler(int signal);