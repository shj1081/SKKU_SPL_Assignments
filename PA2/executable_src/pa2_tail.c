#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>  // Add this line
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEFAULT_NUM_LINES 10
#define BUFFER_SIZE 4096

void print_usage() {
    printf("Usage: pa2_tail [OPTION] [FILE]\n");
    printf("Prints the last lines of a FILE to standard output.\n");
    printf("If the file has fewer lines, it will print the whole file without padding.\n");
    printf("If no FILE is provided, or FILE is -, read standard input.\n");
    printf("Options:\n");
    printf("-n NUM   : Prints up to NUM lines instead of %d\n", DEFAULT_NUM_LINES);
    printf("-h       : Display this help message\n");
}

void print_file(FILE *fp, int num_lines) {
    if (num_lines <= 0) return;  // Early exit if no lines are to be printed.

    char lines_buffer[num_lines][BUFFER_SIZE];  // Buffer to hold the last num_lines lines.
    int current_index = 0;                      // Current index for the circular buffer.
    char buf[BUFFER_SIZE];

    // Initialize circular buffer
    for (int i = 0; i < num_lines; i++) {
        lines_buffer[i][0] = '\0';
    }

    // Read the file line by line into the circular buffer
    while (fgets(buf, BUFFER_SIZE, fp) != NULL) {
        strcpy(lines_buffer[current_index % num_lines], buf);
        current_index++;
    }

    // Calculate start index and the total number of lines to print
    int start_index = current_index > num_lines ? current_index % num_lines : 0;
    int lines_to_print = current_index < num_lines ? current_index : num_lines;

    // Print the last num_lines lines from the circular buffer
    for (int i = 0; i < lines_to_print; i++) {
        printf("%s", lines_buffer[(start_index + i) % num_lines]);
    }
}

int main(int argc, char *argv[]) {
    int num_lines = DEFAULT_NUM_LINES;
    int opt;

    while ((opt = getopt(argc, argv, "n:h")) != -1) {
        switch (opt) {
            case 'n':
                if (isdigit(optarg[0])) {
                    num_lines = atoi(optarg);
                } else {
                    fprintf(stderr, "pa2_head: invalid number of lines: '%s'\n", optarg);
                    return 1;
                }
                break;
            case 'h':
                print_usage();
                return 0;
            default:
                print_usage();
                return 1;
        }
    }

    if (optind == argc || strcmp(argv[optind], "-") == 0) {
        // Read from stdin
        print_file(stdin, num_lines);
    } else {
        struct stat statbuf;                     // struct to store file information
        for (int i = optind; i < argc; i++) {    // loop through all files
            if (stat(argv[i], &statbuf) == 0) {  // if file exists
                if (S_ISDIR(statbuf.st_mode)) {  // if file is a directory
                    fprintf(stderr, "pa2_tail: %s: Is a directory\n", argv[i]);
                    exit(1);
                }
            }

            FILE *file = fopen(argv[i], "r");
            if (!file) {
                fprintf(stderr, "pa2_tail: cannot open '%s' for reading: %s\n", argv[i], strerror(errno));
                continue;  // Continue to the next file instead of exiting
            }
            print_file(file, num_lines);
            fclose(file);
        }
    }

    exit(0);
}