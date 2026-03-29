#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/select.h>

#include "../include/ui.h"
#include "../include/client.h"

extern int client_socket;
// ─── Constants ───────────────────────────────────────────────────────────────

#define MAX_USERS 16
#define UI_MAX_SERVERS 8
#define UI_MAX_CHANNELS 5
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
    SCREEN_COMMAND
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
} UIChannel;

typedef struct
{
    int id;
    char name[MAX_NAME_LEN];
    char description[80];
    char icon[4]; // emoji-like 2-char
    int member_count;
    UIChannel channels[UI_MAX_CHANNELS];
    int channel_count;
} UIServer;

// ─── Global State ────────────────────────────────────────────────────────────

User users[MAX_USERS];
int user_count = 0;
int current_user = -1; // index into users[]

UIServer servers[UI_MAX_SERVERS];
int server_count = 0;

Screen current_screen = SCREEN_LOGIN;
int active_server = -1;
int active_channel = 0;

Screen previous_screen = SCREEN_LOGIN;
char cmd_input[MAX_MSG_LEN] = {0};
int cmd_input_len = 0;

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
        {"MusicLounge", "Beats & vibes 24/7", "~j", 210},
        {"StudyGroup", "Focus & productivity", "::", 98},
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
        UIServer *sv2 = &servers[msgs[i].s];
        UIChannel *ch = &sv2->channels[msgs[i].c];
        if (ch->msg_count >= MAX_MESSAGES)
            continue;
        Message *m = &ch->messages[ch->msg_count++];
        strncpy(m->sender, msgs[i].from, MAX_NAME_LEN - 1);
        strncpy(m->body, msgs[i].msg, MAX_MSG_LEN - 1);
        m->ts = time(NULL) - (50 - i) * 120;
    }
}

// ─── Color Pairs ─────────────────────────────────────────────────────────────
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

// ─── Helpers ─────────────────────────────────────────────────────────────────

/* Draw a box using ACS line-drawing characters.
   pair is the colour pair for the border only. */
void draw_box(int y, int x, int h, int w, int pair)
{
    attron(COLOR_PAIR(pair));
    /* Top edge */
    mvaddch(y, x, ACS_ULCORNER);
    mvhline(y, x + 1, ACS_HLINE, w - 2);
    mvaddch(y, x + w - 1, ACS_URCORNER);
    /* Sides + cleared interior */
    for (int i = 1; i < h - 1; i++)
    {
        mvaddch(y + i, x, ACS_VLINE);
        mvhline(y + i, x + 1, ' ', w - 2);
        mvaddch(y + i, x + w - 1, ACS_VLINE);
    }
    /* Bottom edge */
    mvaddch(y + h - 1, x, ACS_LLCORNER);
    mvhline(y + h - 1, x + 1, ACS_HLINE, w - 2);
    mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);
    attroff(COLOR_PAIR(pair));
}

/* Print a string centred horizontally on row. */
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
    int field; // 0=username  1=password
    char error[80];
    char success[80];
} AuthState;

AuthState auth = {"", "", 0, "", ""};

/* ── Thick bitmap font for TIZCORD logo ────────────────────────────────────
   Each glyph is GLYPH_H rows × GLYPH_W cols, stored MSB-left in a uint16_t.
   Rendered with full-block (█) and half-step (▓) chars for the double-border
   look: outer edge = '█', one-cell inset = '▓', interior = ' '.          */

#define GLYPH_W 7
#define GLYPH_H 9
#define LOGO_LEN 7 /* T I Z C O R D */

