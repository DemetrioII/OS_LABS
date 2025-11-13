#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

typedef struct {
	char message[256];
	int has_message;
	int parent_done;
} shared_data_t;

int main() {
	int fd = open("example.txt", O_RDWR | O_CREAT, 0666);
	if (fd == -1) {
		perror("open");
		exit(1);
	}

	if (ftruncate(fd, sizeof(shared_data_t)) == -1) {
		perror("size");
		close(fd);
		exit(1);
	}

	shared_data_t *shared_data = mmap(
		NULL,
		sizeof(shared_data_t),
		PROT_READ | PROT_WRITE,
		MAP_SHARED,
		fd,
		0 
	);

	if (shared_data == MAP_FAILED) {
		perror("mmap");
		close(fd);
		exit(1);
	}

	shared_data->has_message = 0;
	shared_data->parent_done = 0;

	pid_t child_pid = fork();
	if (child_pid == -1) {
		perror("fork");
		munmap(shared_data, sizeof(shared_data_t));
		close(fd);
		exit(1);
	}

	if (child_pid > 0) {
		printf("👨 parent (PID %d) started\n", getpid());

		char *messages[] = {
			"Привет, мой дорогой дочерний процесс! 👋",
            "Надеюсь, у тебя всё хорошо! 💫",
            "File mapping - это ведь не так сложно, правда? 😊",
            "Мы общаемся через общую память! 🧠",
            "Это последнее сообщение. Пока! 👋",
            NULL   
		};

		for (int i = 0; messages[i] != NULL; ++i) {
			while (shared_data->has_message == 1) {
				usleep(1000);
			}

			strncpy(shared_data->message, messages[i], 255);
			shared_data->message[255] = '\0';

			shared_data->has_message = 1;

			printf("📤 Родитель отправил: %s\n", messages[i]);

			sleep(1);
		}

		while (shared_data->has_message == 1) {
			usleep(1000);
		}

		shared_data->parent_done = 1;

		printf("✅ Родительский процесс завершил отправку сообщений\n");
        
        wait(NULL);
        printf("👋 Родительский процесс завершает работу\n");
	} else {
		printf("👧 Дочерний процесс (PID: %d) начал работу\n", getpid());

		while (1) {
			if (shared_data->has_message == 1) {
				printf("📥 Дочерний получил: %s\n", shared_data->message);
                
                shared_data->has_message = 0;
			}

			if (shared_data->parent_done == 1 && shared_data->has_message == 0) {
				printf("✅ Дочерний процесс завершает работу\n");
                break;
			}

			usleep(100000);
		}
	}

	munmap(shared_data, sizeof(shared_data_t));
	close(fd);

	if (child_pid > 0) {
		unlink("example.txt");
		printf("🧹 Файл %s удалён\n", "example.txt");
	}
	return 0;
}
