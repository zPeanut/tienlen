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

#include "cards.h"

#define PORT 25565

char players[NUM_PLAYERS][30];
Card hands[NUM_PLAYERS][HAND_SIZE];
int running = 1;
int player_count = 0;
int client_sockets[NUM_PLAYERS];
int server_fd;

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

void send_message(int fd, const char* type, const char* content) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s:%s\n", type, content);
    buffer[strlen(buffer)] = '\0';
    write(fd, buffer, strlen(buffer));
    printf("%s\n", buffer);
}

void *io_thread(void* arg) {
    fd_set readfds;
    int max_fd;
    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };

    while (running) {

        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_fd = server_fd;

        pthread_mutex_lock(&player_lock);
        // add all current sockets to set
        for (int i = 0; i < NUM_PLAYERS; i++) {
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
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (client_sockets[i] == -1) {
                    client_sockets[i] = new_socket;
                    memset(players[i], 0, sizeof(players[i]));
                    strncpy(players[i], name, sizeof(name));
                    break;
                }
            }


            player_count++;
            printf("Player %s connected. (%d/%d)\n", name, player_count, NUM_PLAYERS);

            if (player_count == NUM_PLAYERS) {
                Deck *deck = malloc(sizeof(*deck));
                init_deck(deck);
                shuffle_deck(deck);

                for (int i = 0; i < NUM_PLAYERS; i++) {
                    for (int j = 0; j < HAND_SIZE; j++) {
                        hands[i][j] = deck->cards[HAND_SIZE * i + j];
                    }

                    char deal_msg[256] = { 0 };
                    for (int j = 0; j < HAND_SIZE; j++) {
                        int suit = hands[i][j].suit;
                        int rank = hands[i][j].rank;
                        char card_str[10];
                        snprintf(card_str, sizeof(card_str), "%d,%d;", suit, rank);
                        if (j < player_count - 1) strncat(deal_msg, card_str, strlen(card_str));
                    }
                    send_message(client_sockets[i], "DEAL", deal_msg);
                }
                printf("Game started with all players.\n");
                printf("Cards dealt.\n");
                free(deck);
            }

            // comma seperated player list
            char player_list_cn[256] = { 0 };
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if(client_sockets[i] != -1) {
                    strncat(player_list_cn, players[i], strlen(players[i]));
                    if (i < player_count - 1) strncat(player_list_cn, ",", 2);
                }
            }

            // send that to all clients
            for (int i = 0; i < player_count; i++) {
                if(client_sockets[i] != -1) {
                    printf("Sent to %s: ", players[i]);
                    send_message(client_sockets[i], "PLAYERS", player_list_cn);
                }
            }
            pthread_mutex_unlock(&player_lock);
        }

        // check for disconnects
        pthread_mutex_lock(&player_lock);
        for (int i = 0; i < NUM_PLAYERS; i++) {

            if (client_sockets[i] != -1 && FD_ISSET(client_sockets[i], &readfds)) {

                char buffer[256];
                ssize_t b_recv = recv(client_sockets[i], &buffer, 1, MSG_PEEK);

                if (b_recv <= 0) {
                    player_count--;
                    printf("Player %s disconnected. (%d/%d)\n", players[i], player_count, NUM_PLAYERS);
                    close(client_sockets[i]);
                    client_sockets[i] = -1;

                    // make updated player lists (after disconnects)
                    char player_list_dc[256] = { 0 };
                    for (int j = 0; j < NUM_PLAYERS; j++) {
                        if (client_sockets[j] != -1) { // only include connected players
                            strncat(player_list_dc, players[j], strlen(players[j]));
                            if (j < player_count - 1) strncat(player_list_dc, ",", strlen(players[j]));
                        }
                    }

                    // send updated player list to client (with removed names)
                    for (int j = 0; j < NUM_PLAYERS; j++) {
                        if (client_sockets[j] != -1) {
                            send_message(client_sockets[j], "PLAYERS", player_list_dc);
                        }
                    }
                } else {
                    // queue handler
                    read(client_sockets[i], buffer, sizeof(buffer));
                    buffer[b_recv] = '\0';
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
        pthread_mutex_unlock(&player_lock);
    }
    return NULL;
}


int main() {

    for (int i = 0; i < NUM_PLAYERS; i++) {
        client_sockets[i] = -1;
    }

    struct sockaddr_in address;
    signal(SIGINT, handle_ctrlc);
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int reuse = 1;
    int result = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&reuse, sizeof(reuse));
    if ( result < 0 ) {
        perror("ERROR SO_REUSEADDR:");
    }

    printf("Socket erstellt.\n");

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    printf("Port: %d\n", PORT);

    int b = bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    if (b != 0) {
        perror("bind");
        return -1;
    }
    listen(server_fd, NUM_PLAYERS);
    printf("Warten auf andere Spieler...\n");

    // init io queue
    STAILQ_INIT(&message_queue);

    // init io thread
    pthread_t thread_io;
    pthread_create(&thread_io, NULL, io_thread, NULL);

    // game loop
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

            // broadcast to other players
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (client_sockets[i] != -1 && client_sockets[i] != entry->message.client_fd) {
                    write(client_sockets[i], entry->message.buffer, strlen(entry->message.buffer));
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
    close(server_fd);
    return 0;
}
