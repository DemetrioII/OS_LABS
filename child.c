#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

#define BUFFER_SIZE 1024

int main() {
	char *buffer;
	size_t n;
	ssize_t r;

	char *filename;
	getline(&filename, &n, stdin);

	filename[strlen(filename) - 1] = '\0';
	//fprintf(f,"%s", filename);
	//fflush(stdout);
	int file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	dup2(file_fd, STDOUT_FILENO);
	if (file_fd == -1)
	{
		perror("opening file ERROR");
		return EXIT_FAILURE;
	}

	while ((r = getline(&buffer, &n, stdin)) != -1) {
		if (r > 0 && buffer[r - 1] == '\n') {
			buffer[r - 1] = '\0';
			r--;
		}

		if (r > 0 && isupper(buffer[0])) {
			printf("%s\n", buffer);
			// fprintf(stderr, "%s", "SU");
		} else if (r > 0) {
			fprintf(stderr, "Error: %s is not valid\n", buffer);
		} else {
			fprintf( stderr, "Error: empty string\n");
		}
	}
	fflush(stdout);
	free(buffer);
	close(file_fd);
	return 0;
}
