#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#define MAX_LINE 1024
#define MAX_ARGS 100
#define MAX_ENV_VARS 100
#define TIMEOUT_SECONDS 10

// Global variables
pid_t foreground_pid = -1;
char *custom_env[MAX_ENV_VARS] = {NULL};


// Function prototypes
void sigint_handler(int sig);
void execute_command(char **args, int background);
void handle_io_redirection(char **args, int *background);
void execute_with_redirection(char **args, int background, int is_input_redirect, int is_output_redirect);
int parse_input(char *input, char **args);
void timer_handler(int sig);
void handle_builtin_commands(char **args);
void builtin_echo(char **args);
void builtin_env(char **args);
void builtin_setenv(char **args);
void setup_timer();
void cancel_timer();

int main() {
  char input[MAX_LINE];
  char *args[MAX_ARGS];
  int background;

  // Set up signal handlers
  signal(SIGINT, sigint_handler);
  signal(SIGALRM, timer_handler);

  while (1) {
    // Print current working directory in promt
    char cwd[MAX_LINE];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      printf("%s> ",cwd);
    } else {
      printf("shell> ");
    }
    fflush(stdout);

    if (fgets(input, sizeof(input), stdin) == NULL) {
      perror("Error reading input");
      continue;
    }

    // Remove newline character from input
    input[strcspn(input, "\n")] = 0;

    // Check for background process symbol '&'
    background = 0;
    if (input[strlen(input) -1] == '&') {
      background = 1;
      input[strlen(input) -1] = 0; // Remove '&'
    }

    // Tokenize input into arguments
    int num_args = parse_input(input, args);
    if (num_args == 0) continue;  // Empty input, skip


    // Check for built-in commands first
    if (strcmp(args[0], "exit") == 0) {
       break;  // Exit the shell
    } else if (strcmp(args[0], "cd") == 0) {
      if (chdir(args[1] ? args[1] : getenv("HOME")) != 0) {
        perror("cd");
      }
    } else if (strcmp(args[0], "pwd") == 0) {
      char cwd[MAX_LINE];
      if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
      } else {
        perror("pwd");
      }
    } else if (strcmp(args[0], "echo") == 0) {
      builtin_echo(args + 1);
    } else if (strcmp(args[0], "env") == 0) {
      builtin_env(args +1);
    } else if (strcmp(args[0], "setenv") == 0) {
      builtin_setenv(args + 1);
    } else {
      // Handle I/O redirection
      handle_io_redirection(args, &background);

      // Execute external command
      execute_command(args, background);
    }
  }

  return 0;
}

// Signal handler for SIGINT (Ctrl-C)
void sigint_handler(int sig) {
 printf("\nCaught SIGINT, continuing shell...\n");
 if (foreground_pid > 0) {
  kill(foreground_pid, SIGINT);
 }
}

// Timer handler for long-running processes
void timer_handler(int sig) {
  if (foreground_pid > 0) {
    printf("\nTimeout reached. Killing foreground process (PID: %d). \n", foreground_pid);
    kill(foreground_pid, SIGKILL);
    foreground_pid = -1;
  }
}

// Setup timer for lang-running processes
void setup_timer() {
  alarm(TIMEOUT_SECONDS);
}

// Cancel timer
void cancel_timer() {
  alarm(0);
}

// Parse input into arguments
int parse_input(char *input, char **args) {
  int i =0;
  char *token = strtok(input, " ");
  while (token != NULL) {
    // Check for environment variables
    if (token[0] == '$') {
      char *env_value = getenv(token +1);
      if (env_value) {
        args[i++] = env_value;
      }
    } else {
      args[i++] = token;
    }
    token = strtok(NULL, " ");
  }
  args[i] = NULL;  // Null- terminate the arguments list
  return i;
}

// Built-in echo command
void builtin_echo(char **args) {
  while (*args) {
    if ((*args)[0] == '$') {
      // Environment variables
      char *env_value = getenv(*args + 1);
      if  (env_value) {
        printf("%s ", env_value);
      }
    } else {
      printf("%s ", *args);
    }
    args++;
  }
  printf("\n");
}

// Built-in env command
void builtin_env(char **args) {
  extern char **environ;

  if (*args == NULL) {
    // Print all environment variables
    int i;
    for (i = 0; environ[i] != NULL; i++) {
      printf("%s\n", environ[i]);
    }
  } else {
    // Print specific environment variables
    char *env_value = getenv(*args);
    if (env_value) {
      printf("%s\n", env_value);
    }
  }
}

// Built-in setenv command
void builtin_setenv(char **args) {
  if (args[0] && args[1]) {
    if (setenv(args[0], args[1], 1) != 0) {
      perror("setenv");
    }
  } else {
    printf("Usage : setenv VARIABLE VALUE\n");
  }
}

// Execute a command with optional background execution
void execute_command(char **args, int background) {
  pid_t pid = fork();
  if (pid == -1) {
    perror("fork failed");
    return;
  } else if (pid == 0) {
    // Child process
    if (execvp(args[0], args) == -1) {
      perror("execvp failed");
      exit(1);
    }
  } else {
    // Parent process
    if (!background) {
      foreground_pid = pid;
      setup_timer();
      int status;
      waitpid(pid, &status, 0);  // Wait for foreground process to finish
      cancel_timer();
      foreground_pid = -1;
    } else {
      printf("Background process PID: %d\n", pid);
    }
  }
}

// Handle I/O redirection and background check
void handle_io_redirection(char **args, int *background) {
  int input_redirect = 0, output_redirect = 0;
  int input_index = -1, output_index = -1;
  int i;

  for (i = 0; args[i] != NULL; i++) {
    if (strcmp(args[i], "<") == 0) {
      input_redirect = 1;
      input_index = i;
      break;
    }
    if (strcmp(args[i], ">") == 0) {
      output_redirect = 1;
      output_index = i;
      break;
    }
  }

  if (input_redirect || output_redirect) {
    execute_with_redirection(args, *background, input_redirect, output_redirect);
  }
}

// Execute command with I/O redirection
void execute_with_redirection(char **args, int background, int is_input_redirect, int is_output_redirect) {
  pid_t pid = fork();
  if (pid == -1) {
    perror("fork failed");
    return;
  } else if (pid == 0) {
    // Child process
    int redirect_index = is_input_redirect ? 1 : 2;

    if (is_input_redirect) {
      int input_fd = open(args[redirect_index], O_RDONLY);
      if (input_fd == -1) {
        perror("Input file open failed");
        exit(1);
      }
      dup2(input_fd, STDIN_FILENO);
      close(input_fd);
      args[redirect_index - 1] = NULL;
    }

    if (is_output_redirect) {
      int output_fd = open(args[redirect_index], O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (output_fd == -1) {
        perror("Output file open failed");
        exit(1);
      }
      dup2(output_fd, STDOUT_FILENO);
      close(output_fd);
      args[redirect_index - 1] = NULL;
    }

    if (execvp(args[0], args) == -1) {
      perror("execvp failed");
      exit(1);
    }
  } else {
    // Parent process
    if (!background) {
      foreground_pid =pid;
      setup_timer();
      int status;
      waitpid(pid, &status, 0);  // Wait for foreground process to finish
      cancel_timer();
      foreground_pid = -1;
    } else {
      printf("Background process PID: %d\n", pid);
    }
  }
}

