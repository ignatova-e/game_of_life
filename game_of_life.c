#define _POSIX_C_SOURCE 200809L

#include <ncurses.h>
#include <unistd.h>

#define WIDTH 80
#define HEIGHT 25
#define STATUS_Y 0
#define FIELD_Y 1
#define MIN_LINES 26
#define MIN_COLS 80
#define LIVE 'O'

void clear_board(int board[HEIGHT][WIDTH]) {
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            board[y][x] = 0;
        }
    }
}

int is_alive_char(int c) { return (c == LIVE); }

void load_from_stdin(int board[HEIGHT][WIDTH]) {
    char line[128];

    for (int y = 0; y < HEIGHT && fgets(line, sizeof(line), stdin); ++y) {
        for (int x = 0; x < WIDTH && line[x] != '\0' && line[x] != '\n'; ++x) {
            board[y][x] = is_alive_char(line[x]);
        }
    }
}

int count_neighbors(int board[HEIGHT][WIDTH], int y, int x) {
    int c = 0;

    for (int delta_y = -1; delta_y <= 1; ++delta_y) {
        for (int delta_x = -1; delta_x <= 1; ++delta_x) {
            if (!(delta_x == 0 && delta_y == 0)) {
                int new_y = (y + delta_y + HEIGHT) % HEIGHT;
                int new_x = (x + delta_x + WIDTH) % WIDTH;
                c += board[new_y][new_x];
            }
        }
    }

    return c;
}

void step_board(int cur[HEIGHT][WIDTH], int next[HEIGHT][WIDTH]) {
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            int n = count_neighbors(cur, y, x);
            next[y][x] = (cur[y][x] && (n == 2 || n == 3)) || (!cur[y][x] && n == 3);
        }
    }

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            cur[y][x] = next[y][x];
        }
    }
}

void draw_status(int delay_ms) {
    char status[96];

    snprintf(status, sizeof(status), "A: faster  Z: slower  Space: quit   delay: %4d ms", delay_ms);
    mvaddnstr(STATUS_Y, 0, status, (int)sizeof(status) - 1);
    clrtoeol();
}

void draw_board(int b[HEIGHT][WIDTH]) {
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            mvaddch(FIELD_Y + y, x, b[y][x] ? LIVE : ' ');
        }
    }
}

void update_delay(int ch, int *delay_ms) {
    if (ch == 'a' || ch == 'A') {
        if (*delay_ms > 20) {
            *delay_ms -= 10;
        }
    } else if (ch == 'z' || ch == 'Z') {
        if (*delay_ms < 1000) {
            *delay_ms += 10;
        }
    }
}

void process_input(int ch, int *quit, int *delay_ms) {
    if (ch == ' ') {
        *quit = 1;
    } else {
        update_delay(ch, delay_ms);
    }
}

void process_all_input(int *quit, int *delay_ms) {
    int stop = 0;
    while (!stop && !(*quit)) {
        int ch = getch();
        if (ch != ERR) {
            process_input(ch, quit, delay_ms);
        } else {
            stop = 1;
        }
    }
}

int check_screen() {
    int rows;
    int cols;

    getmaxyx(stdscr, rows, cols);
    int result = (rows >= MIN_LINES && cols >= MIN_COLS);
    return result;
}

void attach_tty() {
    FILE *tty_in;
    FILE *tty_out;

    tty_in = fopen("/dev/tty", "r");
    tty_out = fopen("/dev/tty", "w");

    if (tty_in) {
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {
        }

        if (dup2(fileno(tty_in), STDIN_FILENO) != -1) {
            setvbuf(stdin, NULL, _IONBF, 0);
        }
    }

    if (tty_out) {
        fflush(stdout);
        fflush(stderr);

        if (dup2(fileno(tty_out), fileno(stdout)) != -1) {
            setvbuf(stdout, NULL, _IONBF, 0);
        }
        if (dup2(fileno(tty_out), fileno(stderr)) != -1) {
            setvbuf(stderr, NULL, _IONBF, 0);
        }
    }

    if (tty_in) {
        fclose(tty_in);
    }

    if (tty_out) {
        fclose(tty_out);
    }
}

void run_game_loop(int cur[HEIGHT][WIDTH], int next[HEIGHT][WIDTH], int delay_ms) {
    int quit = 0;
    int current_delay = delay_ms;

    while (!quit) {
        erase();
        draw_status(current_delay);
        draw_board(cur);
        refresh();

        process_all_input(&quit, &current_delay);

        if (!quit) {
            step_board(cur, next);
            napms(current_delay);
        }
    }
}

SCREEN *init_ncurses() {
    SCREEN *screen = newterm(NULL, stdout, stdin);

    if (screen) {
        set_term(screen);
        noecho();
        cbreak();
        curs_set(0);
        keypad(stdscr, TRUE);
        nodelay(stdscr, TRUE);

        if (!check_screen()) {
            endwin();
            delscreen(screen);
            screen = NULL;
        }
    }

    return screen;
}

void full_ncurses_cleanup(SCREEN *screen) {
    if (screen) {
        endwin();
        delscreen(screen);
    }
}

int main() {
    int cur[HEIGHT][WIDTH];
    int next[HEIGHT][WIDTH];
    int exit_code;

    clear_board(cur);
    clear_board(next);
    load_from_stdin(cur);
    attach_tty();

    SCREEN *screen = init_ncurses();

    if (!screen) {
        exit_code = 1;
    } else {
        run_game_loop(cur, next, 150);
        exit_code = 0;
    }

    full_ncurses_cleanup(screen);
    return exit_code;
}