# Quick Start Guide

## ? Database Setup Complete!

All SQLite files are installed and ready to use.

---

## Option 1: Build with Visual Studio (Recommended)

### Steps:
1. **Open Visual Studio**
2. **Open your solution/project**
3. **Add sqlite3.c to project:**
   - Right-click project ? `Add` ? `Existing Item`
   - Select `CHAT_APPLICATION_SERVER\sqlite3.c`
4. **Build:** Press `Ctrl+Shift+B`
5. **Run:** Press `F5`

---

## Option 2: Build with Command Line

### Steps:
1. **Open Developer Command Prompt for VS**
   - Search "Developer Command Prompt" in Windows Start Menu
   
2. **Navigate to project:**
   ```cmd
   cd "C:\Users\Debanshu Ghosh\source\repos\chatServer\CHAT_Server"
   ```

3. **Run build script:**
   ```cmd
   build.bat
   ```

4. **Run the server:**
   ```cmd
   cd CHAT_APPLICATION_SERVER
   ChatServer.exe
   ```

---

## Verify Database Works

After starting the server, you should see:
```
[DB] Database opened successfully: chatserver.db
[DB] Database schema initialized successfully
========================================
  CHAT SERVER WITH PRIVATE ROOMS
========================================
Port: 12345
```

---

## Database File Location

The database file `chatserver.db` will be created in the same directory as your executable:
- If built with VS: Usually in `Debug/` or `Release/` folder
- If built with batch: In `CHAT_APPLICATION_SERVER/` folder

---

## Test Message Persistence

1. Start server
2. Connect with client
3. Send messages in a room
4. Stop server (Ctrl+C)
5. Start server again
6. Join the same room
7. Messages should be loaded from database! ?

---

## Files Summary

| File | Purpose |
|------|---------|
| `sqlite3.h` | SQLite header |
| `sqlite3.c` | SQLite implementation (add to project) |
| `Database.h` | Database class header |
| `Database.cpp` | Database implementation |
| `build.bat` | Command-line build script |
| `chatserver.db` | Database file (auto-created) |

---

## Need Help?

If you see errors:
1. Check `SETUP_INSTRUCTIONS.md` for troubleshooting
2. Make sure `sqlite3.c` is in your project
3. Verify all `.cpp` files compile without errors

**Everything is ready! Just build and run! ??**
