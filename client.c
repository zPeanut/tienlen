//
// Created by peanut on 12/05/2025.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "cards.c"
#include "errno.h"

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
        printf("Connection to server has failed.\n");
        return -1;
    }

    fcntl(socket, F_SETFL, flags & ~O_NONBLOCK);
    return 0;
}


int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;

    printf("Server IP:\n");
    printf("-> ");

    char ip[100];
    fgets(ip, 100, stdin);
    ip[strcspn(ip, "\n")] = 0;
    if (ip[0] == '\0') {
        strcpy(ip, "127.0.0.1");
    }

    printf("Port:\n");
    printf("-> ");

    char port[6];
    fgets(port, 5, stdin);
    port[strcspn(port, "\n")] = 0;
    if (port[0] == '\0') {
        strcpy(port, "25565");
    }


    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(port));
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);

    if ((connect_timeout(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr), 8) == -1)) {
        exit(1);
    }

    printf("Connected to %s:%s\n", ip, port);
    char name[30];
    do {
        printf("Enter your name?\n");
        printf("-> ");
        fgets(name, 30, stdin);
        name[strcspn(name, "\n")] = 0;
    } while (name[0] == 0);


    send(sock, name, strlen(name), 0);

    char buffer[1024] = {0};
    read(sock, buffer, sizeof(buffer));
    printf("Server: %s", buffer);

    read(sock, buffer, sizeof(buffer));
    printf("Server: %s", buffer);

    close(sock);
    return 0;
}
