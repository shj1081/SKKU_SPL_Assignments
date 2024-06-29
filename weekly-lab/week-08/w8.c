#include <fcntl.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
// Add other header files you need here

#define MAX_TOKENS 1000

// Helper enums, structs, functions start

typedef enum {
    TOKEN_COMMAND,
    TOKEN_ARGUMENT,
    TOKEN_PIPE,             // |
    TOKEN_REDIRECT_IN,      // <
    TOKEN_REDIRECT_OUT,     // >
    TOKEN_REDIRECT_APPEND,  // >>
    TOKEN_END,
} TokenType;

typedef struct token {
    TokenType type;
    char* data;
} Token;

// only for this exercise, not PA2
// since only one IPC is being used
typedef struct {
    Token operator;
    char* arg0[MAX_TOKENS];
    char* arg1[MAX_TOKENS];
} Command;

// These have been implemented already below
int is_special_char(char c);
int is_pipe(Token token);
int is_redirection(Token token);
void lex(char* cmd, Token* tokens);
void free_tokens(Token* tokens);
int run_command(char* command, char* arg[]);
int evaluate(char* cmd);

// Helper enums, structs, functions end

// if you need to add helper functions, structs, enums, you can add them here
// end

// TODO: implement this function
int parse(Token* tokens, Command* command) {
    // For this exercise (not PA2), there are only one out of three cases at a
    // given time
    // 1. Simple command (+ args)    (operator: TOKEN_END for convenience)
    // 2. Pipe        ( a | b)       (operator: TOKEN_PIPE)
    // 3. Redirection (a>b, a>>b, a<b)       (operator: TOKEN_REDIRECT_*)
    // In the actual PA2, you have to consider multiple pipes and redirections

    // But for this exercise, you can just fill the command struct with the
    // values you need for interpret

    // initialize the command struct
    command->operator=(Token){.type = TOKEN_END, .data = NULL};  // initialize the operator to TOKEN_END
    int is_arg0 = 1;                                             // flag to check if the token is for arg0 or arg1
    int arg0_i = 0;                                              // index for arg0
    int arg1_i = 0;                                              // index for arg1

    for (int i = 0; tokens[i].type != TOKEN_END; i++) {
        // if the token is a pipe, set the operator and change the flag
        if (is_pipe(tokens[i])) {
            command->operator= tokens[i];
            is_arg0 = 0;
            continue;
        }

        // if the token is a redirection, set the operator and change the flag
        if (is_redirection(tokens[i])) {
            command->operator= tokens[i];
            is_arg0 = 0;
            continue;
        }

        // if the token is not a pipe or redirection, add it to the corresponding arg
        if (is_arg0) {
            command->arg0[arg0_i++] = tokens[i].data;
        } else {
            command->arg1[arg1_i++] = tokens[i].data;
        }
    }
    command->arg0[arg0_i] = NULL;
    command->arg1[arg1_i] = NULL;
    return 0;
}

