//
// Created by peanut on 21.05.2025
//


#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

#include "cards.h"

void draw_hand(WINDOW *win, int y, int x, int loop_limit, Card *player_deck, int highlight, const int *selected_cards) {
    for (int i = 0; i < loop_limit; i++) {

        char* s = return_card(player_deck[i]);
        if (strstr(s, S_PIK) != NULL) wattron(win, COLOR_PAIR(WHITE));
        if (strstr(s, S_KREUZ) != NULL) wattron(win, COLOR_PAIR(BLUE));
        if (strstr(s, S_KARO) != NULL) wattron(win, COLOR_PAIR(YELLOW));
        if (strstr(s, S_HERZ) != NULL) wattron(win, COLOR_PAIR(RED));
        if (i == highlight) wattron(win, A_UNDERLINE);
        if (selected_cards[i]) wattron(win, (A_BLINK | A_REVERSE));

        mvwprintw(win, y, x, "%s", s);

        wattroff(win, A_REVERSE);
        wattroff(win, A_UNDERLINE);
        wattroff(win, A_BLINK);
        wattroff(win, COLOR_PAIR(WHITE));
        wattroff(win, COLOR_PAIR(BLUE));
        wattroff(win, COLOR_PAIR(YELLOW));
        wattroff(win, COLOR_PAIR(RED));

        x += (int) strlen(s) + 2; // add 2 spaces between cards
        free(s);
    }
}

void draw_user_list(int width, int height, int line_x, int player_count, char* name, char (*players)[MAX_NAME_LENGTH], WINDOW *win) {
    mvwprintw(win, 2, line_x + 2, "Connected Users:");

    // draw vertical line for connected users
    for (int i = 1; i < height - 1; i++) {
        mvwaddch(win, i, line_x, ACS_VLINE);
    }

    // draw underline
    for (int i = line_x + 2; i < width - 2; i++) {
        mvwaddch(win, 3, i, ACS_HLINE);
    }

    // draw user strings
    for (int i = 0; i < player_count; i++) {
        if (strlen(players[i]) > 0) {
            if (strcmp(players[i], name) == 0) wattron(win, COLOR_PAIR(CYAN));
            mvwprintw(win, 5 + i * 2, line_x + 2, "%s", players[i]);
            wattroff(win, COLOR_PAIR(CYAN));
        }
    }
}

int setup_ncurses_ui() {
    initscr();
    start_color();
    noecho();
    cbreak();
    curs_set(0);

    if (!has_colors()) {
        printw("Your terminal doesnt support colors! Session terminated.\n");
        printw("Press any key to continue...");
        getch();
        endwin();
        return 1;
    }

    // color initialization
    init_pair(WHITE, COLOR_WHITE, COLOR_BLACK);     // spades
    init_pair(BLUE, COLOR_CYAN, COLOR_BLACK);       // clubs
    init_pair(YELLOW, COLOR_YELLOW, COLOR_BLACK);   // diamonds
    init_pair(RED, COLOR_RED, COLOR_BLACK);         // hearts
    init_pair(CYAN, 14, COLOR_BLACK);       // connected user

    return 0;
}
