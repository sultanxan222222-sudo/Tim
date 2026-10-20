#ifndef TIM_TORRENT_QUEUE_H
#define TIM_TORRENT_QUEUE_H

#include "torrent_task.h"
#include <stddef.h>

#define TIM_TORRENT_QUEUE_CAPACITY 32

typedef struct TimTorrentQueue {
    TimTorrentTask tasks[TIM_TORRENT_QUEUE_CAPACITY];
    size_t count;
} TimTorrentQueue;

void tim_torrent_queue_init(TimTorrentQueue *queue);
/* add: index on success; -1 invalid input, -2 full, -3 duplicate URI, -4 input too long. */
int tim_torrent_queue_add(TimTorrentQueue *queue, const char *uri, const char *name);
int tim_torrent_queue_remove(TimTorrentQueue *queue, size_t index);
const TimTorrentTask *tim_torrent_queue_get(const TimTorrentQueue *queue, size_t index);

/* Must be called with real status obtained from the torrent engine. */
int tim_torrent_queue_update(TimTorrentQueue *queue, size_t index,
    const TimTorrentTask *engine_status);

#endif
