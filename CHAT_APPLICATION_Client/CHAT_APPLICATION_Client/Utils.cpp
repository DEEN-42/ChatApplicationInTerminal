#include "Utils.h"
#include <iostream>
#include <WinSock2.h>
#include <windows.h>

using namespace std;

bool initializeWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        cout << "[ERROR] WSAStartup failed: " << result << endl;
        return false;
    }
    return true;
}

string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

int getConsoleWidth() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int columns = 80; // default

    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }

    return columns;
}

void printAlignedMessage(const string& message, bool alignRight) {
    int consoleWidth = getConsoleWidth();
    int maxMessageWidth = consoleWidth - 10; // Leave some margin

    if (maxMessageWidth < 20) maxMessageWidth = 20;

    // Split message into lines if it's too long
    string msg = message;

    if (msg.length() > maxMessageWidth) {
        // Word wrap
        size_t pos = 0;
        while (pos < msg.length()) {
            size_t endPos = pos + maxMessageWidth;

            if (endPos >= msg.length()) {
                endPos = msg.length();
            }
            else {
                // Try to break at a space
                size_t spacePos = msg.rfind(' ', endPos);
                if (spacePos != string::npos && spacePos > pos) {
                    endPos = spacePos;
                }
            }

            string line = msg.substr(pos, endPos - pos);

            if (alignRight) {
                int padding = consoleWidth - line.length() - 2;
                if (padding > 0) {
                    cout << string(padding, ' ') << line << endl;
                }
                else {
                    cout << line << endl;
                }
            }
            else {
                cout << "  " << line << endl;
            }

            pos = endPos;
            if (pos < msg.length() && msg[pos] == ' ') pos++; // Skip the space
        }
    }
    else {
        if (alignRight) {
            int padding = consoleWidth - msg.length() - 2;
            if (padding > 0) {
                cout << string(padding, ' ') << msg << endl;
            }
            else {
                cout << msg << endl;
            }
        }
        else {
            cout << "  " << msg << endl;
        }
    }
}