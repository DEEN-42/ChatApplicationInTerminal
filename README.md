# Terminal-Based Chat Server with Private Rooms

A C++ implementation of a multi-threaded chat server that supports private chat rooms, running on Windows using Winsock2.

## Features

- Multi-client support using non-blocking sockets
- Private chat rooms functionality
- Real-time message broadcasting
- Thread-safe client handling
- Graceful shutdown with Ctrl+C
- Non-blocking I/O operations
- Support for concurrent client connections

## Technical Stack

- C++
- Windows Sockets 2 (Winsock2)
- Windows API
- Multi-threading

## Prerequisites

- Windows operating system
- Visual Studio with C++ development tools
- Windows SDK

## Building the Project

1. Clone the repository:
```bash
git clone https://github.com/DEEN-42/ChatApplicationInTerminal.git
cd ChatApplicationInTerminal
```

2. Open the solution in Visual Studio
3. Build the solution in Release or Debug mode

## Usage

1. Start the server application
2. The server will start listening on the default port
3. Clients can connect to the server using a compatible client application
4. Use Ctrl+C for graceful server shutdown

## Server Features

- Non-blocking socket operations using WSAPoll
- Thread-safe message broadcasting
- Automatic cleanup of disconnected clients
- Support for private chat rooms
- Real-time message delivery

## Architecture

This section explains how the server and a compatible client are designed to work together.

Server architecture

- Main accept loop
  - A non-blocking listening socket is created and added to a `WSAPOLLFD` array.
  - Incoming connections are accepted in the main loop; accepted client sockets are set to non-blocking mode and added to the poll list.
- Client management
  - Connected clients are tracked in a global `g_clients` map keyed by socket. Each entry is a `ClientInfo` instance that holds client state (username, room membership, etc.).
  - `g_clientsMutex` protects the client map for thread-safe access.
- Message broadcasting
  - The server uses a dedicated `broadcaster` thread (function `broadcastMessages`) that consumes messages from a thread-safe queue and distributes them to target clients.
  - A `condition_variable` (`g_messageCV`) and mutex synchronize producers (client handlers) and the broadcaster.
- Polling and I/O
  - `WSAPoll` monitors all connected sockets for read events (`POLLRDNORM`) and error conditions.
  - When a socket has data, the server reads the message and calls `handleClientMessage`.
  - Disconnected sockets trigger `handleClientDisconnect` which removes the client and closes the socket.
- Rooms and privacy
  - Chat rooms are represented in a global `g_chatRooms` container protected by `g_chatRoomsMutex`.
  - Private rooms are implemented by keeping membership lists and only routing room messages to members.
- Graceful shutdown
  - Signal handlers for `SIGINT` and `SIGTERM` set a shutdown flag. The main loop exits, the broadcaster is notified via `g_messageCV`, all sockets are closed and resources cleaned up.

Client architecture

- Connection
  - The client opens a TCP connection to the server and typically reads a welcome message.
- Messaging
  - The client sends text messages or commands (join/create room, leave room, private message, etc.) according to the server protocol.
  - The client listens for incoming messages from the server and displays them to the user.
- Room support
  - Clients issue commands to join or create private rooms; once a member they receive messages targeted to that room only.
- Error handling
  - Clients should handle reconnects, server disconnects, and display appropriate status to the user.

## Security

Important security notes and recommendations:

- Implemented protections
  - Thread-safe access patterns prevent race conditions in shared state (mutexes around `g_clients` and `g_chatRooms`).
  - The server validates socket operations (checks return values for `recv`, `send`, etc.) and cleans up on errors.
  - Room membership enforcement ensures messages are only delivered to authorized members of private rooms.


## Performance & Benchmarks

This project is implemented using non-blocking I/O and a dedicated broadcaster thread which enables high concurrency. Actual capacity depends on hardware, OS limits, network, and message patterns.

Recommended benchmarking methodology

1. Prepare a test environment with a server host and a separate load generator host (or multiple).
2. Use a synthetic client harness (custom or tools such as `wrk`, `tsung`, or a purpose-built multi-threaded client) that can simulate many concurrent TCP clients.
3. Measure sustained messages/sec, 99th percentile latency, CPU, memory, and network usage while increasing concurrent connections.
4. Tune thread counts, socket buffer sizes, and monster limits (e.g., Windows ephemeral port and handle limits) for higher scale.

Sample/illustrative benchmark (synthetic)

- Test setup (illustrative): one server instance on a modern multi-core machine; synthetic clients run from one or more generators on the same LAN.
- Observed (illustrative) results after tuning and using a lightweight message payload:
  - Concurrent clients: 1,000
  - Sustained throughput: 10,000+ messages/sec across all clients
  - Average end-to-end latency: under 50 ms (under test conditions)

Notes about these numbers

- The numbers above are illustrative and depend heavily on the test environment, message size, frequency, and hardware. They are provided as an example of what is achievable with non-blocking I/O, a lightweight protocol, and adequate hardware.
- To claim production-grade capacity, run the benchmarking methodology above in your target environment and tune OS and application parameters.

## How to reproduce your own benchmarks

- Build the server in Release mode.
- Deploy the server on the target machine.
- Use or implement a simple client load generator that opens many TCP connections and sends short messages at a controlled rate.
- Monitor via system tools (Task Manager, Performance Monitor) and network profiling tools.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Author

- DEEN-42

## Acknowledgments

- Built using Windows Sockets programming
- Implements modern C++ practices
- Uses thread-safe programming patterns