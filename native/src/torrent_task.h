#ifndef TIM_TORRENT_TASK_H
#define TIM_TORRENT_TASK_H

/*
 * Tim native torrent task model.
 *
 * This file deliberately contains only task/state definitions. It does not
 * simulate downloading. The future engine adapter must update received_bytes,
 * total_bytes, download_rate and state from real engine callbacks/status.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum TimTorrentState {
    TIM_TORRENT_INVALID = 0,
    TIM_TORRENT_QUEUED,
    TIM_TORRENT_METADATA,
    TIM_TORRENT_DOWNLOADING,
    TIM_TORRENT_PAUSED,
    TIM_TORRENT_VERIFYING,
    TIM_TORRENT_COMPLETE,
    TIM_TORRENT_ERROR
} TimTorrentState;

typedef struct TimTorrentTask {
    char id[41];              /* info-hash when known; empty before metadata */
    char name[256];
    char source_uri[2048];    /* magnet URI or local/remote .torrent URI */
    char error[256];
    uint64_t received_bytes;
    uint64_t total_bytes;
    uint64_t download_rate;   /* bytes per second, from engine */
    uint64_t upload_rate;     /* bytes per second, from engine */
    unsigned int peers;
    unsigned int seeds;
    TimTorrentState state;
} TimTorrentTask;

/* Return 0 if progress is unavailable, otherwise whole percentage [0, 100]. */
static inline unsigned int tim_torrent_progress(const TimTorrentTask *task) {
    if (task == 0 || task->total_bytes == 0) return 0;
    if (task->received_bytes >= task->total_bytes) return 100;
    /* Avoid multiplication overflow on large torrents. */
    return (unsigned int)((task->received_bytes / task->total_bytes) * 100u
        + ((task->received_bytes % task->total_bytes) * 100u) / task->total_bytes);
}

#ifdef __cplusplus
}
#endif

#endif
