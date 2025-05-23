# SQLite TCP Server in C

A multi-process TCP server implemented in C that allows remote clients to send SQL commands and receive responses from a SQLite3 database. This project demonstrates low-level systems programming with sockets, process management, inter-process communication (IPC), and database interaction.

## 🛠️ Features

- Accepts SQL commands from a remote client and executes them using a `sqlite3` subprocess.
- Uses pipes and `fork()/exec()` to communicate with the database securely.
- Non-blocking I/O with `select()` to handle multiple client types:
  - **Client socket** for SQL queries.
  - **Service socket** for administrative commands like server shutdown.
- Graceful shutdown support using a separate control connection and `$die!` command.
- Handles dead child processes with `waitpid()` to avoid zombies.

---

## 📁 Files

| File        | Description |
|-------------|-------------|
| `server.c`  | Main TCP server. Spawns child processes to run SQLite and communicates over pipes. |
| `client.c`  | Connects to the SQL port and sends commands interactively. |
| `service.c` | Connects to the service port and sends a shutdown signal (`$die!`). |

---

## 🧪 How It Works

1. The server listens on two ports:
   - `port1`: for SQL clients
   - `port2`: for service commands
2. When a client connects on `port1`, the server:
   - Forks a child process
   - Executes `sqlite3 foobar.db`
   - Connects the child’s stdin/stdout to pipes
   - Sends client commands to the child and forwards the response back
3. When a service connects on `port2` and sends `$die!`, the server shuts down cleanly.

---

## 🚀 How to Run

### Compile
```bash
gcc -o server server.c
gcc -o client client.c
gcc -o service service.c
