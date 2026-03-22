#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

// ─── Constants ───────────────────────────────────────────────────────────────

#define MAX_USERS 16
#define MAX_SERVERS 8
#define MAX_CHANNELS 5
#define MAX_MESSAGES 64
#define MAX_MSG_LEN 240
#define MAX_NAME_LEN 32
#define MAX_PASS_LEN 32

// ─── Screens ─────────────────────────────────────────────────────────────────

typedef enum
{
    SCREEN_LOGIN,
    SCREEN_SIGNUP,
    SCREEN_SERVERS,
    SCREEN_CHAT,
} Screen;

// ─── Data ────────────────────────────────────────────────────────────────────

typedef struct
{
    char username[MAX_NAME_LEN];
    char password[MAX_PASS_LEN];
    int server_id; // -1 = not in a server
} User;

typedef struct
{
    char sender[MAX_NAME_LEN];
    char body[MAX_MSG_LEN];
    long ts;
} Message;

typedef struct
{
    char name[MAX_NAME_LEN];
    Message messages[MAX_MESSAGES];
    int msg_count;
} Channel;

typedef struct
{
    int id;
    char name[MAX_NAME_LEN];
    char description[80];
    char icon[4]; // emoji-like 2-char
    int member_count;
    Channel channels[MAX_CHANNELS];
    int channel_count;
} Server;

// ─── Global State ────────────────────────────────────────────────────────────

User users[MAX_USERS];
int user_count = 0;
int current_user = -1; // index into users[]

Server servers[MAX_SERVERS];
int server_count = 0;

Screen current_screen = SCREEN_LOGIN;
int active_server = -1;
int active_channel = 0;

// ─── Seed Data ───────────────────────────────────────────────────────────────

void seed_data()
{
    // Seed users
    strcpy(users[0].username, "alice");
    strcpy(users[0].password, "pass");
    users[0].server_id = 0;
    strcpy(users[1].username, "bob");
    strcpy(users[1].password, "pass");
    users[1].server_id = -1;
    user_count = 2;

    // Seed servers
    struct
    {
        const char *name;
        const char *desc;
        const char *icon;
        int members;
    } sv[] = {
        {"GamersHQ", "All things gaming", "GG", 142},
        {"DevCorner", "Code, debug, repeat", "{}", 389},
        {"MusicLounge", "Beats & vibes 24/7", "♪♫", 210},
        {"StudyGroup", "Focus & productivity", ":::", 98},
        {"Foodies Unite", "Recipes, reviews & eats", ">>", 176},
        {"Crypto Talk", "Markets & blockchain", "$$", 503},
    };

    struct
    {
        int s;
        const char *ch;
    } channels_data[] = {
        {0, "general"},
        {0, "lfg"},
        {0, "clips"},
        {1, "general"},
        {1, "help"},
        {1, "projects"},
        {1, "off-topic"},
        {2, "general"},
        {2, "sharing"},
        {2, "requests"},
        {3, "general"},
        {3, "study-room"},
        {3, "resources"},
        {4, "general"},
        {4, "recipes"},
        {4, "restaurants"},
        {5, "general"},
        {5, "news"},
        {5, "trading"},
    };

    struct
    {
        int s;
        int c;
        const char *from;
        const char *msg;
    } msgs[] = {
        {0, 0, "alice", "Welcome to GamersHQ!"},
        {0, 0, "bob", "gg ez"},
        {0, 0, "carol", "Anyone up for ranked?"},
        {0, 1, "dave", "LFG Valorant ranked, plat+"},
        {0, 1, "alice", "I'm in, inv me"},
        {1, 0, "carol", "Just shipped v2.0 today!"},
        {1, 0, "alice", "Let's gooo"},
        {1, 1, "bob", "Getting seg fault on line 42 lol"},
        {1, 1, "dave", "Have you tried turning it off and on?"},
        {2, 0, "eve", "new playlist just dropped"},
        {2, 0, "frank", "link??"},
        {3, 0, "alice", "pomodoro session starting in 5"},
        {3, 1, "bob", "I'll join, need to finish this essay"},
        {4, 0, "carol", "made pasta carbonara tonight"},
        {5, 0, "dave", "BTC above 100k again"},
        {5, 0, "eve", "hodl"},
    };

    server_count = 6;
    for (int i = 0; i < server_count; i++)
    {
        servers[i].id = i;
        strncpy(servers[i].name, sv[i].name, MAX_NAME_LEN - 1);
        strncpy(servers[i].description, sv[i].desc, 79);
        strncpy(servers[i].icon, sv[i].icon, 3);
        servers[i].member_count = sv[i].members;
        servers[i].channel_count = 0;
    }
    for (int i = 0; i < (int)(sizeof(channels_data) / sizeof(channels_data[0])); i++)
    {
        int s = channels_data[i].s;
        int c = servers[s].channel_count++;
        strncpy(servers[s].channels[c].name, channels_data[i].ch, MAX_NAME_LEN - 1);
        servers[s].channels[c].msg_count = 0;
    }
    for (int i = 0; i < (int)(sizeof(msgs) / sizeof(msgs[0])); i++)
    {
        Server *sv2 = &servers[msgs[i].s];
        Channel *ch = &sv2->channels[msgs[i].c];
        if (ch->msg_count >= MAX_MESSAGES)
            continue;
        Message *m = &ch->messages[ch->msg_count++];
        strncpy(m->sender, msgs[i].from, MAX_NAME_LEN - 1);
        strncpy(m->body, msgs[i].msg, MAX_MSG_LEN - 1);
        m->ts = time(NULL) - (50 - i) * 120;
    }
}

