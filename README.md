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

### Architecture Design

#### System Topology
```text
+----------------------+      TCP + TizcordPacket       +----------------------+
|  Client Process      | <----------------------------> |  Server Process      |
|                      |                                |                      |
|  client_main.c       |                                |  main.c              |
|  client.c            |                                |  server.c            |
|  ui.c (ncurses TUI)  |                                |  auth/chat/social/   |
|                      |                                |  server handlers     |
+----------+-----------+                                +----------+-----------+
           |                                                           |
           |                                                           |
           | shared/protocol.h + shared/packet_helper.c               |
           +------------------------------+----------------------------+
                                          |
                                          v
                               +----------------------+
                               |   SQLite Database    |
                               |   db/schema.sql      |
                               |   server/db.c        |
                               +----------------------+
```

#### Layered Design
- Presentation Layer (Client UI): `client/ui.c`
    - Renders terminal screens and handles keyboard input.
    - Runs a select()-based loop over both keyboard input and the server socket so UI and network events are processed in one event loop.
- Client Transport Layer: `client/client.c`, `shared/packet_helper.c`
    - Converts user actions into protocol packets.
    - Uses full-packet send/receive helpers to guarantee complete transmission of fixed-size `TizcordPacket` messages.
- Server Gateway + Dispatcher: `server/server.c`
    - Accepts connections and maintains active `ClientNode` sessions.
    - Dispatches incoming packets by type to specialized handlers.
- Domain Services (Server): `server/auth.c`, `server/tizcord_chat.c`, `server/tizcord_server.c`, `server/tizcord_social.c`
    - Authentication and session lifecycle.
    - Channel/DM messaging and history retrieval.
    - Server/channel membership and moderation workflows.
    - Friends/status social graph operations.
- Persistence Layer: `server/db.c`, `db/schema.sql`
    - SQLite-backed repository-style functions for users, memberships, channels, messages, and direct messages.
    - Passwords are hashed via yescrypt (libcrypt).

#### Protocol Design
- Shared contract: `shared/protocol.h`
- Packet families: `PACKET_AUTH`, `PACKET_DM`, `PACKET_SERVER`, `PACKET_CHANNEL`, `PACKET_SOCIAL`
- Response semantics: standardized response codes (`RESP_OK`, `RESP_ERR_*`, etc.)
- List streaming: list metadata (`list_id`, `list_index`, `list_total`, `list_frame`) supports chunked list/history delivery across multiple packets.

#### Runtime Flow (Example: Sending a Channel Message)
1. User enters a message in the chat screen (`client/ui.c`).
2. Client builds a `PACKET_CHANNEL` with `CHANNEL_MESSAGE` (`client/client.c`).
3. Packet is serialized/sent with `send_full_packet()` (`shared/packet_helper.c`).
4. Server event loop receives the packet and dispatches it (`server/server.c`).
5. Chat service validates sender/channel, persists message, and broadcasts (`server/tizcord_chat.c` + `server/db.c`).
6. Each recipient client receives the packet and redraws UI state (`client/ui.c`).

#### Runtime Flow (Example: Login)
1. Client sends `PACKET_AUTH` with `AUTH_LOGIN`.
2. Server verifies credentials against stored hash (`server/auth.c` + `server/db.c`).
3. On success, server binds session state to the active `ClientNode` and may revoke older sessions for the same account.
4. Social/server membership lists are refreshed and streamed back using framed list packets.

#### Design Properties
- Single-process event-driven concurrency on both client and server with select().
- Strict protocol sharing between components through one shared header (`shared/protocol.h`).
- Clear separation of concerns: UI, transport, business logic, and persistence are isolated by module.
- Persistent state with deterministic restart behavior through SQLite.

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

    ```bash
    # Compile the source code
    make all

    # Initialize the SQLite database and run the necessary SQL migrations
    make db
    ```
### Usage
- Initializing the Server
- Execute the server binary from the project directory. The server accepts optional arguments for the port number and database file:
    ```bash
    ./server 4242 tizcord.db
    ```
**Note: If arguments are omitted, the server defaults to port 4242.**

### Connecting a Client
- Launch the client application in a separate terminal instance, specifying the server's IPv4 address and target port:
    ```bash
    ./client 127.0.0.1 4242
    ```
### Command Line Interface (CLI) Reference
- Within the Tizcord UI, pressing the `F1` key activates the command-line prompt. The following commands are supported:

#### Social Commands:
- `/friend [username]` - Transmit a friend request.
- `/accept [username]` - Authorize an incoming friend request.
- `/reject [username]` - Decline an incoming friend request.
- `/unfriend [username]` - Terminate an existing friendship.
- `/setstatus [status]` - Update your custom profile status (maximum 64 characters).

#### Server Commands:
- `/createserver [name]` - Instantiate a new server.
- `/deleteserver [name]` - Permanently delete a server (Requires Administrator privileges).
- `/createchannel [name]` - Provision a new channel in the active server (Requires Administrator privileges).
- `/deletechannel [name]` - Remove a channel from the active server (Requires Administrator privileges).
- `/kick [username]` - Forcibly disconnect and remove a user from the active server (Requires Administrator privileges).
- `/help` - Display the internal command reference.

### Contributorsd
- Lunear01
- peterlee42
- yiboooooooo
