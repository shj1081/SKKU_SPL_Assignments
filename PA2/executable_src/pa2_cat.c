#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void print_file(FILE* fp) {
    // using putchar to print each character
    int c;
    while ((c = fgetc(fp)) != EOF) {
        putchar(c);
        fflush(stdout);  // flush stdout to ensure output is printed immediately
    }
}

int main(int argc, char* argv[]) {
    int opt;
    // getopt doesn't automatically set optind to 1; it's initialized to 1 by the system.
    while ((opt = getopt(argc, argv, "h")) != -1) {
        switch (opt) {
            case 'h':
                printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
                printf("Concatenate FILE(s) to standard output.\n\n");
                printf("With no FILE, or when FILE is -, read standard input.\n\n");
                printf("  -h display this help and exit\n");
                exit(0);
            default:
                fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], opt);
                fprintf(stderr, "Try '%s -h' for more information.\n", argv[0]);
                exit(1);
        }
    }

    // if no arguments are provided, read from stdins
    if (optind == argc || (argc == 2 && strcmp(argv[1], "-") == 0)) {
        print_file(stdin);
    }

    // if arguments are provided, read from files
    else {
        struct stat statbuf;                     // struct to store file information
        for (int i = optind; i < argc; i++) {    // loop through all files
            if (stat(argv[i], &statbuf) == 0) {  // if file exists
                if (S_ISDIR(statbuf.st_mode)) {  // if file is a directory
                    fprintf(stderr, "pa2_cat: %s: Is a directory\n", argv[i]);
                    exit(1);
                }
            }

            FILE* file = fopen(argv[i], "r");
            // if file is NULL, print error message and continue to next file
            if (file == NULL) {  // failed to open file
                // if file doesnt exist,
                if (errno == ENOENT) {
                    fprintf(stderr, "pa2_cat: %s: No such file or directory\n", argv[i]);
                }
                // if file is a directory,
                else if (errno == EISDIR) {  // if file is a directorys
                    fprintf(stderr, "pa2_cat: %s: Is a directory\n", argv[i]);
                }
                // if file is not readable,
                else if (errno == EACCES) {
                    fprintf(stderr, "pa2_cat: %s: Permission denied\n", argv[i]);
                }
                // other errors
                else {
                    fprintf(stderr, "pa2_cat: %s: %s\n", argv[i], strerror(errno));
                }
                exit(1);
            }

            print_file(file);
            fclose(file);
        }
    }

    exit(0);
}