// ─── Color Pairs ────────────────────────────────────────────────────────────
//  1 = normal text on default bg
//  2 = white on blue  (topbar / active item)
//  3 = white on dark  (sidebar)
//  4 = dim            (timestamps / labels)
//  5 = cyan           (accent / sender)
//  6 = dark borders
//  7 = green          (success / online)
//  8 = red            (error)
//  9 = yellow         (warning / highlight)
// 10 = white on dark-blue (header bar)

void init_colors()
{
    start_color();
    use_default_colors();
    init_pair(1, COLOR_WHITE, -1);
    init_pair(2, COLOR_WHITE, COLOR_BLUE);
    init_pair(3, COLOR_WHITE, COLOR_BLACK);
    init_pair(4, 8, -1);
    init_pair(5, COLOR_CYAN, -1);
    init_pair(6, COLOR_BLACK, -1);
    init_pair(7, COLOR_GREEN, -1);
    init_pair(8, COLOR_RED, -1);
    init_pair(9, COLOR_YELLOW, -1);
    init_pair(10, COLOR_WHITE, COLOR_BLUE);
}

// ─── Helpers ────────────────────────────────────────────────────────────────

void draw_box(int y, int x, int h, int w, int pair)
{
    attron(COLOR_PAIR(pair));
    mvaddch(y, x, '+');
    mvhline(y, x + 1, '-', w - 2);
    mvaddch(y, x + w - 1, '+');
    for (int i = 1; i < h - 1; i++)
    {
        mvaddch(y + i, x, '|');
        mvhline(y + i, x + 1, ' ', w - 2);
        mvaddch(y + i, x + w - 1, '|');
    }
    mvaddch(y + h - 1, x, '+');
    mvhline(y + h - 1, x + 1, '-', w - 2);
    mvaddch(y + h - 1, x + w - 1, '+');
    attroff(COLOR_PAIR(pair));
}

void center_str(int row, const char *s, int pair, int attr)
{
    int cols;
    getmaxyx(stdscr, (int){0}, cols);
    int x = (cols - (int)strlen(s)) / 2;
    if (x < 0)
        x = 0;
    attron(COLOR_PAIR(pair) | attr);
    mvprintw(row, x, "%s", s);
    attroff(COLOR_PAIR(pair) | attr);
}

// ─── Screen: LOGIN / SIGNUP ──────────────────────────────────────────────────

typedef struct
{
    char username[MAX_NAME_LEN];
    char password[MAX_PASS_LEN];
    int field; // 0=username 1=password
    char error[80];
    char success[80];
} AuthState;

AuthState auth = {"", "", 0, "", ""};

