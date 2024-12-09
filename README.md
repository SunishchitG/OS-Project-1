# Shell Design and Documentation


## Design Choices
The shell implementation presented here incorporates several key design choices:

### 1. *Modular Structure*
- *Function-Based Organization*: 
  - The code is divided into well-defined functions, each responsible for a specific task. This modular approach enhances readability, maintainability, and reusability.
- *Global Variables*: 
  - The foreground_pid and custom_env variables are used to manage foreground processes and custom environment variables, respectively. While global variables can be convenient, it's important to use them judiciously to avoid potential side effects.

### 2. *Signal Handling*
- *SIGINT and SIGALRM:*: 
  The shell handles the SIGINT (Ctrl+C) and SIGALRM signals to gracefully interrupt foreground processes and implement a timeout mechanism.
- *Foreground Process Management*: 
  The foreground_pid variable tracks the PID of the currently running foreground process. This information is used to send termination signals and manage the timer.

### 3. *Input Parsing and Command Execution*
- *Tokenization*: 
  The parse_input function tokenizes the user input into individual arguments, considering environment variables.
- *Command Execution*: 
  The execute_command function forks a child process to execute the command. It handles both foreground and background processes, as well as I/O redirection.
- *Built-in Commands*: 
  The shell supports several built-in commands (exit, cd, pwd, echo, env, and setenv) to provide essential functionality.

### 4. *I/O Redirection*
- *Input and Output Redirection*: 
  The handle_io_redirection function identifies input and output redirection operators (< and >), and the execute_with_redirection function handles the actual redirection using open and dup2 system calls.

### 5. *Environment Variables*
- *Custom Environment Variables*: 
  The custom_env array allows for the definition of custom environment variables.
- *Environment Variable Expansion*: 
 The parse_input function expands environment variables within the command line.

---

## Documentation

The codebase is well-documented with clear and concise comments that explain the purpose and functionality of each component.

### Key Documentation Points
- *Function Descriptions*: Each function includes a brief description of its purpose.
- *Variable Explanations*: Key variables are documented to clarify their meaning and usage.
- *Error Handling*: Mechanisms for handling errors, including system call validation and error messages, are explained.
- *Signal Handling*: The purpose of signals and their handling is documented.
- *I/O Redirection*: The implementation and usage of open and dup2 system calls for redirection are described.
- *Environment Variables*: Handling and expansion of both built-in and custom environment variables are explained.

---

## Additional Considerations

1. *Error Handling*: 
   - The shell could be further enhanced by adding more informative error messages and potentially logging errors to a file.

2. *Security*: 
   - Input validation and sanitization are critical to mitigate security risks when
