//
// Created by peanut on 12.05.2025
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <sys/queue.h>

#include "utils/conn_utils.h"
#include "utils/queue_utils.h"
#include "utils/cards.h"
#include "utils/string_utils.h"
#include "utils/hands.h"

int client_sockets[NUM_PLAYERS];
int exempt_players[NUM_PLAYERS];
int running = 1;
int server_fd;
int waiting_player_count = 0; // player count in lobby
int player_at_turn = 0;
int round_has_played = 0; // check if any cards have been played in current round

int previous_hand_size = 0;
Card previous_hand[HAND_SIZE] = { 0 };

STAILQ_HEAD(stailq_head, message_entry) message_queue;
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t player_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

void handle_ctrlc(int signal) {
    printf("Closing server...\n");
    close(server_fd);
    exit(0);
}

typedef struct {
    char (*players)[MAX_NAME_LENGTH];
    Card (*hands)[HAND_SIZE];
    int max_players;
} thread_args;

void* io_thread(void* arg) {
    fd_set readfds;
    int max_fd;
    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    int has_dealt = 0;

    thread_args *args = (thread_args *) arg;

    Card (*hands)[HAND_SIZE] = args->hands;
    char (*players)[MAX_NAME_LENGTH] = args->players;
    int player_count = args->max_players;

    while (running) {

        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_fd = server_fd;

        pthread_mutex_lock(&player_lock);
        // add all current sockets to set
        for (int i = 0; i < player_count; i++) {
            if (client_sockets[i] != -1) {
                FD_SET(client_sockets[i], &readfds);
                if (client_sockets[i] > max_fd) max_fd = client_sockets[i];
            }
        }
        pthread_mutex_unlock(&player_lock);

        int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (activity < 0) continue; // server down

        // handle new connections
        if (FD_ISSET(server_fd, &readfds)) {
            struct sockaddr_in address;
            socklen_t address_length = sizeof(address);
            int new_socket = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &address_length);

            char name[30];
            ssize_t b_recv = read(new_socket, name, sizeof(name) - 1);
            if (b_recv <= 0) {
                printf("User disconnected without entering a name.\n");
                close(new_socket);
                continue;
            }
            name[b_recv] = '\0';

            pthread_mutex_lock(&player_lock);

            // check for duplicate names
            int duplicate = 0;
            for (int i = 0; i < player_count; i++) {
                if (client_sockets[i] != -1 && strcmp(players[i], name) == 0) {
                    duplicate = 1;
                    break;
                }
            }

            if (duplicate) {
                send_message(new_socket, "ERROR", "Name is already taken!");
                printf("Duplicate name %s was rejected from connecting.\n", name);
                close(new_socket);
                pthread_mutex_unlock(&player_lock);
                continue;
            }

            // copy entered names into players array
            for (int i = 0; i < player_count; i++) {
                if (client_sockets[i] == -1) {
                    client_sockets[i] = new_socket;
                    memset(players[i], 0, sizeof(players[i]));
                    strncpy(players[i], name, sizeof(name));
                    break;
                }
            }

            waiting_player_count++;
            printf("%s connected. (%d/%d)\n", name, waiting_player_count, player_count);

            // comma seperated player list
            char player_list_cn[256] = { 0 };
            for (int i = 0; i < player_count; i++) {
                if(client_sockets[i] != -1) {
                    strncat(player_list_cn, players[i], strlen(players[i]));
                    if (i < waiting_player_count - 1) strncat(player_list_cn, ",", 2);
                }
            }
            player_list_cn[strlen(player_list_cn)] = '\0';

            // send that to all clients
            for (int i = 0; i < waiting_player_count; i++) {
                if(client_sockets[i] != -1) {
                    printf("Sent to %s: PLAYERS:%s\n", players[i], player_list_cn);
                    send_message(client_sockets[i], "PLAYERS", player_list_cn);
                }
            }

            for (int i = 0; i < waiting_player_count; i++) {
                if (client_sockets[i] != -1) {
                    char* buffer = int_to_str(player_count);
                    printf("Sent to %s: AMOUNT:%s\n", players[i], buffer);
                    send_message(client_sockets[i], "AMOUNT", buffer);

                    free(buffer);
                }
            }

            if (waiting_player_count == player_count) {
                Deck *deck = malloc(sizeof(*deck));
                init_deck(deck);
                shuffle_deck(deck);

                memset(previous_hand, 0, sizeof(previous_hand));
                previous_hand_size = 0;

                int deck_index = 0;
                for (int i = 0; i < player_count; i++) {
                    if (client_sockets[i] == -1) continue;

                    char deal_msg[256] = { 0 };
                    for (int j = 0; j < HAND_SIZE; j++) {
                        hands[i][j] = deck->cards[deck_index++];

                        int suit = hands[i][j].suit;
                        int rank = hands[i][j].rank;
                        char card_str[10];
                        if (j < HAND_SIZE - 1) {
                            snprintf(card_str, sizeof(card_str), "%d,%d;", suit, rank);
                        } else {
                            snprintf(card_str, sizeof(card_str), "%d,%d", suit, rank);
                        }
                        strncat(deal_msg, card_str, sizeof(deal_msg) - strlen(deal_msg) - 1);
                    }

                    printf("Sent to %s: DEAL:%s\n", players[i], deal_msg);
                    send_message(client_sockets[i], "DEAL", deal_msg);
                }

                printf("Game started with all players.\n");
                printf("Cards dealt.\n");
                has_dealt = 1;
                free(deck);
            }

            if (has_dealt) {
                for (int i = 0; i < player_count; i++) {
                    printf("Sent to %s: TURN:%i\n", players[i], 0);
                    send_message(client_sockets[i], "TURN", 0); // let first connected player always begin
                }
                player_at_turn = 0;
                has_dealt = 0;
            }

            pthread_mutex_unlock(&player_lock);
        }

        // check for disconnects
        pthread_mutex_lock(&player_lock);
        for (int i = 0; i < player_count; i++) {

            if (client_sockets[i] != -1 && FD_ISSET(client_sockets[i], &readfds)) {

                char buffer[256];
                ssize_t b_recv = recv(client_sockets[i], &buffer, 1, MSG_PEEK);

                if (b_recv <= 0) {
                    waiting_player_count--;
                    printf("%s disconnected. (%d/%d)\n", players[i], waiting_player_count, player_count);
                    close(client_sockets[i]);
                    client_sockets[i] = -1;

                    memset(players[i], 0, MAX_NAME_LENGTH);       // reset name
                    for (int j = 0; j < HAND_SIZE; j++) {
                        hands[i][j].suit = -1;  // mark as invalid
                        hands[i][j].rank = -1;
                    }

                    // make updated player lists (after disconnects)
                    char player_list_dc[256] = { 0 };
                    for (int j = 0; j < player_count; j++) {
                        if (client_sockets[j] != -1) { // only include connected players
                            strncat(player_list_dc, players[j], strlen(players[j]));
                            strncat(player_list_dc, ",", 4);
                        }
                    }
                    player_list_dc[strlen(player_list_dc)] = '\0';

                    // send updated player list to client (with removed names)
                    for (int j = 0; j < player_count; j++) {
                        if (client_sockets[j] != -1) {
                            printf("Sent to %s: PLAYERS:%s\n", players[j], player_list_dc);
                            send_message(client_sockets[j], "PLAYERS", player_list_dc);
                        }
                    }
                } else {
                    // queue handler
                    ssize_t bytes_read = read(client_sockets[i], buffer, sizeof(buffer) - 1); // -1 because of \0
                    if (bytes_read > 0) {
                        buffer[bytes_read] = '\0';
                        MessageEntry *entry = malloc(sizeof(MessageEntry));
                        entry->message.client_fd = client_sockets[i];
                        strncpy(entry->message.buffer, buffer, sizeof(buffer));

                        pthread_mutex_lock(&queue_lock);
                        STAILQ_INSERT_TAIL(&message_queue, entry, entries);
                        pthread_cond_signal(&queue_cond);
                        pthread_mutex_unlock(&queue_lock);
                    }

                }
            }
        }
        pthread_mutex_unlock(&player_lock);
    }
    return NULL;
}

