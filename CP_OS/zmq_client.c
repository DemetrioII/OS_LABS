#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <zmq.h>
#include <time.h>
#include "common.h"

volatile int running = 1;

// Поток для приема сообщений
void* receiver_thread(void* arg) {
    void* socket = *(void**)arg;
    
    while (running) {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        
        if (zmq_msg_recv(&msg, socket, ZMQ_DONTWAIT) > 0) {
            if (zmq_msg_size(&msg) == sizeof(Message)) {
                Message received;
                memcpy(&received, zmq_msg_data(&msg), sizeof(Message));
                
                // Форматируем вывод
                if (received.type == MSG_PRIVATE) {
                    printf("\n[ЛС от %s]: %s\n", received.sender, received.content);
                } else {
                    printf("\n%s: %s\n", received.sender, received.content);
                }
                printf("> ");
                fflush(stdout);
            }
        }
        zmq_msg_close(&msg);
        
        usleep(100000); // 100ms для уменьшения нагрузки на CPU
    }
    
    return NULL;
}

int main() {
    void* context = zmq_ctx_new();
    
    // Сокет для обмена сообщениями с сервером
    void* socket = zmq_socket(context, ZMQ_DEALER);
    
    // Устанавливаем уникальный identity
    char identity[50];
    snprintf(identity, sizeof(identity), "client-%d", getpid());
    zmq_setsockopt(socket, ZMQ_IDENTITY, identity, strlen(identity));
    
    zmq_connect(socket, "tcp://localhost:5555");
    
    // Запускаем поток приема
    pthread_t thread;
    pthread_create(&thread, NULL, receiver_thread, &socket);
    
    printf("ZeroMQ клиент запущен. Используйте команды:\n");
    printf("  /login <имя> - войти\n");
    printf("  @<имя> <текст> - личное сообщение\n");
    printf("  /search <слово> - поиск по истории\n");
    printf("  /exit - выход\n");
    printf("  /who - список пользователей онлайн\n\n");
    
    char input[BUFFER_SIZE];
    
    while (running) {
        printf("> ");
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        
        input[strcspn(input, "\n")] = '\0';
        
        if (strlen(input) == 0) {
            continue;
        }
        
        if (strcmp(input, "/exit") == 0) {
            printf("Завершаю работу...\n");
            running = 0;
            break;
        }
        
        // Формируем сообщение
        Message msg;
        memset(&msg, 0, sizeof(Message));
        msg.timestamp = time(NULL);
        
        if (strncmp(input, "/login ", 7) == 0) {
            msg.type = MSG_LOGIN;
            snprintf(msg.content, sizeof(msg.content), "%s", input + 7);
        } else if (input[0] == '@') {
            msg.type = MSG_PRIVATE;
            
            char* space = strchr(input, ' ');
            if (space != NULL) {
                // Сохраняем временную копию получателя
                char recipient[USERNAME_LEN];
                strncpy(recipient, input + 1, space - input - 1);
                recipient[space - input - 1] = '\0';
                
                snprintf(msg.recipient, sizeof(msg.recipient), "%s", recipient);
                snprintf(msg.content, sizeof(msg.content), "%s", space + 1);
            } else {
                // Некорректный формат
                printf("Формат: @имя сообщение\n");
                continue;
            }
        } else if (strncmp(input, "/search ", 8) == 0) {
            msg.type = MSG_SEARCH;
            snprintf(msg.content, sizeof(msg.content), "%s", input + 8);
        } else {
            msg.type = MSG_TEXT;
            snprintf(msg.content, sizeof(msg.content), "%s", input);
        }
        
        // Отправляем сообщение
        zmq_msg_t zmq_msg;
        zmq_msg_init_size(&zmq_msg, sizeof(Message));
        memcpy(zmq_msg_data(&zmq_msg), &msg, sizeof(Message));
        
        if (zmq_msg_send(&zmq_msg, socket, 0) == -1) {
            printf("Ошибка отправки сообщения\n");
        }
        
        zmq_msg_close(&zmq_msg);
    }
    
    // Очистка
    running = 0;
    usleep(200000); // Даем потоку время завершиться
    
    zmq_close(socket);
    zmq_ctx_destroy(context);
    
    pthread_join(thread, NULL);
    return 0;
}