void draw_auth(int rows, int cols, int is_signup)
{
    clear();
    int bw = 44, bh = 14;
    int by = (rows - bh) / 2;
    int bx = (cols - bw) / 2;

    // Background dots pattern
    attron(COLOR_PAIR(4));
    for (int r = 0; r < rows; r += 3)
        for (int c = 0; c < cols; c += 6)
            mvaddch(r, c, '.');
    attroff(COLOR_PAIR(4));

    draw_box(by, bx, bh, bw, 5);

    // Title
    attron(COLOR_PAIR(5) | A_BOLD);
    const char *title = is_signup ? "  Create Account  " : "  Welcome Back  ";
    mvprintw(by + 1, bx + (bw - strlen(title)) / 2, "%s", title);
    attroff(COLOR_PAIR(5) | A_BOLD);

    attron(COLOR_PAIR(4));
    mvhline(by + 2, bx + 2, '-', bw - 4);
    attroff(COLOR_PAIR(4));

    // Username field
    int fy = by + 4;
    attron(COLOR_PAIR(4));
    mvprintw(fy, bx + 3, "Username:");
    attroff(COLOR_PAIR(4));

    int active0 = (auth.field == 0);
    attron(COLOR_PAIR(active0 ? 2 : 1));
    mvprintw(fy + 1, bx + 3, " %-36s ", auth.username);
    attroff(COLOR_PAIR(active0 ? 2 : 1));

    // Password field
    int py = fy + 3;
    attron(COLOR_PAIR(4));
    mvprintw(py, bx + 3, "Password:");
    attroff(COLOR_PAIR(4));

    char masked[MAX_PASS_LEN + 1];
    int plen = strlen(auth.password);
    for (int i = 0; i < plen; i++)
        masked[i] = '*';
    masked[plen] = '\0';

    int active1 = (auth.field == 1);
    attron(COLOR_PAIR(active1 ? 2 : 1));
    mvprintw(py + 1, bx + 3, " %-36s ", masked);
    attroff(COLOR_PAIR(active1 ? 2 : 1));

    // Error / success
    if (auth.error[0])
    {
        attron(COLOR_PAIR(8) | A_BOLD);
        mvprintw(by + bh - 4, bx + 3, "%-38s", auth.error);
        attroff(COLOR_PAIR(8) | A_BOLD);
    }
    if (auth.success[0])
    {
        attron(COLOR_PAIR(7) | A_BOLD);
        mvprintw(by + bh - 4, bx + 3, "%-38s", auth.success);
        attroff(COLOR_PAIR(7) | A_BOLD);
    }

    // Hints
    attron(COLOR_PAIR(4));
    const char *hint = is_signup
                           ? "TAB: switch field  ENTER: create  F2: login"
                           : "TAB: switch field  ENTER: login   F2: signup";
    mvprintw(by + bh - 2, bx + 3, "%-38s", hint);
    attroff(COLOR_PAIR(4));

    // Place cursor
    if (auth.field == 0)
        move(fy + 1, bx + 4 + strlen(auth.username));
    else
        move(py + 1, bx + 4 + plen);

    refresh();
}

void handle_auth_input(int ch, int is_signup)
{
    char *target = auth.field == 0 ? auth.username : auth.password;
    int maxlen = auth.field == 0 ? MAX_NAME_LEN - 1 : MAX_PASS_LEN - 1;
    int len = strlen(target);

    auth.error[0] = '\0';
    auth.success[0] = '\0';

    switch (ch)
    {
    case '\t':
    case KEY_DOWN:
        auth.field = 1 - auth.field;
        break;

    case KEY_UP:
        auth.field = 1 - auth.field;
        break;

    case KEY_BACKSPACE:
    case 127:
        if (len > 0)
            target[--len] = '\0';
        break;

    case '\n':
    case KEY_ENTER:
    {
        if (!strlen(auth.username))
        {
            strcpy(auth.error, "Username required.");
            break;
        }
        if (!strlen(auth.password))
        {
            strcpy(auth.error, "Password required.");
            break;
        }

        if (is_signup)
        {
            // Check duplicate
            for (int i = 0; i < user_count; i++)
            {
                if (!strcmp(users[i].username, auth.username))
                {
                    strcpy(auth.error, "Username already taken.");
                    return;
                }
            }
            if (user_count >= MAX_USERS)
            {
                strcpy(auth.error, "Server full.");
                break;
            }
            strncpy(users[user_count].username, auth.username, MAX_NAME_LEN - 1);
            strncpy(users[user_count].password, auth.password, MAX_PASS_LEN - 1);
            users[user_count].server_id = -1;
            current_user = user_count++;
            current_screen = SCREEN_SERVERS;
        }
        else
        {
            int found = -1;
            for (int i = 0; i < user_count; i++)
            {
                if (!strcmp(users[i].username, auth.username) &&
                    !strcmp(users[i].password, auth.password))
                {
                    found = i;
                    break;
                }
            }
            if (found < 0)
            {
                strcpy(auth.error, "Invalid username or password.");
                break;
            }
            current_user = found;
            if (users[current_user].server_id >= 0)
            {
                active_server = users[current_user].server_id;
                active_channel = 0;
                current_screen = SCREEN_CHAT;
            }
            else
            {
                current_screen = SCREEN_SERVERS;
            }
        }
        auth.username[0] = '\0';
        auth.password[0] = '\0';
        auth.field = 0;
        break;
    }

    case KEY_F(2):
        auth.username[0] = '\0';
        auth.password[0] = '\0';
        auth.field = 0;
        auth.error[0] = '\0';
        current_screen = is_signup ? SCREEN_LOGIN : SCREEN_SIGNUP;
        break;

    default:
        if (ch >= 32 && ch < 127 && len < maxlen)
        {
            target[len] = (char)ch;
            target[len + 1] = '\0';
        }
    }
}

