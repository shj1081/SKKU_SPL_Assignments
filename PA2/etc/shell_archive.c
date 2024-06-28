#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// =========== constants ===========
// Maximum number of tokens in a command line
#define MAX_TOKENS 1000

// =========== global variables ===========

pid_t shell_pgid;            // The process group ID of the shell
int shell_terminal;          // The file descriptor of the terminal for the shell
int job_num = 1;             // The job number
char* shell_name = "shell";  // The name of the shell
char* initial_directory;     // The initial directory of the shell

// =========== structure, enum ===========
// Enum to represent the type of token
typedef enum {
    TOKEN_COMMAND,
    TOKEN_ARGUMENT,
    TOKEN_PIPE,             // |
    TOKEN_REDIRECT_IN,      // <
    TOKEN_REDIRECT_OUT,     // >
    TOKEN_REDIRECT_APPEND,  // >>
    TOKEN_END,
    TOKEN_BACKGROUND,
} TokenType;

// Enum to represent the status of a job or process
typedef enum {
    Ready,
    RUNNING,
    STOPPED,
    DONE
} Status;

// Struct to hold token information
typedef struct token {
    TokenType type;
    char* data;
} Token;

// Struct to hold process information (subprocesses of jobs)
typedef struct Process {
    pid_t pid;               // Process ID
    Status status;           // Status of the process
    struct Process* next;    // Pointer to the next process
    char* args[MAX_TOKENS];  // Arguments for this command
    char* output_file;       // Output file for redirection
    int is_append;           // Append flag for redirection
    int is_next_pipe;        // Flag to indicate if the next process is piped
} Process;

// Struct to hold job information
typedef struct Job {
    int job_num;                    // Job number
    pid_t pgid;                     // Process group ID
    struct Job* next;               // Pointer to the next job
    struct Process* first_process;  // Pointer to the process list
    char* input_file;               // Input file for redirection (for the first process)
    char* output_file;              // Output file for redirection (for the last process)
    Status status;                  // Status of the job
    int is_background;              // Background flag
    int is_notified;                // Notification flag that the job is notified the status to the user
} Job;

Job* first_job = NULL;  // Pointer to the first job (linked list head for active jobs)

// =========== function prototype ===========
// handling input
int is_special_char(char c);         // Check if a character is a special character
int is_pipe(Token token);            // Check if a token is a pipe
int is_redirection(Token token);     // Check if a token is a redirection
void lex(char* cmd, Token* tokens);  // Tokenize the command line
Job* parse(Token* tokens);           // Parse the tokens and create the job structure and return the job

// handling memory
void free_tokens(Token* tokens);      // Free the memory allocated for the tokens
void free_job(Job* job);              // Free the memory allocated for the job
void free_process(Process* process);  // Free the memory allocated for the process

// handling command
int is_builtin(char* args[]);                // Check if the command is a built-in command and return the index of the command (fail: -1)
int is_self_implemented_exec(char* args[]);  // Check if the command is a self-implemented command and return the index of the command (fail: -1)
void builtin_fg(char* args[]);               // Implement the fg command
void builtin_bg(char* args[]);               // Implement the bg command
void builtin_jobs(char* args[]);             // Implement the jobs command
void builtin_cd(char* args[]);               // Implement the cd command
void builtin_pwd(char* args[]);              // Implement the pwd command
void builtin_exit(char* args[]);             // Implement the exit command

// handling jobs and processes
void put_job_in_background(Job* job, int cont);    // Put the job in the background (cont: flag to continue the job)
void put_job_in_foreground(Job* job, int cont);    // Put the job in the foreground (cont: flag to continue the job)
void launch_job(Job* job);                         // Launch the job
int update_process_status(pid_t pid, int status);  // Update the status of the process
void update_status();                              // Update the status of each process in the job list
void wait_for_job_to_finish(Job* job);             // Wait for the job to finish
void sigchld_handler(int sig);                     // Signal handler for SIGCHLD

// test
void print_job_list();  // Print the job list

// =========== helper function ===========
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

