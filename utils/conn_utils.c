//
// Created by must9 on 20.05.2025.
//

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "conn_utils.h"

int connect_timeout(int socket, struct sockaddr *address, socklen_t address_length, int timeout) {
    int flags = fcntl(socket, F_GETFL, 0);
    // non block
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);

    int result = connect(socket, address, address_length);
    if (result == 0) {
        fcntl(socket, F_SETFL, flags | O_NONBLOCK);
        return 0;
    } else if (errno != EINPROGRESS) {
        return -1;
    }

    fd_set writefds;
    struct timeval tv;

    FD_ZERO(&writefds);
    FD_SET(socket, &writefds);

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    result = select(socket + 1, NULL, &writefds, NULL, &tv);
    if (result <= 0) {
        printf("Connection to server has timed out.\n");
        return -1;
    }

    int so_error;
    socklen_t len = sizeof(so_error);
    getsockopt(socket, SOL_SOCKET, SO_ERROR, &so_error, &len);
    if (so_error != 0) {
        switch (so_error) {
            case ECONNREFUSED:
                printf("Connection to server has failed! Server is not running.\n");
                break;
            default:
                printf("Connection to server has failed! Error code (%d)", so_error);
        }
        return -1;
    }
    fcntl(socket, F_SETFL, flags & ~O_NONBLOCK);
    return 0;
}

int setup_connection(int timeout, char (*players)[MAX_NAME_LENGTH], int *max_players, char** get_name) {

    char* ip = get_client_ip();
    int port = get_client_port();

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);

    printf("Trying to connect to %s:%i...\n", ip, port);
    if ((connect_timeout(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr), timeout) == -1)) {
        close(sock);
        exit(1);
    }

    printf("Connected to %s:%d\n", ip, port);

    char* name = get_client_name();
    write(sock, name, strlen(name));
    *get_name = name;

    // build initial userlist from server
    char recv_buffer[256] = { 0 };
    ssize_t received = read(sock, recv_buffer, sizeof(recv_buffer) - 1);
    recv_buffer[received] = '\0';
    if (received > 0) {
        // parse player names with comma
        memset(players, 0, sizeof(*players));
        parse_names(recv_buffer, players);
    }

    char get_max_player_buf[20] = { 0 };
    read(sock, get_max_player_buf, sizeof(get_max_player_buf));
    char* colon = strchr(get_max_player_buf, ':');
    if (colon != NULL) {
        *max_players = (atoi(colon + 1));
    }

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK); // non blocking mode

    free(ip);
    return sock;
}

void send_message(int fd, const char* type, const char* content) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s:%s\n", type, content);
    write(fd, buffer, strlen(buffer));
}


int parse_names(char* buffer, char players[NUM_PLAYERS][MAX_NAME_LENGTH]) {
    static char prev_players[NUM_PLAYERS][MAX_NAME_LENGTH] = { 0 };
    char temp_players[NUM_PLAYERS][MAX_NAME_LENGTH] = { 0 };
    int players_changed = 0;

    char *token = strchr(buffer, ':');
    if (token != NULL) {
        token++;

        token = strtok(token, ",");
        for (int i = 0; i < NUM_PLAYERS && token; i++) {
            strncpy(temp_players[i], token, MAX_NAME_LENGTH - 1);
            temp_players[i][MAX_NAME_LENGTH - 1] = '\0';
            temp_players[i][strcspn(temp_players[i], "\n")] = '\0'; // replace message delimiter '\n' with null termination to avoid buffer overflow
            token = strtok(NULL, ",");
        }
    }

    // compare with previous state
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (strcmp(players[i], temp_players[i]) != 0) {
            players_changed = 1;
            break;
        }
    }

    if (players_changed) {
        // update both current and previous arrays
        memcpy(prev_players, temp_players, sizeof(prev_players));
        memcpy(players, temp_players, sizeof(temp_players));
    }

    return players_changed;
}

char* get_client_ip() {
    printf("Server IP:\n");
    printf("-> ");

    char client_ip[100];
    fgets(client_ip, 100, stdin);
    client_ip[strcspn(client_ip, "\n")] = 0;

    if (client_ip[0] == '\0') {
        strcpy(client_ip, DEFAULT_IP);
    }

    unsigned long str_size = strlen(client_ip) + 1;
    char* ip = malloc(str_size);
    strncpy(ip, client_ip, str_size);
    return ip;
}

int get_client_port() {
    printf("Port:\n");
    printf("-> ");

    char client_port[100];
    fgets(client_port, 100, stdin);
    client_port[strcspn(client_port, "\n")] = 0;
    if (client_port[0] == '\0') {
        snprintf(client_port, 7, "%i", DEFAULT_PORT);
    }
    int port = (int) atol(client_port);
    return port;
}

int get_max_players() {
    char max_amount[10] = { 0 };
    int max_players;
    do {
        printf("Amount of players:\n");
        printf("-> ");
        fgets(max_amount, 10, stdin);

        max_amount[strcspn(max_amount, "\n")] = 0;
        max_players = atoi(max_amount);

        if (max_amount[strspn(max_amount, "0123456789")]) {
            printf("Not a number!\n");
            continue;
        }

        if (max_players < 2) {
            printf("Too few players! Need atleast 2.\n");
            continue;
        }

        if (max_players > 4) {
            printf("Too many players!\n");
            continue;
        }
    } while(max_amount[0] == 0 || max_players > 4 || max_players < 2 || max_amount[strspn(max_amount, "0123456789")]);

    if (max_amount[0] == '\0') {
        max_players = NUM_PLAYERS;
    }

    return max_players;
}

char* get_client_name() {
    char client_name[MAX_NAME_LENGTH];
    do {
        memset(client_name, 0, sizeof(client_name));
        printf("Username:\n");
        printf("-> ");
        fgets(client_name, MAX_NAME_LENGTH, stdin);
        if (strpbrk(client_name, ",;")) {
            printf("Invalid characters (`,` or `;`) in name.\n");
            continue;
        }
        client_name[strcspn(client_name, "\n")] = '\0';
    } while (client_name[0] == 0);

    unsigned long str_size = strlen(client_name) + 1;
    char* name = malloc(str_size);
    strncpy(name, client_name, str_size);
    return name;
}