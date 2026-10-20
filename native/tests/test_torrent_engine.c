#include "../src/torrent_engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct FakeBackend {
    TimTorrentTask status;
    int starts;
    int pauses;
    int resumes;
    int cancels;
    int polls;
    int fail_poll;
} FakeBackend;

static int fake_start(void *context, const char *uri, const char *save_path,
                      void **handle_out) {
    FakeBackend *fake = (FakeBackend *)context;
    if (!uri || !save_path || !handle_out) return -1;
    strcpy(fake->status.source_uri, uri);
    strcpy(fake->status.name, "Engine contract test");
    fake->status.state = TIM_TORRENT_DOWNLOADING;
    fake->status.total_bytes = 1000;
    fake->status.received_bytes = 0;
    *handle_out = fake;
    fake->starts++;
    return 0;
}

static int fake_pause(void *context, void *handle) {
    FakeBackend *fake = (FakeBackend *)context;
    if (handle != fake) return -1;
    fake->pauses++;
    fake->status.state = TIM_TORRENT_PAUSED;
    return 0;
}

static int fake_resume(void *context, void *handle) {
    FakeBackend *fake = (FakeBackend *)context;
    if (handle != fake) return -1;
    fake->resumes++;
    fake->status.state = TIM_TORRENT_DOWNLOADING;
    return 0;
}

static int fake_cancel(void *context, void *handle) {
    FakeBackend *fake = (FakeBackend *)context;
    if (handle != fake) return -1;
    fake->cancels++;
    return 0;
}

static int fake_poll(void *context, void *handle, TimTorrentTask *out) {
    FakeBackend *fake = (FakeBackend *)context;
    if (handle != fake || !out || fake->fail_poll) return -1;
    fake->polls++;
    *out = fake->status;
    return 0;
}

static TimTorrentBackend make_backend(FakeBackend *fake) {
    TimTorrentBackend backend;
    memset(&backend, 0, sizeof(backend));
    backend.context = fake;
    backend.start = fake_start;
    backend.pause = fake_pause;
    backend.resume = fake_resume;
    backend.cancel = fake_cancel;
    backend.poll = fake_poll;
    return backend;
}

static void test_no_backend_is_explicit(void) {
    TimTorrentEngine engine;
    TimTorrentQueue queue;
    memset(&engine, 0, sizeof(engine));
    tim_torrent_queue_init(&queue);
    assert(tim_torrent_engine_init(&engine, &queue, NULL) == TIM_ENGINE_NO_BACKEND);
    assert(engine.initialized == 0);
}

static void test_lifecycle_and_real_status_contract(void) {
    TimTorrentEngine engine;
    TimTorrentQueue queue;
    FakeBackend fake;
    TimTorrentBackend backend;
    const TimTorrentTask *task;

    memset(&fake, 0, sizeof(fake));
    tim_torrent_queue_init(&queue);
    assert(tim_torrent_queue_add(&queue, "magnet:?xt=urn:btih:authorized-test",
                                 "Authorized test") == 0);
    backend = make_backend(&fake);
    assert(tim_torrent_engine_init(&engine, &queue, &backend) == TIM_ENGINE_OK);
    assert(tim_torrent_engine_start(&engine, 0, "/tmp/tim-test") == TIM_ENGINE_OK);
    assert(fake.starts == 1);
    assert(tim_torrent_engine_pause(&engine, 0) == TIM_ENGINE_OK);
    assert(tim_torrent_engine_resume(&engine, 0) == TIM_ENGINE_OK);

    fake.status.received_bytes = 425;
    fake.status.download_rate = 1234;
    fake.status.peers = 7;
    assert(tim_torrent_engine_poll(&engine, 0) == TIM_ENGINE_OK);
    task = tim_torrent_queue_get(&queue, 0);
    assert(task != NULL);
    assert(task->received_bytes == 425);
    assert(task->download_rate == 1234);
    assert(task->peers == 7);
    assert(tim_torrent_progress(task) == 42);

    assert(tim_torrent_engine_cancel(&engine, 0) == TIM_ENGINE_OK);
    assert(fake.cancels == 1);
    assert(tim_torrent_engine_pause(&engine, 0) == TIM_ENGINE_NOT_FOUND);
    tim_torrent_engine_shutdown(&engine);
    assert(engine.initialized == 0);
}

static void test_reject_mismatched_backend_uri_and_poll_errors(void) {
    TimTorrentEngine engine;
    TimTorrentQueue queue;
    FakeBackend fake;
    TimTorrentBackend backend;

    memset(&fake, 0, sizeof(fake));
    tim_torrent_queue_init(&queue);
    assert(tim_torrent_queue_add(&queue, "magnet:?xt=urn:btih:one", "one") == 0);
    backend = make_backend(&fake);
    assert(tim_torrent_engine_init(&engine, &queue, &backend) == TIM_ENGINE_OK);
    assert(tim_torrent_engine_start(&engine, 0, "/tmp/tim-test") == TIM_ENGINE_OK);

    strcpy(fake.status.source_uri, "magnet:?xt=urn:btih:other");
    assert(tim_torrent_engine_poll(&engine, 0) == TIM_ENGINE_BACKEND_ERROR);
    strcpy(fake.status.source_uri, "magnet:?xt=urn:btih:one");
    fake.fail_poll = 1;
    assert(tim_torrent_engine_poll(&engine, 0) == TIM_ENGINE_BACKEND_ERROR);
    tim_torrent_engine_shutdown(&engine);
}

int main(void) {
    test_no_backend_is_explicit();
    test_lifecycle_and_real_status_contract();
    test_reject_mismatched_backend_uri_and_poll_errors();
    puts("All torrent engine contract tests passed (fake backend; no network download).");
    return 0;
}
