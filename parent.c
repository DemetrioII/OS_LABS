#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

#define BUFFER_SIZE 1024

int main() {
	char filename[BUFFER_SIZE];
	int pipe1[2], pipe2[2];
	int status;

	if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
		perror("pipe failed");
		exit(1);
	}

	if (fgets(filename, sizeof(filename), stdin) == NULL) {
		perror("fgets failed");
		exit(1);
	}

	filename[strcspn(filename, "\n")] = '\0';

	int file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (file_fd == -1) {
		perror("opening file ERROR");
		return EXIT_FAILURE;
	}

	char *buffer = NULL;
	// dup2(file_fd, STDOUT_FILENO);
	
	pid_t pid = fork();
	if (pid == -1) {
		perror("fork failed");
		exit(1);
	}

	if (pid == 0) {
		close(pipe1[1]);
		close(pipe2[0]);
		
		dup2(pipe1[0], STDIN_FILENO);

		dup2(pipe2[1], STDERR_FILENO);
		execl("./child", "child", NULL);

		close(pipe1[0]);
		close(pipe2[1]);
	} else {
		close(pipe1[0]);
		close(pipe2[1]);

		size_t len = 0;

		write(pipe1[1], filename, strlen(filename));
		write(pipe1[1], "\n", strlen("\n"));
		char error_msg; //  = malloc(sizeof(char) * BUFFER_SIZE);
		while (1) {
			usleep(10000);
			fcntl(pipe2[0], F_SETFL, O_NONBLOCK);
			while (read(pipe2[0], &error_msg, 1) > 0)
				printf("%c", error_msg);

			fflush(stdout);
			if (getline(&buffer, &len, stdin) == -1)
				break;

			write(pipe1[1], buffer, strlen(buffer));

			// wait(NULL);
			
		}
		//waitpid(pid, &status, 0);
		close(pipe2[0]);
		close(pipe1[1]);
		free(buffer);
		wait(NULL);
	}

	close(file_fd);
    return 0;
}
