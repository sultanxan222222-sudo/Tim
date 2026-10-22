#define _POSIX_C_SOURCE 200809L
#include "file_browser.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int copy_path(char *out, size_t capacity, const char *path) {
    size_t n;
    if (!out || !path) { errno = EINVAL; return -1; }
    n = strlen(path);
    if (n == 0 || n >= capacity) { errno = ENAMETOOLONG; return -1; }
    memcpy(out, path, n + 1);
    return 0;
}

static int join_path(char *out, size_t capacity, const char *base, const char *name) {
    int n;
    size_t base_len;
    if (!out || !base || !name || !name[0] ||
        strchr(name, '/') || !strcmp(name, ".") || !strcmp(name, "..")) {
        errno = EINVAL;
        return -1;
    }
    base_len = strlen(base);
    n = snprintf(out, capacity, "%s%s%s", base,
                 (base_len && base[base_len - 1] == '/') ? "" : "/", name);
    if (n < 0) { errno = EIO; return -1; }
    if ((size_t)n >= capacity) { errno = ENAMETOOLONG; return -1; }
    return 0;
}

int tim_fm_open(tim_file_browser *browser, const char *root) {
    struct stat st;
    if (!browser || !root || !root[0]) { errno = EINVAL; return -1; }
    memset(browser, 0, sizeof(*browser));
    if (copy_path(browser->root, sizeof(browser->root), root) != 0) return -1;
    while (strlen(browser->root) > 1 &&
           browser->root[strlen(browser->root) - 1] == '/')
        browser->root[strlen(browser->root) - 1] = '\0';
    if (stat(browser->root, &st) != 0) return -1;
    if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return -1; }
    if (copy_path(browser->current, sizeof(browser->current), browser->root) != 0)
        return -1;
    return tim_fm_refresh(browser);
}

void tim_fm_close(tim_file_browser *browser) {
    if (!browser) return;
    tim_free_directory(browser->entries, browser->count);
    memset(browser, 0, sizeof(*browser));
}

int tim_fm_refresh(tim_file_browser *browser) {
    tim_dir_entry *next = NULL;
    size_t count = 0;
    if (!browser || !browser->current[0]) { errno = EINVAL; return -1; }
    if (tim_list_directory(browser->current, &next, &count) != 0) return -1;
    tim_free_directory(browser->entries, browser->count);
    browser->entries = next;
    browser->count = count;
    browser->selected = count ? (browser->selected < count ? browser->selected : count - 1) : 0;
    return 0;
}

int tim_fm_select(tim_file_browser *browser, size_t index) {
    if (!browser || index >= browser->count) { errno = ERANGE; return -1; }
    browser->selected = index;
    return 0;
}

int tim_fm_enter_selected(tim_file_browser *browser) {
    char next[TIM_PATH_MAX];
    struct stat st;
    if (!browser || browser->selected >= browser->count) { errno = ERANGE; return -1; }
    if (!browser->entries[browser->selected].is_directory) { errno = ENOTDIR; return -1; }
    if (join_path(next, sizeof(next), browser->current,
                  browser->entries[browser->selected].name) != 0) return -1;
    if (stat(next, &st) != 0) return -1;
    if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return -1; }
    if (copy_path(browser->current, sizeof(browser->current), next) != 0) return -1;
    return tim_fm_refresh(browser);
}

int tim_fm_parent(tim_file_browser *browser) {
    char *slash;
    size_t root_len;
    if (!browser || !browser->current[0]) { errno = EINVAL; return -1; }
    if (!strcmp(browser->current, browser->root)) return 0;
    root_len = strlen(browser->root);
    if (strncmp(browser->current, browser->root, root_len) != 0 ||
        (browser->current[root_len] != '/' && browser->current[root_len] != '\0')) {
        errno = EPERM;
        return -1;
    }
    slash = strrchr(browser->current, '/');
    if (!slash) { errno = EINVAL; return -1; }
    if ((size_t)(slash - browser->current) < root_len)
        copy_path(browser->current, sizeof(browser->current), browser->root);
    else if (slash == browser->current)
        browser->current[1] = '\0';
    else
        *slash = '\0';
    return tim_fm_refresh(browser);
}

int tim_fm_copy_selected(const tim_file_browser *browser,
                         const char *destination_directory,
                         tim_copy_progress_fn progress,
                         void *userdata) {
    char source[TIM_PATH_MAX], destination[TIM_PATH_MAX];
    struct stat st;
    if (!browser || !destination_directory || !destination_directory[0] ||
        browser->selected >= browser->count) { errno = EINVAL; return -1; }
    if (browser->entries[browser->selected].is_directory) { errno = EISDIR; return -1; }
    if (join_path(source, sizeof(source), browser->current,
                  browser->entries[browser->selected].name) != 0) return -1;
    if (join_path(destination, sizeof(destination), destination_directory,
                  browser->entries[browser->selected].name) != 0) return -1;
    if (stat(destination_directory, &st) != 0) return -1;
    if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return -1; }
    return tim_copy_regular_file(source, destination, progress, userdata);
}
