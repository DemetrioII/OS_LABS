#include <zmq.h>
#include <stdio.h>
#include <string.h>

int main() {
    printf("=== Монитор чата (ZeroMQ) ===\n");
    printf("Жду сообщения...\n\n");
    
    void* context = zmq_ctx_new();
    void* subscriber = zmq_socket(context, ZMQ_SUB);
    
    // Подключаемся к серверу
    zmq_connect(subscriber, "tcp://localhost:5556");
    
    // Подписываемся на ВСЕ сообщения
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);
    
    while (1) {
        char buffer[1024];
        
        // Получаем сообщение
        int size = zmq_recv(subscriber, buffer, 
                           sizeof(buffer)-1, 0);
        
        if (size >= 0) {
            buffer[size] = '\0';
            printf("%s\n", buffer);
        }
    }
    
    zmq_close(subscriber);
    zmq_ctx_destroy(context);
    return 0;
}
