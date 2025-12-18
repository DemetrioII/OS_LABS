#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <zmq.h>
#include "common.h"
#include "message_queue.h"

// Структура пользователя
typedef struct {
    char username[USERNAME_LEN];
    void* socket;           // ZeroMQ сокет клиента
    char identity[256];     // ZeroMQ идентификатор
    int is_authenticated;
    time_t last_seen;
} Peer;

Peer peers[MAX_PEERS];
int peer_count = 0;
pthread_mutex_t peers_mutex = PTHREAD_MUTEX_INITIALIZER;

// Очередь сообщений
struct MessageQueue message_queue;

// Поиск пользователя по имени
Peer* find_peer_by_username(const char* username) {
    for (int i = 0; i < MAX_PEERS; i++) {
        if (peers[i].socket != NULL && 
            strcmp(peers[i].username, username) == 0) {
            return &peers[i];
        }
    }
    return NULL;
}

// Поиск пользователя по identity
Peer* find_peer_by_identity(const char* identity) {
    for (int i = 0; i < MAX_PEERS; i++) {
        if (peers[i].socket != NULL && 
            strcmp(peers[i].identity, identity) == 0) {
            return &peers[i];
        }
    }
    return NULL;
}

// Добавление пользователя
Peer* add_peer(void* socket, const char* identity) {
    pthread_mutex_lock(&peers_mutex);
    
    for (int i = 0; i < MAX_PEERS; i++) {
        if (peers[i].socket == NULL) {
            peers[i].socket = socket;
            strncpy(peers[i].identity, identity, sizeof(peers[i].identity)-1);
            peers[i].username[0] = '\0';
            peers[i].is_authenticated = 0;
            peers[i].last_seen = time(NULL);
            
            peer_count++;
            pthread_mutex_unlock(&peers_mutex);
            return &peers[i];
        }
    }
    
    pthread_mutex_unlock(&peers_mutex);
    return NULL;
}

// Удаление пользователя
void remove_peer(const char* identity) {
    pthread_mutex_lock(&peers_mutex);
    
    for (int i = 0; i < MAX_PEERS; i++) {
        if (peers[i].socket != NULL && 
            strcmp(peers[i].identity, identity) == 0) {
            
            printf("Удаляем пользователя: %s\n", peers[i].username);
            peers[i].socket = NULL;
            peers[i].identity[0] = '\0';
            peers[i].username[0] = '\0';
            peers[i].is_authenticated = 0;
            peer_count--;
            break;
        }
    }
    
    pthread_mutex_unlock(&peers_mutex);
}

// Отправка сообщения пользователю (исправленная)
void send_to_peer(void* router_socket, const char* identity, Message* msg) {
    if (router_socket == NULL || identity == NULL || msg == NULL) return;
    
    zmq_msg_t identity_msg, message_msg;
    
    // Сначала отправляем identity
    zmq_msg_init_size(&identity_msg, strlen(identity));
    memcpy(zmq_msg_data(&identity_msg), identity, strlen(identity));
    zmq_msg_send(&identity_msg, router_socket, ZMQ_SNDMORE);
    
    // Потом само сообщение
    zmq_msg_init_size(&message_msg, sizeof(Message));
    memcpy(zmq_msg_data(&message_msg), msg, sizeof(Message));
    zmq_msg_send(&message_msg, router_socket, 0);
    
    zmq_msg_close(&identity_msg);
    zmq_msg_close(&message_msg);
}

// Обработка команды /login (исправленная)
void handle_login(void* router_socket, const char* identity, const char* username) {
    pthread_mutex_lock(&peers_mutex);
    
    // Проверяем, не занято ли имя
    Peer* existing = find_peer_by_username(username);
    if (existing != NULL) {
        Message response;
        memset(&response, 0, sizeof(Message));
        response.type = MSG_LOGIN;
        strncpy(response.sender, "SERVER", sizeof(response.sender)-1);
        strncpy(response.content, "Имя уже занято", sizeof(response.content)-1);
        response.timestamp = time(NULL);
        
        send_to_peer(router_socket, identity, &response);
        pthread_mutex_unlock(&peers_mutex);
        return;
    }
    
    // Находим или создаем peer
    Peer* peer = find_peer_by_identity(identity);
    if (peer == NULL) {
        peer = add_peer(router_socket, identity);
    }
    
    // Устанавливаем имя
    strncpy(peer->username, username, sizeof(peer->username)-1);
    peer->is_authenticated = 1;
    peer->last_seen = time(NULL);
    
    printf("Пользователь %s вошел в систему\n", username);
    
    // Отправляем подтверждение
    Message response;
    memset(&response, 0, sizeof(Message));
    response.type = MSG_LOGIN;
    strncpy(response.sender, "SERVER", sizeof(response.sender)-1);
    strncpy(response.content, "Добро пожаловать!", sizeof(response.content)-1);
    response.timestamp = time(NULL);
    
    send_to_peer(router_socket, identity, &response);
    
    // Доставляем сообщения из очереди
    int delivered = deliver_messages(&message_queue, username, router_socket);
    if (delivered > 0) {
        char msg_content[100];
        snprintf(msg_content, sizeof(msg_content), "Доставлено %d сообщений из очереди", delivered);
        Message queue_msg;
        memset(&queue_msg, 0, sizeof(Message));
        queue_msg.type = MSG_TEXT;
        strncpy(queue_msg.sender, "SERVER", sizeof(queue_msg.sender)-1);
        strncpy(queue_msg.content, msg_content, sizeof(queue_msg.content)-1);
        queue_msg.timestamp = time(NULL);
        
        send_to_peer(router_socket, identity, &queue_msg);
    }
    
    pthread_mutex_unlock(&peers_mutex);
}

