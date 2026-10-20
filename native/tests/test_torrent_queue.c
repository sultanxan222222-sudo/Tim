#include "../src/torrent_queue.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_init_and_add(void) {
    TimTorrentQueue q;
    tim_torrent_queue_init(&q);
    assert(q.count == 0);
    assert(tim_torrent_queue_get(&q, 0) == NULL);
    assert(tim_torrent_queue_add(&q, "magnet:?xt=urn:btih:abc", "Legal test torrent") == 0);
    assert(q.count == 1);
    assert(strcmp(tim_torrent_queue_get(&q, 0)->name, "Legal test torrent") == 0);
    assert(tim_torrent_queue_get(&q, 0)->state == TIM_TORRENT_QUEUED);
}

static void test_reject_invalid_duplicate_and_overlong(void) {
    TimTorrentQueue q;
    char long_uri[sizeof(((TimTorrentTask *)0)->source_uri) + 1];
    char long_name[sizeof(((TimTorrentTask *)0)->name) + 1];
    tim_torrent_queue_init(&q);
    memset(long_uri, 'u', sizeof(long_uri) - 1);
    long_uri[sizeof(long_uri) - 1] = '\0';
    memset(long_name, 'n', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    assert(tim_torrent_queue_add(NULL, "magnet:?x", "x") == -1);
    assert(tim_torrent_queue_add(&q, NULL, "x") == -1);
    assert(tim_torrent_queue_add(&q, "", "x") == -1);
    assert(tim_torrent_queue_add(&q, long_uri, "x") == -4);
    assert(tim_torrent_queue_add(&q, "uri:valid", long_name) == -4);
    assert(tim_torrent_queue_add(&q, "magnet:?x", "first") == 0);
    assert(tim_torrent_queue_add(&q, "magnet:?x", "duplicate") == -3);
    assert(q.count == 1);
}

static void test_remove_and_compact(void) {
    TimTorrentQueue q;
    tim_torrent_queue_init(&q);
    assert(tim_torrent_queue_add(&q, "uri:one", "one") == 0);
    assert(tim_torrent_queue_add(&q, "uri:two", "two") == 1);
    assert(tim_torrent_queue_add(&q, "uri:three", "three") == 2);
    assert(tim_torrent_queue_remove(&q, 1) == 0);
    assert(q.count == 2);
    assert(strcmp(tim_torrent_queue_get(&q, 1)->source_uri, "uri:three") == 0);
    assert(tim_torrent_queue_remove(&q, 2) == -1);
}

static void test_update_requires_matching_uri(void) {
    TimTorrentQueue q;
    TimTorrentTask status;
    const TimTorrentTask *task;
    tim_torrent_queue_init(&q);
    assert(tim_torrent_queue_add(&q, "magnet:?xt=urn:btih:test", "test") == 0);
    memset(&status, 0, sizeof(status));
    strcpy(status.source_uri, "magnet:?xt=urn:btih:other");
    status.state = TIM_TORRENT_DOWNLOADING;
    assert(tim_torrent_queue_update(&q, 0, &status) == -2);
    strcpy(status.source_uri, "magnet:?xt=urn:btih:test");
    status.received_bytes = 50;
    status.total_bytes = 100;
    status.state = TIM_TORRENT_DOWNLOADING;
    assert(tim_torrent_queue_update(&q, 0, &status) == 0);
    task = tim_torrent_queue_get(&q, 0);
    assert(task->state == TIM_TORRENT_DOWNLOADING);
    assert(tim_torrent_progress(task) == 50);
    assert(tim_torrent_queue_update(&q, 4, &status) == -1);
}

static void test_capacity(void) {
    TimTorrentQueue q;
    char uri[32];
    size_t i;
    tim_torrent_queue_init(&q);
    for (i = 0; i < TIM_TORRENT_QUEUE_CAPACITY; ++i) {
        snprintf(uri, sizeof(uri), "test-uri:%lu", (unsigned long)i);
        assert(tim_torrent_queue_add(&q, uri, "capacity test") == (int)i);
    }
    assert(q.count == TIM_TORRENT_QUEUE_CAPACITY);
    assert(tim_torrent_queue_add(&q, "one-too-many", "overflow") == -2);
}

static void test_progress_edges(void) {
    TimTorrentTask task;
    memset(&task, 0, sizeof(task));
    assert(tim_torrent_progress(NULL) == 0);
    assert(tim_torrent_progress(&task) == 0);
    task.total_bytes = 100;
    task.received_bytes = 99;
    assert(tim_torrent_progress(&task) == 99);
    task.received_bytes = 100;
    assert(tim_torrent_progress(&task) == 100);
    task.received_bytes = 101;
    assert(tim_torrent_progress(&task) == 100);
}

int main(void) {
    test_init_and_add();
    test_reject_invalid_duplicate_and_overlong();
    test_remove_and_compact();
    test_update_requires_matching_uri();
    test_capacity();
    test_progress_edges();
    puts("All torrent queue tests passed.");
    return 0;
}
