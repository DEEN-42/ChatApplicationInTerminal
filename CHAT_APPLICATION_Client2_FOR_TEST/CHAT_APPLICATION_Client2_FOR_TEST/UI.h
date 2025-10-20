#pragma once

#include <string>

void displayWelcome();
void displayMenu();
void displayRoomCreated(const std::string& roomId, const std::string& type);
void displayRoomJoined(const std::string& roomId);
void displayRoomsList(const std::string& roomsList);
void displayUsersList(const std::string& usersList, const std::string& username, bool isOwner);