#include "http_url.h"

#include <stddef.h>
#include <string.h>

static int ascii_lower(int c) {
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

static int scheme_matches(const char *url, const char *scheme, size_t length) {
    size_t i;
    for (i = 0; i < length; ++i) {
        if (ascii_lower((unsigned char)url[i]) != scheme[i]) return 0;
    }
    return url[length] == '/' && url[length + 1] == '/';
}

static int is_hex(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static int valid_port(const char *start, const char *end) {
    unsigned int port = 0;
    const char *p;
    if (start == end) return 0;
    for (p = start; p < end; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c < '0' || c > '9') return 0;
        port = port * 10U + (unsigned int)(c - '0');
        if (port > 65535U) return 0;
    }
    return port != 0;
}

static int valid_dns_or_ipv4_host(const char *start, const char *end) {
    const char *label = start;
    const char *p;
    size_t host_length = (size_t)(end - start);

    if (host_length == 0 || host_length > 253) return 0;
    /* An FQDN may end with one dot, but empty interior labels are rejected. */
    if (end > start && end[-1] == '.') --end;
    if (end == start) return 0;

    for (p = start; p <= end; ++p) {
        if (p == end || *p == '.') {
            size_t label_length = (size_t)(p - label);
            if (label_length == 0 || label_length > 63 ||
                label[0] == '-' || label[label_length - 1] == '-') return 0;
            label = p + 1;
            continue;
        }
        if (!( (*p >= 'a' && *p <= 'z') ||
               (*p >= 'A' && *p <= 'Z') ||
               (*p >= '0' && *p <= '9') || *p == '-')) return 0;
    }
    return 1;
}

static int valid_ipv4_tail(const char *start, const char *end) {
    const char *p = start;
    unsigned int parts = 0;
    while (p < end) {
        unsigned int value = 0;
        unsigned int digits = 0;
        const char *segment = p;
        while (p < end && *p >= '0' && *p <= '9') {
            value = value * 10U + (unsigned int)(*p - '0');
            if (value > 255U) return 0;
            ++digits;
            ++p;
        }
        if (digits == 0 || digits > 3 || (digits > 1 && *segment == '0'))
            return 0;
        ++parts;
        if (p == end) break;
        if (*p != '.' || parts >= 4) return 0;
        ++p;
    }
    return parts == 4;
}

/* Parse IPv6 groups instead of accepting any string made of hex and colons:
   a false positive here otherwise lets malformed URLs reach sceHttp. */
static int valid_ipv6_literal(const char *start, const char *end) {
    const char *p = start;
    int words = 0;
    int compressed = 0;
    if (start == end) return 0;

    if (*p == ':') {
        if (p + 1 == end || p[1] != ':') return 0;
        compressed = 1;
        p += 2;
        if (p == end) return 1; /* :: */
    }

    while (p < end) {
        const char *group = p;
        size_t group_len;
        while (p < end && *p != ':') ++p;
        group_len = (size_t)(p - group);
        if (group_len == 0) return 0;

        if (memchr(group, '.', group_len) != NULL) {
            /* IPv4 may appear only as the final 32 bits. */
            if (p != end || !valid_ipv4_tail(group, end)) return 0;
            words += 2;
            break;
        }

        if (group_len > 4) return 0;
        {
            const char *q;
            for (q = group; q < p; ++q)
                if (!is_hex((unsigned char)*q)) return 0;
        }
        ++words;
        if (p == end) break;

        if (p + 1 < end && p[1] == ':') {
            if (compressed) return 0;
            compressed = 1;
            p += 2;
            if (p == end) break;
        } else {
            ++p;
            if (p == end) return 0;
        }
    }

    /* Without :: an IPv6 address has exactly 8 16-bit words. With :: at
       least one word must be omitted, so explicit words must be < 8. */
    return compressed ? (words < 8) : (words == 8);
}

int tim_http_url_is_valid(const char *url) {
    const char *authority, *authority_end, *host_end, *port_start = NULL;
    size_t len = 0, scheme_length, i;

    if (url == NULL) return 0;
    /* Bounded scan prevents unbounded strlen on an overlong user-entered URL. */
    while (len < 2048 && url[len] != '\0') ++len;
    if (len == 2048 || len < 8) return 0;

    for (i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)url[i];
        if (c <= 0x20 || c >= 0x7f || c == '\\' ||
            c == '"' || c == '<' || c == '>' || c == '|' ||
            c == '{' || c == '}' || c == '^' || c == '`') return 0;
        if (c == '%' && (i + 2 >= len ||
            !is_hex((unsigned char)url[i + 1]) ||
            !is_hex((unsigned char)url[i + 2]))) return 0;
    }

    if (scheme_matches(url, "http:", 5)) {
        scheme_length = 7;
    } else if (scheme_matches(url, "https:", 6)) {
        scheme_length = 8;
    } else {
        return 0;
    }

    authority = url + scheme_length;
    authority_end = authority + strcspn(authority, "/?#");
    if (authority == authority_end) return 0;

    /* Reject user-info (credentials), which is not needed for package URLs. */
    for (i = 0; authority + i < authority_end; ++i) {
        if (authority[i] == '@') return 0;
    }

    if (*authority == '[') {
        const char *close = (const char *)memchr(authority, ']',
            (size_t)(authority_end - authority));
        if (close == NULL ||
            !valid_ipv6_literal(authority + 1, close)) return 0;
        host_end = close + 1;
        if (host_end < authority_end) {
            if (*host_end != ':') return 0;
            port_start = host_end + 1;
        }
    } else {
        const char *colon = NULL;
        const char *p;
        host_end = authority_end;
        for (p = authority; p < authority_end; ++p) {
            if (*p == '[' || *p == ']') return 0;
            if (*p == ':') {
                if (colon != NULL) return 0; /* IPv6 must use brackets. */
                colon = p;
            }
        }
        if (colon != NULL) {
            host_end = colon;
            port_start = colon + 1;
        }
        if (!valid_dns_or_ipv4_host(authority, host_end)) return 0;
    }

    if (port_start != NULL && !valid_port(port_start, authority_end)) return 0;
    return 1;
}
