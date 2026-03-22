void draw_layout() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // Define dimensions
    int channels_width = 20;
    int input_height = 3;
    int main_height = max_y - input_height;

    // Create windows: newwin(height, width, start_y, start_x)
    channels_win = newwin(main_height, channels_width, 0, 0);
    chat_win = newwin(main_height, max_x - channels_width, 0, channels_width);
    input_win = newwin(input_height, max_x, main_height, 0);

    // Draw borders
    box(channels_win, 0, 0);
    box(chat_win, 0, 0);
    box(input_win, 0, 0);

    // Add some mock titles (mvwprintw prints at a specific y,x relative to the window)
    mvwprintw(channels_win, 1, 2, " CHANNELS ");
    mvwprintw(channels_win, 3, 2, "# general");
    mvwprintw(channels_win, 4, 2, "# development");

    mvwprintw(chat_win, 1, 2, " CHAT HISTORY ");
    mvwprintw(chat_win, max_y - input_height - 2, 2, "[User1]: Hello Tizcord!");

    mvwprintw(input_win, 1, 2, "Message #general: ");

    // Render the windows
    wrefresh(channels_win);
    wrefresh(chat_win);
    wrefresh(input_win);
}

void cleanup_ui() {
    delwin(channels_win);
    delwin(chat_win);
    delwin(input_win);
    endwin();
}

int main() {
    init_ui();
    draw_layout();

    // Move hardware cursor to the input box for typing
    wmove(input_win, 1, 20);
    wrefresh(input_win);

    // Basic loop just to keep the UI open until 'q' is pressed
    int ch;
    while((ch = wgetch(input_win)) != 'q') {
        // Input handling will go here
    }

    cleanup_ui();
    return 0;
}