void get_next_player(int max_players, int* passed_players, int* player_turn, char (*players)[MAX_NAME_LENGTH]) {

    // ---- debug string ----
    char array_msg[40] = { 0 };
    strcat(array_msg, "{ ");
    for (int i = 0; i < max_players; i++) {
        char number[3];
        snprintf(number, 3, "%i ", passed_players[i]);
        strcat(array_msg, number);
    }
    strcat(array_msg, "}");
    printf("%s\n", array_msg);
    // ----------------------

    // select new player, if player is only one who hasnt passed yet, win round for him
    int count_players = 0;
    for (int i = 0; i < max_players; i++) {
        if (passed_players[i] == 0) count_players++; // count players who havent passed
    }

    int winner_index = -1;
    int current_player = *player_turn;
    int next_player = -1;

    for (int i = 1; i <= max_players; i++) {
        int j = (current_player + i) % max_players;
        if (passed_players[j] == 0) { // if player found who hasnt passed, if hes the only one, win round, if not, his turn
            next_player = j;
            break;
        }
    }

    if (count_players == 1) {
        winner_index = next_player;
        *player_turn = next_player; // winner becomes next player for next round
    } else {
        *player_turn = next_player;
    }

    if (winner_index != -1) {
        for (int i = 0; i < max_players; i++) {
            if (client_sockets[i] != -1) {
                char* msg = int_to_str(winner_index);

                printf("Sent to %s: WIN_HAND:%s\n", players[i], msg);
                send_message(client_sockets[i], "WIN_HAND", msg);

                printf("Sent to %s: TURN:%s\n", players[i], msg);
                send_message(client_sockets[i], "TURN", msg);

                free(msg);
            }
        }

        // reset passes on hand win
        for (int i = 0; i < max_players; i++) {
            if (passed_players[i] == 1) {
                passed_players[i] = 0;
            }
        }
    } else {
        for (int i = 0; i < max_players; i++) {
            if (client_sockets[i] != -1) {
                char* msg = int_to_str(*player_turn);
                printf("Sent to %s: TURN:%s\n", players[i], msg);
                send_message(client_sockets[i], "TURN", msg);

                free(msg);
            }
        }
    }
}

