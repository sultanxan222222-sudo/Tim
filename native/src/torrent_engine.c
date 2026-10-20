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

static int get_active(TimTorrentEngine *engine, size_t index, void **handle) {
    if (!engine_is_valid(engine) || index >= engine->queue->count)
        return TIM_ENGINE_INVALID;
    if (!engine->items[index].active || engine->items[index].handle == NULL)
        return TIM_ENGINE_NOT_FOUND;
    *handle = engine->items[index].handle;
    return TIM_ENGINE_OK;
}

int tim_torrent_engine_init(TimTorrentEngine *engine, TimTorrentQueue *queue,
                            const TimTorrentBackend *backend) {
    if (engine == NULL || queue == NULL || queue->count > TIM_TORRENT_QUEUE_CAPACITY)
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
    void *handle = NULL;
    int rc;
    if (!engine_is_valid(engine) || index >= engine->queue->count ||
        save_path == NULL || save_path[0] == '\0')
        return TIM_ENGINE_INVALID;
    if (engine->items[index].active) return TIM_ENGINE_INVALID;
    rc = engine->backend.start(engine->backend.context,
        engine->queue->tasks[index].source_uri, save_path, &handle);
    if (rc != 0 || handle == NULL) return TIM_ENGINE_BACKEND_ERROR;
    engine->items[index].handle = handle;
    engine->items[index].active = 1;

    memset(&status, 0, sizeof(status));
    if (engine->backend.poll(engine->backend.context, handle, &status) != 0 ||
        strcmp(status.source_uri, engine->queue->tasks[index].source_uri) != 0 ||
        tim_torrent_queue_update(engine->queue, index, &status) != 0) {
        (void)engine->backend.cancel(engine->backend.context, handle);
        engine->items[index].handle = NULL;
        engine->items[index].active = 0;
        return TIM_ENGINE_BACKEND_ERROR;
    }
    return TIM_ENGINE_OK;
}

int tim_torrent_engine_pause(TimTorrentEngine *engine, size_t index) {
    void *handle = NULL;
    int rc = get_active(engine, index, &handle);
    if (rc != TIM_ENGINE_OK) return rc;
    return engine->backend.pause(engine->backend.context, handle) == 0
        ? TIM_ENGINE_OK : TIM_ENGINE_BACKEND_ERROR;
}

int tim_torrent_engine_resume(TimTorrentEngine *engine, size_t index) {
    void *handle = NULL;
    int rc = get_active(engine, index, &handle);
    if (rc != TIM_ENGINE_OK) return rc;
    return engine->backend.resume(engine->backend.context, handle) == 0
        ? TIM_ENGINE_OK : TIM_ENGINE_BACKEND_ERROR;
}

int tim_torrent_engine_cancel(TimTorrentEngine *engine, size_t index) {
    void *handle = NULL;
    int rc = get_active(engine, index, &handle);
    if (rc != TIM_ENGINE_OK) return rc;
    if (engine->backend.cancel(engine->backend.context, handle) != 0)
        return TIM_ENGINE_BACKEND_ERROR;
    engine->items[index].handle = NULL;
    engine->items[index].active = 0;
    engine->queue->tasks[index].state = TIM_TORRENT_QUEUED;
    engine->queue->tasks[index].received_bytes = 0;
    engine->queue->tasks[index].total_bytes = 0;
    engine->queue->tasks[index].download_rate = 0;
    engine->queue->tasks[index].upload_rate = 0;
    engine->queue->tasks[index].peers = 0;
    engine->queue->tasks[index].seeds = 0;
    return TIM_ENGINE_OK;
}

int tim_torrent_engine_poll(TimTorrentEngine *engine, size_t index) {
    TimTorrentTask status;
    void *handle = NULL;
    int rc = get_active(engine, index, &handle);
    if (rc != TIM_ENGINE_OK) return rc;
    memset(&status, 0, sizeof(status));
    if (engine->backend.poll(engine->backend.context, handle, &status) != 0)
        return TIM_ENGINE_BACKEND_ERROR;
    if (strcmp(status.source_uri, engine->queue->tasks[index].source_uri) != 0)
        return TIM_ENGINE_BACKEND_ERROR;
    if (tim_torrent_queue_update(engine->queue, index, &status) != 0)
        return TIM_ENGINE_BACKEND_ERROR;
    if (status.state == TIM_TORRENT_COMPLETE || status.state == TIM_TORRENT_ERROR) {
        engine->items[index].active = 0;
        engine->items[index].handle = NULL;
    }
    return TIM_ENGINE_OK;
}

void tim_torrent_engine_shutdown(TimTorrentEngine *engine) {
    size_t i;
    if (engine == NULL || !engine->initialized) return;
    for (i = 0; i < TIM_TORRENT_QUEUE_CAPACITY; ++i) {
        if (engine->items[i].active && engine->items[i].handle != NULL) {
            (void)engine->backend.cancel(engine->backend.context, engine->items[i].handle);
            engine->items[i].handle = NULL;
            engine->items[i].active = 0;
        }
    }
    if (engine->backend.shutdown != NULL)
        engine->backend.shutdown(engine->backend.context);
    engine->initialized = 0;
}
