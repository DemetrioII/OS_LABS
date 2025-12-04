#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <signal.h>

#define MAX_LEN 4096

volatile sig_atomic_t has_signal = 0;

typedef struct {
	char message[MAX_LEN];
	int has_message;
	int terminate;
} shared_data_t;

void handler_child(int sig) {
	has_signal = 1;
}

int main(int argc, char* argv[]) {
	signal(SIGUSR1, handler_child);
	char *mapping_filename = argv[1];

	char *error_filename = "Errors File Mapping";
	char output_filename[256];

	int fd = open(mapping_filename, O_RDWR);
	int fd_errors = open(error_filename, O_RDWR);

	if (fd == -1 || fd_errors == -1) {
		perror("open");
		exit(1);
	}

	struct stat st;
	fstat(fd, &st);
	if (st.st_size < sizeof(shared_data_t)) {
		fprintf(stderr, "File too small: %ld < %zu\n", st.st_size, sizeof(shared_data_t));
		exit(1);
	}
	shared_data_t *shared_data = mmap(NULL, sizeof(shared_data_t),
			PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);

	shared_data_t *error_data = mmap(NULL, sizeof(shared_data_t), PROT_WRITE | PROT_READ, MAP_SHARED, fd_errors, 0);

	if (shared_data == MAP_FAILED || error_data == MAP_FAILED) {
		perror("mmap");
		close(fd);
		exit(1);
	}

	strncpy(output_filename, shared_data->message, 255);

	FILE* output_file = fopen(output_filename, "w");

	printf("Открыт файл %s для записи\n", output_filename);

	while(1) {
		while (!has_signal) {
			pause();
		}

		has_signal = 0;
		printf("Получено через File Mapping: %s\n", shared_data->message);

		if (shared_data->terminate) {
			printf("Получен сигнал завершения\n");
			break;
		}

		if (isupper((unsigned char)shared_data->message[0])) {
			fprintf(output_file, "%s\n", shared_data->message);
			fflush(output_file);
			error_data->terminate = 1;
			error_data->has_message = 0;
		} else {
			strncpy(error_data->message, "ERROR!", strlen("ERROR!"));
			// error_data->terminate = 0;
			error_data->has_message = 1;
			kill(getppid(), SIGUSR2);
		}

		shared_data->has_message = 0;

		kill(getppid(), SIGUSR2);
		// usleep(10000);
	}

	fclose(output_file);
	munmap(shared_data, sizeof(shared_data_t));
	munmap(error_data, sizeof(shared_data_t));

	close(fd_errors);
	close(fd);

	return 0;
}

