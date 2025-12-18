#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>
#include "common.h"
#include "message_queue.h"

#define HISTORY_FILE "chat_history.dat"
#define MAX_HISTORY_SIZE (10 * 1024 * 1024)

struct Peer {
    char username[USERNAME_LEN];
    int socket_fd;
    int is_authenticated;
    int should_exit;
    struct sockaddr_in address;
};

struct Peer peers[MAX_PEERS];
int peer_count = 0;
struct MessageQueue message_queue;
pthread_mutex_t peers_mutex = PTHREAD_MUTEX_INITIALIZER;

struct Peer* find_peer_by_username(const char* username) {
    for (int i = 0; i < MAX_PEERS; ++i) {
        if (peers[i].socket_fd != -1 && 
            strcmp(peers[i].username, username) == 0) {
            return &peers[i];
        }
    }
    return NULL;
}

struct Peer* find_peer_by_socket(int socket_fd) {
    for (int i = 0; i < MAX_PEERS; ++i) {
        if (peers[i].socket_fd != -1 && peers[i].socket_fd == socket_fd) {
            return &peers[i];
        }
    }
    return NULL;
}

struct Peer* add_peer(int socket_fd, struct sockaddr_in* addr) {
    pthread_mutex_lock(&peers_mutex);
    
    for (int i = 0; i < MAX_PEERS; ++i) {
        if (peers[i].socket_fd == -1) {
            peers[i].socket_fd = socket_fd;
            if (addr != NULL) {
                peers[i].address = *addr;
            }
            peers[i].username[0] = '\0';
            peers[i].is_authenticated = 0;
            peers[i].should_exit = 0;
            ++peer_count;
            
            printf("[SERVER] Добавлен новый пир: socket=%d, всего: %d\n", 
                   socket_fd, peer_count);
            pthread_mutex_unlock(&peers_mutex);
            return &peers[i];
        }
    }
    
    pthread_mutex_unlock(&peers_mutex);
    return NULL;
}

void remove_peer(int socket_fd) {
    pthread_mutex_lock(&peers_mutex);
    
    struct Peer* peer = find_peer_by_socket(socket_fd);
    if (peer != NULL) {
        printf("[SERVER] Удаляем пира: %s (socket: %d)\n", 
               peer->username, peer->socket_fd);
        peer->socket_fd = -1;
        peer->username[0] = '\0';
        peer->is_authenticated = 0;
        peer->should_exit = 1;
        --peer_count;
    }
    
    pthread_mutex_unlock(&peers_mutex);
}

void save_message_to_history(const char* sender, const char* recipient, const char* message) {
    int fd = open(HISTORY_FILE, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        perror("Ошибка открытия файла истории");
        return;
    }
    
    if (ftruncate(fd, MAX_HISTORY_SIZE) < 0) {
        perror("Ошибка ftruncate");
        close(fd);
        return;
    }
    
    void *map = mmap(NULL, MAX_HISTORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("Ошибка mmap");
        close(fd);
        return;
    }
    
    char *end = map;
    while (end < (char*)map + MAX_HISTORY_SIZE - 1 && *end != '\0') {
        end++;
    }
    
    char entry[BUFFER_SIZE * 2];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    strftime(entry, sizeof(entry), "[%Y-%m-%d %H:%M:%S] ", tm_info);
    strcat(entry, sender);
    strcat(entry, " -> ");
    strcat(entry, recipient);
    strcat(entry, ": ");
    strcat(entry, message);
    strcat(entry, "\n");
    
    size_t entry_len = strlen(entry);
    if ((end + entry_len) < (char*)map + MAX_HISTORY_SIZE) {
        strcpy(end, entry);
    } else {
        printf("[HISTORY] История переполнена, не могу сохранить сообщение\n");
    }
    
    munmap(map, MAX_HISTORY_SIZE);
    close(fd);
}