int main() {
    int max_players;
    max_players = get_max_players();

    int port;
    port = get_client_port();

    memset(exempt_players, 0, sizeof(exempt_players));

    for (int i = 0; i < max_players; i++) {
        client_sockets[i] = -1;
    }

    struct sockaddr_in address;
    signal(SIGINT, handle_ctrlc);
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    printf("Socket erstellt.\n");

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    printf("Port: %d\n", port);

    int b = bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    if (b != 0) {
        perror("bind");
        return -1;
    }
    listen(server_fd, max_players);
    printf("Warten auf andere Spieler...\n");

    // init io queue
    STAILQ_INIT(&message_queue);

    // init io thread

    char players[max_players][MAX_NAME_LENGTH];
    Card hands[max_players][HAND_SIZE];
    pthread_t thread_io;
    thread_args *args = malloc(sizeof(thread_args));

    args->players = players;
    args->hands = hands;
    args->max_players = max_players;

    if(pthread_create(&thread_io, NULL, io_thread, args) != 0) {
        free(args);
        return 1;
    }

    // game loop

    int passed_players[max_players];
    memset(passed_players, 0, sizeof(passed_players));

    while(running) {
        pthread_mutex_lock(&queue_lock);

        // wait if nothing is being sent
        while (STAILQ_EMPTY(&message_queue)) {
            pthread_cond_wait(&queue_cond, &queue_lock);
        }

        // enqueue message received from client
        MessageEntry *entry;
        while ((entry = STAILQ_FIRST(&message_queue)) != NULL) {
            printf("Received: %s\n", entry->message.buffer);

            // GAME LOGIC HERE
            if (strstr(entry->message.buffer, "PASS")) {
                char* colon = strchr(entry->message.buffer, ':');
                if (colon != NULL) {
                    player_at_turn = atoi(colon + 1);

                    if (!round_has_played) {
                        send_message(client_sockets[player_at_turn], "ERROR", "Cannot pass first. Play a valid hand!");
                    } else {

                        if (!exempt_players[player_at_turn]) {
                            passed_players[player_at_turn] = 1;
                        }

                        for (int i = 0; i < max_players; i++) {
                            if (client_sockets[i] != -1 && i != player_at_turn) {
                                char* msg = int_to_str(player_at_turn);
                                printf("Sent to %s: PASS:%s\n", players[i], msg);
                                send_message(client_sockets[i], "PASS", msg);

                                free(msg);
                            }
                        }

                        get_next_player(max_players, passed_players, &player_at_turn, players);
                    }
                }
            }

            if (strstr(entry->message.buffer, "WIN_ROUND") || strstr(entry->message.buffer, "INSTANT_WIN")) {
                char* colon = strchr(entry->message.buffer, ':');
                if (colon != NULL) {
                    int player_who_won = atoi(colon + 1);

                    exempt_players[player_who_won] = 1;
                    passed_players[player_who_won] = 2;

                    for (int i = 0; i < max_players; i++) {
                        if (passed_players[i] != 2) {
                            passed_players[i] = 0;
                        }
                    }

                    for (int i = 0; i < max_players; i++) {
                        if (client_sockets[i] != -1) {
                            char* msg = int_to_str(player_who_won);

                            if (strstr(entry->message.buffer, "INSTANT_WIN")) {
                                printf("Sent to %s: INSTANT_WIN:%i\n", players[i], player_who_won);
                                send_message(client_sockets[i], "INSTANT_WIN", msg);

                            } else if (strstr(entry->message.buffer, "WIN_ROUND")) {
                                printf("Sent to %s: WIN_ROUND:%i\n", players[i], player_who_won);
                                send_message(client_sockets[i], "WIN_ROUND", msg);
                            }

                            free(msg);
                        }
                    }

                    int active_count = 0;
                    int last_player = -1;

                    for (int i = 0; i < max_players; i++) {
                        if (client_sockets[i] != -1 && !exempt_players[i]) {
                            active_count++;
                            last_player = i;
                        }
                    }

                    if (active_count == 1) {
                        // last player is loser

                        for (int i = 0; i < max_players; i++) {
                            if (client_sockets[i] != -1) {
                                char msg[20];
                                snprintf(msg, sizeof(msg), "%d", last_player);
                                send_message(client_sockets[i], "LOSER", msg);
                            }
                        }

                        memset(exempt_players, 0, sizeof(exempt_players));

                        // deal new cards for the next round
                        pthread_mutex_lock(&player_lock);

                        Deck *deck = malloc(sizeof(*deck));
                        init_deck(deck);
                        shuffle_deck(deck);
                        int deck_index = 0;


                        for (int i = 0; i < max_players; i++) {
                            if (client_sockets[i] == -1) continue;

                            char deal_msg[256] = {0};
                            for (int j = 0; j < HAND_SIZE; j++) {
                                if (deck->cards[deck_index].suit < PIK || deck->cards[deck_index].suit > HERZ) {
                                    printf("Error dealing cards.\n");
                                    exit(1);
                                }
                                hands[i][j] = deck->cards[deck_index++];

                                memset(previous_hand, 0, sizeof(previous_hand));
                                previous_hand_size = 0;

                                char card_str[10];
                                int suit = hands[i][j].suit;
                                int rank = hands[i][j].rank;
                                snprintf(card_str, sizeof(card_str), "%d,%d%c", suit, rank, (j < HAND_SIZE - 1 ? ';' : '\0'));
                                strncat(deal_msg, card_str, sizeof(deal_msg) - strlen(deal_msg) - 1);
                            }
                            send_message(client_sockets[i], "DEAL", deal_msg);
                        }

                        round_has_played = 0;
                        free(deck);

                        // set loser to start next round
                        player_at_turn = last_player;
                        char msg_turn[10];
                        snprintf(msg_turn, sizeof(msg_turn), "%d", player_at_turn);

                        for (int i = 0; i < max_players; i++) {
                            if (client_sockets[i] != -1) {
                                send_message(client_sockets[i], "TURN", msg_turn);
                            }
                        }

                        // reset passed players and set next turn
                        memset(passed_players, 0, sizeof(passed_players));
                        pthread_mutex_unlock(&player_lock);
                    } else {
                        // continue with remaining players
                        int next_turn = -1;
                        for (int i = 1; i <= max_players; i++) {
                            int j = (player_who_won + i) % max_players;
                            if (client_sockets[j] != -1 && !exempt_players[j]) {
                                next_turn = j;
                                break;
                            }
                        }

                        if (next_turn != -1) {
                            player_at_turn = next_turn;
                            char turn_msg[10];
                            snprintf(turn_msg, sizeof(turn_msg), "%d", player_at_turn);
                            for (int i = 0; i < max_players; i++) {
                                if (client_sockets[i] != -1) {
                                    send_message(client_sockets[i], "TURN", turn_msg);
                                }
                            }
                        }

                        // Reset passes for non-exempt players
                        for (int i = 0; i < max_players; i++) {
                            if (client_sockets[i] != -1 && !exempt_players[i]) {
                                passed_players[i] = 0;
                            }
                        }
                    }
                }

            }

            else if(strstr(entry->message.buffer, "RESET")) {
                char* colon = strchr(entry->message.buffer, ':');
                if (colon != NULL) {
                    int player_who_reset = atoi(colon + 1);

                    pthread_mutex_lock(&player_lock);

                    Deck *deck = malloc(sizeof(*deck));
                    init_deck(deck);
                    shuffle_deck(deck);
                    int deck_index = 0;

                    for (int i = 0; i < max_players; i++) {
                        if (client_sockets[i] == -1) continue;

                        char deal_msg[256] = {0};
                        for (int j = 0; j < HAND_SIZE; j++) {
                            if (deck->cards[deck_index].suit < PIK || deck->cards[deck_index].suit > HERZ) {
                                printf("Error dealing cards.\n");
                                exit(1);
                            }
                            hands[i][j] = deck->cards[deck_index++];

                            char card_str[10];
                            int suit = hands[i][j].suit;
                            int rank = hands[i][j].rank;
                            snprintf(card_str, sizeof(card_str), "%d,%d%c", suit, rank, (j < HAND_SIZE - 1 ? ';' : '\0'));
                            strncat(deal_msg, card_str, sizeof(deal_msg) - strlen(deal_msg) - 1);
                        }
                        send_message(client_sockets[i], "DEAL", deal_msg);
                    }

                    round_has_played = 0;
                    free(deck);

                    player_at_turn = 0;
                    char turn_msg[10];
                    snprintf(turn_msg, sizeof(turn_msg), "%d", player_at_turn);
                    send_message(client_sockets[player_who_reset], "TURN", turn_msg);

                    memset(passed_players, 0, sizeof(passed_players));
                    memset(exempt_players, 0, sizeof(exempt_players));
                    pthread_mutex_unlock(&player_lock);
                }
            }

            else if(strstr(entry->message.buffer, "PLAYED")) {
                char msg[256];
                strncpy(msg, entry->message.buffer, 256);
                char* colon = strchr(entry->message.buffer, ':');
                int player_who_played = 0;

                if (colon != NULL) {
                    player_who_played = atoi(colon + 1);
                }

                int sum = 0;
                for (int i = 0; i < HAND_SIZE; i++) {
                    sum += (int) previous_hand[i].rank;
                }

                // parse played deck
                Card played_hand[HAND_SIZE];
                int played_hand_size = 0;

                char *token = strtok(entry->message.buffer + 9, ";"); // skip prefix

                // TODO: refactor this code
                for (int i = 0; token; i++) {
                    int suit, rank;
                    sscanf(token, "%d,%d", &suit, &rank);

                    if (sum == 0) {
                        previous_hand[previous_hand_size].suit = (Suit) suit;
                        previous_hand[previous_hand_size].rank = (Rank) rank;
                        previous_hand_size++;
                    }

                    played_hand[played_hand_size].suit = (Suit) suit;
                    played_hand[played_hand_size].rank = (Rank) rank;
                    played_hand_size++;

                    token = strtok(NULL, ";");
                }

                // check if move is correct, if so, send received message in its entirety back to all clients where its parsed
                // if player is first in turn, let him always lay


                if (get_hand_type(played_hand, played_hand_size) == INVALID) {
                    printf("Sent to %s: ERROR:Invalid hand!\n", players[player_who_played]);
                    send_message(client_sockets[player_who_played], "ERROR", "Invalid hand!");

                } else if (round_has_played == 1 && !is_hand_higher(played_hand, previous_hand, played_hand_size, previous_hand_size) && !compare_hands(played_hand, previous_hand, played_hand_size, previous_hand_size)) {
                    printf("Sent to %s: ERROR:Hand is not higher than previous hand!\n", players[player_who_played]);
                    send_message(client_sockets[player_who_played], "ERROR", "Hand is not higher than previous hand!");

                } else {
                    for (int i = 0; i < max_players; i++) {
                        printf("Sent to %s: %s\n", players[i], msg);
                        send_message(client_sockets[i], "", msg); // send identical message back
                    }

                    round_has_played = 1;
                    get_next_player(max_players, passed_players, &player_at_turn, players);

                    memcpy(previous_hand, played_hand, played_hand_size * sizeof(Card)); // move current hand to previous hand
                    memset(played_hand, 0, sizeof(played_hand)); // reset played hand to prevent having left over data
                }
            }

            // remove from queue
            STAILQ_REMOVE_HEAD(&message_queue, entries);
            free(entry);
        }

        pthread_mutex_unlock(&queue_lock);
    }

    // exit cleanup
    pthread_join(thread_io, NULL);
    free(args);
    close(server_fd);
    return 0;
}
