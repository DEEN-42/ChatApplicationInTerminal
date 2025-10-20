#pragma once

#include <WinSock2.h>
#include <string>

// Forward declaration of ClientState to avoid including the full header
// This reduces coupling between headers
class ClientState;

void sendMessageToServer(SOCKET serverSocket, ClientState& state);
void receiveMessages(SOCKET serverSocket, ClientState& state);
SOCKET connectToServer(const std::string& serverAddress, int port);