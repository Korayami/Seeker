#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <fnmatch.h>
#include <limits.h>
#include <termios.h>
#include <signal.h>
#include <ctype.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef FNM_CASEFOLD
#define FNM_CASEFOLD 0
#endif

/* ANSI Colors and Styles */
#define COLOR_RESET       "\033[0m"
#define COLOR_BOLD        "\033[1m"
#define COLOR_DIM         "\033[2m"
#define COLOR_ITALIC      "\033[3m"
#define COLOR_UNDERLINE   "\033[4m"

#define COLOR_BORDER      "\033[38;5;67m"
#define COLOR_BORDER_ACT  "\033[38;5;75m"
#define COLOR_TITLE       "\033[1;38;5;81m"
#define COLOR_INPUT_BOX   "\033[48;5;236m\033[38;5;255m"
#define COLOR_INPUT_TEXT  "\033[1;38;5;229m"
#define COLOR_STATUS_BAR  "\033[48;5;236m\033[38;5;250m"
#define COLOR_SEL_BG      "\033[48;5;239m\033[38;5;229m\033[1m"
#define COLOR_LINE_NUM    "\033[38;5;244m"
#define COLOR_MATCH_HIGHLIGHT "\033[1;38;5;220m\033[48;5;238m"
#define COLOR_FILE_HDR    "\033[1;38;5;111m"
#define COLOR_PREVIEW_HL  "\033[48;5;58m\033[1;38;5;229m"

typedef struct {
    char file_path[PATH_MAX];
    size_t line_num;
    char line_text[256];
    int is_file_match_only;
} SearchResult;

#define MAX_RESULTS 10000

typedef enum {
    FOCUS_QUERY = 0,
    FOCUS_FILTER,
    FOCUS_RESULTS
} FocusArea;

typedef struct {
    char query[256];
    size_t query_len;
    char file_filter[256];
    size_t filter_len;
    char start_dir[PATH_MAX];

    SearchResult results[MAX_RESULTS];
    int result_count;
    int selected_index;
    int scroll_offset;

    int term_width;
    int term_height;

    FocusArea focus;
    int running;
    struct termios orig_termios;
} AppState;

static AppState state;

/* --- Terminal Handling --- */

static void disable_raw_mode(void) {
    printf("\033[?25h\033[?1049l"); /* Show cursor, leave alternate buffer */
    fflush(stdout);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &state.orig_termios);
}

static void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &state.orig_termios) == -1) {
        perror("tcgetattr");
        exit(1);
    }
    atexit(disable_raw_mode);

    struct termios raw = state.orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(1);
    }

    printf("\033[?1049h\033[?25l"); /* Enter alternate screen, hide cursor */
    fflush(stdout);
}

static void update_term_size(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        state.term_width = ws.ws_col;
        state.term_height = ws.ws_row;
    } else {
        state.term_width = 80;
        state.term_height = 24;
    }
}

static void sigwinch_handler(int sig) {
    (void)sig;
    update_term_size();
}

/* --- Search Engine --- */

static int pattern_matches(const char *pattern, const char *text) {
    if (!pattern || !text || pattern[0] == '\0') {
        return 0;
    }

    if (fnmatch(pattern, text, FNM_CASEFOLD) == 0) {
        return 1;
    }

    if (strchr(pattern, '*') == NULL && strchr(pattern, '?') == NULL && strchr(pattern, '[') == NULL) {
        if (strcasestr(text, pattern) != NULL) {
            return 1;
        }
    }

    char wrapped[PATH_MAX];
    size_t plen = strlen(pattern);
    if (plen + 3 < sizeof(wrapped)) {
        if (plen > 0 && pattern[plen - 1] != '*') {
            snprintf(wrapped, sizeof(wrapped), "%s*", pattern);
            if (fnmatch(wrapped, text, FNM_CASEFOLD) == 0) return 1;
        }
        snprintf(wrapped, sizeof(wrapped), "*%s*", pattern);
        if (fnmatch(wrapped, text, FNM_CASEFOLD) == 0) return 1;
    }

    return 0;
}

