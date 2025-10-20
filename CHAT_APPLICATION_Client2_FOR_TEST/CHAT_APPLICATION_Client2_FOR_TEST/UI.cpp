#include "UI.h"
#include "Utils.h" // For trim()
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

void displayWelcome() {
    cout << "\n=========================================" << endl;
    cout << "         WELCOME TO CHAT ROOMS!          " << endl;
    cout << "=========================================\n" << endl;
}

void displayMenu() {
    cout << "\n========================================" << endl;
    cout << "           CHAT ROOM COMMANDS" << endl;
    cout << "========================================" << endl;
    cout << "  /CREATE PUBLIC           - Create a public room" << endl;
    cout << "  /CREATE PRIVATE <pass>   - Create a private room" << endl;
    cout << "  /JOIN <room_id>          - Join a public room" << endl;
    cout << "  /JOIN <room_id> <pass>   - Join a private room" << endl;
    cout << "  /LIST                    - List all active rooms" << endl;
    cout << "  /USERS                   - List users in current room" << endl;
    cout << "  /LEAVE                   - Leave current room" << endl;
    cout << "\n  OWNER ONLY COMMANDS:" << endl;
    cout << "  /GETPASSWORD             - View room password" << endl;
    cout << "  /CHANGEPASSWORD <pass>   - Change room password" << endl;
    cout << "  /KICK <username>         - Kick user from room" << endl;
    cout << "  /BAN <username>          - Ban user from room" << endl;
    cout << "  /TRANSFER <username>     - Transfer ownership" << endl;
    cout << "\n  OTHER:" << endl;
    cout << "  @username message        - Send private message" << endl;
    cout << "  /HELP                    - Show this menu" << endl;
    cout << "  quit                     - Exit the chat" << endl;
    cout << "========================================\n" << endl;
}

void displayRoomCreated(const string& roomId, const string& type) {
    cout << "\n+========================================+" << endl;
    cout << "|   ROOM CREATED SUCCESSFULLY!           |" << endl;
    cout << "+========================================+" << endl;
    cout << "Your Room ID: " << roomId << endl;
    cout << "Room Type: " << type << endl;
    cout << "Share this ID with others to invite them!" << endl;
    if (type == "PRIVATE") {
        cout << "Note: Use /GETPASSWORD to view the password" << endl;
    }
    cout << "You can now start chatting...\n" << endl;
}

void displayRoomJoined(const string& roomId) {
    cout << "\n+========================================+" << endl;
    cout << "|   JOINED ROOM SUCCESSFULLY!            |" << endl;
    cout << "+========================================+" << endl;
    cout << "Room ID: " << roomId << endl;
    cout << "You can now start chatting...\n" << endl;
}

void displayRoomsList(const string& roomsList) {
    cout << "\n========================================" << endl;
    cout << "            ACTIVE CHAT ROOMS           " << endl;
    cout << "========================================" << endl;

    if (roomsList.empty() || roomsList == ",") {
        cout << "No active rooms available." << endl;
        cout << "Use /CREATE PUBLIC or /CREATE PRIVATE <pass> to create a new room!" << endl;
    }
    else {
        stringstream ss(roomsList);
        string room;
        int count = 1;

        while (getline(ss, room, ',')) {
            room = trim(room);
            if (!room.empty()) {
                size_t pos = room.find('(');
                if (pos != string::npos) {
                    string roomId = room.substr(0, pos);
                    string remaining = room.substr(pos);

                    size_t closePos = remaining.find(')');
                    string userCount = remaining.substr(1, closePos - 1);

                    string type = "PUBLIC";
                    if (remaining.find("[PRIVATE]") != string::npos) {
                        type = "PRIVATE [*]";
                    }
                    else if (remaining.find("[PUBLIC]") != string::npos) {
                        type = "PUBLIC [+]";
                    }

                    cout << "  " << count++ << ". Room ID: " << roomId
                        << " | Users: " << userCount
                        << " | Type: " << type << endl;
                }
            }
        }
    }
    cout << "========================================\n" << endl;
}

void displayUsersList(const string& usersList, const string& username, bool isOwner) {
    cout << "\n========================================" << endl;
    cout << "         USERS IN CURRENT ROOM           " << endl;
    cout << "========================================" << endl;

    if (usersList.empty() || usersList == ",") {
        cout << "No users in room." << endl;
    }
    else {
        stringstream ss(usersList);
        string user;
        int count = 1;

        while (getline(ss, user, ',')) {
            user = trim(user);
            if (!user.empty()) {
                if (user == username) {
                    cout << "  " << count++ << ". " << user << " (You)" << (isOwner ? " [OWNER]" : "") << endl;
                }
                else {
                    cout << "  " << count++ << ". " << user << endl;
                }
            }
        }
    }
    cout << "========================================\n" << endl;
}