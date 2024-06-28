#include "util.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>


// Initialization of global variables
long long int line_number = 1;
char wordsArr[WORD_ARR_SIZE][WORD_SIZE];
int isContained[WORD_ARR_SIZE];
int word_count = 0;
int min_start_index[2];
int max_start_index[2];
char num_str[100];

int get_user_input(char *inputBuffer) {
    int bytes_read;

    // read the user input
    bytes_read = read(0, inputBuffer, BUFFER_SIZE);

    // check if the read was successful
    if (bytes_read == -1) {
        write(1, "Error: Reading user input failed\n", 34);
        return 0;
    }

    // change the last character to null terminator
    inputBuffer[bytes_read - 1] = '\0';

    return bytes_read-1;
}

void print_int(int num) {
    char num_str[12]; 
    int i = sizeof(num_str) - 1; 

    if (num == 0) {
        // Directly handle zero case
        write(1, "0", 1);
        return;
    }

    // Fill the buffer backward with digits
    num_str[i--] = '\0'; // Null-terminate the string
    while (num > 0) {
        num_str[i--] = (num % 10) + '0';
        num /= 10;
    }

    // Write from the first digit or the negative sign
    write(1, &num_str[i + 1], sizeof(num_str) - i - 2);
}


int str_cmp(const char* s1, const char* s2) {
    while(*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }

    // return the difference of the characters (0 if equal)
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int str_len(const char *str) {
    int i = 0;
    while (str[i] != '\0' && str[i] != '\n' && str[i] != '\r') {
        i++;
    }

    return i;
}


// function to find the character in the string 
// and return the number of characters found
int find_char(char *str, char c) {
    int i = 0;
    int count = 0;
    while (str[i] != '\0') {
        if (str[i] == c) {
            count++;
        }
        i++;
    }

    return count;
}


void to_lower(char *str) {
    int i = 0;
    while (str[i] != '\0') {
        if (str[i] >= 'A' && str[i] <= 'Z') {
            str[i] = str[i] + 32;
        }
        i++;
    }
}

int max(int a, int b) { return (a > b) ? a : b; }


// function to seperate the words in the string
void seperate_words(char *userInput) {
    int i = 0; // index for the user input
    int j = 0; // index for the words array
    int k = 0; // index for the characters in the word

    while (userInput[i] != '\0') {
        // check if the character is a space or *
        if (userInput[i] == ' ' || userInput[i] == '*') {
            wordsArr[j][k] = '\0'; // add null terminator
            j++; // move to the next word
            k = 0; // reset the character index
        } else {
            wordsArr[j][k] = userInput[i];
            k++;
        }
        i++;
    }

    // add the null terminator to the last word
    wordsArr[j][k] = '\0';

    return;
}

// process function to search the pattern in the file
void process(int fd, char *userInput, int type) {
    char file_buffer[BUFFER_SIZE];
    char line_buffer[BUFFER_SIZE];
    int line_buffer_index = 0;
    int read_bytes;


    if (type == 3) {
        userInput[str_len(userInput) - 1] = '\0';
    }

    while ((read_bytes = read(fd, file_buffer, BUFFER_SIZE)) > 0) {

        if (read_bytes != BUFFER_SIZE && file_buffer[read_bytes-1] != '\n') {
            file_buffer[read_bytes++] = '\n';
        }

        for (int i = 0; i < read_bytes; i++) {
            if (file_buffer[i] == '\n') {
                line_buffer[line_buffer_index] = '\0';
                to_lower(line_buffer);
                if (type == 1) {
                    searchPattern(line_buffer, userInput, 0, type);
                } else if (type ==2) {
                    // reset the isContained array
                    for (int j = 0; j < word_count; j++) {
                        isContained[j] = 0;
                    }

                    // check all elements in the words array
                    for (int j = 0; j < word_count; j++) {
                        searchPattern(line_buffer, wordsArr[j], j, type);
                    }

                    // check if all the words are contained
                    int isAllContained = 1;
                    for (int j = 0; j < word_count; j++) {
                        if (!isContained[j]) {
                            isAllContained = 0;
                            break;
                        }
                    }

                    if (isAllContained) {
                        print_int(line_number);
                        write(1, " ", 1);
                    }

                } else if(type == 3) {
                    searchPattern(line_buffer, userInput+1,0, type); 
                } else {
                    min_start_index[0] = -1;
                    min_start_index[1] = -1;
                    max_start_index[0] = -1;
                    max_start_index[1] = -1;

                    searchPattern(line_buffer, wordsArr[0], 0, type);
                    searchPattern(line_buffer, wordsArr[1], 1, type);

                    if (min_start_index[0] != -1 && min_start_index[1] != -1 && 
                    min_start_index[0] + str_len(wordsArr[0]) + 1 < max_start_index[1]) {
                        // check if there exist a word between the two words
                        int isWordExist = 0;
                        for(int i = min_start_index[0] + str_len(wordsArr[0]) + 1; i < max_start_index[1]; i++) {
                            if (line_buffer[i] != ' ') {
                                isWordExist = 1;
                                break;
                            }
                        }
                        if (isWordExist) {
                            print_int(line_number);
                            write(1, " ", 1);
                        }
                    }
                }
                line_number++;
                line_buffer_index = 0;
           }
            else {
                line_buffer[line_buffer_index++] = file_buffer[i];
            }    
        }
    }
    return;
}


// Preprocessing function for Boyer Moore's bad character heuristic
void BadCharHeuristic(char *pattern, int patternLength, int badCharacterShift[MAX_CHARS]) {
    int index;

    // Initialize all occurrences as -1
    for (index = 0; index < MAX_CHARS; index++)
        badCharacterShift[index] = -1;

    // Fill the actual value of the last occurrence of a character
    for (index = 0; index < patternLength; index++)
        badCharacterShift[(int)pattern[index]] = index;
}

// Boyer Moore Algorithm for searching the pattern in the line
void searchPattern(char *text, char *pattern, int word_index, int type) {
    int patternLength = str_len(pattern);
    int textLength = str_len(text);

    int badCharacterShift[MAX_CHARS];

    // Fill the bad character shift array by calling the preprocessing function for the given pattern
    BadCharHeuristic(pattern, patternLength, badCharacterShift);

    int shift = 0;  // shift of the pattern with respect to text
    while (shift <= (textLength - patternLength)) {
        int matchIndex = patternLength - 1;

        // Keep reducing index matchIndex of pattern while characters of pattern and text match at this shift
        while (matchIndex >= 0 && pattern[matchIndex] == text[shift + matchIndex])
            matchIndex--;

        // If the pattern is present at the current shift, then index matchIndex will become -1 after the loop
        if (matchIndex < 0 && (shift == 0 || text[shift - 1] == ' ') && 
            (shift + patternLength == textLength || text[shift + patternLength] == ' ')) {
            if (type == 1 || type == 3) {
                print_int(line_number);
                write(1, ":", 1);
                print_int(shift);
                write(1, " ", 1);

                // Shift the pattern so that the next character in text aligns with the last occurrence of it in pattern
                shift += (shift + patternLength < textLength) ? patternLength - badCharacterShift[(int)text[shift + patternLength]] : 1;
            } 
            
            else if (type == 2) {
                isContained[word_index] = 1;

                // replace the word with white space in the text
                for (int i = 0; i < patternLength; i++) {
                    text[shift + i] = ' ';
                }
                return;
            } 

            else {
                // update the min and max start index
                if (min_start_index[word_index] == -1) {
                    min_start_index[word_index] = shift;
                }
                max_start_index[word_index] = shift;

                // Shift the pattern so that the next character in text aligns with the last occurrence of it in pattern
                shift += (shift + patternLength < textLength) ? patternLength - badCharacterShift[(int)text[shift + patternLength]] : 1;
            }

        } else {
            // Shift the pattern so that the bad character in text aligns with the last occurrence of it in pattern
            shift += max(1, matchIndex - badCharacterShift[(int)text[shift + matchIndex]]);
        }
    }
}