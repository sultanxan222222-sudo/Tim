#ifndef TIM_FILE_BROWSER_H
#define TIM_FILE_BROWSER_H

#include "file_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tim_file_browser {
    char root[TIM_PATH_MAX];
    char current[TIM_PATH_MAX];
    tim_dir_entry *entries;
    size_t count;
    size_t selected;
} tim_file_browser;

/* Initialize a browser rooted at an already-mounted directory. */
int tim_fm_open(tim_file_browser *browser, const char *root);
void tim_fm_close(tim_file_browser *browser);
int tim_fm_refresh(tim_file_browser *browser);
int tim_fm_select(tim_file_browser *browser, size_t index);
int tim_fm_enter_selected(tim_file_browser *browser);
int tim_fm_parent(tim_file_browser *browser);

/* Copy the selected file or directory into destination_directory without overwrite. */
int tim_fm_copy_selected(const tim_file_browser *browser,
                         const char *destination_directory,
                         tim_copy_progress_fn progress,
                         void *userdata);

#ifdef __cplusplus
}
#endif

#endif
