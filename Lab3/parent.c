#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_LEN 4096

volatile sig_atomic_t child_done = 0;

typedef struct {
	char message[MAX_LEN];
	int has_message;
	int terminate;
} shared_data_t;

void handler_parent(int sig) { 
	child_done = 1;
}

int main() {
	char *filename = "FileMapping";

	int fd = open(filename, O_RDWR | O_CREAT, 0666);
	int fd_errors = open("Errors File Mapping", O_CREAT | O_RDWR, 0666);

	if (fd == -1 || fd_errors == -1) {
		perror("open");
		exit(1);
	}

	if (ftruncate(fd, sizeof(shared_data_t)) == -1) {
		perror("ftruncate");
		close(fd);
		exit(1);
	}

	if (ftruncate(fd_errors, sizeof(shared_data_t)) == -1) {
		perror("ftruncate");
		close(fd_errors);
		exit(1);
	}

	shared_data_t *data = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	shared_data_t *errors = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd_errors, 0);
	if (data == MAP_FAILED || errors == MAP_FAILED) {
		perror("mmap");
		close(fd);
		exit(1);
	}

	data->message[0] = '\0';
	data->has_message = 0;
	data->terminate = 0;

	char *s = NULL;
	size_t len = 0;
	getline(&s, &len, stdin);

	if (s[strlen(s) - 1] == '\n') {
		s[strlen(s) - 1] = '\0';
	}
	strncpy(data->message, s, 255);

	signal(SIGUSR2, handler_parent);
	pid_t pid = fork();

	if (pid == -1) {
		perror("fork");
		munmap(data, sizeof(shared_data_t));
		close(fd);
		exit(1);
	}

	if (pid > 0) {
		while (1) {
			fflush(stdout);

			ssize_t read_bytes = getline(&s, &len, stdin);
			if (read_bytes == -1) {
				break;
			}

			if (s[read_bytes - 1] == '\n') {
				s[read_bytes - 1] = '\0';
				--read_bytes;
			}

			if (strcmp(s, "exit") == 0) {
				data->has_message = 1;
				data->terminate = 1;
				kill(pid, SIGUSR1);
				break;
			}

			while (data->has_message) {
				usleep(10000);
			}

			strncpy(data->message, s, MAX_LEN - 1);
			data->message[MAX_LEN - 1] = '\0';
			data->has_message = 1;

			printf("Отправлено через File Mapping: %s\n", s);
			kill(pid, SIGUSR1);

			/*while (!errors->has_message && !errors->terminate) {
				usleep(10000);
			}*/
			child_done = 0;
			while (!child_done) {
				pause();
			}

			if (errors->has_message)
			{
				printf("Дочерний процесс ответил: %s\n", errors->message);
				errors->has_message = 0;
			}
			errors->terminate = 0;
		}
		free(s);

		wait(NULL);
	} else {
		char *args[] = {"./child", filename, NULL};
		execv("./child",  args);
		perror("execv");
		exit(1);
	}

	munmap(data, sizeof(shared_data_t));
	close(fd);
	return 0;
}
