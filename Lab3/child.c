#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>

#define MAX_LEN 1024

typedef struct {
	char message[MAX_LEN];
	int has_message;
	int terminate;
} shared_data_t;

int main(int argc, char* argv[]) {
	char *mapping_filename = argv[1];

	char *error_filename = "Errors File Mapping";
	char *output_filename = "example.txt";

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

	FILE* output_file = fopen(output_filename, "w");

	while(1) {
		if (shared_data->has_message) {
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
				error_data->terminate = 0;
				error_data->has_message = 1;
			}

			shared_data->has_message = 0;
		}
		// usleep(10000);
	}

	fclose(output_file);
	munmap(shared_data, sizeof(shared_data_t));
	munmap(error_data, sizeof(shared_data_t));

	close(fd_errors);
	close(fd);
	
	return 0;
}