/* Bit 6 = leftmost column */
static const uint16_t FONT_T[GLYPH_H] = {
    0b1111111,
    0b1111111,
    0b0011100,
    0b0011100,
    0b0011100,
    0b0011100,
    0b0011100,
    0b0011100,
    0b0011100,
};
static const uint16_t FONT_I[GLYPH_H] = {
    0b1111111,
    0b1111111,
    0b0011100,
    0b0011100,
    0b0011100,
    0b0011100,
    0b0011100,
    0b1111111,
    0b1111111,
};
static const uint16_t FONT_Z[GLYPH_H] = {
    0b1111111,
    0b1111111,
    0b0000111,
    0b0001110,
    0b0011100,
    0b0111000,
    0b1110000,
    0b1111111,
    0b1111111,
};
static const uint16_t FONT_C[GLYPH_H] = {
    0b1111111,
    0b1111111,
    0b1100000,
    0b1100000,
    0b1100000,
    0b1100000,
    0b1100000,
    0b1111111,
    0b1111111,
};
static const uint16_t FONT_O[GLYPH_H] = {
    0b1111111,
    0b1111111,
    0b1100011,
    0b1100011,
    0b1100011,
    0b1100011,
    0b1100011,
    0b1111111,
    0b1111111,
};
static const uint16_t FONT_R[GLYPH_H] = {
    0b1111111,
    0b1111111,
    0b1100011,
    0b1100011,
    0b1111110,
    0b1111110,
    0b1100110,
    0b1100011,
    0b1100011,
};
static const uint16_t FONT_D[GLYPH_H] = {
    0b0111110,
    0b1111111,
    0b1100011,
    0b1100011,
    0b1100011,
    0b1100011,
    0b1100011,
    0b1111111,
    0b0111110,
};

static const uint16_t *TIZCORD_GLYPHS[LOGO_LEN] = {
    FONT_T, FONT_I, FONT_Z, FONT_C, FONT_O, FONT_R, FONT_D};

/* Total pixel width of the logo with 1-col gaps between letters */
static const int ascii_logo_lines = GLYPH_H; /* kept for layout compat */

/* Draw the thick logo at terminal row `start_row`, centred on `cols`.
   Uses colour pair `pair` (should be A_BOLD | COLOR_PAIR(5) or similar). */
static void draw_logo(int start_row, int cols, int pair)
{
    /* Total width: LOGO_LEN glyphs × GLYPH_W + (LOGO_LEN-1) gaps of 1 */
    int total_w = LOGO_LEN * GLYPH_W + (LOGO_LEN - 1);
    int lx = (cols - total_w) / 2;
    if (lx < 0)
        lx = 0;

    for (int row = 0; row < GLYPH_H; row++)
    {
        int cx = lx;
        for (int g = 0; g < LOGO_LEN; g++)
        {
            uint16_t bits = TIZCORD_GLYPHS[g][row];
            for (int col = 0; col < GLYPH_W; col++)
            {
                int set = (bits >> (GLYPH_W - 1 - col)) & 1;
                attron(COLOR_PAIR(pair) | A_BOLD);
                mvaddch(start_row + row, cx + col,
                        set ? (wchar_t)0x2588 /* █ full block */
                            : ' ');
                attroff(COLOR_PAIR(pair) | A_BOLD);
            }
            cx += GLYPH_W;
            if (g < LOGO_LEN - 1)
                cx += 1; /* gap */
        }
    }
}

