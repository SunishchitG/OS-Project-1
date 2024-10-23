#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_COMMAND_LINE_LEN 1024
#define MAX_COMMAND_LINE_ARGS 128

char prompt[] = "> ";
char delimiters[] = " \t\r\n";
extern char **environ;

// Function declarations
void execute_command(char *arguments[], int redirect_in, int redirect_out, int pipe_pos, bool background);
void handle_signal(int sig);
void change_directory(char *path);
void print_env();
void set_env_var(char *name, char *value);

int main() {
    char command_line[MAX_COMMAND_LINE_LEN];
    char *arguments[MAX_COMMAND_LINE_ARGS];
    char cwd[MAX_COMMAND_LINE_LEN];

    signal(SIGINT, handle_signal); // Handle Ctrl-C signal

    while (true) {
        // Print the shell prompt with the current working directory
        getcwd(cwd, sizeof(cwd));
        printf("%s%s", cwd, prompt);
        fflush(stdout);

        // Read input from stdin
        if (fgets(command_line, MAX_COMMAND_LINE_LEN, stdin) == NULL && ferror(stdin)) {
            fprintf(stderr, "fgets error");
            exit(0);
        }

        // If user input is EOF (ctrl+d), exit the shell
        if (feof(stdin)) {
            printf("\n");
            return 0;
        }

        // Tokenize the command line input
        int i = 0;
        arguments[i] = strtok(command_line, delimiters);
        while (arguments[i] != NULL && i < MAX_COMMAND_LINE_ARGS - 1) {
            i++;
            arguments[i] = strtok(NULL, delimiters);
        }

        // Skip empty commands
        if (arguments[0] == NULL) {
            continue;
        }

        // Check for built-in commands
        if (strcmp(arguments[0], "cd") == 0) {
            change_directory(arguments[1]);
            continue;
        } else if (strcmp(arguments[0], "pwd") == 0) {
            getcwd(cwd, sizeof(cwd));
            printf("%s\n", cwd);
            continue;
        } else if (strcmp(arguments[0], "exit") == 0) {
            exit(0);
        } else if (strcmp(arguments[0], "env") == 0) {
            print_env();
            continue;
        } else if (strcmp(arguments[0], "setenv") == 0) {
            set_env_var(arguments[1], arguments[2]);
            continue;
        }

        // Handle background processes
        bool background = false;
        if (strcmp(arguments[i - 1], "&") == 0) {
            background = true;
            arguments[i - 1] = NULL;  // Remove '&' from arguments
        }

        // Handle I/O redirection and piping
        int redirect_in = -1, redirect_out = -1, pipe_pos = -1;
        int j;
        for (j = 0; j < i; j++) {
            if (strcmp(arguments[j], "<") == 0) {
                redirect_in = j;
            } else if (strcmp(arguments[j], ">") == 0) {
                redirect_out = j;
            } else if (strcmp(arguments[j], "|") == 0) {
                pipe_pos = j;
            }
        }

        // Execute command (handles forking and redirection)
        execute_command(arguments, redirect_in, redirect_out, pipe_pos, background);
    }

    return 0;
}

// Handle SIGINT (Ctrl-C) to prevent shell from quitting
void handle_signal(int sig) {
    printf("\nCaught signal %d. Use 'exit' to quit the shell.\n", sig);
    printf("> ");
    fflush(stdout);
}

// Change directory
void change_directory(char *path) {
    if (path == NULL) {
        fprintf(stderr, "No path specified for cd\n");
    } else if (chdir(path) != 0) {
        perror("cd failed");
    }
}

// Print environment variables
void print_env() {
    for (int i = 0; environ[i] != NULL; i++) {
        printf("%s\n", environ[i]);
    }
}

// Set environment variable
void set_env_var(char *name, char *value) {
    if (setenv(name, value, 1) != 0) {
        perror("setenv failed");
    }
}

// Execute a command with redirection and piping
void execute_command(char *arguments[], int redirect_in, int redirect_out, int pipe_pos, bool background) {
    int fd_in, fd_out, pipefd[2];
    pid_t pid, pipe_pid;

    // Handle piping
    if (pipe_pos != -1) {
        arguments[pipe_pos] = NULL;  // Split the command into two parts
        if (pipe(pipefd) == -1) {
            perror("pipe failed");
            exit(1);
        }

        pipe_pid = fork();
        if (pipe_pid == 0) {
            // Child process for the first command
            close(pipefd[0]);  // Close unused read end of pipe
            dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to the pipe's write end
            close(pipefd[1]);

            execvp(arguments[0], arguments);
            perror("execvp failed");
            exit(1);
        } else {
            // Parent process for the second command
            wait(NULL);  // Wait for the first process to complete
            close(pipefd[1]);  // Close unused write end of pipe
            dup2(pipefd[0], STDIN_FILENO);  // Redirect stdin to the pipe's read end
            close(pipefd[0]);

            execvp(arguments[pipe_pos + 1], &arguments[pipe_pos + 1]);
            perror("execvp failed");
            exit(1);
        }
    }

    // Handle forking for non-piped commands
    pid = fork();
    if (pid == 0) {
        // Child process
        if (redirect_in != -1) {
            fd_in = open(arguments[redirect_in + 1], O_RDONLY);
            if (fd_in == -1) {
                perror("open for input failed");
                exit(1);
            }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
            arguments[redirect_in] = NULL;
        }

        if (redirect_out != -1) {
            fd_out = open(arguments[redirect_out + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_out == -1) {
                perror("open for output failed");
                exit(1);
            }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
            arguments[redirect_out] = NULL;
        }

        execvp(arguments[0], arguments);
        perror("execvp failed");
        exit(1);
    } else if (pid > 0) {
        // Parent process
        if (!background) {
            waitpid(pid, NULL, 0);  // Wait for child to finish unless it's a background process
        }
    } else {
        perror("fork failed");
    }
}
