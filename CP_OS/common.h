#ifndef COMMON_H
#define COMMON_H

#include <time.h>

#define PORT 8888
#define BUFFER_SIZE 1024
#define USERNAME_LEN 50
#define MAX_PEERS 100
#define MAX_QUEUE_SIZE 100

// Типы сообщений
typedef enum {
    MSG_TEXT = 1,
    MSG_PRIVATE = 2,
    MSG_LOGIN = 3,
    MSG_SEARCH = 4,
    MSG_LOGOUT = 5,
    MSG_WHO = 6
} MessageType;

// Структура сообщения для единого формата
typedef struct {
    MessageType type;
    char sender[USERNAME_LEN];
    char recipient[USERNAME_LEN];
    char content[BUFFER_SIZE];
    time_t timestamp;
} Message;

#endif
