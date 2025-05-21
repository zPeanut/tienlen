//
// Created by peanut on 12.05.2025
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <ncurses.h>

#include "utils/conn_utils.h"
#include "utils/draw_utils.h"
#include "utils/hands.h"

int waiting_player_count = 0;

char* return_hand_type(int type) {
    char* s = malloc(16);
    const char* rank_names[] = { "Unbekannt", "High Card", "Paar", "Trips", "Quads", "Straße", "Zweier Straße" };
    sprintf(s, "%s", rank_names[type]);
    return s;
}

int get_hand_type(Card *hand, int size) {

    if (size == 1) return HIGH;
    if (size == 2 && hand[0].rank == hand[1].rank) return PAIR;
    if (size == 3 && hand[0].rank == hand[1].rank && hand[1].rank == hand[2].rank) return TRIPS;
    if (size == 4 && hand[0].rank == hand[1].rank && hand[1].rank == hand[2].rank && hand[2].rank == hand[3].rank) return QUADS;

    // straight needs atleast 3 cards
    if (size >= 3) {
        int is_straight = 1;
        for (int i = 0; i < size - 1; i++) {
            if (hand[i].rank + 1 != hand[i+1].rank) {
                is_straight = 0;
                break;
            }
        }
        if (is_straight) return STRASSE;
    }

    // two pair straight needs at least 6 cards and be even
    if (size >= 6 && size % 2 == 0) {
        int valid = 1;
        for (int i = 0; i < size; i += 2) {
            // is the neighboring card the same suit?
            if (hand[i].rank != hand[i+1].rank) {
                valid = 0;
                break;
            }
            if (i < size - 2 && hand[i].rank + 1 != hand[i+2].rank) {
                // is the second next card lesser rank?
                valid = 0;
                break;
            }
        }
        if (valid) return ZWEIER_STRASSE;
    }

    return INVALID;
}

int is_valid_hand(Card* hand, int size) {
    return get_hand_type(hand, size) != INVALID;
}

