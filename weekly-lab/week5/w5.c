#include <linux/limits.h> // PATH_MAX 
#include <readline/history.h> // add_history 
#include <readline/readline.h> // readline 
#include <stdio.h> // printf 
#include <stdlib.h> // free 
#include <string.h> // strtok_r
#include <sys/wait.h> // wait
#include <unistd.h> // fork, execvp

#define MAX_ARGS 1000

int main() {
    while(1) {
        // readline
        char* cmd = readline("$ ");

        if (cmd == NULL) {
            printf("Error: Failed to read input\n"); 
            wait(NULL);
            exit(1);
        }

        add_history(cmd);

        // strtok_r
        char* args[MAX_ARGS]; 
        char* save_ptr;
        char* ptr = strtok_r(cmd, " ", &save_ptr); 
        int i = 0;
        while (ptr != NULL) { 
            if (i >= MAX_ARGS - 1) { 
                printf("Error: Too many arguments\n"); 
                exit(2);
            }
            args[i++] = ptr;
            ptr = strtok_r(NULL, " ", &save_ptr); 
        }
        args[i] = NULL;
        
        if (strcmp(args[0], "exit") == 0) {
            if (args[1] == NULL) {
                printf("exit\n");
                exit(0);
            } else {
                printf("exit\n");
                exit(atoi(args[1]));
            }
        }

        if (fork() == 0) {
            execvp(args[0], args);

            // only executed if execvp fails
            printf("%s: command not found\n", args[0]);
            exit(1);
        } else {
            // wait for child process to finish
            wait(NULL);
        }
    }
}
