#include "../src/http_url.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(tim_http_url_is_valid("https://example.com/game.pkg"));
    assert(tim_http_url_is_valid("http://192.168.1.10:8080/test.pkg"));
    assert(tim_http_url_is_valid("https://example.com/path?q=1"));
    assert(!tim_http_url_is_valid(NULL));
    assert(!tim_http_url_is_valid(""));
    assert(!tim_http_url_is_valid("ftp://example.com/game.pkg"));
    assert(!tim_http_url_is_valid("https:///game.pkg"));
    assert(!tim_http_url_is_valid("https://"));
    assert(!tim_http_url_is_valid("https://user:pass@example.com/game.pkg"));
    assert(!tim_http_url_is_valid("https://example.com/a b.pkg"));
    assert(!tim_http_url_is_valid("https://example.com/line\nbreak.pkg"));
    puts("HTTP URL validation tests passed");
    return 0;
}
