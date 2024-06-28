#ifndef UTIL_H
#define UTIL_H

#include <sys/types.h>

// Defines
#define PA1EXIT "PA1EXIT\0"
#define BUFFER_SIZE 12800
#define WORD_ARR_SIZE 4096
#define WORD_SIZE 4096
#define MAX_CHARS 256

// Global variables
extern long long int line_number;
extern char wordsArr[WORD_ARR_SIZE][WORD_SIZE];
extern int isContained[WORD_ARR_SIZE];
extern int word_count;
extern int min_start_index[2];
extern int max_start_index[2];
extern char num_str[100];

// Function prototypes
int get_user_input(char *inputBuffer);
void print_int(int num);
int str_cmp(const char* s1, const char* s2);
int str_len (const char *str);
int find_char(char *str, char c);
void to_lower(char *str);
int max(int a, int b);
void seperate_words(char *userInput);
void process(int fd, char *userInput, int type);
void searchPattern(char *text, char *pattern, int word_index, int type);
void BadCharHeuristic(char *pattern, int patternLength, int badCharacterShift[MAX_CHARS]);

#endif // UTIL_H
