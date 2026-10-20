#ifndef TIM_TORRENT_ENGINE_H
#define TIM_TORRENT_ENGINE_H

#include "torrent_queue.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Backend boundary for a real torrent library.
 *
 * The backend must perform real torrent operations and report real status.
 * This interface intentionally contains no fake downloader or synthetic
 * progress. Until a backend is supplied, init fails with TIM_ENGINE_NO_BACKEND.
 */
typedef enum TimEngineResult {
    TIM_ENGINE_OK = 0,
    TIM_ENGINE_INVALID = -1,
    TIM_ENGINE_NO_BACKEND = -2,
    TIM_ENGINE_BACKEND_ERROR = -3,
    TIM_ENGINE_NOT_FOUND = -4
} TimEngineResult;

typedef struct TimTorrentBackend {
    void *context;
    int (*start)(void *context, const char *uri, const char *save_path,
                 void **handle_out);
    int (*pause)(void *context, void *handle);
    int (*resume)(void *context, void *handle);
    int (*cancel)(void *context, void *handle);
    int (*poll)(void *context, void *handle, TimTorrentTask *status_out);
    void (*shutdown)(void *context);
} TimTorrentBackend;

typedef struct TimEngineItem {
    void *handle;
    int active;
} TimEngineItem;

typedef struct TimTorrentEngine {
    TimTorrentQueue *queue;
    TimTorrentBackend backend;
    TimEngineItem items[TIM_TORRENT_QUEUE_CAPACITY];
    int initialized;
} TimTorrentEngine;

int tim_torrent_engine_init(TimTorrentEngine *engine, TimTorrentQueue *queue,
                            const TimTorrentBackend *backend);
int tim_torrent_engine_start(TimTorrentEngine *engine, size_t index,
                             const char *save_path);
int tim_torrent_engine_pause(TimTorrentEngine *engine, size_t index);
int tim_torrent_engine_resume(TimTorrentEngine *engine, size_t index);
int tim_torrent_engine_cancel(TimTorrentEngine *engine, size_t index);
/* Poll one active task and copy real backend status into the queue. */
int tim_torrent_engine_poll(TimTorrentEngine *engine, size_t index);
void tim_torrent_engine_shutdown(TimTorrentEngine *engine);

#ifdef __cplusplus
}
#endif

#endif
