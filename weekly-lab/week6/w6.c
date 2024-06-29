#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>


#define MAX_CRON_ENTRIES 1000
#define BUF_SIZE 512

// Signal handler for SIGCHLD
void sigchld_handler(int sig) {
    // Use waitpid to wait for all terminated children
    int status;
    // The use of WNOHANG ensures the call does not block if no child has exited
    while (waitpid(-1, &status, WNOHANG) > 0) {
        continue;
    }
}


int main(void) {
    setlogmask(LOG_MASK(LOG_EMERG));
    openlog("cron", LOG_PID, LOG_LPR);
    syslog(LOG_EMERG, "logging started");

    // Set up the signal handler for SIGCHLD
    signal(SIGCHLD, sigchld_handler);


    unsigned int pid;
    time_t t;
    struct tm *tm;
    int fd;
    char buf[BUF_SIZE];
    int fd0, fd1, fd2;


    // open the crontab file
    fd = open("/tmp/crontab", O_RDWR);
    if (errno == ENOENT) {
        perror("crontab does not exist");
        exit(1);
    }

    switch (fork()) {
        case -1:
            return -1;
        case 0:
            break;
        default:
            _exit(0);
    }

    if (setsid() < 0) {
        perror("Failed to create a new session");
        exit(2);
    }

    if (chdir("/") < 0) {
        perror("Failed to change directory to root directory");
        exit(3);
    }

    umask(0);

    for (int i = 0; i < 3; i++) {
        close(i);
    }

    fd0 = open("/dev/null", O_RDWR);
    fd1 = open("/dev/null", O_RDWR);
    fd2 = open("/dev/null", O_RDWR);    

    setlogmask(LOG_MASK(LOG_EMERG));
    openlog("cron", LOG_PID, LOG_LPR);


    if (fd < 0) {
        perror("Failed to open crontab file");
        exit(1);
    }

    char argv[MAX_CRON_ENTRIES][3][BUF_SIZE];
    int cron_idx = 0;
    ssize_t read_bytes;
   
    while ((read_bytes = read(fd, buf, BUF_SIZE)) > 0) {
        buf[read_bytes] = '\0';
        char *line = NULL;
        char *pos = buf;
        int rewind_offset = 0;

        // separate the lines using strtok_r
        while ((line = strtok_r(pos, "\n", &pos)) != NULL) {

            rewind_offset = strlen(line) + 1; // calculate the not fully read line length to rewind the file pointer
            
            // variables for strtok_r
            char* token = NULL;
            char* save_ptr;

            // tokenize the line using strtok_r
            token = strtok_r(line, " ", &save_ptr);
            int i = 0;
            while (token != NULL && i < 3) {
                strcpy(argv[cron_idx][i++], token);
                token = strtok_r(NULL, " ", &save_ptr);
            }
            cron_idx++;
        }

        // if the last line is not fully read, rewind the file pointer to read the last line in the next iteration
        if (read_bytes == BUF_SIZE) {
            lseek(fd, -rewind_offset, SEEK_CUR);
            cron_idx--; // the last line is not fully read
        }
    }


    while (1) {

        // update the current time (For check min , hour)
        // second is no need cause sleep(60 - tm->tm_sec % 60) means sleep until the next minute (second = 0)
        t = time(NULL);
        tm = localtime(&t);

        
        for (int i = 0; i < cron_idx; i++) {
            int min = (strcmp(argv[i][0], "*") == 0) ? -1 : strtol(argv[i][0], NULL, 10);
            int hour = (strcmp(argv[i][1], "*") == 0) ? -1 : strtol(argv[i][1], NULL, 10);

            

            // case 1: min hour cmd = * * cmd -> run every minute
            if (min == -1 && hour == -1) {
                    pid = fork();
                    if (pid == 0) {
                        execl("/bin/sh", "/bin/sh", "-c", argv[i][2], (char *)NULL);
                        perror("execl failed");
                        _exit(EXIT_FAILURE); // Ensure child process exits regardless.
                    } 
                    else {
                        waitpid(pid, NULL, WNOHANG);
                    }
            }

            // case 2: min hour cmd = min * cmd -> run every hour in min^th minute
            else if (min != -1 && hour == -1) {
                if (tm->tm_min == min) {
                    pid = fork();
                    if (pid == 0) {
                        execl("/bin/sh", "/bin/sh", "-c", argv[i][2], (char *)NULL);
                        perror("execl failed");
                        _exit(EXIT_FAILURE); // Ensure child process exits regardless.
                    } 
                    // else {
                    //     wait (NULL);
                    // }
                }
            }

            // case 3 : min hour cmd = min hour cmd -> run every day in min^th minute of hour^th hour
            else if (min != -1 && hour != -1) {
                if (tm->tm_min == min && tm->tm_hour == hour) {
                    pid = fork();
                    if (pid == 0) {
                        execl("/bin/sh", "/bin/sh", "-c", argv[i][2], (char *)NULL);
                        perror("execl failed");
                        _exit(EXIT_FAILURE); // Ensure child process exits regardless.
                    } 
                    // else {
                    //     wait (NULL);
                    // }
                }
            }

            else { // case 4: min hour cmd = * hour cmd -> run every day in hour^th hour
                if (tm->tm_hour == hour) {
                    pid = fork();
                    if (pid == 0) {
                        execl("/bin/sh", "/bin/sh", "-c", argv[i][2], (char *)NULL);
                        perror("execl failed");
                        _exit(EXIT_FAILURE); // Ensure child process exits regardless.
                    } 
                    // else {
                    //     wait (NULL);
                    //  }
                }
            }
        }
        // reap the zombie processes of the child processes
        while (waitpid(-1, NULL, WNOHANG) > 0){
            continue;
        }

        // sleep until the next minute (second = 0)
        t = time(NULL);
        tm = localtime(&t);

        sleep(60 - tm->tm_sec % 60);
    }
    close(fd);
    close(fd0);
    close(fd1);
    close(fd2);

    return 0;
}
