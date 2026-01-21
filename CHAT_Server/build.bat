@echo off
echo ========================================
echo Building Chat Server with SQLite
echo ========================================

cd CHAT_APPLICATION_SERVER

echo.
echo [1/2] Compiling SQLite...
cl /c sqlite3.c /O2 /DNDEBUG /MD /EHsc

if %errorlevel% neq 0 (
    echo ERROR: SQLite compilation failed!
    pause
    exit /b 1
)

echo.
echo [2/2] Building Chat Server...
cl /EHsc /MD /Fe:ChatServer.exe ^
   main.cpp ^
   Server.cpp ^
   ClientInfo.cpp ^
   ChatRoom.cpp ^
   Message.cpp ^
   Utilities.cpp ^
   Globals.cpp ^
   Database.cpp ^
   sqlite3.obj ^
   ws2_32.lib

if %errorlevel% neq 0 (
    echo ERROR: Server compilation failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo BUILD SUCCESSFUL!
echo ========================================
echo Executable: CHAT_APPLICATION_SERVER\ChatServer.exe
echo.
pause