static int is_binary_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return 1;

    char buffer[1024];
    size_t n = fread(buffer, 1, sizeof(buffer), fp);
    fclose(fp);

    for (size_t i = 0; i < n; i++) {
        if (buffer[i] == '\0') return 1;
    }
    return 0;
}

static void search_file_content(const char *path, const char *pattern, int name_matched) {
    if (state.result_count >= MAX_RESULTS) return;

    if (is_binary_file(path)) {
        if (name_matched && state.result_count < MAX_RESULTS) {
            SearchResult *r = &state.results[state.result_count++];
            strncpy(r->file_path, path, sizeof(r->file_path) - 1);
            r->line_num = 0;
            strncpy(r->line_text, "(Filename matches)", sizeof(r->line_text) - 1);
            r->is_file_match_only = 1;
        }
        return;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) return;

    char *line = NULL;
    size_t len = 0;
    ssize_t read_bytes;
    size_t line_num = 0;
    int found_in_file = 0;

    while ((read_bytes = getline(&line, &len, fp)) != -1 && state.result_count < MAX_RESULTS) {
        line_num++;
        while (read_bytes > 0 && (line[read_bytes - 1] == '\n' || line[read_bytes - 1] == '\r')) {
            line[--read_bytes] = '\0';
        }

        if (pattern_matches(pattern, line)) {
            SearchResult *r = &state.results[state.result_count++];
            strncpy(r->file_path, path, sizeof(r->file_path) - 1);
            r->line_num = line_num;
            
            /* Trim leading spaces for display */
            char *trimmed = line;
            while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
            strncpy(r->line_text, trimmed, sizeof(r->line_text) - 1);
            r->is_file_match_only = 0;
            found_in_file++;
        }
    }

    free(line);
    fclose(fp);

    if (found_in_file == 0 && name_matched && state.result_count < MAX_RESULTS) {
        SearchResult *r = &state.results[state.result_count++];
        strncpy(r->file_path, path, sizeof(r->file_path) - 1);
        r->line_num = 0;
        strncpy(r->line_text, "(Filename matches)", sizeof(r->line_text) - 1);
        r->is_file_match_only = 1;
    }
}

static void search_directory(const char *dir_path, const char *pattern, const char *file_filter) {
    if (state.result_count >= MAX_RESULTS) return;

    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && state.result_count < MAX_RESULTS) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char child_path[PATH_MAX];
        size_t dir_len = strlen(dir_path);
        if (dir_len > 0 && dir_path[dir_len - 1] == '/') {
            snprintf(child_path, sizeof(child_path), "%s%s", dir_path, entry->d_name);
        } else {
            snprintf(child_path, sizeof(child_path), "%s/%s", dir_path, entry->d_name);
        }

        struct stat st;
        if (lstat(child_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            /* Skip hidden version control and cache directories for performance */
            if (strcmp(entry->d_name, ".git") == 0 || strcmp(entry->d_name, "node_modules") == 0 ||
                strcmp(entry->d_name, ".venv") == 0) {
                continue;
            }
            search_directory(child_path, pattern, file_filter);
            continue;
        }

        /* Check file filter if present */
        if (file_filter && file_filter[0] != '\0') {
            int filter_matched = pattern_matches(file_filter, entry->d_name);
            if (!filter_matched && strchr(file_filter, '/') != NULL) {
                filter_matched = pattern_matches(file_filter, child_path);
            }
            if (!filter_matched) continue;
        }

        int name_matched = pattern_matches(pattern, entry->d_name);
        if (S_ISREG(st.st_mode)) {
            search_file_content(child_path, pattern, name_matched);
        }
    }

    closedir(dir);
}

static void perform_search(void) {
    state.result_count = 0;
    state.selected_index = 0;
    state.scroll_offset = 0;

    if (state.query_len == 0) return;

    search_directory(state.start_dir, state.query, state.filter_len > 0 ? state.file_filter : NULL);
}

