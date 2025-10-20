#include "Utilities.h"
#include "Globals.h"

string getCurrentTimestamp() {
    auto now = chrono::system_clock::now();
    time_t nowTime = chrono::system_clock::to_time_t(now);
    tm localTm;
    localtime_s(&localTm, &nowTime);

    char buffer[20];
    strftime(buffer, sizeof(buffer), "[%H:%M:%S]", &localTm);
    return string(buffer);
}

string generateRoomId() {
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_int_distribution<> dis(100000, 999999);
    return to_string(dis(gen));
}

string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == string::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

void sendToClient(SOCKET clientSocket, const string& message) {
    int result = send(clientSocket, message.c_str(),
        static_cast<int>(message.length()), 0);
    if (result == SOCKET_ERROR) {
        cout << "[ERROR] Failed to send to client " << clientSocket
            << ": " << WSAGetLastError() << endl;
    }
}

bool initializeWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        cout << "[ERROR] WSAStartup failed: " << result << endl;
        return false;
    }
    cout << "[INFO] Winsock initialized successfully" << endl;
    return true;
}

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        cout << "\n[SIGNAL] Shutdown signal received. Shutting down gracefully..." << endl;
        g_shutdownRequested = true;
    }
}