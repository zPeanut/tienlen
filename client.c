//
// Created by peanut on 12.05.2025
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <ncurses.h>
#include <signal.h>

#include "utils/conn_utils.h"
#include "utils/draw_utils.h"
#include "utils/hands.h"
#include "utils/string_utils.h"

int waiting_player_count = 0;

static volatile int running = 1;

void handle_ctrlc(int signal) {
    running = 0;
}

int main() {
    signal(SIGINT, handle_ctrlc);
    setlocale(LC_ALL, "");

    // ---- BEGIN VARIABLE DECLARATION ----
    int all_players_connected = 0;
    int any_selected = 0;                   // check if any cards are even selected
    int animation_flag = 0;                 // animation_flag = check if animation already has_played
    int choice;                             // choice = key input
    int client_position = 0;                // get current player array position (used for turns and highlight)
    char display[32][MAX_MESSAGE_LENGTH];  // 32 strings with max message length
    int game_start_flag = 1;
    int hand_empty_check = 0;
    int hand_size = HAND_SIZE;              // max win_hand size (always 13, even if fewer than 4 players are connected)
    int hand_type = 0;
    int hand_dirty = 1;
    int has_played = 0;                     // keep track if it's your turn or not
    int has_cleared = 0;                    // check if server window has been cleared
    int has_won_hand = 0;
    int has_won_round = 0;
    int highlight = 0;                      // highlight animation_flag for selected card
    int line_count = 0;                     // line count for window
    int message_dirty = 1;
    char* name;
    int played_hand_size = 0;
    int player_count;
    char players[NUM_PLAYERS][MAX_NAME_LENGTH] = { 0 };
    int received_hand_size = 0;
    static char recv_buffer[4096] = { 0 };  // global buffer to collect data
    static size_t recv_buffer_len = 0;      // track buffer length
    int score[NUM_PLAYERS] = { 0 };         // track score on won round
    int selected_cards[hand_size];          // array of flags - checks if card at index is highlighted to be has_played
    int sock = setup_connection(8, players, &player_count, &name);  // setup client connection to server
    int straight_length = 0;
    int user_list_dirty = 1;
    int total_len;                          // total length of win_hand (used for centering)
    int turn = 0;                           // turn check animation_flag
    int waiting_dots_index = 0;

    Card player_deck[hand_size]; // current hand
    Card played_hand[hand_size]; // hand played on turn
    Card received_hand[HAND_SIZE] = { -1 }; // hand played by others
    memset(played_hand, 0, hand_size * sizeof(Card)); // need to init array to NULL to check if win_server are inside it

    hand_size = HAND_SIZE;
    memset(player_deck, 0, sizeof(player_deck));
    memset(selected_cards, 0, sizeof(selected_cards));
    // ---- END VARIABLE DECLARATION ----


    // ---- BEGIN UI INIT ----
    setup_ncurses_ui();

    int y_max, x_max;
    getmaxyx(stdscr, y_max, x_max);

    int height = y_max - 7;
    int width = x_max - 10;
    int line_x = 3 * (width / 4); // 3/4th of the screen
    WINDOW *win_hand = newwin(0, width, height, 5); // client hand
    WINDOW *win_server = newwin(height, line_x, 0, 5); // server messages
    WINDOW *win_user = newwin(height, width - line_x, 0, line_x + 5); // connected users
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

    draw_user_list(width, height, line_x, player_count, score, name, players, win_user);
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if(strlen(players[i]) > 0) waiting_player_count++;
    }
    all_players_connected = (waiting_player_count == player_count);  // check if with current connection, enough players are connected

    wrefresh(win_user);
    wrefresh(win_hand);
    wrefresh(win_server);
    // ---- END UI INIT ----


    // game loop
    while (running) {

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
                memset(players, 0, sizeof(players));
                if (parse_names(recv_buffer, players)) { // only update if players changed

                    // redraw ONLY the user list window
                    werase(win_user);
                    box(win_user, 0, 0);

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

                user_list_dirty = 1;
            }

            else if (strstr(recv_buffer, "AMOUNT")) {
                char* colon = strchr(recv_buffer, ':');
                if (colon != NULL) {
                    player_count = atoi(colon + 1);
                }
            }

            else if (strstr(recv_buffer, "ERROR")) {
                char* colon = strchr(recv_buffer, ':');
                char error_msg[256];

                if (colon != NULL) {
                    char* msg = colon + 1;
                    snprintf(error_msg, sizeof(error_msg), "%s", msg);
                }

                line_count--;
                add_message(display, error_msg, &line_count, &message_dirty);
                memset(selected_cards, 0, sizeof(selected_cards));
                highlight = 0;
            }

            else if (strstr(recv_buffer, "DEAL")) {
                memset(selected_cards, 0, sizeof(selected_cards)); // reset
                memset(player_deck, 0, sizeof(player_deck)); // reset
                memset(received_hand, 0, sizeof(received_hand));
                received_hand[0].suit = -1;
                has_won_round = 0;

                hand_size = HAND_SIZE;
                char *token = strtok(recv_buffer + 5, ";"); // skip prefix

                for (int i = 0; i < hand_size && token; i++) {
                    int suit, rank;
                    sscanf(token, "%d,%d", &suit, &rank);
                    player_deck[i].suit = (Suit) suit;
                    player_deck[i].rank = (Rank) rank;

                    token = strtok(NULL, ";");
                }

                qsort(player_deck, hand_size, sizeof(Card), compare_by_rank); // sort win_server by rank

                int count = 0;
                for (int i = 0; i < hand_size; i++) {
                    if (player_deck[i].rank == ZWEI) {
                        count++;
                    }
                }

                if (count == 4) {
                    char* msg = int_to_str(client_position);
                    send_message(sock, "INSTANT_WIN", msg);
                    free(msg);
                }

                has_won_hand = 0;
                animation_flag = 0;
                flushinp();

                char thisisdumb[10];
                snprintf(thisisdumb, sizeof(thisisdumb), "%i Deal", client_position);
                send_message(sock, "Received", thisisdumb);
            }

            else if (strstr(recv_buffer, "LOSER")) {
                char* colon = strchr(recv_buffer, ':');
                if (colon != NULL) {
                    int player_who_lost = atoi(colon + 1);
                    char msg[90];
                    if (client_position == player_who_lost) {
                        snprintf(msg, sizeof(msg), "You lost!");
                    } else {
                        snprintf(msg, sizeof(msg), "%s lost and starts the next round.", players[player_who_lost]);
                    }
                    add_message(display, msg, &line_count, &message_dirty);
                }
            }

            else if (strstr(recv_buffer, "INSTANT_WIN")) {
                memset(received_hand, 0, sizeof(received_hand));
                received_hand[0].suit = -1;
                char* colon = strchr(recv_buffer, ':');
                if (colon != NULL) {
                    int player_who_won = atoi(colon + 1);

                    char msg[90];
                    if (client_position == player_who_won) {
                        snprintf(msg, sizeof(msg), "You won this round due to having four 2's on hand!");
                        has_won_round = 1;
                        score[client_position]++;
                    } else {
                        snprintf(msg, sizeof(msg), "%s has won this round due to having four 2's on hand.", players[player_who_won]);
                        has_won_round = 0;
                        score[player_who_won]++;
                    }
                    add_message(display, msg, &line_count, &message_dirty);
                }
            }

            else if (strstr(recv_buffer, "WIN_ROUND")) {
                memset(received_hand, 0, sizeof(received_hand));
                received_hand[0].suit = -1;
                char* colon = strchr(recv_buffer, ':');
                if (colon != NULL) {
                    int player_who_won = atoi(colon + 1);

                    char msg[90];
                    if (client_position == player_who_won) {
                        snprintf(msg, sizeof(msg), "You won this round!");
                        has_won_round = 1;
                        score[client_position]++;
                    } else {
                        snprintf(msg, sizeof(msg), "%s has won this round.", players[player_who_won]);
                        has_won_round = 0;
                        score[player_who_won]++;
                    }
                    add_message(display, msg, &line_count, &message_dirty);
                }
            }

            else if (strstr(recv_buffer, "WIN_HAND")) {
                char* colon = strchr(recv_buffer, ':');
                if (colon != NULL) {
                    int player_who_won = atoi(colon + 1);

                    char msg[60];

                    if (client_position == player_who_won && !has_won_round) {
                        snprintf(msg, sizeof(msg), "You won this hand!");
                        has_won_hand = 1;

                        add_message(display, msg, &line_count, &message_dirty);
                    } else {
                        snprintf(msg, sizeof(msg), "%s has won this hand.", players[player_who_won]);
                        has_won_hand = 0;

                        add_message(display, msg, &line_count, &message_dirty);
                    }

                }
            }

            else if (strstr(recv_buffer, "PLAYED")) {

                // you either played a valid hand OR someone else played a hand

                received_hand_size = 0;
                memset(received_hand, 0, sizeof(received_hand));

                char* colon = strchr(recv_buffer, ':');
                if (colon != NULL) {
                    int player_who_played = atoi(colon + 1);
                    char *cards_start = strchr(colon + 1, ':') + 1;

                    char msg[128] = { 0 };
                    snprintf(msg, sizeof(msg), "%s played: ", players[player_who_played]);

                    char *card = strtok(cards_start, ";");
                    while (card) {
                        int suit, rank;
                        sscanf(card, "%d,%d", &suit, &rank);

                        Card temp_card = { (Suit) suit, (Rank) rank };
                        received_hand[received_hand_size++] = temp_card;

                        char *card_str = return_card(temp_card);
                        strncat(msg, card_str, sizeof(msg) - strlen(msg) - 1);
                        strncat(msg, " ", sizeof(msg) - strlen(msg) - 1);

                        free(card_str);
                        card = strtok(NULL, ";");
                    }

                    hand_type = get_hand_type(received_hand, received_hand_size);
                    if (hand_type == STRASSE || hand_type == ZWEI_PAAR_STRASSE) {
                        straight_length = (hand_type == ZWEI_PAAR_STRASSE) ? received_hand_size / 2 : received_hand_size;
                    }

                    if (player_who_played == client_position) {
                        // if you played the hand, deduct the played cards from your current hand
                        int new_index = 0;
                        for (int i = 0; i < hand_size; i++) {
                            if (!selected_cards[i]) {
                                player_deck[new_index++] = player_deck[i];
                            }
                        }
                        hand_size = new_index;

                        if (hand_size > 0) {
                            if (highlight > hand_size - 1) highlight = 0;
                        } else if (hand_size == 0) {
                            char* won_msg = int_to_str(client_position);
                            send_message(sock, "WIN_ROUND", won_msg);
                            has_won_round = 1;

                            free(won_msg);
                        }
                    }

                    memset(selected_cards, 0, hand_size * sizeof(int));
                    memset(played_hand, 0, hand_size * sizeof(Card));
                    turn = 0;
                    line_count--;
                    add_message(display, msg, &line_count, &message_dirty);
                }
            }

            else if (strstr(recv_buffer, "TURN")) {
                char* colon = strchr(recv_buffer, ':');
                if (colon != NULL) {
                    int player_at_turn = atoi(colon + 1);

                    char msg[40] = { 0 };
                    if (player_at_turn == client_position) {
                        snprintf(msg, sizeof(msg), "Your turn.");
                        turn = 1;
                    } else {
                        snprintf(msg, sizeof(msg), "%s's turn.", players[player_at_turn]);
                        turn = 0;
                    }
                    add_message(display, msg, &line_count, &message_dirty);

                    char thisisdumb[10];
                    snprintf(thisisdumb, sizeof(thisisdumb), "%i Turn", client_position);
                    send_message(sock, "Received", thisisdumb);
                }
            }

            else if (strstr(recv_buffer, "PASS")) {
                char* colon = strchr(recv_buffer, ':');
                if (colon != NULL) {
                    int player_who_passed = atoi(colon + 1);
                    char msg[60];
                    snprintf(msg, sizeof(msg), "%s has passed.", players[player_who_passed]);
                    line_count--;
                    add_message(display, msg, &line_count, &message_dirty);
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
        if (user_list_dirty) {
            draw_user_list(width, height, line_x, player_count, score, name, players, win_user);
            user_list_dirty = 0;
        }

        // waiting room
        if (!all_players_connected) {
            animation_flag = 1;
            if(!has_cleared) {
                memset(display, 0, sizeof(display));
                werase(win_server);
                box(win_server, 0, 0);
                wrefresh(win_server);
                line_count = 0;
                has_cleared = 1;
            }

            game_start_flag = 1;
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
            usleep(1000 * 1000);

            // enable input
            keypad(win_hand, true);
            nodelay(win_hand, true); // make input non-blocking
            animation_flag = 0;
            game_start_flag = 0;

            int sum = 0;
            for (int i = 0; i < hand_size; i++) {
                sum += (int) player_deck[i].rank;
            }

            if (sum == 0) {
                char* msg = int_to_str(client_position);
                send_message(sock, "RESET", msg);
                animation_flag = 1;

                free(msg);
            }
        }

        // -- animation begin --
        if (!animation_flag) {

            int animation_delay = 75 * 1000; // 150ms

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

                usleep(animation_delay);
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

        if (message_dirty) {
            // draw messages
            werase(win_server);  // Clear entire window
            box(win_server, 0, 0);  // Redraw border

            char hand_type_str[50] = {0};
            char* str_straight_length = int_to_str(straight_length);
            char* str_ret_hand_type = return_hand_type(hand_type);

            snprintf(hand_type_str, sizeof(hand_type_str), " %s%s%s ",
                     (hand_type == STRASSE || hand_type == ZWEI_PAAR_STRASSE ? str_straight_length : ""),
                     (hand_type == STRASSE || hand_type == ZWEI_PAAR_STRASSE ? "er " : ""),
                     str_ret_hand_type);

            if (strstr(hand_type_str, "Unbekannt") != NULL) wattron(win_server, COLOR_PAIR(WHITE));
            if (strstr(hand_type_str, "High Card") != NULL) wattron(win_server, COLOR_PAIR(RED));
            if (strstr(hand_type_str, "Paar") != NULL) wattron(win_server, COLOR_PAIR(ORANGE));
            if (strstr(hand_type_str, "Trips") != NULL) wattron(win_server, COLOR_PAIR(YELLOW));
            if (strstr(hand_type_str, "Quads") != NULL) wattron(win_server, COLOR_PAIR(GREEN));
            if (strstr(hand_type_str, "Straße") != NULL) wattron(win_server, COLOR_PAIR(BLUE));
            if (strstr(hand_type_str, "Zwei-Paar Straße") != NULL) wattron(win_server, COLOR_PAIR(PURPLE));

            mvwprintw(win_server, 0, 2, "%s", hand_type_str);

            wattroff(win_server, COLOR_PAIR(WHITE));
            wattroff(win_server, COLOR_PAIR(RED));
            wattroff(win_server, COLOR_PAIR(ORANGE));
            wattroff(win_server, COLOR_PAIR(YELLOW));
            wattroff(win_server, COLOR_PAIR(GREEN));
            wattroff(win_server, COLOR_PAIR(BLUE));
            wattroff(win_server, COLOR_PAIR(PURPLE));

            free(str_ret_hand_type);
            free(str_straight_length);

            for (int i = 0; i < line_count; ++i) {
                int y1 = i * 2 + 2; // +1 offset if box border exists

                char line_buffer[256];
                strncpy(line_buffer, display[i], sizeof(line_buffer) - 1);
                line_buffer[sizeof(line_buffer) - 1] = '\0';

                int current_x = 2;

                char *token = strtok(line_buffer, " ");
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

            wrefresh(win_server);

            message_dirty = 0;
        }
        // --- END UI SECTION ---

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
                            // build played hand string and send to server

                            char played_msg[256] = { 0 };

                            char* str_client = int_to_str(client_position);
                            strncat(played_msg, str_client, sizeof(played_msg) - strlen(played_msg) - 1);
                            free(str_client);

                            strncat(played_msg, ":", 2);

                            for (int i = 0; i < played_hand_size; i++) {
                                int suit = played_hand[i].suit;
                                int rank = played_hand[i].rank;

                                char card_str[10];
                                if (i < played_hand_size - 1) {
                                    snprintf(card_str, sizeof(card_str), "%d,%d;", suit, rank);
                                } else {
                                    snprintf(card_str, sizeof(card_str), "%d,%d", suit, rank);
                                }
                                strncat(played_msg, card_str, sizeof(played_msg) - strlen(played_msg) - 1);
                            }
                            send_message(sock, "PLAYED", played_msg);
                        } else {
                            // PASSED
                            char* buf = int_to_str(client_position);
                            send_message(sock, "PASS", buf);
                            free(buf);

                            line_count--;
                            add_message(display, "You passed.", &line_count, &message_dirty);
                        }
                    }
                    wrefresh(win_user);
                    wrefresh(win_server);
                }
                    break;
                default:
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