// Обработка личного сообщения (исправленная)
void handle_private_message(void* router_socket, Peer* sender, const char* recipient_name, const char* text) {
    Peer* recipient = find_peer_by_username(recipient_name);
    
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.type = MSG_PRIVATE;
    strncpy(msg.sender, sender->username, sizeof(msg.sender)-1);
    strncpy(msg.recipient, recipient_name, sizeof(msg.recipient)-1);
    strncpy(msg.content, text, sizeof(msg.content)-1);
    msg.timestamp = time(NULL);
    
    if (recipient != NULL && recipient->is_authenticated) {
        // Получатель онлайн - отправляем ему
        send_to_peer(router_socket, recipient->identity, &msg);
        
        // Подтверждение отправителю
        Message ack;
        memset(&ack, 0, sizeof(Message));
        ack.type = MSG_TEXT;
        strncpy(ack.sender, "SERVER", sizeof(ack.sender)-1);
        strncpy(ack.content, "Сообщение доставлено", sizeof(ack.content)-1);
        ack.timestamp = time(NULL);
        
        send_to_peer(router_socket, sender->identity, &ack);
        
        printf("%s -> %s: %s\n", sender->username, recipient_name, text);
    } else {
        // Сохраняем в очередь
        if (enqueue(&message_queue, sender->username, recipient_name, text) == 0) {
            Message response;
            memset(&response, 0, sizeof(Message));
            response.type = MSG_TEXT;
            strncpy(response.sender, "SERVER", sizeof(response.sender)-1);
            strncpy(response.content, "Сообщение сохранено в очередь", sizeof(response.content)-1);
            response.timestamp = time(NULL);
            
            send_to_peer(router_socket, sender->identity, &response);
        }
    }
}

// Основной цикл сервера
int main() {
    void* context = zmq_ctx_new();
    void* router = zmq_socket(context, ZMQ_ROUTER);
    
    zmq_bind(router, "tcp://*:5555");
    printf("ZeroMQ сервер запущен на порту 5555\n");
    
    init_queue(&message_queue);
    
    while (1) {
        // Получаем identity клиента
        zmq_msg_t identity_msg;
        zmq_msg_init(&identity_msg);
        int rc = zmq_msg_recv(&identity_msg, router, 0);
        if (rc == -1) {
            printf("Ошибка получения identity\n");
            zmq_msg_close(&identity_msg);
            continue;
        } else {
			printf("Пришёл identity\n");
		}
        
        char identity[256];
        int identity_len = zmq_msg_size(&identity_msg);
        memcpy(identity, zmq_msg_data(&identity_msg), identity_len);
        identity[identity_len] = '\0';
        
        // Получаем сообщение
        zmq_msg_t message_msg;
        zmq_msg_init(&message_msg);
        rc = zmq_msg_recv(&message_msg, router, 0);
        if (rc == -1) {
            printf("Ошибка получения сообщения\n");
            zmq_msg_close(&identity_msg);
            zmq_msg_close(&message_msg);
            continue;
        }
        
        if (zmq_msg_size(&message_msg) == sizeof(Message)) {
            Message msg;
            memset(&msg, 0, sizeof(Message));
            memcpy(&msg, zmq_msg_data(&message_msg), sizeof(Message));
            
            // Находим отправителя
            Peer* sender = find_peer_by_identity(identity);
            
            if (msg.type == MSG_LOGIN) {
                // Команда /login
                handle_login(router, identity, msg.content);
                
            } else if (sender != NULL && sender->is_authenticated) {
                // Пользователь аутентифицирован
                if (msg.type == MSG_PRIVATE) {
                    // Личное сообщение
                    handle_private_message(router, sender, msg.recipient, msg.content);
                    printf("%s -> %s: %s\n", sender->username, msg.recipient, msg.content);
                } else if (msg.type == MSG_SEARCH) {
                    // Поиск по истории
                    // TODO: реализовать
                    Message response;
                    memset(&response, 0, sizeof(Message));
                    response.type = MSG_TEXT;
                    strncpy(response.sender, "SERVER", sizeof(response.sender)-1);
                    strncpy(response.content, "Поиск по истории пока не реализован", sizeof(response.content)-1);
                    response.timestamp = time(NULL);
                    send_to_peer(router, identity, &response);
                } else if (msg.type == MSG_LOGOUT) {
                    // Выход
                    remove_peer(identity);
                }
            } else {
                // Пользователь не аутентифицирован
                Message response;
                memset(&response, 0, sizeof(Message));
                response.type = MSG_TEXT;
                strncpy(response.sender, "SERVER", sizeof(response.sender)-1);
                strncpy(response.content, "Сначала выполните /login <имя>", sizeof(response.content)-1);
                response.timestamp = time(NULL);
                send_to_peer(router, identity, &response);
            }
            
            // Обновляем время последней активности
            if (sender != NULL) {
                sender->last_seen = time(NULL);
            }
        } else {
            printf("Некорректный размер сообщения: %zu байт\n", zmq_msg_size(&message_msg));
        }
        
        zmq_msg_close(&identity_msg);
        zmq_msg_close(&message_msg);
        
        // Очистка неактивных пользователей
        time_t now = time(NULL);
        pthread_mutex_lock(&peers_mutex);
        for (int i = 0; i < MAX_PEERS; i++) {
            if (peers[i].socket != NULL && (now - peers[i].last_seen) > 300) {
                printf("Автоотключение: %s (таймаут)\n", peers[i].username);
                peers[i].socket = NULL;
                peers[i].is_authenticated = 0;
                peer_count--;
            }
        }
        pthread_mutex_unlock(&peers_mutex);
    }
    
    zmq_close(router);
    zmq_ctx_destroy(context);
    return 0;
}
