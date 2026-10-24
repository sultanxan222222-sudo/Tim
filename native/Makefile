CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -O2

.PHONY: test clean
test: build/test_torrent_queue build/test_torrent_engine build/test_app build/test_http_url
	./build/test_torrent_queue
	./build/test_torrent_engine
	./build/test_app
	./build/test_http_url

build:
	mkdir -p build

build/test_torrent_queue: src/torrent_queue.c tests/test_torrent_queue.c src/torrent_queue.h src/torrent_task.h | build
	$(CC) $(CFLAGS) src/torrent_queue.c tests/test_torrent_queue.c -o $@

build/test_torrent_engine: src/torrent_queue.c src/torrent_engine.c tests/test_torrent_engine.c src/torrent_engine.h src/torrent_queue.h src/torrent_task.h | build
	$(CC) $(CFLAGS) src/torrent_queue.c src/torrent_engine.c tests/test_torrent_engine.c -o $@

build/test_app: src/tim_app.c tests/test_app.c src/tim_app.h | build
	$(CC) $(CFLAGS) src/tim_app.c tests/test_app.c -o $@

build/test_http_url: src/http_url.c tests/test_http_url.c src/http_url.h | build
	$(CC) $(CFLAGS) src/http_url.c tests/test_http_url.c -o $@

clean:
	rm -rf build
