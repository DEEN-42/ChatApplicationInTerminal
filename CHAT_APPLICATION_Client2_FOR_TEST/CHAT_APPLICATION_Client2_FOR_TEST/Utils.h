#pragma once

#include <string>

bool initializeWinsock();
std::string trim(const std::string& str);
int getConsoleWidth();
void printAlignedMessage(const std::string& message, bool alignRight);