void draw_auth(int rows, int cols, int is_signup)
{
    erase();

    /* Box dimensions – wide enough for the field label + content. */
    int bw = 48; /* must be >= field width (38) + 2*margin(3) + 2*border(1) = 45 */
    int bh = 13;
    int logo_top = 1; /* first row of logo */

    /* Vertically centre the logo + a gap + the box */
    int total_h = ascii_logo_lines + 1 + bh;
    int by = (rows - total_h) / 2;
    if (by < logo_top + ascii_logo_lines + 1)
        by = logo_top + ascii_logo_lines + 1;
    int bx = (cols - bw) / 2;
    if (bx < 0)
        bx = 0;

    /* Recompute logo start so it sits above the box */
    int ly = by - ascii_logo_lines - 1;
    if (ly < 0)
        ly = 0;

    /* Background dot pattern */
    attron(COLOR_PAIR(4));
    for (int r = 0; r < rows; r += 3)
        for (int c = 0; c < cols; c += 6)
            mvaddch(r, c, '.');
    attroff(COLOR_PAIR(4));

    /* Logo */
    draw_logo(ly, cols, 5);

    /* Form box */
    draw_box(by, bx, bh, bw, 5);

    /* Title */
    const char *title = is_signup ? " Create Account " : " Welcome Back ";
    int tx = bx + (bw - (int)strlen(title)) / 2;
    attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(by + 1, tx, "%s", title);
    attroff(COLOR_PAIR(5) | A_BOLD);

    /* Separator under title */
    attron(COLOR_PAIR(4));
    mvhline(by + 2, bx + 1, ACS_HLINE, bw - 2);
    attroff(COLOR_PAIR(4));

    /* Field inner width:  bw - 2 (border) - 2*3 (margin) = bw-8
       With the leading/trailing space in the printw below that's bw-10.
       We'll use a fixed value that fits inside bw=46:  bw-10 = 36. */
    int fw = bw - 10; /* 38 for bw=48 */

    /* ── Username field ── */
    int fy = by + 4;
    attron(COLOR_PAIR(4));
    mvprintw(fy, bx + 3, "Username");
    attroff(COLOR_PAIR(4));

    int active0 = (auth.field == 0);
    attron(COLOR_PAIR(active0 ? 2 : 1));
    mvprintw(fy + 1, bx + 3, " %-*s ", fw, auth.username);
    attroff(COLOR_PAIR(active0 ? 2 : 1));

    /* ── Password field ── */
    int py = fy + 3;
    attron(COLOR_PAIR(4));
    mvprintw(py, bx + 3, "Password");
    attroff(COLOR_PAIR(4));

    char masked[MAX_PASS_LEN + 1];
    int plen = strlen(auth.password);
    for (int i = 0; i < plen; i++)
        masked[i] = '*';
    masked[plen] = '\0';

    int active1 = (auth.field == 1);
    attron(COLOR_PAIR(active1 ? 2 : 1));
    mvprintw(py + 1, bx + 3, " %-*s ", fw, masked);
    attroff(COLOR_PAIR(active1 ? 2 : 1));

    /* ── Error / success message ── */
    int msg_row = by + bh - 3;
    if (auth.error[0])
    {
        attron(COLOR_PAIR(8) | A_BOLD);
        mvprintw(msg_row, bx + 3, "%-*.*s", fw + 2, fw + 2, auth.error);
        attroff(COLOR_PAIR(8) | A_BOLD);
    }
    if (auth.success[0])
    {
        attron(COLOR_PAIR(7) | A_BOLD);
        mvprintw(msg_row, bx + 3, "%-*.*s", fw + 2, fw + 2, auth.success);
        attroff(COLOR_PAIR(7) | A_BOLD);
    }

    /* ── Hint line ── */
    /* Separator above hint */
    attron(COLOR_PAIR(4));
    mvhline(by + bh - 2, bx + 1, ACS_HLINE, bw - 2);
    attroff(COLOR_PAIR(4));

    const char *hint = is_signup
                           ? "TAB/field  ENTER/create  F2/login"
                           : "TAB/field  ENTER/login   F2/signup";
    int hx = bx + (bw - (int)strlen(hint)) / 2;
    attron(COLOR_PAIR(4));
    mvprintw(by + bh - 1, hx, "%s", hint);
    attroff(COLOR_PAIR(4));

    /* Place cursor inside the active field */
    if (auth.field == 0)
        move(fy + 1, bx + 4 + (int)strlen(auth.username));
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
            // Just send the packet and set a loading message!
            send_register(auth.username, auth.password);
            strcpy(auth.success, "Waiting for server...");
            auth.error[0] = '\0';
            return; // Break out immediately, the listener thread will do the rest
        } 
        else
        {
            // Trigger the network packet
            send_login(auth.username, auth.password);
            strcpy(auth.success, "Authenticating...");
            auth.error[0] = '\0';
            return; // Break out immediately, let the listener thread catch the reply
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


// ─── Screen: SERVER LIST ─────────────────────────────────────────────────────

int server_cursor = 0; /* highlighted row */

void draw_server_list(int rows, int cols)
{
    erase();

    /* ── Top bar ── */
    attron(COLOR_PAIR(10) | A_BOLD);
    mvhline(0, 0, ' ', cols);
    mvprintw(0, 2, " SERVER BROWSER");
    if (current_user >= 0)
    {
        const char *uname = users[current_user].username;
        mvprintw(0, cols - (int)strlen(uname) - 3, "[%s]", uname);
    }
    attroff(COLOR_PAIR(10) | A_BOLD);

    /* ── Sub-header ── */
    attron(COLOR_PAIR(4));
    mvprintw(2, 3, "Choose a server to join, or press ESC to logout.");
    mvhline(3, 0, ACS_HLINE, cols);
    attroff(COLOR_PAIR(4));

    /* ── Column headers ── */
    attron(COLOR_PAIR(9) | A_BOLD);
    mvprintw(4, 3, "%-3s  %-20s  %-36s  %s", "#", "SERVER", "DESCRIPTION", "MEMBERS");
    attroff(COLOR_PAIR(9) | A_BOLD);
    attron(COLOR_PAIR(4));
    mvhline(5, 0, ACS_HLINE, cols);
    attroff(COLOR_PAIR(4));

    /* ── Server rows ── */
    /* Compute max usable width for description so it never overflows */
    int desc_col = 30;                   /* column where description starts */
    int mbr_col = 68;                    /* column where member count starts */
    int desc_w = mbr_col - desc_col - 2; /* ≈36 chars */
    int mbr_w = cols - mbr_col - 2;      /* remainder */
    if (mbr_w < 10)
        mbr_w = 10;

    for (int i = 0; i < server_count; i++)
    {
        int row = 6 + i * 2;
        UIServer *sv = &servers[i];

        if (i == server_cursor)
        {
            attron(COLOR_PAIR(2) | A_BOLD);
            mvhline(row, 0, ' ', cols);
            mvprintw(row, 3, "%-3d  %-20.*s  %-*.*s  %d members",
                     i + 1,
                     20, sv->name,
                     desc_w, desc_w, sv->description,
                     sv->member_count);
            attroff(COLOR_PAIR(2) | A_BOLD);
        }
        else
        {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(row, 3, "%-3d", i + 1);
            attroff(COLOR_PAIR(5) | A_BOLD);

            attron(COLOR_PAIR(1));
            mvprintw(row, 8, "%-20.*s", 20, sv->name);
            attroff(COLOR_PAIR(1));

            attron(COLOR_PAIR(4));
            mvprintw(row, desc_col, "%-*.*s", desc_w, desc_w, sv->description);
            mvprintw(row, mbr_col, "%d members", sv->member_count);
            attroff(COLOR_PAIR(4));
        }
    }

    /* ── Status bar ── */
    attron(COLOR_PAIR(3));
    mvhline(rows - 1, 0, ' ', cols);
    mvprintw(rows - 1, 2, " UP/DOWN: navigate   ENTER: join   ESC: logout  F1: Command Line");
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
    case 27: /* ESC = logout */
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

// ─── Screen: CHAT ────────────────────────────────────────────────────────────

char chat_input[MAX_MSG_LEN] = {0};
int chat_input_len = 0;

/* Width of the sidebar panel */
#define SIDEBAR_W 20

void draw_chat(int rows, int cols)
{
    erase();
    UIServer *sv = &servers[active_server];

    /* ── Sidebar background (columns 0..SIDEBAR_W-1) ── */
    attron(COLOR_PAIR(3));
    for (int r = 0; r < rows - 1; r++)
        mvhline(r, 0, ' ', SIDEBAR_W);
    attroff(COLOR_PAIR(3));

    /* Server name header */
    attron(COLOR_PAIR(10) | A_BOLD);
    mvhline(0, 0, ' ', SIDEBAR_W);
    mvprintw(0, 1, " %-*.*s", SIDEBAR_W - 3, SIDEBAR_W - 3, sv->name);
    attroff(COLOR_PAIR(10) | A_BOLD);

    /* "CHANNELS" label */
    attron(COLOR_PAIR(4));
    mvprintw(2, 2, "CHANNELS");
    attroff(COLOR_PAIR(4));

    /* Channel list */
    int ch_label_w = SIDEBAR_W - 5; /* room for " # " prefix + border */
    for (int i = 0; i < sv->channel_count; i++)
    {
        int row = 4 + i * 2;
        if (i == active_channel)
        {
            attron(COLOR_PAIR(2) | A_BOLD);
            mvprintw(row, 1, " # %-*.*s", ch_label_w, ch_label_w, sv->channels[i].name);
            attroff(COLOR_PAIR(2) | A_BOLD);
        }
        else
        {
            attron(COLOR_PAIR(3));
            mvprintw(row, 1, " # %-*.*s", ch_label_w, ch_label_w, sv->channels[i].name);
            attroff(COLOR_PAIR(3));
        }
    }

    /* Bottom sidebar buttons */
    attron(COLOR_PAIR(9) | A_BOLD);
    mvprintw(rows - 3, 1, " << Servers");
    attroff(COLOR_PAIR(9) | A_BOLD);

    attron(COLOR_PAIR(8) | A_BOLD);
    mvprintw(rows - 2, 1, " Logout");
    attroff(COLOR_PAIR(8) | A_BOLD);

    /* Sidebar right border */
    attron(COLOR_PAIR(6));
    for (int r = 0; r < rows - 1; r++)
        mvaddch(r, SIDEBAR_W, ACS_VLINE);
    attroff(COLOR_PAIR(6));

    /* ── Top bar (main area) ── */
    int main_x = SIDEBAR_W + 1;
    int main_w = cols - main_x;

    attron(COLOR_PAIR(10) | A_BOLD);
    mvhline(0, main_x, ' ', main_w);
    mvprintw(0, main_x + 1, " # %s", sv->channels[active_channel].name);
    if (current_user >= 0)
    {
        const char *uname = users[current_user].username;
        int ux = cols - (int)strlen(uname) - 2;
        if (ux > main_x + 1)
            mvprintw(0, ux, "%s ", uname);
    }
    attroff(COLOR_PAIR(10) | A_BOLD);

    /* ── Messages area ── */
    UIChannel *chan = &sv->channels[active_channel];

    /* Clear message area */
    attron(COLOR_PAIR(1));
    for (int r = 1; r < rows - 2; r++)
        mvhline(r, main_x, ' ', main_w);
    attroff(COLOR_PAIR(1));

    int msg_area = rows - 4; /* rows available for messages */
    int start = chan->msg_count > msg_area ? chan->msg_count - msg_area : 0;
    int body_col = main_x + 20;       /* column where message body begins */
    int body_w = cols - body_col - 1; /* available width for body text */
    if (body_w < 1)
        body_w = 1;

    for (int i = start; i < chan->msg_count; i++)
    {
        int row = 2 + (i - start);
        Message *m = &chan->messages[i];

        char tstr[8];
        struct tm *tm_info = localtime(&m->ts);
        strftime(tstr, sizeof(tstr), "%H:%M", tm_info);

        attron(COLOR_PAIR(4));
        mvprintw(row, main_x + 1, "%s", tstr);
        attroff(COLOR_PAIR(4));

        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(row, main_x + 7, "%-12.*s", 12, m->sender);
        attroff(COLOR_PAIR(5) | A_BOLD);

        attron(COLOR_PAIR(1));
        mvprintw(row, body_col, "%.*s", body_w, m->body);
        attroff(COLOR_PAIR(1));
    }

    /* ── Input separator ── */
    attron(COLOR_PAIR(6));
    mvhline(rows - 3, main_x, ACS_HLINE, main_w);
    attroff(COLOR_PAIR(6));

    /* ── Input line ── */
    char prompt_buf[MAX_NAME_LEN + 4];
    snprintf(prompt_buf, sizeof(prompt_buf), "[#%s] ", sv->channels[active_channel].name);
    int prompt_len = strlen(prompt_buf);
    int input_col = main_x + 1 + prompt_len;
    int input_w = cols - input_col - 1;
    if (input_w < 1)
        input_w = 1;

    attron(COLOR_PAIR(1));
    mvhline(rows - 2, main_x, ' ', main_w);
    attron(COLOR_PAIR(4));
    mvprintw(rows - 2, main_x + 1, "%s", prompt_buf);
    attroff(COLOR_PAIR(4));
    mvprintw(rows - 2, input_col, "%.*s", input_w, chat_input);
    attroff(COLOR_PAIR(1));

    /* ── Status bar ── */
    attron(COLOR_PAIR(3));
    mvhline(rows - 1, 0, ' ', cols);
    mvprintw(rows - 1, 1, " TAB: channels  ENTER: send  CLICK: channel  q: quit");
    attroff(COLOR_PAIR(3));

    /* Cursor */
    int cx = input_col + chat_input_len;
    if (cx >= cols)
        cx = cols - 1;
    move(rows - 2, cx);
    refresh();
}

void handle_chat_input(int ch)
{
    UIServer *sv = &servers[active_server];

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
            UIChannel *c = &sv->channels[active_channel];
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
            if (me.x < SIDEBAR_W)
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
                int rows;
                getmaxyx(stdscr, rows, (int){0});
                if (me.y == rows - 3)
                {
                    users[current_user].server_id = -1;
                    active_server = -1;
                    server_cursor = 0;
                    current_screen = SCREEN_SERVERS;
                }
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

// ─── Network Integration (Thread-Safe Callbacks) ─────────────────────────────

void ui_handle_auth_response(TizcordPacket *packet) {
    if (packet->payload.auth.action == AUTH_REGISTER || packet->payload.auth.action == AUTH_LOGIN) {
        if (packet->payload.auth.status_code == 0) {
            // Network Register/Login succeeded! 
            // Proceed into the mock local state to trick the UI flow
            strncpy(users[user_count].username, auth.username, MAX_NAME_LEN - 1);
            strncpy(users[user_count].password, auth.password, MAX_PASS_LEN - 1);
            users[user_count].server_id = -1;
            current_user = user_count++;
            
            // Clear inputs and transition screens
            auth.username[0] = '\0';
            auth.password[0] = '\0';
            auth.field = 0;
            current_screen = SCREEN_SERVERS;
            
            strcpy(auth.success, "Authentication Successful!");
            auth.error[0] = '\0';
        } else {
            // Server rejected or network died
            strcpy(auth.error, "Invalid Username or Password.");
            auth.success[0] = '\0';
        }
    }
}

void ui_receive_channel_message(TizcordPacket *packet) {
    // For now, we'll map the incoming message directly to the active server/channel
    // In a full implementation, you'd match packet->payload.channel.server_id
    if (active_server >= 0 && active_server < server_count) {
        UIServer *sv = &servers[active_server];
        
        // Extract channel index safely
        int c_idx = (int)(intptr_t)packet->payload.channel.channel_id;
        
        if (c_idx >= 0 && c_idx < sv->channel_count) {
            UIChannel *c = &sv->channels[c_idx];
            if (c->msg_count < MAX_MESSAGES) {
                Message *m = &c->messages[c->msg_count++];
                snprintf(m->sender, MAX_NAME_LEN, "%lld", (long long)packet->sender_id);
                strncpy(m->body, packet->payload.channel.message, MAX_MSG_LEN - 1);
                m->ts = packet->timestamp;
            }
        }
    }
}

void ui_edit_channel_message(TizcordPacket *packet) {
    // TODO: Iterate through c->messages and string-match the message_id to overwrite body
    (void)packet; 
}

void ui_delete_channel_message(TizcordPacket *packet) {
    // TODO: Iterate through c->messages, find message_id, and shift array left to delete
    (void)packet;
}

void ui_receive_dm_message(TizcordPacket *packet) {
    // TODO: Add logic for a dedicated DM screen
    (void)packet;
}

void ui_update_server_state(TizcordPacket *packet) {
    if (packet == NULL) {
        return;
    }

    if (packet->payload.server.action != SERVER_LIST_JOINED) {
        return;
    }

    if (packet->payload.server.status_code == 0) {
        int incoming_id = (int)packet->payload.server.server_id;

        for (int i = 0; i < server_count; i++) {
            if (servers[i].id == incoming_id) {
                return;
            }
        }

        if (server_count >= UI_MAX_SERVERS) {
            return;
        }

        UIServer *sv = &servers[server_count++];
        memset(sv, 0, sizeof(*sv));
        sv->id = incoming_id;
        strncpy(sv->name, packet->payload.server.server_name, MAX_NAME_LEN - 1);
        strncpy(sv->description, "Joined server", sizeof(sv->description) - 1);
        strncpy(sv->icon, "[]", sizeof(sv->icon) - 1);
    }
}

// ─── Screen: COMMAND LINE ────────────────────────────────────────────────────

void draw_command(int rows, int cols)
{
    erase(); // Ensures nothing is in the background

    int bw = 60; // Box width
    int bh = 3;  // Box height
    int by = (rows - bh) / 2;
    int bx = (cols - bw) / 2;

    if (bx < 0) bx = 0;
    if (by < 0) by = 0;

    // Draw the centered box
    draw_box(by, bx, bh, bw, 5);

    // Draw the label above the box
    attron(COLOR_PAIR(4));
    mvprintw(by - 1, bx, "Command Line (ESC to cancel, ENTER to submit)");
    attroff(COLOR_PAIR(4));

    // Draw the input prompt and current text
    attron(COLOR_PAIR(1));
    mvprintw(by + 1, bx + 2, "> %s", cmd_input);
    attroff(COLOR_PAIR(1));

    // Place the blinking cursor at the end of the input
    move(by + 1, bx + 4 + cmd_input_len);
    refresh();
}

void handle_command_input(int ch)
{
    switch (ch)
    {
        case 27: // ESC key
            // Cancel and return to previous screen
            current_screen = previous_screen;
            break;

        case '\n':
        case KEY_ENTER:
            // TODO: Process the actual command text here if needed!
            
            // Clear the input and return to the previous screen
            cmd_input[0] = '\0';
            cmd_input_len = 0;
            current_screen = previous_screen;
            break;

        case KEY_BACKSPACE:
        case 127:
            // Delete character
            if (cmd_input_len > 0)
                cmd_input[--cmd_input_len] = '\0';
            break;

        default:
            // Type character
            if (ch >= 32 && ch < 127 && cmd_input_len < MAX_MSG_LEN - 1)
            {
                cmd_input[cmd_input_len++] = (char)ch;
                cmd_input[cmd_input_len] = '\0';
            }
            break;
    }
}

// ─── Main Network & UI Loop ──────────────────────────────────────────────────

// Process incoming network data
void process_network_packet(TizcordPacket *packet) {
    switch (packet->type) {
        case CHANNEL:
            if (packet->payload.channel.action == CHANNEL_MESSAGE) {
                ui_receive_channel_message(packet);
            } else if (packet->payload.channel.action == CHANNEL_MESSAGE_EDIT) {
                ui_edit_channel_message(packet);
            } else if (packet->payload.channel.action == CHANNEL_MESSAGE_DELETE) {
                ui_delete_channel_message(packet);
            }
            break;
        case DM:
            if (packet->payload.dm.action == DM_MESSAGE) {
                ui_receive_dm_message(packet);
            }
            break;
        case SERVER:
            ui_update_server_state(packet);
            break;
        case AUTH:
            ui_handle_auth_response(packet);
            break;
        default:
            break;
    }
}

void start_ui(void)
{
    seed_data();

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    curs_set(1);
    init_colors();

    // Make getch() non-blocking so we can drain it cleanly after select() wakes up
    nodelay(stdscr, TRUE); 

    int rows, cols, ch;

    // --- INITIAL DRAW ---
    getmaxyx(stdscr, rows, cols);
    switch (current_screen)
    {
        case SCREEN_LOGIN: draw_auth(rows, cols, 0); break;
        case SCREEN_SIGNUP: draw_auth(rows, cols, 1); break;
        case SCREEN_SERVERS: draw_server_list(rows, cols); break;
        case SCREEN_CHAT: draw_chat(rows, cols); break;
        case SCREEN_COMMAND: draw_command(rows, cols); break; // FIXED warning
    }

    // --- SINGLE THREADED EVENT LOOP ---
    while (1)
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        
        // 1. Always listen to Standard Input (Keyboard/Mouse)
        FD_SET(STDIN_FILENO, &read_fds);
        int max_fd = STDIN_FILENO;

        // 2. Listen to the network socket if connected
        if (client_socket != -1) {
            FD_SET(client_socket, &read_fds);
            if (client_socket > max_fd) {
                max_fd = client_socket;
            }
        }

        // Block until input arrives from EITHER the keyboard OR the network
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            continue; // Handle interrupt signals cleanly
        }

        int needs_redraw = 0; // FIXED scoping issue

        // --- CHECK NETWORK ---
        if (client_socket != -1 && FD_ISSET(client_socket, &read_fds)) {
            TizcordPacket packet;
            int bytes_read = read(client_socket, &packet, sizeof(TizcordPacket));
            
            if (bytes_read > 0) {
                process_network_packet(&packet);
                needs_redraw = 1;
            } else if (bytes_read == 0) {
                // Server disconnected gracefully
                close(client_socket);
                client_socket = -1;
            }
        }

        // --- CHECK USER INPUT ---
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            // Drain all available keys since getch() is non-blocking
            while ((ch = getch()) != ERR) {
                if (ch == 'q' && current_screen == SCREEN_CHAT) {
                    goto exit_ui_loop; // Break out of nested loops cleanly
                }

                // Global F1 intercept to toggle command mode
                if (ch == KEY_F(1) && current_screen != SCREEN_COMMAND) {
                    previous_screen = current_screen;
                    current_screen = SCREEN_COMMAND;
                    cmd_input[0] = '\0';
                    cmd_input_len = 0;
                    needs_redraw = 1;
                    continue;
                }

                // Route input to the active screen
                switch (current_screen)
                {
                    case SCREEN_LOGIN: handle_auth_input(ch, 0); break;
                    case SCREEN_SIGNUP: handle_auth_input(ch, 1); break;
                    case SCREEN_SERVERS: handle_server_input(ch); break;
                    case SCREEN_CHAT: handle_chat_input(ch); break;
                    case SCREEN_COMMAND: handle_command_input(ch); break;
                }
                needs_redraw = 1;
            }
        }

        // --- BATCH REDRAW ---
        if (needs_redraw) {
            getmaxyx(stdscr, rows, cols);
            switch (current_screen)
            {
                case SCREEN_LOGIN: draw_auth(rows, cols, 0); break;
                case SCREEN_SIGNUP: draw_auth(rows, cols, 1); break;
                case SCREEN_SERVERS: draw_server_list(rows, cols); break;
                case SCREEN_CHAT: draw_chat(rows, cols); break;
                case SCREEN_COMMAND: draw_command(rows, cols); break;
            }
        }
    }

exit_ui_loop:
    endwin();
}