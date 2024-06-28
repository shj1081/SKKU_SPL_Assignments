#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void move_file(const char *source, const char *dest) {
    if (rename(source, dest) == -1) {
        switch (errno) {
            case EACCES:
            case ENOTEMPTY:
            case EEXIST:
                fprintf(stderr, "pa2_mv: cannot move '%s' to '%s': Permission denied\n", source, dest);
                break;
            case ENOENT:
                fprintf(stderr, "pa2_mv: cannot stat '%s': No such file or directory\n", source);
                break;
            case EINVAL:
                fprintf(stderr, "pa2_mv: cannot move '%s' to a subdirectory of itself, '%s'\n", source, dest);
                break;
            default:
                fprintf(stderr, "pa2_mv: %s\n", strerror(errno));
                break;
        }
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: pa2_mv source_file target_file_or_directory\n");
        exit(1);
    }

    const char *dest = argv[argc - 1];
    struct stat statbuf;
    int stat_result = stat(dest, &statbuf);
    int is_directory = stat_result == 0 && S_ISDIR(statbuf.st_mode);

    for (int i = 1; i < argc - 1; ++i) {
        char dest_path[1024];
        if (is_directory) {
            snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, basename(argv[i]));
        } else {
            if (argc != 3) {
                fprintf(stderr, "pa2_mv: '%s' is not a directory\n", dest);
                exit(1);
            }
            strncpy(dest_path, dest, sizeof(dest_path) - 1);
            dest_path[sizeof(dest_path) - 1] = '\0';
        }

        if (strcmp(argv[i], dest_path) == 0) {
            fprintf(stderr, "pa2_mv: '%s' and '%s' are the same file\n", argv[i], dest_path);
            continue;
        }

        move_file(argv[i], dest_path);
    }

    exit(0);
}
