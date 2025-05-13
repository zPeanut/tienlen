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

#define PORT 25565

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);  // localhost

    printf("Was ist dein Name?\n");
    printf("-> ");
    char name[30];
    scanf("%s", name);
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

    // game logic


    close(sock);
    return 0;
}
