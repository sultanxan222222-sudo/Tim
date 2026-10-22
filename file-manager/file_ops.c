#define _POSIX_C_SOURCE 200809L
#include "file_ops.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static char *tim_strdup(const char *s) {
    size_t n;
    char *p;
    if (!s) { errno = EINVAL; return NULL; }
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

int tim_copy_regular_file(const char *source, const char *destination,
                          tim_copy_progress_fn progress, void *userdata) {
    struct stat st;
    int in_fd = -1, out_fd = -1, saved_errno = 0;
    unsigned char *buffer = NULL;
    uint64_t copied = 0, total;
    int result = -1;

    if (!source || !destination || strcmp(source, destination) == 0) {
        errno = EINVAL;
        return -1;
    }
    if (lstat(source, &st) != 0) return -1;
    if (!S_ISREG(st.st_mode)) { errno = EINVAL; return -1; }

    /* O_EXCL prevents silently overwriting an existing destination. */
    in_fd = open(source, O_RDONLY);
    if (in_fd < 0) goto done;
    out_fd = open(destination, O_WRONLY | O_CREAT | O_EXCL, st.st_mode & 0777);
    if (out_fd < 0) goto done;

    buffer = (unsigned char *)malloc(TIM_COPY_BUFFER_SIZE);
    if (!buffer) goto done;
    total = (uint64_t)st.st_size;

    for (;;) {
        ssize_t n = read(in_fd, buffer, TIM_COPY_BUFFER_SIZE);
        size_t offset = 0;
        if (n == 0) break;
        if (n < 0) {
            if (errno == EINTR) continue;
            goto done;
        }
        while (offset < (size_t)n) {
            ssize_t w = write(out_fd, buffer + offset, (size_t)n - offset);
            if (w < 0) {
                if (errno == EINTR) continue;
                goto done;
            }
            if (w == 0) { errno = EIO; goto done; }
            offset += (size_t)w;
        }
        copied += (uint64_t)n;
        if (progress && progress(copied, total, userdata) != 0) {
            errno = ECANCELED;
            goto done;
        }
    }

    if (fsync(out_fd) != 0) goto done;
    result = 0;

done:
    saved_errno = errno;
    free(buffer);
    if (in_fd >= 0) close(in_fd);
    if (out_fd >= 0) {
        if (close(out_fd) != 0 && result == 0) {
            saved_errno = errno;
            result = -1;
        }
    }
    /* Remove an incomplete destination after a failed copy. */
    if (result != 0 && destination) unlink(destination);
    errno = saved_errno;
    return result;
}

int tim_list_directory(const char *path, tim_dir_entry **entries, size_t *count) {
    DIR *dir;
    struct dirent *de;
    tim_dir_entry *items = NULL;
    size_t used = 0, capacity = 0;

    if (!path || !entries || !count) { errno = EINVAL; return -1; }
    *entries = NULL;
    *count = 0;
    dir = opendir(path);
    if (!dir) return -1;

    errno = 0;
    while ((de = readdir(dir)) != NULL) {
        char full[TIM_PATH_MAX];
        struct stat st;
        tim_dir_entry item;
        size_t need;

        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        need = strlen(path) + 1 + strlen(de->d_name) + 1;
        if (need > sizeof(full)) { errno = ENAMETOOLONG; goto fail; }
        if (snprintf(full, sizeof(full), "%s/%s", path, de->d_name) < 0) {
            errno = EIO; goto fail;
        }
        if (lstat(full, &st) != 0) goto fail;
        /* Avoid presenting symlinks as traversable folders. */
        if (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode)) continue;

        item.name = tim_strdup(de->d_name);
        if (!item.name) goto fail;
        item.size = S_ISREG(st.st_mode) ? (uint64_t)st.st_size : 0;
        item.is_directory = S_ISDIR(st.st_mode) ? 1 : 0;

        if (used == capacity) {
            size_t next = capacity ? capacity * 2 : 16;
            tim_dir_entry *grown = (tim_dir_entry *)realloc(items, next * sizeof(*items));
            if (!grown) { free(item.name); goto fail; }
            items = grown;
            capacity = next;
        }
        items[used++] = item;
        errno = 0;
    }
    if (errno != 0) goto fail;
    closedir(dir);
    *entries = items;
    *count = used;
    return 0;

fail:
    {
        int e = errno ? errno : EIO;
        closedir(dir);
        tim_free_directory(items, used);
        errno = e;
        return -1;
    }
}

void tim_free_directory(tim_dir_entry *entries, size_t count) {
    size_t i;
    for (i = 0; i < count; ++i) free(entries[i].name);
    free(entries);
}
