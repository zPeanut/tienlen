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

#define PORT 25565
#define NUM_PLAYERS 1

char players[NUM_PLAYERS][30];

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

int main() {
    int server_fd, new_socket, client_sockets[NUM_PLAYERS] = {0};
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    printf("Socket erstellt.\n");
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    printf("Port: %d\n", PORT);
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, NUM_PLAYERS);
    printf("Warten auf andere Spieler...\n");

    int player = 0;
    while (player < NUM_PLAYERS) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        client_sockets[player] = new_socket;

        char name[30];
        recv(new_socket, name, sizeof(name) - 1, 0);
        printf("Player %s connected.\n", name);
        strcpy(players[player], name);

        player++;
    }

    for (int i = 0; i < NUM_PLAYERS; i++) {
        send(client_sockets[i], "Game Start!\n", strlen("Game Start!\n"), 0);
        printf("%s\n", players[i]);
    }


    // gameplay logic
    Deck *deck = malloc(sizeof(*deck));
    init_deck(deck);
    shuffle_deck(deck);
    Card hands[4][13];

    for (int j = 0; j < 13; j++) {
        for (int i = 0; i < 4 /* NUM PLAYERS */; i++) {
            usleep(150000);
            hands[i][j] = deck->cards[deck->index];
            if (i == 0) {
                printf("Player %d Card %d", i, j);
                print_card(deck->cards[deck->index]);
            }
            deck->index++;
        }
    }



    close(server_fd);
    return 0;
}
