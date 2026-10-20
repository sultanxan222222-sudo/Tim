CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -O2

.PHONY: test clean

test: build/test_torrent_queue
	./build/test_torrent_queue

build/test_torrent_queue: src/torrent_queue.c tests/test_torrent_queue.c src/torrent_queue.h src/torrent_task.h
	mkdir -p build
	$(CC) $(CFLAGS) src/torrent_queue.c tests/test_torrent_queue.c -o $@

clean:
	rm -rf build
