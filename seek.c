#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fnmatch.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef FNM_CASEFOLD
#define FNM_CASEFOLD 0
#endif

static void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <pattern> [-d <directory>] [-f <file_filter>]\n", prog_name);
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s \"Desktop Hintergr*\"\n", prog_name);
    fprintf(stderr, "  %s \"Des*Hin\"\n", prog_name);
    fprintf(stderr, "  %s \"Desk\" -d some/directory/path\n", prog_name);
    fprintf(stderr, "  %s \"Desk\" -d ../../somepath\n", prog_name);
    fprintf(stderr, "  %s \"stdio\" -f \"*.c\"\n", prog_name);
    fprintf(stderr, "  %s \"Hintergrund\" -f \"*.md\" -d ../../somepath\n", prog_name);
}

/*
 * Checks if a pattern matches target text.
 * Supports:
 * - Direct glob matching (e.g. "Des*Hin", "*stdio*", "*.c")
 * - Substring matching if no wildcards are present (e.g. "std", "Desk")
 * - Partial matching with wildcards (e.g. "*pattern*" or "pattern*")
 */
static int pattern_matches(const char *pattern, const char *text) {
    if (!pattern || !text) {
        return 0;
    }

    /* 1. Direct fnmatch */
    if (fnmatch(pattern, text, FNM_CASEFOLD) == 0) {
        return 1;
    }

    /* 2. Substring match if pattern has no wildcards */
    if (strchr(pattern, '*') == NULL && strchr(pattern, '?') == NULL && strchr(pattern, '[') == NULL) {
        if (strcasestr(text, pattern) != NULL) {
            return 1;
        }
    }

    /* 3. Try partial wildcard matches */
    char wrapped[PATH_MAX];
    size_t plen = strlen(pattern);
    if (plen + 3 < sizeof(wrapped)) {
        /* If pattern doesn't end with '*', try 'pattern*' */
        if (plen > 0 && pattern[plen - 1] != '*') {
            snprintf(wrapped, sizeof(wrapped), "%s*", pattern);
            if (fnmatch(wrapped, text, FNM_CASEFOLD) == 0) {
                return 1;
            }
        }

        /* If pattern doesn't start or end with '*', try '*pattern*' */
        snprintf(wrapped, sizeof(wrapped), "*%s*", pattern);
        if (fnmatch(wrapped, text, FNM_CASEFOLD) == 0) {
            return 1;
        }
    }

    return 0;
}

/* Checks if a file appears to be binary by checking the first 1KB for null bytes */
static int is_binary_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return 1;
    }

    char buffer[1024];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), fp);
    fclose(fp);

    for (size_t i = 0; i < bytes_read; i++) {
        if (buffer[i] == '\0') {
            return 1;
        }
    }
    return 0;
}

