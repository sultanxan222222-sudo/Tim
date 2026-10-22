#ifndef TIM_FILE_OPS_H
#define TIM_FILE_OPS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TIM_PATH_MAX 1024
#define TIM_COPY_BUFFER_SIZE (256u * 1024u)

typedef int (*tim_copy_progress_fn)(uint64_t copied, uint64_t total, void *userdata);

/* Returns 0 on success, -1 on failure (errno is set). */
int tim_copy_regular_file(const char *source,
                          const char *destination,
                          tim_copy_progress_fn progress,
                          void *userdata);

/* Returns 0 on success, -1 on failure. Only returns regular files/directories
   and skips "." and "..". The caller owns the returned memory. */
typedef struct tim_dir_entry {
    char *name;
    uint64_t size;
    unsigned char is_directory;
} tim_dir_entry;

int tim_list_directory(const char *path, tim_dir_entry **entries, size_t *count);
void tim_free_directory(tim_dir_entry *entries, size_t count);

#ifdef __cplusplus
}
#endif
#endif
