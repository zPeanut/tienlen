//
// Created by peanut on 12/05/2025.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "cards.c"
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/queue.h>

#define PORT 25565
#define NUM_PLAYERS 4

char players[NUM_PLAYERS][30];
Card hands[NUM_PLAYERS][13];
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
        if (activity < 0 && running) continue;

        // handle new connections
        if (FD_ISSET(server_fd, &readfds)) {
            struct sockaddr_in address;
            socklen_t address_length = sizeof(address);

            int new_socket = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &address_length);
            char name[30];
            ssize_t b_recv = recv(new_socket, name, sizeof(name) - 1, 0);
            if (b_recv <= 0) {
                // disconnect before entering name -> handle with message maybe?
                close(new_socket);
                continue;
            }

            pthread_mutex_lock(&player_lock);

            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (client_sockets[i] == -1) {
                    client_sockets[i] = new_socket;
                    strcpy(players[i], name);
                    break;
                }
            }
            player_count++;
            printf("Player %s connected. (%d/%d)\n", name, player_count, NUM_PLAYERS);

            // comma seperated player list
            char player_list[256] = "";
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if(client_sockets[i] != -1) {
                    strcat(player_list, players[i]);
                    if (i < player_count - 1) strcat(player_list, ",");
                }
            }

            // send that to all clients
            for (int i = 0; i < player_count; i++) {
                if(client_sockets[i] != -1) {
                    send(client_sockets[i], player_list, strlen(player_list), 0);
                }
            }
            pthread_mutex_unlock(&player_lock);
        }

        pthread_mutex_lock(&player_lock);

        // check for disconnects
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
                    char player_list[256] = "";
                    for (int j = 0; j < NUM_PLAYERS; j++) {
                        if (client_sockets[j] != -1) { // Only include connected players
                            strcat(player_list, players[j]);
                            if (j < NUM_PLAYERS - 1) strcat(player_list, ",");
                        }
                    }

                    for (int j = 0; j < NUM_PLAYERS; j++) {
                        if (client_sockets[j] != -1) {
                            send(client_sockets[j], player_list, strlen(player_list), 0);
                        }
                    }
                } else {
                    // queue handler

                    buffer[b_recv] = '\0';
                    MessageEntry *entry = malloc(sizeof(MessageEntry));
                    entry->message.client_fd = client_sockets[i];
                    strcpy(entry->message.buffer, buffer);

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
    printf("Socket erstellt.\n");
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    printf("Port: %d\n", PORT);
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
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

        MessageEntry *entry;
        STAILQ_FOREACH(entry, &message_queue, entries) {

            printf("Received: %s\n", entry->message.buffer);

            // broadcast to other players
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (client_sockets[i] != -1 && client_sockets[i] != entry->message.client_fd) {
                    send(client_sockets[i], entry->message.buffer, strlen(entry->message.buffer), 0);
                }
            }

            // remove from q
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
