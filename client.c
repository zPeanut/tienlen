//
// Created by peanut on 12/05/2025.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "cards.c"


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
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);  // localhost
    printf("Connected to %s:%s\n", ip, port);
    char name[30];
    do {
        printf("Enter your name?\n");
        printf("-> ");
        fgets(name, 30, stdin);
        name[strcspn(name, "\n")] = 0;
    } while (name[0] == 0);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        printf("Connection to server has failed!\n");
        exit(1);
    }
    send(sock, name, strlen(name), 0);

    char buffer[1024] = {0};
    read(sock, buffer, sizeof(buffer));
    printf("Server: %s", buffer);

    read(sock, buffer, sizeof(buffer));
    printf("Server: %s", buffer);


    close(sock);
    return 0;
}
