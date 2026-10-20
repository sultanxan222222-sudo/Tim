#include "torrent_queue.h"
#include <stddef.h>
#include <string.h>

/*
 * Engine-neutral queue storage.
 * This module manages queue entries only. It intentionally does not claim to
 * download data: a real libtorrent/PS4 adapter must call update_from_engine()
 * with verified statistics from the engine.
 */

void tim_torrent_queue_init(TimTorrentQueue *queue) {
    if (queue) {
        memset(queue, 0, sizeof(*queue));
    }
}

int tim_torrent_queue_add(TimTorrentQueue *queue, const char *uri, const char *name) {
    TimTorrentTask *task;
    size_t i;
    if (!queue || !uri || uri[0] == '\0') return -1;
    if (queue->count >= TIM_TORRENT_QUEUE_CAPACITY) return -2;
    for (i = 0; i < queue->count; ++i) {
        if (strcmp(queue->tasks[i].source_uri, uri) == 0) return -3;
    }
    task = &queue->tasks[queue->count];
    memset(task, 0, sizeof(*task));
    strncpy(task->source_uri, uri, sizeof(task->source_uri) - 1);
    if (name) strncpy(task->name, name, sizeof(task->name) - 1);
    task->state = TIM_TORRENT_QUEUED;
    queue->count++;
    return (int)(queue->count - 1);
}

int tim_torrent_queue_remove(TimTorrentQueue *queue, size_t index) {
    if (!queue || index >= queue->count) return -1;
    if (index + 1 < queue->count) {
        memmove(&queue->tasks[index], &queue->tasks[index + 1],
            (queue->count - index - 1) * sizeof(queue->tasks[0]));
    }
    queue->count--;
    memset(&queue->tasks[queue->count], 0, sizeof(queue->tasks[0]));
    return 0;
}

const TimTorrentTask *tim_torrent_queue_get(const TimTorrentQueue *queue, size_t index) {
    if (!queue || index >= queue->count) return NULL;
    return &queue->tasks[index];
}

/* Called only by a real torrent-engine adapter; never use as fake progress. */
int tim_torrent_queue_update(TimTorrentQueue *queue, size_t index,
    const TimTorrentTask *engine_status) {
    if (!queue || !engine_status || index >= queue->count) return -1;
    if (strcmp(queue->tasks[index].source_uri, engine_status->source_uri) != 0)
        return -2;
    queue->tasks[index] = *engine_status;
    return 0;
}