// ─── Screen: SERVER LIST ────────────────────────────────────────────────────

int server_cursor = 0; // highlighted row

void draw_server_list(int rows, int cols)
{
    clear();

    // Top bar
    attron(COLOR_PAIR(10) | A_BOLD);
    mvhline(0, 0, ' ', cols);
    mvprintw(0, 2, " SERVER BROWSER");
    if (current_user >= 0)
        mvprintw(0, cols - strlen(users[current_user].username) - 4,
                 "[%s]", users[current_user].username);
    attroff(COLOR_PAIR(10) | A_BOLD);

    // Sub-header
    attron(COLOR_PAIR(4));
    mvprintw(2, 3, "Choose a server to join, or press ESC to go back.");
    mvhline(3, 0, '-', cols);
    attroff(COLOR_PAIR(4));

    // Column headers
    attron(COLOR_PAIR(9) | A_BOLD);
    mvprintw(4, 3, "%-3s  %-20s  %-36s  %s", "#", "SERVER", "DESCRIPTION", "MEMBERS");
    attroff(COLOR_PAIR(9) | A_BOLD);
    attron(COLOR_PAIR(4));
    mvhline(5, 0, '-', cols);
    attroff(COLOR_PAIR(4));

    // Server rows
    for (int i = 0; i < server_count; i++)
    {
        int row = 6 + i * 2;
        Server *sv = &servers[i];

        if (i == server_cursor)
        {
            attron(COLOR_PAIR(2) | A_BOLD);
            mvhline(row, 0, ' ', cols);
            mvprintw(row, 3, "%-3d  %-20s  %-36s  %d members",
                     i + 1, sv->name, sv->description, sv->member_count);
            attroff(COLOR_PAIR(2) | A_BOLD);
        }
        else
        {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(row, 3, "%-3d", i + 1);
            attroff(COLOR_PAIR(5) | A_BOLD);
            attron(COLOR_PAIR(1));
            mvprintw(row, 8, "%-20s", sv->name);
            attroff(COLOR_PAIR(1));
            attron(COLOR_PAIR(4));
            mvprintw(row, 30, "%-36s", sv->description);
            mvprintw(row, 68, "%d members", sv->member_count);
            attroff(COLOR_PAIR(4));
        }
    }

    // Status bar
    attron(COLOR_PAIR(3));
    mvhline(rows - 1, 0, ' ', cols);
    mvprintw(rows - 1, 2, " UP/DOWN: navigate   ENTER/CLICK: join   ESC: logout");
    attroff(COLOR_PAIR(3));

    refresh();
}

