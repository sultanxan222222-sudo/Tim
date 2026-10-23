#include "http_url.h"
#include <stddef.h>
#include <string.h>

int tim_http_url_is_valid(const char *url) {
    const char *authority;
    const char *end;
    size_t i, len;
    if (url == NULL) return 0;
    len = strlen(url);
    if (len < 9 || len >= 2048) return 0;
    for (i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)url[i];
        if (c <= 0x20 || c == 0x7f) return 0;
    }
    if (strncmp(url, "https://", 8) == 0) authority = url + 8;
    else if (strncmp(url, "http://", 7) == 0) authority = url + 7;
    else return 0;
    if (*authority == '\0' || *authority == '/' || *authority == '?' ||
        *authority == '#') return 0;
    end = authority + strcspn(authority, "/?#");
    if (end == authority) return 0;
    /* Reject user-info in URLs to avoid credentials being stored in catalog
       files and sent unexpectedly. IPv6 literals remain allowed. */
    for (i = 0; authority + i < end; ++i) {
        if (authority[i] == '@') return 0;
    }
    return 1;
}
