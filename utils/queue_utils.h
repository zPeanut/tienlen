//
// Created by must9 on 21.05.2025
//

#ifndef TIENLEN_QUEUE_UTILS_H
#define TIENLEN_QUEUE_UTILS_H

#include <sys/queue.h>

typedef struct {
    int client_fd;
    char buffer[256];
} Message;

typedef struct message_entry {
    Message message;
    STAILQ_ENTRY(message_entry) entries;
} MessageEntry;

#endif //TIENLEN_QUEUE_UTILS_H