void handle_server_input(int ch)
{
    switch (ch)
    {
    case KEY_UP:
        if (server_cursor > 0)
            server_cursor--;
        break;
    case KEY_DOWN:
        if (server_cursor < server_count - 1)
            server_cursor++;
        break;
    case '\n':
    case KEY_ENTER:
        active_server = server_cursor;
        active_channel = 0;
        users[current_user].server_id = active_server;
        current_screen = SCREEN_CHAT;
        break;
    case 27: // ESC = logout
        current_user = -1;
        active_server = -1;
        server_cursor = 0;
        current_screen = SCREEN_LOGIN;
        break;
    case KEY_MOUSE:
    {
        MEVENT me;
        if (getmouse(&me) == OK && me.bstate & BUTTON1_CLICKED)
        {
            for (int i = 0; i < server_count; i++)
            {
                if (me.y == 6 + i * 2)
                {
                    server_cursor = i;
                    active_server = i;
                    active_channel = 0;
                    users[current_user].server_id = active_server;
                    current_screen = SCREEN_CHAT;
                    break;
                }
            }
        }
        break;
    }
    }
}

// ─── Screen: CHAT ───────────────────────────────────────────────────────────

char chat_input[MAX_MSG_LEN] = {0};
int chat_input_len = 0;

void draw_chat(int rows, int cols)
{
    clear();
    Server *sv = &servers[active_server];

    // ── Sidebar (0..17) ──
    attron(COLOR_PAIR(3));
    for (int r = 0; r < rows - 1; r++)
        mvhline(r, 0, ' ', 18);
    attroff(COLOR_PAIR(3));

    // Server name in sidebar
    attron(COLOR_PAIR(10) | A_BOLD);
    mvhline(0, 0, ' ', 18);
    mvprintw(0, 1, " %-16s", sv->name);
    attroff(COLOR_PAIR(10) | A_BOLD);

    // Channel label
    attron(COLOR_PAIR(4));
    mvprintw(2, 2, "CHANNELS");
    attroff(COLOR_PAIR(4));

    // Channel list
    for (int i = 0; i < sv->channel_count; i++)
    {
        int row = 4 + i * 2;
        if (i == active_channel)
        {
            attron(COLOR_PAIR(2) | A_BOLD);
            mvprintw(row, 1, " # %-14s", sv->channels[i].name);
            attroff(COLOR_PAIR(2) | A_BOLD);
        }
        else
        {
            attron(COLOR_PAIR(3));
            mvprintw(row, 1, " # %-14s", sv->channels[i].name);
            attroff(COLOR_PAIR(3));
        }
    }

    // Servers button at bottom of sidebar
    attron(COLOR_PAIR(9) | A_BOLD);
    mvprintw(rows - 3, 1, " << Servers");
    attroff(COLOR_PAIR(9) | A_BOLD);

    attron(COLOR_PAIR(8) | A_BOLD);
    mvprintw(rows - 2, 1, " Logout");
    attroff(COLOR_PAIR(8) | A_BOLD);

    // Sidebar border
    attron(COLOR_PAIR(6));
    for (int r = 0; r < rows - 1; r++)
        mvaddch(r, 18, '|');
    attroff(COLOR_PAIR(6));

    // ── Top bar (19..) ──
    attron(COLOR_PAIR(10) | A_BOLD);
    mvhline(0, 19, ' ', cols - 19);
    mvprintw(0, 20, " # %s", sv->channels[active_channel].name);
    if (current_user >= 0)
        mvprintw(0, cols - strlen(users[current_user].username) - 3,
                 " %s ", users[current_user].username);
    attroff(COLOR_PAIR(10) | A_BOLD);

    // ── Messages ──
    Channel *ch = &sv->channels[active_channel];
    int msg_area = rows - 4;
    int start = ch->msg_count > msg_area ? ch->msg_count - msg_area : 0;

    attron(COLOR_PAIR(1));
    for (int r = 1; r < rows - 2; r++)
        mvhline(r, 19, ' ', cols - 19);
    attroff(COLOR_PAIR(1));

    for (int i = start; i < ch->msg_count; i++)
    {
        int row = 2 + (i - start);
        Message *m = &ch->messages[i];
        char tstr[8];
        struct tm *tm = localtime(&m->ts);
        strftime(tstr, sizeof(tstr), "%H:%M", tm);

        attron(COLOR_PAIR(4));
        mvprintw(row, 20, "%s", tstr);
        attroff(COLOR_PAIR(4));

        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(row, 26, "%-12s", m->sender);
        attroff(COLOR_PAIR(5) | A_BOLD);

        attron(COLOR_PAIR(1));
        mvprintw(row, 39, "%.*s", cols - 40, m->body);
        attroff(COLOR_PAIR(1));
    }

    // ── Input area ──
    attron(COLOR_PAIR(6));
    mvhline(rows - 3, 19, '-', cols - 19);
    attroff(COLOR_PAIR(6));

    attron(COLOR_PAIR(1));
    mvhline(rows - 2, 19, ' ', cols - 19);
    mvprintw(rows - 2, 20, "[#%s] %s", sv->channels[active_channel].name, chat_input);
    attroff(COLOR_PAIR(1));

    // ── Status bar ──
    attron(COLOR_PAIR(3));
    mvhline(rows - 1, 0, ' ', cols);
    mvprintw(rows - 1, 1, " TAB: channels  ENTER: send  CLICK: select channel  q: quit");
    attroff(COLOR_PAIR(3));

    // Cursor
    int cx = 20 + 2 + strlen(sv->channels[active_channel].name) + 2 + chat_input_len;
    move(rows - 2, cx);
    refresh();
}

