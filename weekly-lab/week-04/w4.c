#include <unistd.h> // read, write
#include <fcntl.h> // open
#include <stdio.h> // perror
#include <stdlib.h> // exit

long long int line_number = 1;

void print_line_number(int fd_new) {
    char line_number_str[10];
    long long int temp = line_number;

    // get the digits of the line number
    int digits = 0;
    while (temp > 0) {
        temp = temp / 10;
        digits++;
    }

    // convert the line number to a string
    temp = line_number;
    for (int i = digits - 1; i >= 0; i--) {
        line_number_str[i] = (temp % 10) + '0';
        temp = temp / 10;
    }

    // write the line number to the new file
    write(fd_new, line_number_str, digits);
    write(fd_new, " | ", 3);

    // increment the line number
    line_number++;
}

int main(int argc, char *argv[]) {

    // Check if the number of arguments is correct
    if (argc <= 1 || argc > 2) {
        perror("Usage: ./ex4 <filename>");
        exit(1);
    }
    
    // get the filename and create a new filename
    char *filename = argv[1];
    char new_filename[4096];
    int i;
    for (i = 0; filename[i] != '\0'; i++) {
        new_filename[i] = filename[i];
    }

    i = i - 4; 
    new_filename[i++] = '_';
    new_filename[i++] = 'n';
    new_filename[i++] = 'u';
    new_filename[i++] = 'm';
    new_filename[i++] = '.';
    new_filename[i++] = 't';
    new_filename[i++] = 'x';
    new_filename[i++] = 't';
    new_filename[i] = '\0';

    // open the original file
    int fd_og;
    int fd_new;
    if ((fd_og = open(filename, O_RDONLY)) == -1) {
        perror("Error opening the file");
        exit(1);
    }

    // open the new file
    if ((fd_new = open(new_filename, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1) {
        perror("Error opening the new file");
        exit(1);
    }

    // read the original file line by line and write to the new file with line numbers
    char write_buffer[4096];
    int read_bytes;
    int buffer_idx;

    // write the first line number
    print_line_number(fd_new);

    // from 2nd line, write the line number after '\n'
    while ((read_bytes = read(fd_og, write_buffer, 4096)) > 0) {
        buffer_idx = 0;
        for (int i = 0; i < read_bytes; i++) {
            if (write_buffer[i] == '\n') {
                write(fd_new, write_buffer + buffer_idx, i - buffer_idx + 1);
                buffer_idx = i + 1;
                if (read_bytes != 4096 && i + 1 == read_bytes) {
                    break;
                }
                print_line_number(fd_new);
            }
        }

        // write the remaining characters
        write(fd_new, write_buffer + buffer_idx, read_bytes - buffer_idx);
    }

    close(fd_og);
    close(fd_new);

    return 0;
}