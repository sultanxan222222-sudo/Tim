#define _POSIX_C_SOURCE 200809L
#include "file_ops.h"
#include "file_browser.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL %s:%d: %s (errno=%d)\n", __FILE__, __LINE__, #x, errno); \
    return 1; \
} } while (0)

static int progress_ok(uint64_t copied, uint64_t total, void *userdata) {
    int *calls = (int *)userdata;
    (void)total;
    if (copied > 0) ++*calls;
    return 0;
}

static int progress_cancel(uint64_t copied, uint64_t total, void *userdata) {
    (void)copied; (void)total; (void)userdata;
    return 1;
}

static int write_text(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    if (fwrite(text, 1, strlen(text), f) != strlen(text)) {
        fclose(f); return -1;
    }
    return fclose(f);
}

static int file_equals(const char *path, const char *expected) {
    char buf[128];
    size_t n;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    n = fread(buf, 1, sizeof(buf) - 1, f);
    if (ferror(f)) { fclose(f); return 0; }
    buf[n] = '\0';
    if (fclose(f) != 0) return 0;
    return strcmp(buf, expected) == 0;
}

int main(void) {
    char temp[] = "/tmp/tim-file-ops-XXXXXX";
    char source[256], destination[256], existing[256], cancelled[256], folder[256];
    tim_dir_entry *entries = NULL;
    tim_file_browser browser;
    size_t count = 0, i;
    int calls = 0, found_file = 0, found_folder = 0;

    CHECK(mkdtemp(temp) != NULL);
    CHECK(snprintf(source, sizeof(source), "%s/source.txt", temp) > 0);
    CHECK(snprintf(destination, sizeof(destination), "%s/copied.txt", temp) > 0);
    CHECK(snprintf(existing, sizeof(existing), "%s/existing.txt", temp) > 0);
    CHECK(snprintf(cancelled, sizeof(cancelled), "%s/cancelled.txt", temp) > 0);
    CHECK(snprintf(folder, sizeof(folder), "%s/subfolder", temp) > 0);
    CHECK(mkdir(folder, 0700) == 0);
    CHECK(write_text(source, "PS4 file manager test payload") == 0);

    CHECK(tim_copy_regular_file(source, destination, progress_ok, &calls) == 0);
    CHECK(file_equals(destination, "PS4 file manager test payload"));
    CHECK(calls > 0);

    CHECK(write_text(existing, "keep me") == 0);
    CHECK(tim_copy_regular_file(source, existing, NULL, NULL) == -1);
    CHECK(file_equals(existing, "keep me"));

    CHECK(tim_copy_regular_file(source, cancelled, progress_cancel, NULL) == -1);
    CHECK(access(cancelled, F_OK) != 0);

    CHECK(tim_list_directory(temp, &entries, &count) == 0);
    for (i = 0; i < count; ++i) {
        if (!strcmp(entries[i].name, "source.txt") && !entries[i].is_directory)
            found_file = 1;
        if (!strcmp(entries[i].name, "subfolder") && entries[i].is_directory)
            found_folder = 1;
    }
    CHECK(found_file && found_folder);
    tim_free_directory(entries, count);

    CHECK(tim_fm_open(&browser, temp) == 0);
    CHECK(tim_fm_parent(&browser) == 0); /* root cannot be escaped */
    for (i = 0; i < browser.count; ++i) {
        if (!strcmp(browser.entries[i].name, "source.txt")) break;
    }
    CHECK(i < browser.count);
    CHECK(tim_fm_select(&browser, i) == 0);
    CHECK(tim_fm_copy_selected(&browser, folder, NULL, NULL) == 0);
    {
        char copied_into_folder[256];
        CHECK(snprintf(copied_into_folder, sizeof(copied_into_folder), "%s/source.txt", folder) > 0);
        CHECK(file_equals(copied_into_folder, "PS4 file manager test payload"));
        CHECK(unlink(copied_into_folder) == 0);
    }
    tim_fm_close(&browser);

    CHECK(unlink(source) == 0);
    CHECK(unlink(destination) == 0);
    CHECK(unlink(existing) == 0);
    CHECK(rmdir(folder) == 0);
    CHECK(rmdir(temp) == 0);
    puts("All file operation tests passed.");
    return 0;
}
