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
- [x] TLS 1.3 between client and server (OpenSSL, pinned self-signed certificate)
- [ ] End-to-End encryption

### Features
- Real-Time Messaging: Facilitates low-latency communication across community channels and direct messages.
- Non-blocking User Interface: Utilizes the ncurses library alongside select() system calls to ensure the terminal remains fully responsive during network I/O operations.
- Server and Role Management: Supports server creation, channel administration, and role-based access control for actions such as member moderation.
- Social Systems: Includes a comprehensive friend request system and real-time user status broadcasting.
- Persistent Storage: SQLite stores user credentials, chat histories, and server states. Passwords are hashed with yescrypt. Messages are stored as plaintext.

### Architecture Overview
- Tizcord operates on a concurrent model designed for maximum efficiency:
    - Server-Side Concurrency: The server process uses `select()` to monitor the listening socket and active clients. Packet reads and writes are currently blocking, so a slow or fragmented peer can still stall the event loop.
    - Client-Side Event Loop: The client application integrates standard input (STDIN_FILENO) and the network socket into a single select() loop. This ensures synchronous UI updates and non-blocking interactions.
    - Custom Wire Protocol: Network communication is standardized by a fixed-size C structure designated as TizcordPacket. Large data payloads, such as historical message retrieval, are securely chunked and streamed using sequence frames (LIST_FRAME_START, LIST_FRAME_MIDDLE, LIST_FRAME_END) to maintain memory safety and prevent buffer overflows.

### Packet writes and byte order

All packet sends go through `packet_send`, which copies the packet, converts its 32-bit integer and enum fields with `htonl`, converts its 64-bit integer fields to big-endian order, and writes the full packet even when a socket send is short. `packet_receive` reads a full packet and converts those fields back to host byte order before dispatch. `htons` is used separately for the TCP port, which is a 16-bit socket address field. Character arrays are copied unchanged.

The wire packet still uses the C `TizcordPacket` structure layout, including compiler padding and union layout. Both peers must use a compatible ABI and the same protocol version. A fully portable wire format would serialize fields into explicit byte offsets instead of transmitting the structure layout.

### Dependencies
- To compile and execute Tizcord, the following dependencies are required:
    - gcc (GNU Compiler Collection)
    - make (Build automation tool)
    - libncurses-dev (Terminal UI library)
    - libsqlite3-dev (Database library)
    - libcrypt-dev (Cryptographic hashing library)
    - libssl-dev (OpenSSL, for TLS)

### Performance benchmark

On Linux, build the server and run the end-to-end channel benchmark with an isolated temporary SQLite database:

```bash
make -C server
python3 benchmarks/bench.py --rounds 100 --stress
```

The runner opens 1, 8, 32, then 64 clients, registers them before timing, and sends one outstanding channel message per client per round. A message completes when its sender receives the broadcast echo. JSON output includes **sent messages per second**, p50/p95/p99 echo latency, server CPU utilization over each timed interval, and peak server RSS from Linux `/proc`. It also checks split packet writes, a peer that stops mid-packet, rapid connection churn, a slow reader, full history retrieval, and persisted history after restart. The stress cases are bounded probes, not proof of indefinite stability. A `fail` result identifies a limit or regression; do not turn it into a performance claim.

Run on an otherwise idle Linux host and record the CPU model, kernel, compiler flags, storage, benchmark command, and raw JSON alongside any published result. The current implementation uses blocking socket I/O and SQLite writes per message; results are specific to the test host and workload. The script requires only Python 3 standard library modules and Linux `/proc`.

### Build Instructions
- Clone the repository and navigate to the project root directory.
- Compile the source code and initialize the database using the provided Makefile:

    ```bash
    # Compile the source code
    make all

    # Initialize the SQLite database and run the necessary SQL migrations
    make db
    ```
### TLS

Client and server speak TLS 1.3 over the TCP socket. TLS encrypts traffic on the network. The server still decrypts and can read every message.

**Pinned certificate** — the client trusts only the server certificate it is given, not system CAs. It also checks the certificate's IP address matches the address it dialed.

Generate a self-signed certificate and key into `certs/` (gitignored):

```bash
make cert IP=192.168.1.20   # LAN address clients dial; 127.0.0.1 is always included
```

- Copy `certs/server.crt` to every client. Regenerating it means recopying it.
- Keep `certs/server.key` on the server only. Anyone holding it can impersonate the server.
- A client dialing an IP not listed in the certificate fails the handshake.

The server drops a client that does not finish the TLS handshake within 5 seconds. A stalled handshake does not block other clients.

**`--insecure`** — runs without TLS; all traffic is plaintext. Meant for local benchmarks only.

### End-to-End Encryption (planned, not implemented)

Planned design: clients exchange public keys and derive a shared key with X25519 key agreement. Messages are encrypted with AES-GCM using that key, so the server never sees plaintext. None of this exists in the code yet.

### Usage
- Initializing the Server
- Execute the server binary from the project directory with the port, database file, and certificate pair:
    ```bash
    ./server 4242 tizcord.db ../certs/server.crt ../certs/server.key
    ```
**Note: Port, database, and either the certificate pair or `--insecure` are required.**

### Connecting a Client
- Launch the client application in a separate terminal instance, specifying the server's IPv4 address, port, and the server's certificate (or `--insecure` for a server running without TLS):
    ```bash
    ./client 127.0.0.1 4242 ../certs/server.crt
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

### Contributors
- Lunear01
- peterlee42
- yiboooooooo