int main() {
    setlocale(LC_ALL, "");

    // ---- BEGIN VARIABLE DECLARATION ----
    int all_players_connected = 0;
    int any_selected = 0;                   // check if any cards are even selected
    int animation_flag = 0;                 // animation_flag = check if animation already has_played
    int choice;                             // choice = key input
    int client_position = 0;                // get current player array position (used for turns and highlight)
    char display[32][MAX_MESSAGE_LENGTH];  // 32 strings with max message length
    int game_start_flag = 1;
    int hand_size = HAND_SIZE;              // max win_hand size (always 13, even if fewer than 4 players are connected)
    int hand_type = 0;
    int has_played = 0;                     // keep track if it's your turn or not
    int has_cleared = 0;
    int highlight = 0;                      // highlight animation_flag for selected card
    int line_count = 0;                     // line count for window
    char* name;
    int played_hand_size = 0;
    int player_count;
    char players[NUM_PLAYERS][MAX_NAME_LENGTH] = { 0 };
    static char recv_buffer[4096] = { 0 };  // global buffer to collect data
    static size_t recv_buffer_len = 0;      // track buffer length
    int score[NUM_PLAYERS] = { 0 };         // track score on won round
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



    int win_height, win_width;
    getmaxyx(win_hand, win_height, win_width);

    // calculate client position
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (strcmp(players[i], name) == 0) {
            client_position = i;
        }
    }

    // initial rendering of userlist
    int line_x = 3 * (width / 4); // 3/4th of the screen

    draw_user_list(width, height, line_x, player_count, score, name, players, win_user);
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if(strlen(players[i]) > 0) waiting_player_count++;
    }
    all_players_connected = (waiting_player_count == player_count);  // check if with current connection, enough players are connected

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
                        draw_user_list(width, height, line_x, player_count, score, name, players, win_user);
                    }
                }
                all_players_connected = (waiting_player_count == player_count);
                if (!all_players_connected) has_cleared = 0;

                wnoutrefresh(win_user); // queue for refresh
            }

            else if (strstr(recv_buffer, "AMOUNT")) {
                char* colon = strchr(recv_buffer, ':');
                if (colon) {
                    player_count = atoi(colon + 1);
                }
            }

            else if (strstr(recv_buffer, "DEAL")) {
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
                flushinp();
            }

            else if (strstr(recv_buffer, "WIN_HAND")) {
                char* colon = strchr(recv_buffer, ':');
                if (colon) {
                    int player_who_won = atoi(colon + 1);

                    char msg[60];
                    if (client_position == player_who_won) {
                        snprintf(msg, sizeof(msg), "You won this hand!");
                    } else {
                        snprintf(msg, sizeof(msg), "%s has won this hand.", players[player_who_won]);
                    }
                    add_message(display, msg, &line_count);
                }
            }

            else if (strstr(recv_buffer, "PLAYED")) {

                char *colon = strchr(recv_buffer, ':');
                if (colon) {
                    int player_who_played = atoi(colon + 1);
                    char *cards_start = strchr(colon + 1, ':') + 1;

                    char msg[128] = { 0 };
                    snprintf(msg, sizeof(msg), "%s played: ", players[player_who_played]);

                    char *card = strtok(cards_start, ";");
                    Card received_hand[HAND_SIZE] = { 0 };
                    int index = 0;
                    while (card) {
                        int suit, rank;
                        sscanf(card, "%d,%d", &suit, &rank);

                        Card temp_card = { (Suit) suit, (Rank) rank };
                        received_hand[index++] = temp_card;
                        char *card_str = return_card(temp_card);
                        strncat(msg, card_str, sizeof(msg) - strlen(msg) - 1);
                        strncat(msg, " ", sizeof(msg) - strlen(msg) - 1);

                        free(card_str);
                        card = strtok(NULL, ";");
                    }
                    hand_type = get_hand_type(received_hand, index);
                    line_count--;
                    add_message(display, msg, &line_count);
                }
            }

            else if (strstr(recv_buffer, "TURN")) {
                char* colon = strchr(recv_buffer, ':');
                if (colon) {
                    int player_at_turn = atoi(colon + 1);

                    int win_y, win_x;
                    getmaxyx(win_server, win_y, win_x);
                    mvwhline(win_server, 0, 1, ACS_HLINE, win_x - 2);

                    char hand_type_str[50] = {0};
                    snprintf(hand_type_str, sizeof(hand_type_str), " hand type: %s ", return_hand_type(hand_type));
                    mvwprintw(win_server, 0, 2, "%s", hand_type_str);
                    wrefresh(win_server);

                    char msg[40] = { 0 };
                    if (player_at_turn == client_position) {
                        snprintf(msg, sizeof(msg), "Your turn.");
                        turn = 1;
                    } else {
                        snprintf(msg, sizeof(msg), "%s's turn.", players[player_at_turn]);
                    }
                    add_message(display, msg, &line_count);
                }
            }

            else if (strstr(recv_buffer, "PASS")) {
                char* colon = strchr(recv_buffer, ':');
                if (colon) {
                    int player_who_passed = atoi(colon + 1);
                    char msg[60];
                    snprintf(msg, sizeof(msg), "%s has passed.", players[player_who_passed]);
                    line_count--;
                    add_message(display, msg, &line_count);
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
        draw_user_list(width, height, line_x, player_count, score, name, players, win_user);
        mvwprintw(win_server, 0, 2, " hand type: %s ", return_hand_type(hand_type));

        // waiting room
        if (!all_players_connected) {

            if(!has_cleared) {
                memset(display, 0, sizeof(display));
                werase(win_server);
                box(win_server, 0, 0);
                wrefresh(win_server);
                line_count = 0;
                has_cleared = 1;
            }

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

            // enable input
            keypad(win_hand, true);
            nodelay(win_hand, true); // make input non-blocking
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


        // draw messages
        for (int i = 0; i < line_count; ++i) {
            int y1 = i * 2 + 2; // +1 offset if box border exists

            char line_buffer[256];
            strncpy(line_buffer, display[i], sizeof(line_buffer) - 1);
            line_buffer[sizeof(line_buffer) - 1] = '\0';

            int current_x = 2;

            char* token = strtok(line_buffer, " ");
            while (token != NULL) {
                int is_card = 0;
                int color_pair = 0;
                size_t token_len = strlen(token);
                size_t suit_len = strlen(S_PIK);

                // does token end with suit symbol
                if (token_len >= suit_len) {
                    char *suit_pos = token + token_len - suit_len; // point to the last 3 bytes

                    if (memcmp(suit_pos, S_PIK, suit_len) == 0) {
                        color_pair = COLOR_PAIR(WHITE);
                        is_card = 1;
                    } else if (memcmp(suit_pos, S_KREUZ, suit_len) == 0) {
                        color_pair = COLOR_PAIR(BLUE);
                        is_card = 1;
                    } else if (memcmp(suit_pos, S_KARO, suit_len) == 0) {
                        color_pair = COLOR_PAIR(YELLOW);
                        is_card = 1;
                    } else if (memcmp(suit_pos, S_HERZ, suit_len) == 0) {
                        color_pair = COLOR_PAIR(RED);
                        is_card = 1;
                    }
                }

                if (is_card) wattron(win_server, color_pair);
                mvwprintw(win_server, y1, current_x, "%s", token);
                wattroff(win_server, color_pair);

                current_x += strlen(token);

                char *next_token = strtok(NULL, " ");
                if (next_token != NULL) {
                    mvwprintw(win_server, y1, current_x, " ");
                    current_x++;
                }

                token = next_token;
            }
        }
        // --- END UI SECTION ---


        // --- BEGIN GAME LOGIC ---

        if (turn) {
            if (has_played) {
                char display_msg[70] = { 0 };

                if (any_selected) {
                    mvwhline(win_server, 4, 2, ' ', line_x - 2);
                    char* display_msg_1 = "You played: ";
                    strncat(display_msg, display_msg_1, strlen(display_msg_1));

                    for (int i = 0; i < played_hand_size; i++) {
                        char *msg = return_card(played_hand[i]);

                        strncat(display_msg, msg, strlen(msg));
                        strncat(display_msg, " ", strlen(" ") + 1);

                        free(msg);
                    }
                    display_msg[strlen(display_msg)] = '\0';
                    line_count--; // replace "your turn." message
                    add_message(display, display_msg, &line_count);
                } else {
                    line_count--;
                    add_message(display, "You passed.", &line_count);
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
                        played_hand_size = 0;
                        for (int i = 0; i < hand_size; i++) {
                            if (selected_cards[i]) {
                                any_selected = 1;
                                played_hand[played_hand_size++] = player_deck[i];
                            }
                        }

                        if (any_selected) {
                            if (is_valid_hand(played_hand, played_hand_size)) {
                                // VALID HAND
                                for (int i = 0; i < hand_size; i++) {
                                    if (!selected_cards[i]) {
                                        player_deck[new_index++] = player_deck[i];
                                    }
                                }
                                hand_size = new_index; // Update hand size

                                // build played hand string and send to server
                                char played_msg[50] = {0};
                                char buf[10] = {0};

                                // add player number to message
                                // sending something like "PLAYED:1:2,5;2,6;2,7"
                                snprintf(buf, sizeof(buf), "%i:", client_position);
                                strncat(played_msg, buf, sizeof(played_msg) - strlen(played_msg) - 1);
                                for (int i = 0; i < played_hand_size; i++) {
                                    Suit s = played_hand[i].suit;
                                    Rank r = played_hand[i].rank;
                                    char card_str[10];
                                    if (i < played_hand_size - 1) {
                                        snprintf(card_str, sizeof(card_str), "%d,%d;", s, r);
                                    } else {
                                        snprintf(card_str, sizeof(card_str), "%d,%d", s, r);
                                    }
                                    strncat(played_msg, card_str, sizeof(played_msg) - strlen(played_msg) - 1);
                                }
                                send_message(sock, "PLAYED", played_msg);

                                hand_type = get_hand_type(played_hand, played_hand_size);
                                memset(selected_cards, 0, hand_size * sizeof(int));
                                if (highlight > hand_size) highlight = hand_size - 1;
                                has_played = 1;
                            } else {
                                // INVALID HAND
                                line_count--;
                                add_message(display, "Invalid hand!", &line_count);
                                memset(selected_cards, 0, hand_size * sizeof(int));
                            }
                        } else {
                            // PASSED
                            char buf[10] = {0};
                            snprintf(buf, sizeof(buf), "%i", client_position);
                            send_message(sock, "PASS", buf);
                            has_played = 1;
                        }

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
