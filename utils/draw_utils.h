//
// Created by must9 on 21.05.2025
//

#ifndef TIENLEN_DRAWUTILS_H
#define TIENLEN_DRAWUTILS_H

#include "macros.h"
#include "cards.h"

int setup_ncurses_ui();
void draw_user_list(int width, int height, int line_x, int player_count, int* score, char* name, char (*players)[MAX_NAME_LENGTH], WINDOW *win);
void draw_hand(WINDOW *win, int y, int x, int loop_limit, Card *player_deck, int highlight, const int *selected_cards);
void add_message(char (*messages)[MAX_MESSAGE_LENGTH], char* buf, int *line_count, int *dirty_flag);

#endif //TIENLEN_DRAWUTILS_H