void handle_chat_input(int ch)
{
    Server *sv = &servers[active_server];

    switch (ch)
    {
    case '\t':
        active_channel = (active_channel + 1) % sv->channel_count;
        chat_input[0] = '\0';
        chat_input_len = 0;
        break;

    case '\n':
    case KEY_ENTER:
        if (chat_input_len > 0)
        {
            Channel *c = &sv->channels[active_channel];
            if (c->msg_count < MAX_MESSAGES)
            {
                Message *m = &c->messages[c->msg_count++];
                strncpy(m->sender, users[current_user].username, MAX_NAME_LEN - 1);
                strncpy(m->body, chat_input, MAX_MSG_LEN - 1);
                m->ts = time(NULL);
            }
            chat_input[0] = '\0';
            chat_input_len = 0;
        }
        break;

    case KEY_BACKSPACE:
    case 127:
        if (chat_input_len > 0)
            chat_input[--chat_input_len] = '\0';
        break;

    case KEY_MOUSE:
    {
        MEVENT me;
        if (getmouse(&me) == OK && me.bstate & BUTTON1_CLICKED)
        {
            // Click on channel in sidebar
            if (me.x < 18)
            {
                for (int i = 0; i < sv->channel_count; i++)
                {
                    if (me.y == 4 + i * 2)
                    {
                        active_channel = i;
                        chat_input[0] = '\0';
                        chat_input_len = 0;
                        break;
                    }
                }
                // << Servers button
                int rows;
                getmaxyx(stdscr, rows, (int){0});
                if (me.y == rows - 3)
                {
                    users[current_user].server_id = -1;
                    active_server = -1;
                    server_cursor = 0;
                    current_screen = SCREEN_SERVERS;
                }
                // Logout
                if (me.y == rows - 2)
                {
                    users[current_user].server_id = -1;
                    current_user = -1;
                    active_server = -1;
                    current_screen = SCREEN_LOGIN;
                }
            }
        }
        break;
    }

    default:
        if (ch >= 32 && ch < 127 && chat_input_len < MAX_MSG_LEN - 1)
        {
            chat_input[chat_input_len++] = (char)ch;
            chat_input[chat_input_len] = '\0';
        }
    }
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main()
{
    seed_data();

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    curs_set(1);
    init_colors();

    int rows, cols, ch;

    while (1)
    {
        getmaxyx(stdscr, rows, cols);

        switch (current_screen)
        {
        case SCREEN_LOGIN:
            draw_auth(rows, cols, 0);
            break;
        case SCREEN_SIGNUP:
            draw_auth(rows, cols, 1);
            break;
        case SCREEN_SERVERS:
            draw_server_list(rows, cols);
            break;
        case SCREEN_CHAT:
            draw_chat(rows, cols);
            break;
        }

        ch = getch();
        if (ch == 'q' && current_screen == SCREEN_CHAT)
            break;

        switch (current_screen)
        {
        case SCREEN_LOGIN:
            handle_auth_input(ch, 0);
            break;
        case SCREEN_SIGNUP:
            handle_auth_input(ch, 1);
            break;
        case SCREEN_SERVERS:
            handle_server_input(ch);
            break;
        case SCREEN_CHAT:
            handle_chat_input(ch);
            break;
        }
    }

    endwin();
    return 0;
}