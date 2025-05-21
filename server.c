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

#include "utils/connutils.h"

int client_sockets[NUM_PLAYERS];
int running = 1;
int server_fd;
int waiting_player_count = 0;

typedef struct {
    int client_fd;
    char buffer[256];
} Message;

typedef struct message_entry {
    Message message;
    STAILQ_ENTRY(message_entry) entries;
} MessageEntry;

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

void *io_thread(void* arg) {
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
                // disconnect before entering name -> TODO: handle with message maybe?
                close(new_socket);
                continue;
            }
            name[b_recv] = '\0';


            pthread_mutex_lock(&player_lock);
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
                    char buffer[4];
                    snprintf(buffer, 4, "%i", player_count);

                    printf("Sent to %s: AMOUNT:%s\n", players[i], buffer);
                    send_message(client_sockets[i], "AMOUNT", buffer);
                }
            }

            if (waiting_player_count == player_count) {
                Deck *deck = malloc(sizeof(*deck));
                init_deck(deck);
                shuffle_deck(deck);

                int deck_index = 0;
                for (int i = 0; i < player_count; i++) {
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
                    printf("Sent to %s: TURN:%s\n", players[i], "0");
                    send_message(client_sockets[i], "TURN", "0");
                }
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

                    // make updated player lists (after disconnects)
                    char player_list_dc[256] = { 0 };
                    for (int j = 0; j < player_count; j++) {
                        if (client_sockets[j] != -1) { // only include connected players
                            strncat(player_list_dc, players[j], strlen(players[j]));
                            if (j < waiting_player_count - 1) strncat(player_list_dc, ",", 2);
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


int main() {
    int max_players;
    max_players = get_max_players();

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
    address.sin_port = htons(DEFAULT_PORT);
    printf("Port: %d\n", DEFAULT_PORT);

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

        if (waiting_player_count < max_players) {
            printf("Server closed!\n");
            pthread_join(thread_io, NULL);
            close(server_fd);
            return 0;
        }

        // enqueue message received from client
        MessageEntry *entry;
        while ((entry = STAILQ_FIRST(&message_queue)) != NULL) {
            printf("Received: %s\n", entry->message.buffer);

            if (strstr(entry->message.buffer, "PASS")) {
                char* colon = strchr(entry->message.buffer, ':');
                if (colon) {
                    int player_at_turn = atoi(colon + 1);

                    passed_players[player_at_turn] = 1;

                    // ---- debug string ----
                    char array_msg[40];
                    strcat(array_msg, "{ ");
                    for (int i = 0; i < max_players; i++) {
                        char number[3];
                        snprintf(number, 3, "%i ", passed_players[i]);
                        strcat(array_msg, number);
                    }
                    strcat(array_msg, "}");
                    printf("%s\n", array_msg);
                    // ----------------------

                    for (int i = 0; i < max_players; i++) {
                        if (client_sockets[i] != -1 && i != player_at_turn) {
                            char msg[10] = { 0 };
                            snprintf(msg, sizeof(msg), "%i", player_at_turn);
                            printf("Sent to %s: PASS:%s\n", players[i], msg);
                            send_message(client_sockets[i], "PASS", msg);
                        }
                    }

                    // select new player, if player is only one who hasnt passed yet, win round for him
                    int count_players = 0;
                    for (int i = 0; i < max_players; i++) {
                        if (passed_players[i] == 0) count_players++; // count players who havent passed
                    }

                    int winner_index = -1;
                    for (int i = 0; i < max_players; i++) {
                        if (passed_players[i] == 0) { // if player found who hasnt passed, if hes the only one, win round, if not, his turn
                            if (count_players == 1) {
                                winner_index = i;
                            }
                            player_at_turn = i;
                            break;
                        }
                    }

                    if (winner_index != -1) {
                        for (int i = 0; i < max_players; i++) {
                            if (client_sockets[i] != -1) {
                                char msg[10] = { 0 };
                                snprintf(msg, sizeof(msg), "%i", winner_index);
                                printf("Sent to %s: WIN:%s\n", players[i], msg);
                                send_message(client_sockets[i], "WIN_HAND", msg);
                            }
                        }

                        // reset passes on round win
                        memset(passed_players, 0, sizeof(passed_players));
                    } else {
                        for (int i = 0; i < max_players; i++) {
                            if (client_sockets[i] != -1) {
                                char msg[10] = { 0 };
                                snprintf(msg, sizeof(msg), "%i", player_at_turn);
                                printf("Sent to %s: TURN:%s\n", players[i], msg);
                                send_message(client_sockets[i], "TURN", msg);
                            }
                        }
                    }

                    // TODO: fix win_server clear on disconnect
                }
            }

            // GAME LOGIC HERE


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
