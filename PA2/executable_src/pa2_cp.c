#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void copy_file(const char *source, const char *dest) {
    FILE *src = fopen(source, "rb");
    if (!src) {
        fprintf(stderr, "pa2_cp: cannot open '%s' for reading: %s\n", source, strerror(errno));
        exit(1);
    }

    FILE *dst = fopen(dest, "wb");  // if file does not exist, it will be created
    if (!dst) {
        fclose(src);
        fprintf(stderr, "pa2_cp: cannot open '%s' for writing: %s\n", dest, strerror(errno));
        exit(1);
    }

    char buffer[1024];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }

    fclose(src);
    fclose(dst);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        if (argc == 1) {  // No arguments (only the executable name
            fprintf(stderr, "pa2_cp: missing file operand\n");
        } else if (argc == 2) {
            fprintf(stderr, "pa2_cp: missing destination file operand after '%s'\n", argv[1]);
        }
        exit(1);
    }

    const char *dest_path = argv[argc - 1];
    struct stat statbuf;

    // Determine if the last argument is a directory or not
    int is_directory = stat(dest_path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode);

    for (int i = 1; i < argc - 1; ++i) {
        if (stat(argv[i], &statbuf) != 0) {
            fprintf(stderr, "pa2_cp: cannot stat '%s': No such file or directory\n", argv[i]);
            continue;  // Skip this iteration, proceed to next file
        }

        char full_dest_path[1024];
        if (is_directory) {
            snprintf(full_dest_path, sizeof(full_dest_path), "%s/%s", dest_path, basename(argv[i]));
        } else {
            if (argc == 3) {  // Only two arguments mean direct file-to-file copy
                strncpy(full_dest_path, dest_path, sizeof(full_dest_path) - 1);
                full_dest_path[sizeof(full_dest_path) - 1] = '\0';
            } else {
                fprintf(stderr, "pa2_cp: target '%s' is not a directory\n", dest_path);
                exit(1);
            }
        }

        copy_file(argv[i], full_dest_path);
    }

    exit(0);
}