/* --- UI Rendering --- */

static void render_ui(void) {
    update_term_size();

    /* Clear screen and move cursor to top-left */
    printf("\033[H");

    int w = state.term_width;
    int h = state.term_height;

    /* 1. Header Bar */
    printf("%s%s SEEKUI %s─ %sText & File Finder%s │ Dir: %s%.30s%s\033[K\n",
           COLOR_BOLD, COLOR_TITLE, COLOR_DIM, COLOR_RESET, COLOR_DIM, COLOR_BOLD, state.start_dir, COLOR_RESET);

    /* 2. Search & Filter Input Bar */
    int query_box_w = (w * 6) / 10;
    int filter_box_w = w - query_box_w - 7;
    if (filter_box_w < 10) filter_box_w = 10;

    printf("┌ Search %.*s┬ Filter %.*s┐\033[K\n",
           query_box_w - 9 > 0 ? query_box_w - 9 : 0, "─────────────────────────────────────────────────────────────",
           filter_box_w - 9 > 0 ? filter_box_w - 9 : 0, "─────────────────────────────────────────────────────────────");

    /* Render Query Input */
    printf("│ %s", state.focus == FOCUS_QUERY ? COLOR_BORDER_ACT : COLOR_RESET);
    if (state.focus == FOCUS_QUERY) {
        printf("%s%-*.*s%s", COLOR_INPUT_TEXT, query_box_w - 4, query_box_w - 4, state.query, COLOR_RESET);
    } else {
        printf("%-*.*s", query_box_w - 4, query_box_w - 4, state.query_len > 0 ? state.query : "(all)");
    }

    /* Render Filter Input */
    printf(" │ %s", state.focus == FOCUS_FILTER ? COLOR_BORDER_ACT : COLOR_RESET);
    if (state.focus == FOCUS_FILTER) {
        printf("%s%-*.*s%s", COLOR_INPUT_TEXT, filter_box_w - 4, filter_box_w - 4, state.file_filter, COLOR_RESET);
    } else {
        printf("%-*.*s", filter_box_w - 4, filter_box_w - 4, state.filter_len > 0 ? state.file_filter : "*.c, *.py, *");
    }
    printf(" │\033[K\n");

    printf("├%.*s┴%.*s┤\033[K\n",
           query_box_w - 1, "─────────────────────────────────────────────────────────────",
           w - query_box_w, "─────────────────────────────────────────────────────────────");

    /* Split Main Area: Left (Results List) vs Right (File Preview) */
    int main_h = h - 6;
    if (main_h < 3) main_h = 3;

    int left_w = (w * 4) / 10;
    if (left_w < 25) left_w = 25;
    int right_w = w - left_w - 3;

    /* Read preview lines if a file is selected */
    char preview_lines[64][256];
    size_t preview_line_nums[64];
    int preview_count = 0;
    size_t target_line = 0;

    if (state.result_count > 0 && state.selected_index < state.result_count) {
        SearchResult *sel = &state.results[state.selected_index];
        target_line = sel->line_num;

        FILE *fp = fopen(sel->file_path, "r");
        if (fp) {
            char *line = NULL;
            size_t len = 0;
            ssize_t read_bytes;
            size_t cur_line = 0;

            int start_line = (int)target_line - (main_h / 2);
            if (start_line < 1) start_line = 1;

            while ((read_bytes = getline(&line, &len, fp)) != -1) {
                cur_line++;
                if ((int)cur_line >= start_line && preview_count < main_h) {
                    while (read_bytes > 0 && (line[read_bytes - 1] == '\n' || line[read_bytes - 1] == '\r')) {
                        line[--read_bytes] = '\0';
                    }
                    preview_line_nums[preview_count] = cur_line;
                    strncpy(preview_lines[preview_count], line, sizeof(preview_lines[0]) - 1);
                    preview_lines[preview_count][sizeof(preview_lines[0]) - 1] = '\0';
                    preview_count++;
                }
                if (preview_count >= main_h) break;
            }
            free(line);
            fclose(fp);
        }
    }

    /* Render Results and Preview split rows */
    for (int i = 0; i < main_h; i++) {
        /* Left Pane: Results item */
        int item_idx = state.scroll_offset + i;
        if (item_idx < state.result_count) {
            SearchResult *r = &state.results[item_idx];
            int is_sel = (item_idx == state.selected_index);

            char item_buf[512];
            if (r->is_file_match_only) {
                snprintf(item_buf, sizeof(item_buf), " %s (file)", r->file_path);
            } else {
                snprintf(item_buf, sizeof(item_buf), " %s:%zu: %s", r->file_path, r->line_num, r->line_text);
            }

            if (is_sel) {
                printf("%s> %-*.*s%s│", COLOR_SEL_BG, left_w - 2, left_w - 2, item_buf, COLOR_RESET);
            } else {
                printf("  %-*.*s│", left_w - 2, left_w - 2, item_buf);
            }
        } else if (state.result_count == 0 && i == 0) {
            printf("  %s(No matches found)%s%*s│", COLOR_DIM, COLOR_RESET, left_w - 20 > 0 ? left_w - 20 : 0, "");
        } else {
            printf("%-*s│", left_w, "");
        }

        /* Right Pane: File Preview row */
        if (i < preview_count) {
            size_t lnum = preview_line_nums[i];
            int is_target = (lnum == target_line);

            if (is_target) {
                printf("%s%4zu │ %-*.*s%s", COLOR_PREVIEW_HL, lnum, right_w - 7, right_w - 7, preview_lines[i], COLOR_RESET);
            } else {
                printf("%s%4zu%s │ %-*.*s", COLOR_LINE_NUM, lnum, COLOR_RESET, right_w - 7, right_w - 7, preview_lines[i]);
            }
        } else {
            printf("%-*s", right_w, "");
        }

        printf("\033[K\n");
    }

    /* 4. Status Bar */
    printf("└%.*s┴%.*s┘\033[K\n",
           left_w, "─────────────────────────────────────────────────────────────",
           right_w + 1, "─────────────────────────────────────────────────────────────");

    printf("%s Matches: %d | [Tab] Switch | [↑/↓] Navigate | [Enter] Open | [Esc] Exit %s\033[K",
           COLOR_STATUS_BAR, state.result_count, COLOR_RESET);

    fflush(stdout);
}

