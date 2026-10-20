#include "torrent_queue.h"
#include <stddef.h>
#include <string.h>

/*
 * Engine-neutral queue storage.
 * This module manages queue entries only; it does not simulate downloading.
 * A real engine adapter must supply verified status to tim_torrent_queue_update.
 */

void tim_torrent_queue_init(TimTorrentQueue *queue) {
    if (queue) memset(queue, 0, sizeof(*queue));
}

static int queue_is_valid(const TimTorrentQueue *queue) {
    return queue != NULL && queue->count <= TIM_TORRENT_QUEUE_CAPACITY;
}

int tim_torrent_queue_add(TimTorrentQueue *queue, const char *uri, const char *name) {
    TimTorrentTask *task;
    size_t i;
    if (!queue || !uri || uri[0] == '\0') return -1;
    /* Reject overlong input rather than silently storing a truncated URI. */
    if (strlen(uri) >= sizeof(queue->tasks[0].source_uri)) return -4;
    if (name && strlen(name) >= sizeof(queue->tasks[0].name)) return -4;
    if (queue->count >= TIM_TORRENT_QUEUE_CAPACITY) return -2;
    for (i = 0; i < queue->count; ++i) {
        if (strcmp(queue->tasks[i].source_uri, uri) == 0) return -3;
    }
    task = &queue->tasks[queue->count];
    memset(task, 0, sizeof(*task));
    memcpy(task->source_uri, uri, strlen(uri) + 1);
    if (name) memcpy(task->name, name, strlen(name) + 1);
    task->state = TIM_TORRENT_QUEUED;
    queue->count++;
    return (int)(queue->count - 1);
}

int tim_torrent_queue_remove(TimTorrentQueue *queue, size_t index) {
    if (!queue_is_valid(queue) || index >= queue->count) return -1;
    if (index + 1 < queue->count) {
        memmove(&queue->tasks[index], &queue->tasks[index + 1],
            (queue->count - index - 1) * sizeof(queue->tasks[0]));
    }
    queue->count--;
    memset(&queue->tasks[queue->count], 0, sizeof(queue->tasks[0]));
    return 0;
}

const TimTorrentTask *tim_torrent_queue_get(const TimTorrentQueue *queue, size_t index) {
    if (!queue_is_valid(queue) || index >= queue->count) return NULL;
    return &queue->tasks[index];
}

/* Called only by a real torrent-engine adapter; never use as fake progress. */
int tim_torrent_queue_update(TimTorrentQueue *queue, size_t index,
    const TimTorrentTask *engine_status) {
    if (!queue_is_valid(queue) || !engine_status || index >= queue->count) return -1;
    /* Engine adapters must provide terminated strings before status is copied. */
    if (memchr(engine_status->source_uri, '\0',
            sizeof(engine_status->source_uri)) == NULL ||
        memchr(engine_status->name, '\0', sizeof(engine_status->name)) == NULL ||
        memchr(engine_status->error, '\0', sizeof(engine_status->error)) == NULL ||
        memchr(engine_status->id, '\0', sizeof(engine_status->id)) == NULL)
        return -3;
    if (strcmp(queue->tasks[index].source_uri, engine_status->source_uri) != 0)
        return -2;
    queue->tasks[index] = *engine_status;
    return 0;
}