// TODO: implement this function
int interpret(Command command) {
    // if the input is empty, return
    if (command.arg0[0] == NULL) {
        return 0;
    }

    switch (command.operator.type) {
        case TOKEN_PIPE: {
            // since only two processes are involved
            // you have to consider the actual number of pids in PA2
            pid_t pids[2];

            // since only one pair of pipe is involved
            int pipefd[2];

            // create the pipe
            if (pipe(pipefd) == -1) {
                perror("pipe");
                exit(1);
            }

            if ((pids[0] = fork()) == 0) {
                close(pipefd[0]);  // close the read end of the pipe
                dup2(pipefd[1], STDOUT_FILENO);
                execvp(command.arg0[0], command.arg0);
                perror("execvp");
                exit(1);
            }

            if ((pids[1] = fork()) == 0) {
                waitpid(pids[0], NULL, 0);
                close(pipefd[1]);  // close the write end of the pipe

                // get the input from the pipe
                dup2(pipefd[0], STDIN_FILENO);
                execvp(command.arg1[0], command.arg1);
                perror("execvp");
                exit(1);
            }

            // close the pipe
            close(pipefd[0]);
            close(pipefd[1]);

            // wait for the child processes
            waitpid(pids[0], NULL, 0);
            waitpid(pids[1], NULL, 0);

            break;
        }

        case TOKEN_REDIRECT_IN: {
            // fork a child process
            pid_t pid = fork();

            if (pid == 0) {
                // open the file
                int fd = open(command.arg1[0], O_RDONLY);
                if (fd == -1) {
                    perror("open");
                    exit(1);
                }

                // redirect stdin to the file
                if (dup2(fd, STDIN_FILENO) == -1) {
                    perror("dup2");
                    exit(1);
                }

                // run the command
                execvp(command.arg0[0], command.arg0);
            } else {
                // wait for the child process
                waitpid(pid, NULL, 0);
            }

            break;
        }
        case TOKEN_REDIRECT_OUT: {
            // fork a child process
            pid_t pid = fork();

            if (pid == 0) {
                // open the file
                int fd = open(command.arg1[0], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    perror("open");
                    exit(1);
                }

                // redirect stdout to the file
                if (dup2(fd, STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(1);
                }

                // run the command
                execvp(command.arg0[0], command.arg0);

            } else {
                // wait for the child process
                waitpid(pid, NULL, 0);
            }
            break;
        }
        case TOKEN_REDIRECT_APPEND: {
            // fork a child process
            pid_t pid = fork();

            if (pid == 0) {
                // open the file
                int fd = open(command.arg1[0], O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd == -1) {
                    perror("open");
                    exit(1);
                }

                // redirect stdout to the file
                if (dup2(fd, STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(1);
                }

                // run the command
                execvp(command.arg0[0], command.arg0);
            } else {
                // wait for the child process
                waitpid(pid, NULL, 0);
            }
            break;
        }
        case TOKEN_END:
            // Normal case
            run_command(command.arg0[0], command.arg0);
            break;

        default:
            fprintf(stderr, "Invalid operator: %s\n", command.operator.data);
            exit(1);
            break;
    }
    return 0;
}

// You don't need to change these functions
// These are helper functions

int is_special_char(char c) {
    return c == '<' || c == '>' || c == '|' || c == '\'' || c == '"' ||
           c == '\0' || isspace(c);
}

int is_pipe(Token token) {
    return token.type == TOKEN_PIPE;
}

int is_redirection(Token token) {
    return token.type == TOKEN_REDIRECT_IN || token.type == TOKEN_REDIRECT_OUT ||
           token.type == TOKEN_REDIRECT_APPEND;
}

// functon for lexing the command (lexing : converting string to tokens)
void lex(char* cmd, Token* tokens) {
    int token_i = 0;

    for (const char* curr_char = cmd; *curr_char != '\0'; curr_char++) {
        if (isspace(*curr_char))
            continue;

        switch (*curr_char) {
            case '|':
                tokens[token_i++] = (Token){.type = TOKEN_PIPE, .data = "|"};
                break;
            case '<':
                tokens[token_i++] = (Token){.type = TOKEN_REDIRECT_IN, .data = "<"};
                break;
            case '>':
                if (*(curr_char + 1) == '>') {
                    tokens[token_i++] =
                        (Token){.type = TOKEN_REDIRECT_APPEND, .data = ">>"};
                    curr_char++;
                } else {
                    tokens[token_i++] = (Token){.type = TOKEN_REDIRECT_OUT, .data = ">"};
                }
                break;
            case '\'':
            case '\"': {
                // not required for PA2, it just makes checking commands much easier
                // since some commands almost require quotes (i.e awk)
                char quote = *curr_char;
                const char* start = ++curr_char;
                while (*curr_char != quote && *curr_char != '\0') {
                    curr_char++;
                }
                tokens[token_i++] = (Token){
                    .type = TOKEN_ARGUMENT,
                    .data = strndup(start, curr_char - start),
                };
            } break;
            default: {
                const char* command_start = curr_char;

                TokenType type = (is_pipe(tokens[token_i - 1]) || token_i == 0)
                                     ? TOKEN_COMMAND
                                     : TOKEN_ARGUMENT;

                while (!is_special_char(*curr_char)) {
                    curr_char++;
                }

                tokens[token_i++] =
                    (Token){.type = type,
                            .data = strndup(command_start, curr_char - command_start)};
                curr_char--;
                break;
            }
        }
    }

    tokens[token_i] = (Token){.type = TOKEN_END, .data = NULL};
}

void free_tokens(Token* tokens) {
    for (int i = 0; tokens[i].type != TOKEN_END; i++) {
        if (tokens[i].type == TOKEN_COMMAND || tokens[i].type == TOKEN_ARGUMENT) {
            free(tokens[i].data);
        }
    }
}

int run_command(char* command, char* arg[]) {
    if (strncmp(command, "exit", 5) == 0) {
        if (arg[1] != NULL) {
            // In PA2, don't do this. You have to clean up the shell before closing.
            exit(strtol(arg[1], NULL, 10));
        }
        exit(0);
    }

    pid_t pid;
    int child_status;

    switch (pid = fork()) {
        case -1:
            perror("fork");
            exit(1);
            break;
        case 0:
            execvp(command, arg);
            perror("execvp");
            exit(1);
            break;
        default:
            pid = wait(&child_status);
            break;
    }

    return 0;
}

int evaluate(char* cmd) {
    Token tokens[MAX_TOKENS];
    Command command;  // don't use this in PA2
    lex(cmd, tokens);
    parse(tokens, &command);
    interpret(command);
    free_tokens(tokens);
    return 0;
}

int main() {
    size_t size;
    char* cmd;
    char* ptr;

    char* arg[128];
    char* arg2[128];
    char path[128];
    char path2[128];
    int child_status;
    int fd[2];
    int fdr;

    while (1) {
        int num = 0;

        char* cmd;

        if ((cmd = readline("$ ")) == NULL)
            break;

        add_history(cmd);
        evaluate(cmd);
        free(cmd);
    }

    return 0;
}