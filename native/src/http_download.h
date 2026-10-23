#ifndef TIM_HTTP_DOWNLOAD_H
#define TIM_HTTP_DOWNLOAD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum TimHttpDownloadState {
    TIM_HTTP_IDLE = 0,
    TIM_HTTP_CONNECTING,
    TIM_HTTP_DOWNLOADING,
    TIM_HTTP_COMPLETE,
    TIM_HTTP_ERROR,
    TIM_HTTP_CANCELLED
} TimHttpDownloadState;

typedef struct TimHttpDownload {
    int connection_id;
    int request_id;
    FILE *output_file; /* opaque to callers; stdio FILE is forward-declared below */
    char url[2048];
    char destination[1024];
    char partial_path[1030];
    char error[256];
    uint64_t received_bytes;
    uint64_t total_bytes; /* zero when the server does not provide a length */
    int http_status;
    TimHttpDownloadState state;
} TimHttpDownload;

/* Initialize networking/SSL/HTTP once before creating downloads. */
int tim_http_download_system_init(void);
/* destination is the final local filename; data is written to destination.part
   and renamed only after a successful complete transfer. */
int tim_http_download_start(TimHttpDownload *download, const char *url,
                            const char *destination);
/* Read at most max_bytes per call. Returns 1 while active, 0 on completion,
   -1 on error. A zero max_bytes uses a 32 KiB chunk. */
int tim_http_download_step(TimHttpDownload *download, size_t max_bytes);
void tim_http_download_cancel(TimHttpDownload *download);
void tim_http_download_system_shutdown(void);

#ifdef __cplusplus
}
#endif
#endif
