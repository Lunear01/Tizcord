```text
  ████████╗ ██║ ███████╗   █████╗  ██████╗  ██████╗  █████╗ 
  ╚══██╔══╝ ██║ ╚══███╔╝ ██╔════╝ ██╔═══██╗ ██╔══██╗ ██╔══██╗
     ██║    ██║   ███╔╝  ██║      ██║   ██║ ██████╔╝ ██║  ██║
     ██║    ██║  ███╔╝   ██║      ██║   ██║ ██╔══██╗ ██║  ██║
     ██║    ██║ ███████╗ ╚██████╗ ╚██████╔╝ ██║  ██║ ██████╔╝
     ╚═╝    ╚═╝ ╚══════╝  ╚═════╝  ╚═════╝  ╚═╝  ╚═╝ ╚═════╝
```
### What is it? 
- Tizcord is a robust, terminal-based concurrent chat application inspired by Discord and built entirely in C. It leverages a multiplexed client-server architecture over TCP sockets to facilitate real-time communication directly within a command-line interface.

### TODO LIST: 
- [x] Login page and user authentication
- [x] Basic functionality: Establishing connection, sending and receiving messages
- [x] Online status indicators
- [x] Custom user status messages
- [x] Channels: Creation implemented, Removal pending
    - [x] Only allow administrators to create and delete channels
- [x] Private Messages
    - [x] Save direct messages to the database
- [x] Message timestamps 
- [x] Terminal User Interface (TUI) integration

### Features
- Real-Time Messaging: Facilitates low-latency communication across community channels and direct messages.
- Non-blocking User Interface: Utilizes the ncurses library alongside select() system calls to ensure the terminal remains fully responsive during network I/O operations.
- Server and Role Management: Supports server creation, channel administration, and role-based access control for actions such as member moderation.
- Social Systems: Includes a comprehensive friend request system and real-time user status broadcasting.
- Persistent Storage: Implements a SQLite backend for the secure storage of user credentials, chat histories, and server states, utilizing yescrypt for cryptographic password hashing.

### Architecture Overview
- Tizcord operates on a concurrent model designed for maximum efficiency:
    - Server-Side Concurrency: The server process employs the select() system call to monitor the listening socket and all active client connections simultaneously. This guarantees that the server never blocks on a single user while others await processing.
    - Client-Side Event Loop: The client application integrates standard input (STDIN_FILENO) and the network socket into a single select() loop. This ensures synchronous UI updates and non-blocking interactions.
    - Custom Wire Protocol: Network communication is standardized by a fixed-size C structure designated as TizcordPacket. Large data payloads, such as historical message retrieval, are securely chunked and streamed using sequence frames (LIST_FRAME_START, LIST_FRAME_MIDDLE, LIST_FRAME_END) to maintain memory safety and prevent buffer overflows.

### Dependencies
- To compile and execute Tizcord, the following dependencies are required:
    - gcc (GNU Compiler Collection)
    - make (Build automation tool)
    - libncurses-dev (Terminal UI library)
    - libsqlite3-dev (Database library)
    - libcrypt-dev (Cryptographic hashing library)

### Build Instructions
- Clone the repository and navigate to the project root directory.
- Compile the source code and initialize the database using the provided Makefile:

    # Compile the source code
    make all

    # Initialize the SQLite database and run the necessary SQL migrations
    make db

### Usage
- Initializing the Server
- Execute the server binary from the project directory. The server accepts optional arguments for the port number and database file:

./server 4242 tizcord.db

Note: If arguments are omitted, the server defaults to port 4242.

### Connecting a Client
- Launch the client application in a separate terminal instance, specifying the server's IPv4 address and target port:

./client 127.0.0.1 4242

### Command Line Interface (CLI) Reference
- Within the Tizcord UI, pressing the / key activates the command-line prompt. The following commands are supported:

#### Social Commands:
    - /friend [username] - Transmit a friend request.
    - /accept [username] - Authorize an incoming friend request.
    - /reject [username] - Decline an incoming friend request.
    - /unfriend [username] - Terminate an existing friendship.
    - /setstatus [status] - Update your custom profile status (maximum 64 characters).

#### Server Commands:
    - /createserver [name] - Instantiate a new server.
    - /deleteserver [name] - Permanently delete a server (Requires Administrator privileges).
    - /createchannel [name] - Provision a new channel in the active server (Requires Administrator privileges).
    - /deletechannel [name] - Remove a channel from the active server (Requires Administrator privileges).
    - /kick [username] - Forcibly disconnect and remove a user from the active server (Requires Administrator privileges).
    - /help - Display the internal command reference.

### Contributors
    - Lunear01
    - peterlee42
    - yiboooooooo
