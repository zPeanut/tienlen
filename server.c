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

#define PORT 25565
#define NUM_PLAYERS 4

char players[NUM_PLAYERS][30];
Card hands[NUM_PLAYERS][13];
int running = 1;
int player_count = 0;
int client_sockets[NUM_PLAYERS] = { 0 };
int server_fd;

pthread_mutex_t player_lock = PTHREAD_MUTEX_INITIALIZER;

void handle_ctrlc(int signal) {
    printf("Closing server...\n");
    close(server_fd);
    exit(0);
}

void init_deck(Deck *deck) {
    int i = 0;
    for (int j = 0; j < NUM_SUITS; j++) {
        for (int k = 0; k < NUM_RANKS; k++) {
            deck -> cards[i].suit = j;
            deck -> cards[i].rank = k;
            i++;
        }
    }
    deck -> index = 0;
}

void shuffle_deck(Deck *deck) {
    srand(time(NULL));
    for (int i = NUM_CARDS - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Card temp = deck -> cards[i];
        deck -> cards[i] = deck -> cards[j];
        deck -> cards[j] = temp;
    }
}

Card deal_card(Deck *deck) {
    if (deck -> index >= NUM_CARDS) {
        exit(1);
    }
    return deck -> cards[deck -> index++];
}

void print_card(Card card) {
    const char *suit_names[] = {"♠", "♣", "♦", "♥"};
    const char *rank_names[] = {"2", "3", "4", "5", "6", "7", "8", "9", "10","J", "Q", "K", "A"};
    printf("[%s%s]\n", rank_names[card.rank], suit_names[card.suit]);
}

void send_message(const char *message, int except_fd) {
    pthread_mutex_lock(&player_lock);
    for (int i = 0; i < player_count; i++) {
        if (client_sockets[i] != except_fd) {
            send(client_sockets[i], message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&player_lock);
}


int main() {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

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

    fd_set readfds;
    struct timeval tv;
    tv.tv_sec = 3;  // check every 3 sec
    tv.tv_usec = 0;

    while (player_count < NUM_PLAYERS) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_fd = server_fd;

        // add all current sockets to set
        for (int i = 0; i < player_count; i++) {
            if (client_sockets[i] != -1) {
                FD_SET(client_sockets[i], &readfds);
                if (client_sockets[i] > max_fd) max_fd = client_sockets[i];
            }
        }

        // check for new connections
        int active = select(max_fd + 1, &readfds, NULL, NULL, &tv);

        if (FD_ISSET(server_fd, &readfds)) {
            int new_socket = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &addrlen);

            char name[30];
            ssize_t b_recv = recv(new_socket, name, sizeof(name) - 1, 0);
            name[b_recv] = '\0';

            client_sockets[player_count] = new_socket;
            strcpy(players[player_count], name);

            player_count++;
            printf("Player %s connected. (%d/%d)\n", name, player_count, NUM_PLAYERS);
        }

        // check for disconnects
        for (int i = 0; i < player_count; i++) {
            if (client_sockets[i] != -1 && FD_ISSET(client_sockets[i], &readfds)) {
                char temp;
                int b_recv = (int) recv(client_sockets[i], &temp, 1, MSG_PEEK);
                if (b_recv <= 0) {
                    player_count--;
                    printf("Player %s disconnected. (%d/%d)\n", players[i], player_count, NUM_PLAYERS);

                    close(client_sockets[i]);
                    client_sockets[i] = -1;
                }
            }
        }
    }

    // gameplay logic
    Deck *deck = malloc(sizeof(*deck));
    init_deck(deck);

    // shuffling cards and dealing
    shuffle_deck(deck);
    char* str1 = "Shuffling cards...\n";
    for (int i = 0; i < NUM_PLAYERS; i++) {
        send(client_sockets[i], str1, strlen(str1), 0);
        usleep(500 * 1000);
    }

    char* str2 = "Dealing cards...\n";
    for (int i = 0; i < NUM_PLAYERS; i++) {
        send(client_sockets[i], str2, strlen(str2), 0);
        usleep(500 * 1000);
    }

    for (int j = 0; j < 13; j++) {
        for (int i = 0; i < NUM_PLAYERS; i++) {
            hands[i][j] = deck->cards[deck->index];
            deck->index++;
        }
    }

    close(server_fd);
    return 0;
}
