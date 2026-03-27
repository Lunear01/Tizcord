-- test_data.sql
-- Generated with Gemini on 2026-03-27

-- Enable foreign keys to ensure data integrity during inserts
PRAGMA foreign_keys = ON;

-- 1. Create Users
INSERT INTO users (username, email, password_hash) VALUES 
('Alice_Admin', 'alice@example.com', 'hashed_pass_1'),
('Bob_Gamer', 'bob@example.com', 'hashed_pass_2'),
('Charlie_Dev', 'charlie@example.com', 'hashed_pass_3'),
('Diana_Design', 'diana@example.com', 'hashed_pass_4');

-- 2. Create Friendships
-- Alice and Bob are confirmed friends
INSERT INTO friends (user_id, friend_id, is_accepted) VALUES (1, 2, 1);
-- Charlie sent a friend request to Alice (pending)
INSERT INTO friends (user_id, friend_id, is_accepted) VALUES (3, 1, 0);
-- Diana and Charlie are confirmed friends
INSERT INTO friends (user_id, friend_id, is_accepted) VALUES (4, 3, 1);

-- 3. Create Servers
INSERT INTO servers (name) VALUES 
('The Code Cafe'),
('Weekend Warriors');

-- 4. Assign Server Members
-- Alice (Admin), Charlie, and Diana are in 'The Code Cafe'
INSERT INTO server_members (server_id, user_id, is_admin) VALUES 
(1, 1, 1), 
(1, 3, 0), 
(1, 4, 0);

-- Bob (Admin) and Alice are in 'Weekend Warriors'
INSERT INTO server_members (server_id, user_id, is_admin) VALUES 
(2, 2, 1), 
(2, 1, 0);

-- 5. Create Channels
-- Channels for 'The Code Cafe'
INSERT INTO channels (server_id, name) VALUES 
(1, 'general'),
(1, 'python-help'),
(1, 'ui-ux-design');

-- Channels for 'Weekend Warriors'
INSERT INTO channels (server_id, name) VALUES 
(2, 'general'),
(2, 'looking-for-group');

-- 6. Insert Messages
-- Chat history in 'The Code Cafe' -> 'general' (channel_id 1)
INSERT INTO messages (channel_id, user_id, content) VALUES 
(1, 1, 'Welcome to The Code Cafe, everyone!'),
(1, 3, 'Thanks Alice! Glad to be here.'),
(1, 4, 'Hello! Love the new server layout.');

-- Chat history in 'The Code Cafe' -> 'python-help' (channel_id 2)
INSERT INTO messages (channel_id, user_id, content) VALUES 
(2, 3, 'Does anyone remember the syntax for a list comprehension?'),
(2, 1, 'Sure! It is: [expression for item in iterable]');

-- Chat history in 'Weekend Warriors' -> 'looking-for-group' (channel_id 5)
INSERT INTO messages (channel_id, user_id, content) VALUES 
(5, 2, 'Anyone online for some matches tonight?'),
(5, 1, 'I might be on later around 8 PM EST.');