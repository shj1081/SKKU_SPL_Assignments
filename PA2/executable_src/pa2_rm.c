#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "pa2_rm: missing operand\n");
        return 1;
    }

    int status = 0;

    for (int i = 1; i < argc; i++) {
        if (unlink(argv[i]) == -1) {
            fprintf(stderr, "pa2_rm: cannot remove '%s': %s\n", argv[i], strerror(errno));
            status = 1;  // Set status to 1 to indicate an error occurred, but continue with other files.
        }
    }

    exit(status);
}