/* Searches the text content of a file line-by-line for the pattern */
static int search_file_content(const char *path, const char *pattern, int name_matched) {
    if (is_binary_file(path)) {
        if (name_matched) {
            printf("File \"%s\":\n", path);
            printf("    (Filename matches)\n");
            printf("------------------------------\n");
            return 1;
        }
        return 0;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        if (name_matched) {
            printf("File \"%s\":\n", path);
            printf("    (Filename matches)\n");
            printf("------------------------------\n");
            return 1;
        }
        return 0;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read_bytes;
    size_t line_num = 0;
    int matches_found = 0;
    int header_printed = 0;

    while ((read_bytes = getline(&line, &len, fp)) != -1) {
        line_num++;
        /* Strip trailing newline characters */
        while (read_bytes > 0 && (line[read_bytes - 1] == '\n' || line[read_bytes - 1] == '\r')) {
            line[--read_bytes] = '\0';
        }

        if (pattern_matches(pattern, line)) {
            if (!header_printed) {
                printf("File \"%s\":\n", path);
                header_printed = 1;
            }
            printf("    Line %zu: %s\n", line_num, line);
            matches_found++;
        }
    }

    free(line);
    fclose(fp);

    if (matches_found > 0) {
        printf("------------------------------\n");
    } else if (name_matched) {
        printf("File \"%s\":\n", path);
        printf("    (Filename matches)\n");
        printf("------------------------------\n");
        return 1;
    }

    return matches_found;
}

static void search_directory(const char *dir_path, const char *pattern, const char *file_filter, int *match_count) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "seek: cannot open '%s': %s\n", dir_path, strerror(errno));
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip '.' and '..' */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /* Construct child path cleanly without double slashes */
        char child_path[PATH_MAX];
        size_t dir_len = strlen(dir_path);
        int ret;
        if (dir_len > 0 && dir_path[dir_len - 1] == '/') {
            ret = snprintf(child_path, sizeof(child_path), "%s%s", dir_path, entry->d_name);
        } else {
            ret = snprintf(child_path, sizeof(child_path), "%s/%s", dir_path, entry->d_name);
        }

        if (ret < 0 || (size_t)ret >= sizeof(child_path)) {
            fprintf(stderr, "seek: path too long: %s/%s\n", dir_path, entry->d_name);
            continue;
        }

        /* Use lstat to avoid following cyclic symlinks */
        struct stat st;
        if (lstat(child_path, &st) != 0) {
            fprintf(stderr, "seek: cannot stat '%s': %s\n", child_path, strerror(errno));
            continue;
        }

        /* If directory, recurse into it */
        if (S_ISDIR(st.st_mode)) {
            int name_matched = pattern_matches(pattern, entry->d_name);
            if (!name_matched && strchr(pattern, '/') != NULL) {
                name_matched = pattern_matches(pattern, child_path);
            }

            /* Only report directory match if it satisfies file_filter (if set) */
            int filter_matched = (file_filter == NULL) ||
                                 pattern_matches(file_filter, entry->d_name) ||
                                 (strchr(file_filter, '/') != NULL && pattern_matches(file_filter, child_path));

            if (name_matched && filter_matched) {
                printf("File \"%s\":\n", child_path);
                printf("    (Directory matches)\n");
                printf("------------------------------\n");
                if (match_count) {
                    (*match_count)++;
                }
            }

            /* Always recurse into subdirectories */
            search_directory(child_path, pattern, file_filter, match_count);
            continue;
        }

        /* For non-directory entries: check if entry satisfies file_filter */
        if (file_filter != NULL) {
            int filter_matched = pattern_matches(file_filter, entry->d_name);
            if (!filter_matched && strchr(file_filter, '/') != NULL) {
                filter_matched = pattern_matches(file_filter, child_path);
            }
            if (!filter_matched) {
                continue;
            }
        }

        int name_matched = pattern_matches(pattern, entry->d_name);
        if (!name_matched && strchr(pattern, '/') != NULL) {
            name_matched = pattern_matches(pattern, child_path);
        }

        /* If regular file, search file content */
        if (S_ISREG(st.st_mode)) {
            int found = search_file_content(child_path, pattern, name_matched);
            if (match_count) {
                *match_count += found;
            }
        } else {
            /* Other types (symlinks, fifos, etc.) */
            if (name_matched) {
                printf("File \"%s\":\n", child_path);
                printf("    (Filename matches)\n");
                printf("------------------------------\n");
                if (match_count) {
                    (*match_count)++;
                }
            }
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    const char *pattern = NULL;
    const char *start_dir = NULL;
    const char *file_filter = NULL;

    /* Parse command-line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 < argc) {
                start_dir = argv[++i];
            } else {
                fprintf(stderr, "Error: -d requires a directory path\n");
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "-f") == 0) {
            if (i + 1 < argc) {
                file_filter = argv[++i];
            } else {
                fprintf(stderr, "Error: -f requires a filename filter pattern\n");
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        } else {
            if (pattern == NULL) {
                pattern = argv[i];
            } else if (start_dir == NULL) {
                /* Allow directory as an optional positional argument (e.g. seek "std" .) */
                start_dir = argv[i];
            } else {
                fprintf(stderr, "Error: Unexpected argument '%s'\n", argv[i]);
                print_usage(argv[0]);
                return EXIT_FAILURE;
            }
        }
    }

    if (start_dir == NULL) {
        start_dir = ".";
    }

    if (pattern == NULL) {
        fprintf(stderr, "Error: Missing search pattern\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    /* Verify that start_dir exists and is accessible as a directory */
    struct stat root_st;
    if (stat(start_dir, &root_st) != 0) {
        fprintf(stderr, "seek: cannot access '%s': %s\n", start_dir, strerror(errno));
        return EXIT_FAILURE;
    }
    if (!S_ISDIR(root_st.st_mode)) {
        fprintf(stderr, "seek: '%s' is not a directory\n", start_dir);
        return EXIT_FAILURE;
    }

    int match_count = 0;
    search_directory(start_dir, pattern, file_filter, &match_count);

    printf("Total: %d Results\n", match_count);

    return EXIT_SUCCESS;
}
