#include "message_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>

void init_queue(MessageQueue* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

int enqueue_message(MessageQueue* queue, MessageType type,
                   const char* sender, const char* recipient,
                   const char* content) {
    
    pthread_mutex_lock(&queue->lock);
    
    if (queue->count >= MAX_QUEUE_SIZE) {
        pthread_mutex_unlock(&queue->lock);
        return -1;  
	}
    
    QueuedMessage* msg = malloc(sizeof(QueuedMessage));
    if (msg == NULL) {
        pthread_mutex_unlock(&queue->lock);
        return -2; 
    }
    
    msg->type = type;
    
    strncpy(msg->sender, sender, sizeof(msg->sender)-1);
    msg->sender[sizeof(msg->sender)-1] = '\0';
    
    strncpy(msg->recipient, recipient, sizeof(msg->recipient)-1);
    msg->recipient[sizeof(msg->recipient)-1] = '\0';
    
    strncpy(msg->content, content, sizeof(msg->content)-1);
    msg->content[sizeof(msg->content)-1] = '\0';
    
    msg->timestamp = time(NULL);
    msg->next = NULL;
    
    if (queue->tail == NULL) {
        queue->head = queue->tail = msg;
    } else {
        queue->tail->next = msg;
        queue->tail = msg;
    }
    
    queue->count++;
    
    pthread_cond_signal(&queue->cond);
    
    printf("[QUEUE] Сообщение добавлено в очередь: %s -> %s (тип: %d, всего: %d)\n",
           sender, recipient, type, queue->count);
    
    pthread_mutex_unlock(&queue->lock);
    return 0; 
}

// Получение следующего сообщения для указанного получателя
int dequeue_for_recipient(MessageQueue* queue, const char* recipient,
                         Message* output, int socket_fd) {
    
    pthread_mutex_lock(&queue->lock);
    
    QueuedMessage* current = queue->head;
    QueuedMessage* prev = NULL;
    int found = 0;
    
    while (current != NULL) {
        if (strcmp(current->recipient, recipient) == 0) {
            // Нашли сообщение для получателя
            if (output != NULL) {
                output->type = current->type;
                strncpy(output->sender, current->sender, USERNAME_LEN);
                strncpy(output->recipient, current->recipient, USERNAME_LEN);
                strncpy(output->content, current->content, BUFFER_SIZE);
                output->timestamp = current->timestamp;
            }
            
            QueuedMessage* to_free = current;
            
            if (prev == NULL) {
                queue->head = current->next;
            } else {
                prev->next = current->next;
            }
            
            if (current == queue->tail) {
                queue->tail = prev;
            }
            
            current = current->next;
            free(to_free);
            queue->count--;
            
            found = 1;
            break;
        }
        
        prev = current;
        current = current->next;
    }
    
    pthread_mutex_unlock(&queue->lock);
    return found; 
}

QueuedMessage* peek_next_for_recipient(MessageQueue* queue, const char* recipient) {
    pthread_mutex_lock(&queue->lock);
    
    QueuedMessage* current = queue->head;
    while (current != NULL) {
        if (strcmp(current->recipient, recipient) == 0) {
            pthread_mutex_unlock(&queue->lock);
            return current;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&queue->lock);
    return NULL;
}

void clear_queue(MessageQueue* queue) {
    pthread_mutex_lock(&queue->lock);
    
    QueuedMessage* current = queue->head;
    while (current != NULL) {
        QueuedMessage* next = current->next;
        free(current);
        current = next;
    }
    
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    
    pthread_mutex_unlock(&queue->lock);
}

void queue_stats(MessageQueue* queue) {
    pthread_mutex_lock(&queue->lock);
    
    printf("=== Статистика очереди ===\n");
    printf("Всего сообщений в очереди: %d\n", queue->count);
    
    QueuedMessage* current = queue->head;
    while (current != NULL) {
        int count_for_user = 0;
        QueuedMessage* temp = queue->head;
        
        while (temp != NULL) {
            if (strcmp(temp->recipient, current->recipient) == 0) {
                count_for_user++;
            }
            temp = temp->next;
        }
        
        printf("  Для %s: %d сообщений\n", current->recipient, count_for_user);
        
        while (current->next != NULL && 
               strcmp(current->next->recipient, current->recipient) == 0) {
            current = current->next;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&queue->lock);
}