void search_in_history(int fd, const char* username, const char* keyword) {
    FILE *history_file = fopen(HISTORY_FILE, "r");
    
    Message response;
    memset(&response, 0, sizeof(Message));
    response.type = MSG_TEXT;
    strncpy(response.sender, "SERVER", USERNAME_LEN);
    response.timestamp = time(NULL);
    
    if (history_file == NULL) {
        strncpy(response.content, "История сообщений пуста или файл не найден", BUFFER_SIZE);
        enqueue_message(&message_queue, MSG_TEXT, "SERVER", username, response.content);
        return;
    }
    
    char found_messages[BUFFER_SIZE] = "";
    char line[BUFFER_SIZE * 2];
    int found_count = 0;
    int total_messages = 0;
    
    printf("[SEARCH] Поиск для пользователя '%s' по ключевому слову '%s'\n", username, keyword);
    
    while (fgets(line, sizeof(line), history_file) != NULL) {
        total_messages++;
        line[strcspn(line, "\n")] = '\0';
        
        int is_user_related = 0;
        
        char sender_pattern[USERNAME_LEN + 10];
        snprintf(sender_pattern, sizeof(sender_pattern), "] %s -> ", username);
        
        char recipient_pattern[USERNAME_LEN + 10];
        snprintf(recipient_pattern, sizeof(recipient_pattern), " -> %s: ", username);
        
        if (strstr(line, sender_pattern) != NULL || 
            strstr(line, recipient_pattern) != NULL) {
            is_user_related = 1;
        }
        
        if (is_user_related && 
            (keyword[0] == '\0' || strstr(line, keyword) != NULL)) {
            found_count++;
            
            char temp[BUFFER_SIZE * 2];
            if (found_count <= 20) {
                snprintf(temp, sizeof(temp), "%d. %s\n", found_count, line);
                
                if (strlen(found_messages) + strlen(temp) < sizeof(found_messages) - 50) {
                    strncat(found_messages, temp, sizeof(found_messages) - strlen(found_messages) - 1);
                } else {
                    strncat(found_messages, "... (ещё сообщения не помещаются)\n", 
                           sizeof(found_messages) - strlen(found_messages) - 1);
                    break;
                }
            }
        }
    }
    
    fclose(history_file);
    
    char result[BUFFER_SIZE];
    if (keyword[0] == '\0') {
        if (found_count == 0) {
            snprintf(result, sizeof(result), 
                    "В вашей истории нет сообщений\n"
                    "Всего проверено сообщений: %d", total_messages);
        } else {
            snprintf(result, sizeof(result), 
                    "=== Ваша история сообщений (%d из %d найдено) ===\n%s"
                    "================================================",
                    found_count, total_messages, found_messages);
        }
    } else {
        if (found_count == 0) {
            snprintf(result, sizeof(result), 
                    "По запросу '%s' ничего не найдено\n"
                    "Проверено %d сообщений", keyword, total_messages);
        } else {
            snprintf(result, sizeof(result), 
                    "=== Результаты поиска '%s' (%d из %d найдено) ===\n%s"
                    "====================================================",
                    keyword, found_count, total_messages, found_messages);
        }
    }
    
    strncpy(response.content, result, BUFFER_SIZE);
    enqueue_message(&message_queue, MSG_TEXT, "SERVER", username, response.content);
    
    printf("[SEARCH] Найдено %d сообщений для пользователя '%s'\n", found_count, username);
}

void* delivery_thread(void* arg) {
    printf("[DELIVERY] Поток доставки запущен\n");
    
    while (1) {
        pthread_mutex_lock(&peers_mutex);
        
        for (int i = 0; i < MAX_PEERS; i++) {
            if (peers[i].socket_fd != -1 && 
                peers[i].is_authenticated && 
                !peers[i].should_exit) {
                
                Message msg;
                memset(&msg, 0, sizeof(Message));
                
                while (dequeue_for_recipient(&message_queue, peers[i].username, &msg, peers[i].socket_fd)) {
                    printf("[DELIVERY] Доставляем сообщение для %s от %s: %s\n", 
                           peers[i].username, msg.sender, msg.content);
                    
                    if (send(peers[i].socket_fd, &msg, sizeof(Message), 0) < 0) {
                        if (errno == EPIPE || errno == ECONNRESET) {
                            printf("[DELIVERY] Соединение с %s разорвано\n", peers[i].username);
                            peers[i].should_exit = 1;
                        } else {
                            printf("[DELIVERY] Ошибка отправки для %s: %s\n", 
                                   peers[i].username, strerror(errno));
                        }
                        break;
                    }
                    
                    memset(&msg, 0, sizeof(Message));
                }
            }
        }
        
        pthread_mutex_unlock(&peers_mutex);
        usleep(100000);
    }
    
    return NULL;
}

