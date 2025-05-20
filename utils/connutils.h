//
// Created by peanut on 20.05.2025
//

#ifndef C_CONNUTILS_H
#define C_CONNUTILS_H

#include "../cards.h"

#define DEFAULT_PORT 25565
#define DEFAULT_IP "127.0.0.1"
#define MAX_NAME_LENGTH 30

int waiting_player_count = 0;

int connect_timeout(int socket, struct sockaddr *address, socklen_t address_length, int timeout);
int setup_connection(int timeout, char (*players)[MAX_NAME_LENGTH], int *max_players);

char* get_client_ip();
int get_client_port();
char* get_client_name();
int get_max_players();
int parse_names(char* buffer, char players[NUM_PLAYERS][MAX_NAME_LENGTH]);

#endif //C_CONNUTILS_H
