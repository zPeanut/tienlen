//
// Created by peanut on 12.05.2025
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <ncurses.h>

#include "utils/connutils.h"

int waiting_player_count = 0;

void draw_hand(WINDOW *win, int y, int x, int loop_limit, Card *player_deck, int highlight, const int *selected_cards) {
    for (int i = 0; i < loop_limit; i++) {

        char* s = return_card(player_deck[i]);
        if (strstr(s, PIK) != NULL) wattron(win, COLOR_PAIR(WHITE));
        if (strstr(s, KREUZ) != NULL) wattron(win, COLOR_PAIR(BLUE));
        if (strstr(s, KARO) != NULL) wattron(win, COLOR_PAIR(YELLOW));
        if (strstr(s, HERZ) != NULL) wattron(win, COLOR_PAIR(RED));
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

void draw_players(char players[][MAX_NAME_LENGTH], char* client_name, int index, int line_x, WINDOW *win) {
    if (strcmp(players[index], client_name) == 0) wattron(win, COLOR_PAIR(CYAN));
    mvwprintw(win, 5 + index * 2, line_x + 2, "%s", players[index]);
    wattroff(win, COLOR_PAIR(CYAN));
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


int main() {
    setlocale(LC_ALL, "");

    // ---- BEGIN VARIABLE DECLARATION ----
    char players[NUM_PLAYERS][MAX_NAME_LENGTH] = { 0 };
    int all_players_connected = 0;
    int any_selected = 0;           // check if any cards are even selected
    int animation_flag = 0;         // animation_flag = check if animation already has_played
    int choice;                     // choice = key input
    int client_position = 0;        // get current player array position (used for turns and highlight)
    int game_start_flag = 1;
    int hand_size = HAND_SIZE;      // max win_hand size (always 13, even if fewer than 4 players are connected)
    int has_played = 0;             // keep track if it's your turn or not
    int highlight = 0;              // highlight animation_flag for selected card
    char* name;
    int played_hand_size = 0;
    int player_count;
    static char recv_buffer[4096] = { 0 };  // global buffer to collect data
    static size_t recv_buffer_len = 0;      // track buffer length
    int selected_cards[hand_size];          // array of flags - checks if card at index is highlighted to be has_played
    int sock = setup_connection(8, players, &player_count, &name);  // setup client connection to server
    int total_len;                          // total length of win_hand (used for centering)
    int turn = 0;                           // turn check animation_flag
    int waiting_dots_index = 0;

    Card player_deck[hand_size]; // current win_hand
    Card played_hand[hand_size]; // has_played win_hand (on turn)
    memset(played_hand, 0, hand_size * sizeof(int)); // need to init array to NULL to check if win_server are inside it
    // ---- END VARIABLE DECLARATION ----


    // ---- BEGIN UI INIT ----
    setup_ncurses_ui();

    int y_max, x_max;
    getmaxyx(stdscr, y_max, x_max);

    int height = y_max - 7;
    int width = x_max - 10;
    WINDOW *win_hand = newwin(0, width, height, 5); // client hand
    WINDOW *win_server = newwin(height, width, 0, 5); // server messages
    WINDOW *win_user = newwin(height, width, 0, 5); // connected users
    box(win_hand, 0, 0);
    box(win_server, 0, 0);
    box(win_user, 0, 0);

    keypad(win_hand, true);
    nodelay(win_hand, true); // make input non-blocking

    int win_height, win_width;
    getmaxyx(win_hand, win_height, win_width);

    // calculate client position
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (strcmp(players[i], name) == 0) {
            client_position = i;
            break;
        }
    }

    // initial rendering of userlist
    int line_x = 3 * (width / 4); // 3/4th of the screen
    mvwprintw(win_user, 2, line_x + 2, "Connected Users: %s", name);

    for (int i = 1; i < height - 1; i++) {
        mvwaddch(win_user, i, line_x, ACS_VLINE); // draw vertical line for connected users
    }

    for (int i = line_x + 2; i < width - 2; i++) {
        mvwaddch(win_user, 3, i, ACS_HLINE); // draw underline
    }
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if(strlen(players[i]) > 0) {
            draw_players(&players[i], name, i, line_x, win_user);
            waiting_player_count++;
        }
        all_players_connected = (waiting_player_count == player_count);  // check if with current connection, enough players are connected
    }

    wrefresh(win_user);
    wrefresh(win_hand);
    // ---- END UI INIT ----


    // game loop
    while (1) {

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds); // monitor socket

        // --- BEGIN RECEIVING DATA ---
        char buffer[256];
        ssize_t recv_loop = read(sock, buffer, sizeof(buffer) - 1);
        if (recv_loop > 1) {
            recv_buffer[recv_buffer_len] = '\0';

            memcpy(recv_buffer + recv_buffer_len, buffer, recv_loop); // append to global buffer
            recv_buffer_len += recv_loop;

        } else if (recv_loop == 0) {
            goto end;
        }

        char *parsed_message_end;
        while ((parsed_message_end = strchr(recv_buffer, '\n')) != NULL) { // '\n' is server delimiter between message types

            *parsed_message_end = '\0';

            // individual message parsing
            if (strstr(recv_buffer, "PLAYERS")) {
                if (parse_names(recv_buffer, players)) { // only update if players changed

                    waiting_player_count = 0;
                    for (int i = 0; i < player_count; i++) {
                        if (strlen(players[i]) > 0) waiting_player_count++; // calculate new player count
                    }
                }

                for (int i = line_x + 2; i < width - 2; i++) {
                    for (int j = 0; j < player_count; j++) {
                        mvwaddch(win_user, 5 + j * 2, i, ' '); // clear old users
                        draw_players(players, name, i, line_x, win_user);
                    }
                }
                all_players_connected = (waiting_player_count == player_count);
                wnoutrefresh(win_user); // queue for refresh

            } else if (strstr(recv_buffer, "DEAL")) {
                memset(selected_cards, 0, sizeof(selected_cards)); // reset
                char *token = strtok(recv_buffer + 5, ";"); // skip prefix

                for (int i = 0; i < hand_size && token; i++) {
                    int suit, rank;
                    sscanf(token, "%d,%d", &suit, &rank);
                    player_deck[i].suit = (Suit) suit;
                    player_deck[i].rank = (Rank) rank;

                    token = strtok(NULL, ";");
                }
                qsort(player_deck, hand_size, sizeof(Card), compare_by_rank); // sort win_server by rank
                animation_flag = 0; // enable animation
            }

            else if (strstr(recv_buffer, "AMOUNT")) {
                char* colon = strchr(recv_buffer, ':');
                if (colon) {
                    player_count = atoi(colon + 1);
                }
            }

            else if (strstr(recv_buffer, "TURN")) {
                char* colon = strchr(recv_buffer, ':');
                if (colon) {
                    int player_at_turn = atoi(colon + 1);
                    mvwprintw(win_server, 2, 2, "client position: %i", client_position);
                    wrefresh(win_server);

                    if (player_at_turn == client_position) {
                        turn = 1;
                    }
                }
            }

            // remove parsed message from buffer
            size_t remaining = recv_buffer_len - (parsed_message_end - recv_buffer + 1);
            memmove(recv_buffer, parsed_message_end + 1, remaining);
            recv_buffer_len = remaining;
            recv_buffer[recv_buffer_len] = '\0';
        }
        // --- END RECEIVING DATA ---


        // --- BEGIN UI SECTION ---
        int x;
        int y = win_height / 2;

        // win_user loop
        mvwprintw(win_user, 2, line_x + 2, "Connected Users:");
        for (int i = 1; i < height - 1; i++) {
            mvwaddch(win_user, i, line_x, ACS_VLINE); // draw vertical line for connected users
        }

        for (int i = line_x + 2; i < width - 2; i++) {
            mvwaddch(win_user, 3, i, ACS_HLINE); // draw underline
        }

        for (int i = 0; i < player_count; i++) {
            if (strlen(players[i]) > 0) {
                draw_players(players, name, i, line_x, win_user);
            }
        }

        // waiting room
        if (!all_players_connected) {
            game_start_flag = 1;
            animation_flag = 0; // ensure animation plays again when reconnected
            char* waiting_msg = " Waiting for players to connect";
            char* dots[] = { " ", ". ", ".. ", "... "};
            int num_frames = 4;

            char full_msg[50];
            snprintf(full_msg, sizeof(full_msg), "%s%s", waiting_msg, dots[waiting_dots_index % num_frames]);

            mvwhline(win_hand, win_height / 2, 2, ' ', win_width - 10);
            mvwprintw(win_hand, win_height / 2, (int) (win_width - strlen(full_msg) + (waiting_dots_index % num_frames) - 1) / 2, "%s", full_msg);
            mvwprintw(win_hand, win_height / 2 + 1, (win_width - 8) / 2, "(%d/%d)", waiting_player_count, player_count);

            waiting_dots_index++;
            wrefresh(win_hand);
            wrefresh(win_user);
            napms(1000); // add 100ms delay to reduce cpu usage
            doupdate();
            continue;
        }

        if (game_start_flag) {
            wrefresh(win_user);

            werase(win_hand);
            box(win_hand, 0, 0);

            char* game_start_msg = "All players connected. Game start!";
            mvwprintw(win_hand, win_height / 2, (int) (win_width - strlen(game_start_msg)) / 2, "%s", game_start_msg);
            wrefresh(win_hand);
            // usleep(2000 * 1000); TODO: add this back on prod
            game_start_flag = 0;
        }

        // -- animation begin --
        if (!animation_flag) {
            werase(win_hand);
            box(win_hand, 0, 0);

            for (int i = 0; i < hand_size; i++) {

                // calculate total length for centering
                total_len = 0;
                for (int j = 0; j <= i; j++) {
                    char* s = return_card(player_deck[j]);
                    total_len += (int) strlen(s);
                    if (j < i) total_len += 2;
                    free(s);
                }

                x = (win_width - total_len) / 2;

                mvwhline(win_hand, y, 2, ' ', win_width - 10);
                draw_hand(win_hand, y, x, i + 1, player_deck, highlight, selected_cards);

                wrefresh(win_hand);
                wrefresh(win_server);
                wrefresh(win_user);
                usleep(100 * 1000);
            }
            animation_flag = 1;
        }
        // -- animation end --

        mvwhline(win_hand, y, 2, ' ', win_width - 10);

        // calculate total length again
        total_len = 0;
        for (int j = 0; j < hand_size; j++) {
            char* s = return_card(player_deck[j]);
            total_len += (int) strlen(s);
            if (j < hand_size - 1) total_len += 2;
            free(s);
        }

        x = (win_width - total_len) / 2;
        draw_hand(win_hand, y, x, hand_size, player_deck, highlight, selected_cards);
        // --- END UI SECTION ---


        // --- BEGIN GAME LOGIC ---

        if (turn) {
            if (!has_played) {
                mvwprintw(win_server, 4, 2, "Your turn.");
            } else {
                int x_pos = 14; // starting x pos after "you has_played: "

                if (any_selected) {
                    mvwhline(win_server, 4, 2, ' ', line_x - 2);
                    mvwprintw(win_server, 4, 2, "You played: ");
                    wrefresh(win_server);
                    for (int i = 0; i < played_hand_size; i++) {
                        char *msg = return_card(played_hand[i]);

                        if (strstr(msg, PIK)) wattron(win_server, COLOR_PAIR(WHITE));
                        else if (strstr(msg, KREUZ)) wattron(win_server, COLOR_PAIR(BLUE));
                        else if (strstr(msg, KARO)) wattron(win_server, COLOR_PAIR(YELLOW));
                        else if (strstr(msg, HERZ)) wattron(win_server, COLOR_PAIR(RED));

                        mvwprintw(win_server, 4, x_pos, "%s", msg);
                        x_pos += (int) strlen(msg);

                        wattroff(win_server, COLOR_PAIR(WHITE));
                        wattroff(win_server, COLOR_PAIR(BLUE));
                        wattroff(win_server, COLOR_PAIR(YELLOW));
                        wattroff(win_server, COLOR_PAIR(RED));
                        free(msg);
                    }
                } else {
                    mvwprintw(win_server, 4, 2, "You passed.");
                    char buf[10];
                    snprintf(buf, strlen(buf), "%i", client_position);
                    send_message(sock, "PASS", buf);
                }

                memset(played_hand, 0, hand_size * sizeof(int));
                played_hand_size = 0;
                has_played = 0;
                any_selected = 0;
                turn = 0;
            }
        }

        wrefresh(win_server);
        // --- END GAME LOGIC ---


        // --- PARSE INPUT ---
        choice = wgetch(win_hand);
        if (choice != ERR) {
            switch (choice) {

                case KEY_LEFT:
                    highlight--;
                    if (highlight == -1) highlight = hand_size - 1;
                    break;

                case KEY_RIGHT:
                    highlight++;
                    if (highlight == hand_size) highlight = 0;
                    break;

                case 10: // enter
                case KEY_UP:
                    selected_cards[highlight] = !selected_cards[highlight];
                    break;

                case 32: { // space
                    if (turn) {
                        int new_index = 0;
                        any_selected = 0;

                        for (int i = 0; i < hand_size; i++) {
                            if (selected_cards[i]) {
                                any_selected = 1;
                                played_hand[played_hand_size++] = player_deck[i];
                            } else {
                                player_deck[new_index++] = player_deck[i];
                            }
                            selected_cards[i] = 0;
                        }
                        has_played = 1;
                        hand_size = new_index;
                        if (highlight > new_index) highlight = new_index - 1;
                        memset(selected_cards, 0, hand_size * sizeof(int));
                    }
                }
                    break;
                default:
                    goto end;
            }
        }
        doupdate();
    }

    end:
    // exit program
    move(0,0);
    endwin();
    close(sock);
    free(name);
    printf("Server closed!\n");
    return 0;
}