void* client_handler_thread(void* arg) {
    int client_fd = *(int*)arg;
    free(arg);
    
    printf("[HANDLER] Обработчик запущен для fd=%d\n", client_fd);
    
    struct Peer* peer = NULL;
    
    while (1) {
        Message msg;
        memset(&msg, 0, sizeof(Message));
        
        int bytes = recv(client_fd, &msg, sizeof(Message), 0);
        
        if (bytes <= 0) {
            if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                usleep(50000);
                continue;
            }
            
            printf("[HANDLER] Клиент %d отключился (ошибка: %s)\n", 
                   client_fd, strerror(errno));
            remove_peer(client_fd);
            close(client_fd);
            break;
        }
        
        if (bytes != sizeof(Message)) {
            printf("[HANDLER] Некорректный размер сообщения от fd=%d: %d байт\n", 
                   client_fd, bytes);
            continue;
        }
        
        peer = find_peer_by_socket(client_fd);
        
        if (msg.type == MSG_LOGIN || msg.type == MSG_SEARCH || 
            msg.type == MSG_WHO || msg.type == MSG_LOGOUT) {
            
            printf("[HANDLER] Команда типа %d от fd=%d\n", msg.type, client_fd);
            
            if (msg.type == MSG_LOGIN) {
                if (peer == NULL) {
                    peer = add_peer(client_fd, NULL);
                }
                
                pthread_mutex_lock(&peers_mutex);
                
                struct Peer* existing = find_peer_by_username(msg.content);
                if (existing != NULL && existing->socket_fd != client_fd) {
                    enqueue_message(&message_queue, MSG_TEXT, "SERVER", "NEW_USER", 
                                   "Имя уже занято");
                    pthread_mutex_unlock(&peers_mutex);
                    continue;
                }
                
                strncpy(peer->username, msg.content, USERNAME_LEN);
                peer->is_authenticated = 1;
                peer->should_exit = 0;
                
                printf("[HANDLER] Пользователь %s вошёл в систему (socket: %d)\n", 
                       peer->username, client_fd);
                
                enqueue_message(&message_queue, MSG_TEXT, "SERVER", peer->username, 
                               "Добро пожаловать в чат! Сообщения доставляются через очередь.");
                
                pthread_mutex_unlock(&peers_mutex);
            }
            else if (msg.type == MSG_SEARCH) {
                if (peer == NULL || !peer->is_authenticated) {
                    enqueue_message(&message_queue, MSG_TEXT, "SERVER", "NEW_USER", 
                                   "Сначала выполните /login");
                    continue;
                }
                search_in_history(client_fd, peer->username, msg.content);
            }
            else if (msg.type == MSG_WHO) {
                if (peer == NULL || !peer->is_authenticated) {
                    enqueue_message(&message_queue, MSG_TEXT, "SERVER", "NEW_USER", 
                                   "Сначала выполните /login");
                    continue;
                }
                
                pthread_mutex_lock(&peers_mutex);
                char user_list[BUFFER_SIZE] = "Пользователи онлайн: ";
                int count = 0;
                
                for (int i = 0; i < MAX_PEERS; i++) {
                    if (peers[i].socket_fd != -1 && peers[i].is_authenticated) {
                        if (count > 0) {
                            strncat(user_list, ", ", sizeof(user_list) - strlen(user_list) - 1);
                        }
                        strncat(user_list, peers[i].username, sizeof(user_list) - strlen(user_list) - 1);
                        count++;
                    }
                }
                
                if (count == 0) {
                    strncpy(user_list, "Нет пользователей онлайн", sizeof(user_list));
                }
                
                enqueue_message(&message_queue, MSG_TEXT, "SERVER", peer->username, user_list);
                pthread_mutex_unlock(&peers_mutex);
            }
            else if (msg.type == MSG_LOGOUT) {
                if (peer != NULL) {
                    enqueue_message(&message_queue, MSG_TEXT, "SERVER", peer->username, 
                                   "Вы вышли из системы");
                    remove_peer(client_fd);
                }
            }
        }
        else {
            if (peer == NULL || !peer->is_authenticated) {
                enqueue_message(&message_queue, MSG_TEXT, "SERVER", "NEW_USER", 
                               "Сначала выполните /login <имя>");
                continue;
            }
            
            strncpy(msg.sender, peer->username, USERNAME_LEN);
            msg.timestamp = time(NULL);
            
            if (msg.type == MSG_PRIVATE) {
                printf("[HANDLER] Личное сообщение: %s -> %s: %s\n", 
                       peer->username, msg.recipient, msg.content);
                
                save_message_to_history(peer->username, msg.recipient, msg.content);
                
                enqueue_message(&message_queue, MSG_PRIVATE, peer->username, 
                               msg.recipient, msg.content);
                
                enqueue_message(&message_queue, MSG_TEXT, "SERVER", peer->username, 
                               "Личное сообщение помещено в очередь доставки");
            }
            else if (msg.type == MSG_TEXT) {
                printf("[HANDLER] Публичное сообщение от %s: %s\n", 
                       peer->username, msg.content);
                
                save_message_to_history(peer->username, "ALL", msg.content);
                
                pthread_mutex_lock(&peers_mutex);
                for (int i = 0; i < MAX_PEERS; i++) {
                    if (peers[i].socket_fd != -1 && 
                        peers[i].is_authenticated && 
                        peers[i].socket_fd != client_fd) {
                        enqueue_message(&message_queue, MSG_TEXT, peer->username, 
                                       peers[i].username, msg.content);
                    }
                }
                pthread_mutex_unlock(&peers_mutex);
                
                enqueue_message(&message_queue, MSG_TEXT, "SERVER", peer->username, 
                               "Публичное сообщение разослано");
            }
        }
    }
    
    printf("[HANDLER] Обработчик завершен для fd=%d\n", client_fd);
    return NULL;
}

