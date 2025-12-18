#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include "common.h"

#define SERVER_IP "127.0.0.1"

volatile int running = 1;
int sock = 0;
pthread_mutex_t display_mutex = PTHREAD_MUTEX_INITIALIZER;
char current_username[USERNAME_LEN] = "";

void clear_input_line() {
    printf("\r\033[K");
    fflush(stdout);
}

void print_message(const Message* msg) {
    char timestamp_str[20];
    struct tm* tm_info = localtime(&msg->timestamp);
    strftime(timestamp_str, sizeof(timestamp_str), "%H:%M:%S", tm_info);
    
    if (msg->type == MSG_PRIVATE) {
        if (strcmp(msg->sender, current_username) == 0) {
            printf("[%s] Вы -> %s: %s\n", timestamp_str, msg->recipient, msg->content);
        } else {
            printf("[%s] ЛС от %s: %s\n", timestamp_str, msg->sender, msg->content);
        }
    } else if (strcmp(msg->sender, "SERVER") == 0) {
        printf("[%s] \033[1;36m[Сервер]:\033[0m %s\n", timestamp_str, msg->content);
    } else {
        printf("[%s] %s: %s\n", timestamp_str, msg->sender, msg->content);
    }
}

void* receiver_thread(void* arg) {
    Message msg;
    int consecutive_errors = 0;
    int first_message = 1;
    
    printf("[Приемник] Поток запущен\n");
    
    while (running) {
        memset(&msg, 0, sizeof(Message));
        
        int bytes = recv(sock, &msg, sizeof(Message), MSG_DONTWAIT);
        
        if (bytes > 0) {
            if (bytes == sizeof(Message)) {
                pthread_mutex_lock(&display_mutex);
                
                if (!first_message) {
                    clear_input_line();
                }
                first_message = 0;
                
                print_message(&msg);
                
                printf("> ");
                fflush(stdout);
                
                pthread_mutex_unlock(&display_mutex);
                
                consecutive_errors = 0;
            } else {
                printf("[Приемник] Некорректный размер сообщения: %d байт\n", bytes);
                consecutive_errors++;
            }
        } else if (bytes == 0) {
            pthread_mutex_lock(&display_mutex);
            if (!first_message) {
                clear_input_line();
            }
            printf("\n\033[1;31m[Система] Сервер отключился\033[0m\n");
            pthread_mutex_unlock(&display_mutex);
            running = 0;
            break;
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                consecutive_errors++;
                if (consecutive_errors > 10) {
                    printf("[Приемник] Слишком много ошибок, завершение\n");
                    running = 0;
                    break;
                }
            }
        }
        
        usleep(50000);
    }
    
    printf("[Приемник] Поток завершен\n");
    return NULL;
}

int send_message_safe(Message* msg) {
    int attempts = 0;
    
    while (attempts < 3 && running) {
        if (send(sock, msg, sizeof(Message), 0) < 0) {
            printf("[Клиент] Ошибка отправки (попытка %d): %s\n", 
                   attempts + 1, strerror(errno));
            attempts++;
            sleep(1);
        } else {
            return 1;
        }
    }
    
    return 0;
}

void print_welcome() {
    pthread_mutex_lock(&display_mutex);
    printf("\n\033[1;35m=== Система обмена сообщениями (Очередная версия) ===\033[0m\n");
    printf("\033[1;33mКоманды:\033[0m\n");
    printf("  \033[1;32m/login <имя>\033[0m    - войти в систему\n");
    printf("  \033[1;32m@<имя> <текст>\033[0m  - личное сообщение\n");
    printf("  \033[1;32m/search <слово>\033[0m - поиск по истории\n");
    printf("  \033[1;32m/who\033[0m            - список пользователей онлайн\n");
    printf("  \033[1;32m/logout\033[0m         - выйти из системы\n");
    printf("  \033[1;32m/exit\033[0m           - завершить программу\n");
    printf("  \033[1;32m/help\033[0m           - показать это сообщение\n\n");
    pthread_mutex_unlock(&display_mutex);
}

