CC = gcc
CFLAGS = -Wall -Wextra -pthread
TARGETS = server client

all: $(TARGETS)

server: server.c message_queue.c common.h message_queue.h
	$(CC) $(CFLAGS) -o server server.c message_queue.c

client: client.c common.h
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f $(TARGETS) *.o chat_history.dat

run_server:
	./server

run_client:
	./client