void cleanup_server() {
    printf("[SERVER] Очистка ресурсов...\n");
    
    pthread_mutex_lock(&peers_mutex);
    
    for (int i = 0; i < MAX_PEERS; i++) {
        if (peers[i].socket_fd != -1) {
            close(peers[i].socket_fd);
            peers[i].socket_fd = -1;
        }
    }
    
    clear_queue(&message_queue);
    
    pthread_mutex_unlock(&peers_mutex);
    
    printf("[SERVER] Очистка завершена\n");
}

int main() {
    printf("=== Сервер с архитектурой на основе очереди сообщений ===\n");
    printf("Инициализация...\n");
    
    for (int i = 0; i < MAX_PEERS; ++i) {
        peers[i].socket_fd = -1;
        peers[i].should_exit = 0;
    }
    
    init_queue(&message_queue);
    
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Ошибка создания сокета");
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Ошибка setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Ошибка привязки сокета");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 10) < 0) {
        perror("Ошибка прослушивания");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    printf("Сервер запущен на порту %d\n", PORT);
    printf("Ожидание подключений...\n");
    
    pthread_t delivery_tid;
    if (pthread_create(&delivery_tid, NULL, delivery_thread, NULL) != 0) {
        perror("Ошибка создания потока доставки");
        cleanup_server();
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    pthread_detach(delivery_tid);
    
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    fd_set read_fds;
    int max_fd = server_fd;
    
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        
        pthread_mutex_lock(&peers_mutex);
        for (int i = 0; i < MAX_PEERS; i++) {
            if (peers[i].socket_fd != -1) {
                FD_SET(peers[i].socket_fd, &read_fds);
                if (peers[i].socket_fd > max_fd) {
                    max_fd = peers[i].socket_fd;
                }
            }
        }
        pthread_mutex_unlock(&peers_mutex);
        
        int ready = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("Ошибка select");
            break;
        }
        
        if (ready == 0) {
            static time_t last_stats = 0;
            time_t now = time(NULL);
            if (now - last_stats >= 30) {
                printf("\n=== Статистика сервера ===\n");
                printf("Подключено пользователей: %d\n", peer_count);
                queue_stats(&message_queue);
                printf("=========================\n\n");
                last_stats = now;
            }
            continue;
        }
        
        if (FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
            if (client_fd >= 0) {
                printf("[MAIN] Новое подключение: fd=%d, IP=%s:%d\n", 
                       client_fd, inet_ntoa(client_addr.sin_addr), 
                       ntohs(client_addr.sin_port));
                
                int flags = fcntl(client_fd, F_GETFL, 0);
                fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
                
                add_peer(client_fd, &client_addr);
                
                pthread_t handler_tid;
                int* client_fd_ptr = malloc(sizeof(int));
                *client_fd_ptr = client_fd;
                
                if (pthread_create(&handler_tid, NULL, client_handler_thread, client_fd_ptr) != 0) {
                    perror("Ошибка создания обработчика");
                    free(client_fd_ptr);
                    remove_peer(client_fd);
                    close(client_fd);
                } else {
                    pthread_detach(handler_tid);
                    
                    enqueue_message(&message_queue, MSG_TEXT, "SERVER", "NEW_USER",
                                   "Добро пожаловать! Используйте /login <имя> для входа");
                }
            }
        }
    }
    
    cleanup_server();
    close(server_fd);
    
    printf("[SERVER] Сервер завершил работу\n");
    return 0;
}
