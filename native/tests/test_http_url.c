#include "../src/http_url.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char too_long[2049];

    assert(tim_http_url_is_valid("https://example.com/game.pkg"));
    assert(tim_http_url_is_valid("HTTP://192.168.1.10:8080/test.pkg"));
    assert(tim_http_url_is_valid("https://example.com/path?q=1"));
    assert(tim_http_url_is_valid("https://example.com./file.pkg"));
    assert(tim_http_url_is_valid("http://localhost:8080/file.pkg"));
    assert(tim_http_url_is_valid("https://[2001:db8::1]:8443/a.pkg"));
    assert(tim_http_url_is_valid("http://a/x")); /* legal one-label host */

    assert(!tim_http_url_is_valid(NULL));
    assert(!tim_http_url_is_valid(""));
    assert(!tim_http_url_is_valid("ftp://example.com/game.pkg"));
    assert(!tim_http_url_is_valid("https:///game.pkg"));
    assert(!tim_http_url_is_valid("https://"));
    assert(!tim_http_url_is_valid("https://user:pass@example.com/game.pkg"));
    assert(!tim_http_url_is_valid("https://example.com/a b.pkg"));
    assert(!tim_http_url_is_valid("https://example.com/line\nbreak.pkg"));
    assert(!tim_http_url_is_valid("https://example.com/path\\other.pkg"));
    assert(!tim_http_url_is_valid("http://:8080/file.pkg"));
    assert(!tim_http_url_is_valid("http://example.com:"));
    assert(!tim_http_url_is_valid("http://example.com:0/file.pkg"));
    assert(!tim_http_url_is_valid("http://example.com:65536/file.pkg"));
    assert(!tim_http_url_is_valid("http://example.com:abc/file.pkg"));
    assert(!tim_http_url_is_valid("https://-bad.example/file.pkg"));
    assert(!tim_http_url_is_valid("https://example..com/file.pkg"));
    assert(!tim_http_url_is_valid("https://[:::]/file.pkg"));
    assert(!tim_http_url_is_valid("https://[::1]junk/file.pkg"));
    assert(!tim_http_url_is_valid("https://example.com/%ZZ/file.pkg"));

    memset(too_long, 'a', sizeof(too_long));
    memcpy(too_long, "https://", 8);
    too_long[sizeof(too_long) - 1] = '\0';
    assert(!tim_http_url_is_valid(too_long));

    puts("HTTP URL validation tests passed");
    return 0;
}