/* Open the selected file in user's editor */
static void open_in_editor(const char *file_path, size_t line_num) {
    const char *editor = getenv("EDITOR");
    if (!editor || editor[0] == '\0') {
        editor = "nano";
    }

    char cmd[PATH_MAX + 64];
    /* If nano or vim/nvim, pass +<line> option */
    if (strstr(editor, "nano") != NULL || strstr(editor, "vi") != NULL) {
        snprintf(cmd, sizeof(cmd), "%s +%zu \"%s\"", editor, line_num, file_path);
    } else {
        snprintf(cmd, sizeof(cmd), "%s \"%s\"", editor, file_path);
    }

    disable_raw_mode();
    system(cmd);
    enable_raw_mode();
}

static void handle_input(void) {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return;

    if (c == 27) { /* Escape sequence */
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) {
            /* Bare ESC */
            if (state.focus != FOCUS_QUERY) {
                state.focus = FOCUS_QUERY;
            } else {
                state.running = 0;
            }
            return;
        }
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return;

        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': /* Up */
                    if (state.selected_index > 0) {
                        state.selected_index--;
                        if (state.selected_index < state.scroll_offset) {
                            state.scroll_offset = state.selected_index;
                        }
                    }
                    break;
                case 'B': /* Down */
                    if (state.selected_index + 1 < state.result_count) {
                        state.selected_index++;
                        int main_h = state.term_height - 6;
                        if (state.selected_index >= state.scroll_offset + main_h) {
                            state.scroll_offset = state.selected_index - main_h + 1;
                        }
                    }
                    break;
                case '5': /* Page Up */
                    read(STDIN_FILENO, &seq[2], 1);
                    state.selected_index -= 10;
                    if (state.selected_index < 0) state.selected_index = 0;
                    state.scroll_offset = state.selected_index;
                    break;
                case '6': /* Page Down */
                    read(STDIN_FILENO, &seq[2], 1);
                    state.selected_index += 10;
                    if (state.selected_index >= state.result_count) {
                        state.selected_index = state.result_count > 0 ? state.result_count - 1 : 0;
                    }
                    state.scroll_offset = state.selected_index;
                    break;
            }
        }
    } else if (c == 9) { /* Tab */
        state.focus = (state.focus + 1) % 3;
    } else if (c == 13 || c == 10) { /* Enter */
        if (state.focus == FOCUS_QUERY || state.focus == FOCUS_FILTER) {
            perform_search();
            state.focus = FOCUS_RESULTS;
        } else if (state.focus == FOCUS_RESULTS && state.result_count > 0) {
            SearchResult *r = &state.results[state.selected_index];
            open_in_editor(r->file_path, r->line_num > 0 ? r->line_num : 1);
        }
    } else if (c == 127 || c == 8) { /* Backspace */
        if (state.focus == FOCUS_QUERY) {
            if (state.query_len > 0) {
                state.query[--state.query_len] = '\0';
                perform_search();
            }
        } else if (state.focus == FOCUS_FILTER) {
            if (state.filter_len > 0) {
                state.file_filter[--state.filter_len] = '\0';
                perform_search();
            }
        }
    } else if (c == 3) { /* Ctrl+C */
        state.running = 0;
    } else if (c == 4) { /* Ctrl+D */
        state.running = 0;
    } else if (isprint(c)) {
        if (state.focus == FOCUS_QUERY) {
            if (state.query_len + 1 < sizeof(state.query) - 1) {
                state.query[state.query_len++] = c;
                state.query[state.query_len] = '\0';
                perform_search();
            }
        } else if (state.focus == FOCUS_FILTER) {
            if (state.filter_len + 1 < sizeof(state.file_filter) - 1) {
                state.file_filter[state.filter_len++] = c;
                state.file_filter[state.filter_len] = '\0';
                perform_search();
            }
        } else if (state.focus == FOCUS_RESULTS) {
            if (c == 'q') {
                state.running = 0;
            } else if (c == 'j') {
                /* Vim down */
                if (state.selected_index + 1 < state.result_count) {
                    state.selected_index++;
                    int main_h = state.term_height - 6;
                    if (state.selected_index >= state.scroll_offset + main_h) {
                        state.scroll_offset = state.selected_index - main_h + 1;
                    }
                }
            } else if (c == 'k') {
                /* Vim up */
                if (state.selected_index > 0) {
                    state.selected_index--;
                    if (state.selected_index < state.scroll_offset) {
                        state.scroll_offset = state.selected_index;
                    }
                }
            } else if (c == '/') {
                state.focus = FOCUS_QUERY;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    memset(&state, 0, sizeof(state));
    strncpy(state.start_dir, ".", sizeof(state.start_dir) - 1);
    state.focus = FOCUS_QUERY;
    state.running = 1;

    /* Parse optional initial arguments (e.g. seekui "Hintergrund" -d ../../path -f "*.md") */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            strncpy(state.start_dir, argv[++i], sizeof(state.start_dir) - 1);
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            strncpy(state.file_filter, argv[++i], sizeof(state.file_filter) - 1);
            state.filter_len = strlen(state.file_filter);
        } else if (argv[i][0] != '-') {
            strncpy(state.query, argv[i], sizeof(state.query) - 1);
            state.query_len = strlen(state.query);
        }
    }

    signal(SIGWINCH, sigwinch_handler);
    enable_raw_mode();
    update_term_size();

    if (state.query_len > 0) {
        perform_search();
    }

    while (state.running) {
        render_ui();
        handle_input();
    }

    return 0;
}