void print_help() {
    pthread_mutex_lock(&display_mutex);
    clear_input_line();
    printf("\n\033[1;33mСправка по командам:\033[0m\n");
    printf("  \033[1;32m/login username\033[0m     - Войти под указанным именем\n");
    printf("  \033[1;32m@username текст\033[0m     - Отправить личное сообщение\n");
    printf("  \033[1;32mПростой текст\033[0m       - Отправить сообщение всем\n");
    printf("  \033[1;32m/search слово\033[0m       - Поиск в истории сообщений\n");
    printf("  \033[1;32m/search\033[0m             - Показать всю историю\n");
    printf("  \033[1;32m/who\033[0m                - Кто онлайн\n");
    printf("  \033[1;32m/logout\033[0m             - Выйти из системы\n");
    printf("  \033[1;32m/exit\033[0m               - Завершить программу\n");
    printf("  \033[1;32m/help\033[0m               - Показать справку\n\n");
    printf("> ");
    fflush(stdout);
    pthread_mutex_unlock(&display_mutex);
}

int connect_to_server() {
    struct sockaddr_in serv_addr;
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("[Клиент] Ошибка создания сокета: %s\n", strerror(errno));
        return 0;
    }
    
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("[Клиент] Неверный адрес сервера\n");
        close(sock);
        return 0;
    }
    
    printf("[Клиент] Подключение к серверу %s:%d...\n", SERVER_IP, PORT);
    
    int connect_result = connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (connect_result < 0 && errno != EINPROGRESS) {
        printf("[Клиент] Не удалось подключиться: %s\n", strerror(errno));
        close(sock);
        return 0;
    }
    
    fd_set write_fds;
    struct timeval timeout;
    
    FD_ZERO(&write_fds);
    FD_SET(sock, &write_fds);
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    
    int select_result = select(sock + 1, NULL, &write_fds, NULL, &timeout);
    
    if (select_result <= 0) {
        printf("[Клиент] Таймаут подключения\n");
        close(sock);
        return 0;
    }
    
    int so_error;
    socklen_t len = sizeof(so_error);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
    
    if (so_error != 0) {
        printf("[Клиент] Ошибка подключения: %s\n", strerror(so_error));
        close(sock);
        return 0;
    }
    
    flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
    
    printf("[Клиент] Подключение успешно!\n");
    return 1;
}

