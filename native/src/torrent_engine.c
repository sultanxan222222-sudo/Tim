#include "torrent_engine.h"
#include <string.h>

static int backend_is_complete(const TimTorrentBackend *backend) {
    return backend != NULL &&
        backend->start != NULL &&
        backend->pause != NULL &&
        backend->resume != NULL &&
        backend->cancel != NULL &&
        backend->poll != NULL;
}

static int engine_is_valid(const TimTorrentEngine *engine) {
    return engine != NULL && engine->initialized &&
        engine->queue != NULL && backend_is_complete(&engine->backend);
}

static int find_item(const TimTorrentEngine *engine, const char *uri) {
    size_t i;
    for (i = 0; i < TIM_TORRENT_QUEUE_CAPACITY; ++i) {
        if (engine->items[i].active &&
            strcmp(engine->items[i].source_uri, uri) == 0)
            return (int)i;
    }
    return -1;
}

static int find_free_item(const TimTorrentEngine *engine) {
    size_t i;
    for (i = 0; i < TIM_TORRENT_QUEUE_CAPACITY; ++i)
        if (!engine->items[i].active) return (int)i;
    return -1;
}

static int get_active(TimTorrentEngine *engine, size_t index,
                      TimEngineItem **item_out) {
    int item_index;
    if (!engine_is_valid(engine) || index >= engine->queue->count)
        return TIM_ENGINE_INVALID;
    item_index = find_item(engine, engine->queue->tasks[index].source_uri);
    if (item_index < 0) return TIM_ENGINE_NOT_FOUND;
    *item_out = &engine->items[item_index];
    return TIM_ENGINE_OK;
}

int tim_torrent_engine_init(TimTorrentEngine *engine, TimTorrentQueue *queue,
                            const TimTorrentBackend *backend) {
    if (engine == NULL || queue == NULL ||
        queue->count > TIM_TORRENT_QUEUE_CAPACITY)
        return TIM_ENGINE_INVALID;
    memset(engine, 0, sizeof(*engine));
    if (!backend_is_complete(backend)) return TIM_ENGINE_NO_BACKEND;
    engine->queue = queue;
    engine->backend = *backend;
    engine->initialized = 1;
    return TIM_ENGINE_OK;
}

int tim_torrent_engine_start(TimTorrentEngine *engine, size_t index,
                             const char *save_path) {
    TimTorrentTask status;
    TimTorrentTask *task;
    TimEngineItem *item;
    void *handle = NULL;
    int item_index;
    int rc;
    if (!engine_is_valid(engine) || index >= engine->queue->count ||
        save_path == NULL || save_path[0] == '\0')
        return TIM_ENGINE_INVALID;
    task = &engine->queue->tasks[index];
    if (find_item(engine, task->source_uri) >= 0) return TIM_ENGINE_INVALID;
    item_index = find_free_item(engine);
    if (item_index < 0) return TIM_ENGINE_BACKEND_ERROR;

    rc = engine->backend.start(engine->backend.context,
        task->source_uri, save_path, &handle);
    if (rc != 0 || handle == NULL) return TIM_ENGINE_BACKEND_ERROR;
    item = &engine->items[item_index];
    item->handle = handle;
    item->active = 1;
    memcpy(item->source_uri, task->source_uri, strlen(task->source_uri) + 1);

    memset(&status, 0, sizeof(status));
    if (engine->backend.poll(engine->backend.context, handle, &status) != 0 ||
        strcmp(status.source_uri, task->source_uri) != 0 ||
        tim_torrent_queue_update(engine->queue, index, &status) != 0) {
        (void)engine->backend.cancel(engine->backend.context, handle);
        memset(item, 0, sizeof(*item));
        return TIM_ENGINE_BACKEND_ERROR;
    }
    return TIM_ENGINE_OK;
}

int tim_torrent_engine_pause(TimTorrentEngine *engine, size_t index) {
    TimEngineItem *item = NULL;
    int rc = get_active(engine, index, &item);
    if (rc != TIM_ENGINE_OK) return rc;
    return engine->backend.pause(engine->backend.context, item->handle) == 0
        ? TIM_ENGINE_OK : TIM_ENGINE_BACKEND_ERROR;
}

int tim_torrent_engine_resume(TimTorrentEngine *engine, size_t index) {
    TimEngineItem *item = NULL;
    int rc = get_active(engine, index, &item);
    if (rc != TIM_ENGINE_OK) return rc;
    return engine->backend.resume(engine->backend.context, item->handle) == 0
        ? TIM_ENGINE_OK : TIM_ENGINE_BACKEND_ERROR;
}

int tim_torrent_engine_cancel(TimTorrentEngine *engine, size_t index) {
    TimEngineItem *item = NULL;
    TimTorrentTask *task;
    int rc = get_active(engine, index, &item);
    if (rc != TIM_ENGINE_OK) return rc;
    if (engine->backend.cancel(engine->backend.context, item->handle) != 0)
        return TIM_ENGINE_BACKEND_ERROR;
    memset(item, 0, sizeof(*item));
    task = &engine->queue->tasks[index];
    task->state = TIM_TORRENT_QUEUED;
    task->received_bytes = 0;
    task->total_bytes = 0;
    task->download_rate = 0;
    task->upload_rate = 0;
    task->peers = 0;
    task->seeds = 0;
    return TIM_ENGINE_OK;
}

int tim_torrent_engine_poll(TimTorrentEngine *engine, size_t index) {
    TimTorrentTask status;
    TimEngineItem *item = NULL;
    int rc = get_active(engine, index, &item);
    if (rc != TIM_ENGINE_OK) return rc;
    memset(&status, 0, sizeof(status));
    if (engine->backend.poll(engine->backend.context, item->handle, &status) != 0)
        return TIM_ENGINE_BACKEND_ERROR;
    if (strcmp(status.source_uri, engine->queue->tasks[index].source_uri) != 0)
        return TIM_ENGINE_BACKEND_ERROR;
    if (tim_torrent_queue_update(engine->queue, index, &status) != 0)
        return TIM_ENGINE_BACKEND_ERROR;
    if (status.state == TIM_TORRENT_COMPLETE || status.state == TIM_TORRENT_ERROR)
        memset(item, 0, sizeof(*item));
    return TIM_ENGINE_OK;
}

void tim_torrent_engine_shutdown(TimTorrentEngine *engine) {
    size_t i;
    if (engine == NULL || !engine->initialized) return;
    for (i = 0; i < TIM_TORRENT_QUEUE_CAPACITY; ++i) {
        if (engine->items[i].active && engine->items[i].handle != NULL)
            (void)engine->backend.cancel(engine->backend.context,
                                         engine->items[i].handle);
        memset(&engine->items[i], 0, sizeof(engine->items[i]));
    }
    if (engine->backend.shutdown != NULL)
        engine->backend.shutdown(engine->backend.context);
    engine->initialized = 0;
}
