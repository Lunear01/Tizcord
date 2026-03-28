-- schema.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS "users" (
    id INTEGER PRIMARY KEY,
    username TEXT NOT NULL UNIQUE,
    email TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    created_at INTEGER DEFAULT (CAST(strftime('%s', 'now') AS INTEGER))
);

CREATE TABLE IF NOT EXISTS "friends" (
    id INTEGER PRIMARY KEY,
    user_id INTEGER NOT NULL,
    friend_id INTEGER NOT NULL,
    is_accepted INTEGER DEFAULT 0,
    created_at INTEGER DEFAULT (CAST(strftime('%s', 'now') AS INTEGER)),
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (friend_id) REFERENCES users(id) ON DELETE CASCADE,
    UNIQUE (user_id, friend_id)
);

CREATE TABLE IF NOT EXISTS "sessions" (
    id INTEGER PRIMARY KEY,
    user_id INTEGER NOT NULL,
    token TEXT NOT NULL UNIQUE,
    created_at INTEGER DEFAULT (CAST(strftime('%s', 'now') AS INTEGER)),
    expires_at INTEGER NOT NULL,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS "servers" (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    created_at INTEGER DEFAULT (CAST(strftime('%s', 'now') AS INTEGER))
);

CREATE TABLE IF NOT EXISTS "server_members" (
    id INTEGER PRIMARY KEY,
    server_id INTEGER NOT NULL,
    user_id INTEGER NOT NULL,
    is_admin INTEGER DEFAULT 0,
    created_at INTEGER DEFAULT (CAST(strftime('%s', 'now') AS INTEGER)),
    FOREIGN KEY (server_id) REFERENCES servers(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    UNIQUE (server_id, user_id)
);

CREATE TABLE IF NOT EXISTS "channels" (
    id INTEGER PRIMARY KEY,
    server_id INTEGER NOT NULL,
    name TEXT NOT NULL,
    created_at INTEGER DEFAULT (CAST(strftime('%s', 'now') AS INTEGER)),
    FOREIGN KEY (server_id) REFERENCES servers(id) ON DELETE CASCADE,
    UNIQUE (server_id, name)
);

CREATE TABLE IF NOT EXISTS "messages" (
    id INTEGER PRIMARY KEY,
    channel_id INTEGER NOT NULL,
    user_id INTEGER NOT NULL,
    content TEXT NOT NULL,
    created_at INTEGER DEFAULT (CAST(strftime('%s', 'now') AS INTEGER)),
    FOREIGN KEY (channel_id) REFERENCES channels(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);