void lex(char* cmd, Token* tokens) {  // Tokenize the command
    int token_i = 0;

    for (const char* curr_char = cmd; *curr_char != '\0'; curr_char++) {
        if (isspace(*curr_char))  // Skip whitespace
            continue;

        switch (*curr_char) {  // Check for special characters
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
            case '&':
                tokens[token_i++] = (Token){.type = TOKEN_BACKGROUND, .data = "&"};
                break;
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

    tokens[token_i] = (Token){.type = TOKEN_END, .data = NULL};  // Add the end token
}

Job* parse(Token* tokens) {
    // if the first token is end, return NULL
    if (tokens[0].type == TOKEN_END) {
        return NULL;
    }

    // create job and process
    Job* job = (Job*)malloc(sizeof(Job));
    if (job == NULL) {
        perror("malloc");
        exit(1);
    }
    Process* first_process = NULL;
    Process* current_process = NULL;
    int is_background = 0;  // flag for background job
    int arg_idx = 0;

    for (int i = 0; tokens[i].type != TOKEN_END; i++) {
        switch (tokens[i].type) {
            case TOKEN_COMMAND:
                // if first process
                if (first_process == NULL) {
                    first_process = (Process*)malloc(sizeof(Process));
                    if (first_process == NULL) {
                        perror("malloc");
                        exit(1);
                    }
                    current_process = first_process;
                }
                // if not first process
                else {
                    current_process->next = (Process*)malloc(sizeof(Process));
                    if (current_process->next == NULL) {
                        perror("malloc");
                        exit(1);
                    }
                    current_process = current_process->next;
                }
                // initialize process
                current_process->pid = 0;
                current_process->next = NULL;
                current_process->output_file = NULL;
                current_process->is_append = 0;
                current_process->is_next_pipe = 0;
                current_process->args[arg_idx++] = strdup(tokens[i].data);
                current_process->args[arg_idx] = NULL;
                break;
            case TOKEN_ARGUMENT:
                current_process->args[arg_idx++] = strdup(tokens[i].data);
                current_process->args[arg_idx] = NULL;  // maintain NULL terminated
                break;
            case TOKEN_PIPE:
                current_process->args[arg_idx] = NULL;  // maintain NULL terminated
                current_process->is_next_pipe = 1;      // set next pipe flag
                // if output file is set and is_next_pipe is set, reset output file and make `tee` process
                if (current_process->output_file != NULL) {
                    Process* tee_process = (Process*)malloc(sizeof(Process));
                    tee_process->pid = 0;
                    tee_process->next = NULL;
                    tee_process->output_file = NULL;
                    tee_process->is_next_pipe = 1;
                    tee_process->args[0] = strdup("tee");
                    if (current_process->is_append == 1) {
                        tee_process->args[1] = strdup("-a");
                        tee_process->args[2] = strdup(current_process->output_file);
                        tee_process->args[3] = NULL;
                    } else {
                        tee_process->args[1] = strdup(current_process->output_file);
                        tee_process->args[2] = NULL;
                    }
                    current_process->is_append = 0;  // should be reset after tee process setup
                    current_process->next = tee_process;
                    current_process->output_file = NULL;
                    current_process = tee_process;
                }
                arg_idx = 0;  // reset arg index for next process
                break;
            case TOKEN_REDIRECT_IN:
                i++;                                       // skip the next token
                job->input_file = strdup(tokens[i].data);  // input file for the first process
                break;
            case TOKEN_REDIRECT_OUT:
                i++;                                                    // skip the next token
                current_process->output_file = strdup(tokens[i].data);  // output file for the current(last) process
                current_process->is_append = 0;                         // reset append flag
                break;
            case TOKEN_REDIRECT_APPEND:
                i++;                                                    // skip the next token
                current_process->output_file = strdup(tokens[i].data);  // output file for the current(last) process
                current_process->is_append = 1;                         // set append flag
                break;
            case TOKEN_BACKGROUND:
                is_background = 1;  // set background flag for job
                break;
            default:
                break;
        }
    }
    // set the job information
    job->job_num = job_num++;
    job->pgid = 0;
    job->next = NULL;
    job->first_process = first_process;
    job->output_file = current_process->output_file;
    job->status = Ready;
    job->is_background = is_background;
    job->is_notified = 0;

    return job;
}

void free_tokens(Token* tokens) {
    for (int i = 0; tokens[i].type != TOKEN_END; i++) {
        if (tokens[i].type == TOKEN_COMMAND || tokens[i].type == TOKEN_ARGUMENT) {
            free(tokens[i].data);
        }
    }
}

void free_process(Process* process) {
    for (int i = 0; process->args[i] != NULL; i++) {
        free(process->args[i]);
    }

    if (process->output_file != NULL) {
        free(process->output_file);
    }

    if (process->next != NULL) {
        free_process(process->next);
    }
    free(process);
}

void free_job(Job* job) {
    Process* process = job->first_process;
    Process* next_process;
    while (process != NULL) {
        next_process = process->next;
        free_process(process);
        process = next_process;
    }

    if (job->input_file != NULL) {
        free(job->input_file);
    }
    if (job->output_file != NULL) {
        free(job->output_file);
    }
    free(job);
}

int is_builtin(char* args[]) {
    char* builtins[] = {"pwd", "exit", "jobs"};
    for (int i = 0; builtins[i] != NULL; i++) {
        if (strcmp(args[0], builtins[i]) == 0) {
            return i;
        }
    }
    return -1;
}

// TODO: implement the built-in commands
int is_self_implemented_exec(char* args[]) {
    return 0;
}

void builtin_fg(char* args[]) {
    // if no argument, put the first stopped background job in the foreground
    if (args[1] == NULL) {
        Job* job = first_job;
        while (job != NULL) {
            if (job->is_background) {
                put_job_in_foreground(job, 1);
                return;
            }
            job = job->next;
        }
        printf("%s:fg: current: no such job\n", shell_name);
    }
    // if argument is given, put the job in the foreground
    else {
        int job_num = atoi(args[1] + 1);
        Job* job = first_job;
        while (job != NULL) {
            if (job->job_num == job_num && job->is_background) {
                put_job_in_foreground(job, 1);
                return;
            }
            job = job->next;
        }
        printf("%s:fg: %d: no such job\n", shell_name, job_num);
    }
}

void builtin_bg(char* args[]) {
    // if no argument, put the first stopped background job in the background
    if (args[1] == NULL) {
        Job* job = first_job;
        while (job != NULL) {
            if (job->is_background && job->status == STOPPED) {
                put_job_in_background(job, 1);
                return;
            }
            job = job->next;
        }
        printf("%s:bg: current: no such job\n", shell_name);
    }

    // if argument is given, put the job in the background
    else {
        int job_num = atoi(args[1] + 1);
        Job* job = first_job;
        while (job != NULL) {
            if (job->job_num == job_num && job->status == STOPPED) {
                put_job_in_background(job, 1);
                return;
            }
            job = job->next;
        }
        printf("%s:bg: %d: no such job\n", shell_name, job_num);
    }
}

void builtin_jobs(char* args[]) {
    int recency = 0;
    Job* job = first_job->next;
    char* full_args[MAX_TOKENS];
    while (job != NULL) {
        if (job->status == RUNNING) {
            if (recency == 0) {
                printf("[%d]+ Running     ", job->job_num);
                for (int i = 0; job->first_process->args[i] != NULL; i++) {
                    printf("%s ", job->first_process->args[i]);
                }
                recency = 1;
            } else if (recency == 1) {
                printf("[%d]- Running     ", job->job_num);
                for (int i = 0; job->first_process->args[i] != NULL; i++) {
                    printf("%s ", job->first_process->args[i]);
                }
                recency = 2;
            } else {
                printf("[%d]  Running     ", job->job_num);
                for (int i = 0; job->first_process->args[i] != NULL; i++) {
                    printf("%s ", job->first_process->args[i]);
                }
            }
            if (job->is_background) {
                printf("&\n");
            } else {
                printf("\n");
            }
        }
        job = job->next;
    }
    return;
}
void builtin_cd(char* args[]) {
    int status;
    if (args[1] == NULL) {
        chdir(getenv("HOME"));
        return;
    }
    char* path = strdup(args[1]);
    status = chdir(path);
    free(path);

    // if the directory change is failed, print the error message
    if (status < 0) {
        switch (errno) {
            case EACCES:
                fprintf(stderr, "cd: %s: Permission denied\n", args[1]);
                break;
            case ENOENT:
                fprintf(stderr, "cd: %s: No such file or directory\n", args[1]);
                break;
            case ENOTDIR:
                fprintf(stderr, "cd: %s: Not a directory\n", args[1]);
                break;
            default:
                fprintf(stderr, "cd: %s: Error\n", args[1]);
                break;
        }
    }
    return;
}

void builtin_exit(char* args[]) {
    if (args[1] != NULL && args[2] != NULL) {
        fprintf(stderr, "%s: exit: too many arguments\n", shell_name);
        exit(2);
    }
    int code = 0;
    if (args[1] != NULL) {
        char* endptr;
        code = strtol(args[1], &endptr, 10);
        if (*endptr != '\0') {  // if the argument is not a number
            fprintf(stderr, "%s: %s: numeric argument required\n", shell_name, args[1]);
            exit(2);
        }
    }
    if (code >= 255) {
        exit(255);
    } else {
        exit(code);
    }
}

void put_job_in_foreground(Job* job, int cont) {
    // Put the job in the foreground
    tcsetpgrp(shell_terminal, job->pgid);
    job->is_background = 0;

    // Send a SIGCONT signal to the process group to continue
    if (cont) {
        if (kill(-job->pgid, SIGCONT) < 0) {
            perror("kill (SIGCONT)");
            exit(1);
        }
    }

    // Wait for the job to finish
    wait_for_job_to_finish(job);

    // Put the shell back in the foreground
    tcsetpgrp(shell_terminal, shell_pgid);
}

void put_job_in_background(Job* job, int cont) {
    // Put the job in the background
    job->is_background = 1;

    if (cont) {
        // Send a SIGCONT signal to the process group to continue negative pgid
        // negative pgid : the signal is sent to the entire process group)
        if (kill(-job->pgid, SIGCONT) < 0) {
            perror("error in sending SIGCONT");
            exit(1);
        }
    }
}

void launch_job(Job* job) {
    int status;                     // status of the child process for waitpid
    int pipefd[2];                  // pipe file descriptor (0 : read, 1 : write), we fork so only need 1 pipe
    int input_file = STDIN_FILENO;  // input from the pipe (default: stdin)
    int output_file;                // output to the pipe
    Process* process;               // process pointer to iterate through the job's processes

    // set the status of the job to running
    job->status = RUNNING;

    // update input file for the first process
    if (job->input_file != NULL) {
        input_file = open(job->input_file, O_RDONLY);
    }

    // check if the command is only 1 command
    if (strcmp(job->first_process->args[0], "fg") == 0) {
        job->status = DONE;
        builtin_fg(job->first_process->args);
        return;
    } else if (strcmp(job->first_process->args[0], "bg") == 0) {
        job->status = DONE;
        builtin_bg(job->first_process->args);
        return;
    } else if (strcmp(job->first_process->args[0], "exit") == 0) {
        job->status = DONE;
        builtin_exit(job->first_process->args);
        return;
    } else if (strcmp(job->first_process->args[0], "cd") == 0) {
        job->status = DONE;
        builtin_cd(job->first_process->args);
        return;
    }

    // link the new job to the head of the job list
    job->next = first_job;
    first_job = job;

    // iterate through the processes of the job
    for (process = job->first_process; process != NULL; process = process->next) {
        // create pipe if the next process is piped
        if (process->is_next_pipe) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                exit(1);
            }
            output_file = pipefd[1];
        }
        // if the next process is not piped, set the output file to the output file or stdout
        else if (process->output_file != NULL) {
            if (process->is_append) {
                output_file = open(process->output_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            } else {
                output_file = open(process->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }
        } else {
            output_file = STDOUT_FILENO;
        }

        // fork the process
        pid_t pid = fork();

        // error in forking
        if (pid < 0) {
            perror("fork");
            exit(1);
        }

        // child process (execute the process)
        if (pid == 0) {
            if (isatty(shell_terminal)) {
                pid_t process_pid = getpid();  // get the process id
                if (job->pgid == 0) {          // set the process group id to the process id (first process_pid)
                    job->pgid = process_pid;
                }
                setpgid(process_pid, job->pgid);  // set the process group id to the job's process group id

                if (!job->is_background) {                 // if the job is not background, put the job in the foreground
                    tcsetpgrp(shell_terminal, job->pgid);  // set the terminal to the job's process group id
                }

                // restore to the default signal handler (shell ignores some signals)
                signal(SIGINT, SIG_DFL);   // Ctrl+C : interrupt
                signal(SIGQUIT, SIG_DFL);  // Ctrl+`\` : quit from shell
                signal(SIGTSTP, SIG_DFL);  // Ctrl+Z : stop
                signal(SIGTTIN, SIG_DFL);  // background process trying to read from terminal
                signal(SIGTTOU, SIG_DFL);  // background process trying to write to terminal
            }

            // redirect the input and output file
            if (input_file != STDIN_FILENO) {
                dup2(input_file, STDIN_FILENO);
                close(input_file);
            }
            if (output_file != STDOUT_FILENO) {
                dup2(output_file, STDOUT_FILENO);
                close(output_file);
            }

            if (strcmp(process->args[0], "jobs") == 0) {
                builtin_jobs(process->args);
                exit(0);
            }

            // TODO: execute the process
            if (execvp(process->args[0], process->args) < 0) {
                fprintf(stderr, "%s: %s: command not found\n", shell_name, process->args[0]);
                exit(1);
            }
        }

        // parent process
        process->pid = pid;  // set the process id to the pid
        if (isatty(shell_terminal)) {
            if (job->pgid == 0) {  // set the process group id to the process id (first process_pid)
                job->pgid = pid;
            }
            // do in the parent process too (bcs of the race condition)
            setpgid(pid, job->pgid);  // set the process group id to the job's process group id
        }
        // close the input and output file
        if (input_file != STDIN_FILENO) {
            close(input_file);
        }
        if (output_file != STDOUT_FILENO) {
            close(output_file);
        }
        input_file = pipefd[0];  // set the input to the pipe
    }

    // if the shell is not interactive, just wait for the job to finish
    if (!isatty(shell_terminal)) {
        wait_for_job_to_finish(job);
    }
    // if shell is interactive , job should be put in the foreground or background
    else if (!job->is_background) {
        put_job_in_foreground(job, 0);  // contain waiting for the job to finish
    } else {
        printf("[%d] %d\n", job->job_num, job->pgid);
        put_job_in_background(job, 0);  // does not send SIGCONT for the first process
    }
}

int update_process_status(pid_t pid, int status) {
    Job* job;
    Process* process;

    if (pid > 0) {
        // Update the status for the process
        for (job = first_job; job; job = job->next) {
            for (process = job->first_process; process != NULL; process = process->next) {
                if (process->pid == pid) {
                    if (WIFEXITED(status) || WIFSIGNALED(status)) {  // if the process is done or killed
                        // if the process is the last process of the job
                        if (process->next == NULL) {
                            job->status = DONE;
                        }

                    } else if (WIFSTOPPED(status)) {  // if the process is stopped
                        job->status = STOPPED;
                        job->is_background = 1;
                        printf("\n[%d] Stopped %d\n", job->job_num, job->pgid);
                    }
                    return 0;
                }
            }
        }
        return -1;                             // Process not found
    } else if (pid == 0 || errno == ECHILD) {  // if the process is not found
        return -1;
    } else {
        perror("waitpid");
        return -1;
    }
}

void update_status() {
    int status;  // status of the child process for waitpid
    pid_t pid;   // process id

    do {
        // wait for any child process to change state
        pid = waitpid(-1, &status, WUNTRACED | WNOHANG);  // -1 : wait for any child process
    } while (!update_process_status(pid, status));        // iterate until waitpid returns -1
}

void wait_for_job_to_finish(Job* job) {
    // int status;  // status of the child process for waitpid
    // pid_t pid;   // process id

    // do {
    //     // wait for any child process to change state
    //     pid = waitpid(-1, &status, WUNTRACED);       // -1 : wait for any child process, return value
    // } while (!update_process_status(pid, status) &&  // iterate until waitpid returns -1 and job is done or stopped
    //          !job->status == DONE &&
    //          !job->status == STOPPED);

    int status;

    while (waitpid(-job->pgid, &status, WUNTRACED) > 0) {
        if (WIFSTOPPED(status)) {
            job->status = STOPPED;
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            job->status = DONE;
            // remove_job_from_job_list(job);
        }
    }

    update_status();  // Update the status of each job and process
    Job* iter_job = first_job;
    Job* prev_job = NULL;

    while (iter_job != NULL) {
        if (iter_job->status == DONE) {
            if (prev_job == NULL) {
                first_job = iter_job->next;
            } else {
                prev_job->next = iter_job->next;
            }
            Job* next_job = iter_job->next;
            free(iter_job);
            iter_job = next_job;

        } else {
            prev_job = iter_job;
            iter_job = iter_job->next;
        }
    }
}

void sigchld_handler(int sig) {
    update_status();  // Update the status of each job and process
}

void print_job_list() {
    Job* job = first_job;
    while (job != NULL) {
        printf("[%d] %d\n", job->pgid, job->pgid);
        printf("status: %d\n", job->status);
        Process* process = job->first_process;
        while (process != NULL) {
            printf("pid: %d\n", process->pid);
            printf("args: ");
            for (int i = 0; process->args[i] != NULL; i++) {
                printf("%s ", process->args[i]);
            }
            printf("\n");
            process = process->next;
        }
        job = job->next;
    }
}

// =========== main function ===========
int main() {
    // Set up the shell
    shell_terminal = STDIN_FILENO;        // Get the file descriptor of the terminal for the shell
    initial_directory = getcwd(NULL, 0);  // Get the initial directory of the shell

    if (isatty(shell_terminal)) {
        // Check if the shell is in the foreground
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);

        // signal handling
        signal(SIGINT, SIG_IGN);   // ignore Ctrl+C : interrupt
        signal(SIGQUIT, SIG_IGN);  // ignore Ctrl+`\` : quit from shell
        signal(SIGTSTP, SIG_IGN);  // ignore Ctrl+Z : stop
        signal(SIGTTIN, SIG_IGN);  // ignore background process trying to read from terminal
        signal(SIGTTOU, SIG_IGN);  // ignore background process trying to write to terminal

        // Put the shell in its own process group
        shell_pgid = getpid();
        if (setpgid(shell_pgid, shell_pgid) < 0) {
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }

        // Grab control of the terminal by making the shell the foreground process group on the terminal
        tcsetpgrp(shell_terminal, shell_pgid);
    }

    // Set up the signal handler for SIGCHLD
    signal(SIGCHLD, sigchld_handler);

    while (1) {
        printf("%s ", getcwd(NULL, 0));
        char* cmd = readline("$ ");
        if (cmd == NULL) break;
        add_history(cmd);
        Token tokens[MAX_TOKENS];      // Array to hold the tokens of the command line
        lex(cmd, tokens);              // Tokenize the command line
        Job* new_job = parse(tokens);  // Parse the tokens and create the job structure
        if (new_job == NULL) {         // if the job is empty, free the tokens and continue
            free_tokens(tokens);
            continue;
        }
        // Launch the job
        launch_job(new_job);
        // free the tokens
        free_tokens(tokens);
    }

    return 0;
}
