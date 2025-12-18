#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <pthread.h>
#include <time.h>
#include "common.h"  // Добавляем для MessageType

// Структура для сообщения в очереди (расширенная)
typedef struct QueuedMessage {
    MessageType type;        // Тип сообщения
    char sender[USERNAME_LEN];
    char recipient[USERNAME_LEN];
    char content[BUFFER_SIZE];
    time_t timestamp;
    struct QueuedMessage* next;
} QueuedMessage;

// Структура очереди
typedef struct MessageQueue {
    QueuedMessage* head;
    QueuedMessage* tail;
    int count;
    pthread_mutex_t lock;
    pthread_cond_t cond;     // Условная переменная для ожидания сообщений
} MessageQueue;

// Прототипы функций
void init_queue(MessageQueue* queue);
int enqueue_message(MessageQueue* queue, MessageType type, 
                   const char* sender, const char* recipient, 
                   const char* content);
int dequeue_for_recipient(MessageQueue* queue, const char* recipient, 
                         Message* output, int socket_fd);
void clear_queue(MessageQueue* queue);
void queue_stats(MessageQueue* queue);
QueuedMessage* peek_next_for_recipient(MessageQueue* queue, const char* recipient);

#endif
