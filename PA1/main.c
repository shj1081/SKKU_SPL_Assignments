#include "util.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

// main function
int main(int argc, char *argv[]) {

    if (argc <= 1 || argc > 2) {
        write(1, "Usage: ./pa1 <filename>\n", 24);
        exit(1);
    }
    char *filename = argv[1];


    int fd;
    if ((fd = open(filename, O_RDONLY)) == -1) {
        write(1, "Error: File open failed\n", 24);
        exit(1);
    }

    /*
    search the word for user input in the file based on input form
    type 1: single keyword
    type 2: multiple keywords
    type 3: exact keyword
    type 4: simple regular expression
    PA1EXIT: exit the program
    */
    char userInput[BUFFER_SIZE];

    while(1) {
        // get the user input
        get_user_input(userInput);

        // check the form of the input and process
        if (!str_cmp(userInput, PA1EXIT)) {
            close(fd);
            exit(0);
        }

        to_lower(userInput);
        // Process type 3 : Searching the occurrences of consecutive words
        if (userInput[0] == '"') {
            process(fd, userInput, 3);
        }
        // Process type 4 : Searching simple regular expressing keyword locations
        else if (find_char(userInput, '*')) {
            seperate_words(userInput);
            word_count = 2;
            process(fd, userInput, 4);
        }
        // Process type 2: Searching the occurrences of multiple words
        else if ((word_count =  find_char(userInput, ' ') + 1) > 1) {
            seperate_words(userInput);
            process(fd, userInput, 2);

        }

        else if(userInput[0] == ' ' || userInput[0] == '\n' || userInput[0] == '\0') {
            continue;
        }
        else {
            process(fd, userInput, 1);
        }

        lseek(fd, 0, SEEK_SET);
        line_number = 1;
        write(1, "\n", 1);
    }

    return 0;
}