int main() {
    printf("[Клиент] Запуск клиента...\n");
    
    if (!connect_to_server()) {
        return 1;
    }
    
    pthread_t thread;
    if (pthread_create(&thread, NULL, receiver_thread, NULL) != 0) {
        printf("[Клиент] Ошибка создания потока приема\n");
        close(sock);
        return 1;
    }
    
    print_welcome();
    
    char input[BUFFER_SIZE];
    int logged_in = 0;
    int first_input = 1;
    
    while (running) {
        if (first_input) {
            first_input = 0;
        } else {
            pthread_mutex_lock(&display_mutex);
            printf("> ");
            fflush(stdout);
            pthread_mutex_unlock(&display_mutex);
        }
        
        if (!fgets(input, sizeof(input), stdin)) {
            if (feof(stdin)) {
                printf("\n[Клиент] Конец ввода\n");
                running = 0;
            }
            break;
        }
        
        input[strcspn(input, "\n")] = '\0';
        
        if (strlen(input) == 0) {
            continue;
        }
        
        Message msg;
        memset(&msg, 0, sizeof(Message));
        msg.timestamp = time(NULL);
        
        if (strcmp(input, "/exit") == 0) {
            printf("[Клиент] Завершаю работу...\n");
            
            if (logged_in) {
                Message logout_msg;
                memset(&logout_msg, 0, sizeof(Message));
                logout_msg.type = MSG_LOGOUT;
                logout_msg.timestamp = time(NULL);
                strncpy(logout_msg.sender, current_username, USERNAME_LEN);
                send_message_safe(&logout_msg);
                usleep(100000);
            }
            
            running = 0;
            break;
        }
        else if (strcmp(input, "/help") == 0) {
            print_help();
            continue;
        }
        else if (strncmp(input, "/login ", 7) == 0) {
            if (logged_in) {
                printf("[Клиент] Вы уже вошли как %s. Используйте /logout сначала.\n", 
                       current_username);
                continue;
            }
            
            char* username = input + 7;
            if (strlen(username) == 0) {
                printf("[Клиент] Укажите имя пользователя: /login имя\n");
                continue;
            }
            
            if (strlen(username) >= USERNAME_LEN) {
                printf("[Клиент] Слишком длинное имя пользователя (макс %d символов)\n", 
                       USERNAME_LEN - 1);
                continue;
            }
            
            msg.type = MSG_LOGIN;
            strncpy(msg.content, username, sizeof(msg.content)-1);
            
            if (send_message_safe(&msg)) {
                strncpy(current_username, username, sizeof(current_username)-1);
                logged_in = 1;
                printf("[Клиент] Запрос на вход как '%s' отправлен\n", current_username);
            }
        }
        else if (input[0] == '@') {
            if (!logged_in) {
                printf("[Клиент] Сначала выполните /login\n");
                continue;
            }
            
            msg.type = MSG_PRIVATE;
            strncpy(msg.sender, current_username, USERNAME_LEN);
            
            char* space = strchr(input, ' ');
            if (space != NULL) {
                size_t recipient_len = space - input - 1;
                if (recipient_len >= USERNAME_LEN || recipient_len == 0) {
                    printf("[Клиент] Неверное имя получателя\n");
                    continue;
                }
                
                strncpy(msg.recipient, input + 1, recipient_len);
                msg.recipient[recipient_len] = '\0';
                
                if (strcmp(msg.recipient, current_username) == 0) {
                    printf("[Клиент] Нельзя отправить сообщение самому себе\n");
                    continue;
                }
                
                strncpy(msg.content, space + 1, sizeof(msg.content)-1);
                
                if (strlen(msg.content) == 0) {
                    printf("[Клиент] Сообщение не может быть пустым\n");
                    continue;
                }
                
                if (send_message_safe(&msg)) {
                    printf("[Клиент] Личное сообщение для '%s' отправлено в очередь\n", 
                           msg.recipient);
                }
            } else {
                printf("[Клиент] Формат: @имя сообщение\n");
            }
        }
        else if (strncmp(input, "/search ", 8) == 0) {
            if (!logged_in) {
                printf("[Клиент] Сначала выполните /login\n");
                continue;
            }
            
            msg.type = MSG_SEARCH;
            strncpy(msg.content, input + 8, sizeof(msg.content)-1);
            strncpy(msg.sender, current_username, USERNAME_LEN);
            
            if (send_message_safe(&msg)) {
                printf("[Клиент] Запрос поиска отправлен\n");
            }
        }
        else if (strcmp(input, "/search") == 0) {
            if (!logged_in) {
                printf("[Клиент] Сначала выполните /login\n");
                continue;
            }
            
            msg.type = MSG_SEARCH;
            msg.content[0] = '\0';
            strncpy(msg.sender, current_username, USERNAME_LEN);
            
            if (send_message_safe(&msg)) {
                printf("[Клиент] Запрос всей истории отправлен\n");
            }
        }
        else if (strcmp(input, "/who") == 0) {
            if (!logged_in) {
                printf("[Клиент] Сначала выполните /login\n");
                continue;
            }
            
            msg.type = MSG_WHO;
            strncpy(msg.sender, current_username, USERNAME_LEN);
            
            if (send_message_safe(&msg)) {
                printf("[Клиент] Запрос списка пользователей отправлен\n");
            }
        }
        else if (strcmp(input, "/logout") == 0) {
            if (!logged_in) {
                printf("[Клиент] Вы не вошли в систему\n");
                continue;
            }
            
            msg.type = MSG_LOGOUT;
            strncpy(msg.sender, current_username, USERNAME_LEN);
            
            if (send_message_safe(&msg)) {
                printf("[Клиент] Запрос выхода отправлен\n");
                logged_in = 0;
                current_username[0] = '\0';
            }
        }
        else if (input[0] == '/') {
            printf("[Клиент] Неизвестная команда. Используйте /help для списка команд.\n");
        }
        else {
            if (!logged_in) {
                printf("[Клиент] Сначала выполните /login\n");
                continue;
            }
            
            msg.type = MSG_TEXT;
            strncpy(msg.sender, current_username, USERNAME_LEN);
            strncpy(msg.content, input, sizeof(msg.content)-1);
            
            if (strlen(msg.content) == 0) {
                printf("[Клиент] Сообщение не может быть пустым\n");
                continue;
            }
            
            if (send_message_safe(&msg)) {
                printf("[Клиент] Сообщение отправлено в очередь\n");
            }
        }
        
        usleep(50000);
    }
    
    running = 0;
    printf("[Клиент] Завершение работы...\n");
    
    usleep(300000);
    
    if (sock > 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }
    
    pthread_join(thread, NULL);
    
    printf("[Клиент] Программа завершена\n");
    return 0;
}
