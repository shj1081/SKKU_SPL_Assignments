# System Programming Lab PA1 : File I/O

## Objective

- using Unix File I/O and data structures
- implementing a program that performs various text search functions on a file specified as a command line input

## Tasks

1. **Single Word Search**
   - If the program receives user input containing only a single word, it searches for all occurrences of the word in the given file.
   - Output format: `[line number]:[start index of the word]`

2. **Multiple Words Search**
   - If the program receives user input containing multiple words separated by a single space and no quotation marks, it searches for lines containing all the words, regardless of order.
   - Output format: `[line number]`

3. **Consecutive Words Search**
   - If the program receives user input containing multiple words wrapped in double quotation marks (“”), it searches for lines containing the entire phrase.
   - Output format: `[line number]:[start index of the phrase]`
   - If a line contains multiple instances of the phrase, all occurrences are printed.

4. **Regular Expression Search**
   - If the program receives user input where an asterisk (*) is between two words (e.g., `[word1]*[word2]`), it searches for lines containing at least one word between word1 and word2.
   - Output format: `[line number]`
   - If a line contains multiple instances, they are printed only once.

## How to compile and run

```bash
make pa1
./pa1 [file name] [search query]
```

## Score

100/100