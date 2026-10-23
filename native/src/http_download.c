#include "http_download.h"
#include "http_url.h"

#include <stdbool.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>
#include <orbis/Net.h>
#include <orbis/NetCtl.h>
#include <orbis/_types/http.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#define TIM_HTTP_POOL_SIZE (2U * 1024U * 1024U)
#define TIM_HTTP_DEFAULT_CHUNK (32U * 1024U)
#define TIM_HTTP_MAX_CHUNK (256U * 1024U)

static int g_net_ready = 0;
static int g_netctl_ready = 0;
static int g_ssl_ready = 0;
static int g_http_context = -1;
static int g_http_template = -1;

static void set_error(TimHttpDownload *d, const char *message) {
    if (d == NULL) return;
    snprintf(d->error, sizeof(d->error), "%s", message ? message : "Unknown error");
    d->state = TIM_HTTP_ERROR;
}

static void close_request(TimHttpDownload *d) {
    if (d == NULL) return;
    if (d->request_id >= 0) {
        sceHttpDeleteRequest(d->request_id);
        d->request_id = -1;
    }
    if (d->connection_id >= 0) {
        sceHttpDeleteConnection(d->connection_id);
        d->connection_id = -1;
    }
}

static void close_output(TimHttpDownload *d) {
    if (d != NULL && d->output_file != NULL) {
        fclose(d->output_file);
        d->output_file = NULL;
    }
}

int tim_http_download_system_init(void) {
    int rc;
    if (g_http_template >= 0) return 0;

    rc = sceNetInit();
    if (rc < 0) return -1;
    g_net_ready = 1;

    rc = sceNetCtlInit();
    if (rc < 0) goto fail;
    g_netctl_ready = 1;

    rc = sceSslInit(SSL_POOLSIZE);
    if (rc < 0) goto fail;
    g_ssl_ready = 1;

    g_http_context = sceHttpInit(1, 1, TIM_HTTP_POOL_SIZE);
    if (g_http_context < 0) goto fail;

    g_http_template = sceHttpCreateTemplate(g_http_context,
        "TIM-PS4-Downloader/0.1", ORBIS_HTTP_VERSION_1_1, 0);
    if (g_http_template < 0) goto fail;
    return 0;

fail:
    tim_http_download_system_shutdown();
    return -1;
}

int tim_http_download_start(TimHttpDownload *d, const char *url,
                            const char *destination) {
    int rc, length_result = ORBIS_HTTP_CONTENTLEN_NOT_FOUND;
    size_t content_length = 0;
    int status = 0;
    if (d == NULL || !tim_http_url_is_valid(url) || destination == NULL ||
        destination[0] != '/' || strlen(destination) >= sizeof(d->destination))
        return -1;
    memset(d, 0, sizeof(*d));
    d->connection_id = -1;
    d->request_id = -1;
    d->state = TIM_HTTP_CONNECTING;
    snprintf(d->url, sizeof(d->url), "%s", url);
    snprintf(d->destination, sizeof(d->destination), "%s", destination);
    if (snprintf(d->partial_path, sizeof(d->partial_path), "%s.part",
                 destination) >= (int)sizeof(d->partial_path)) {
        set_error(d, "Destination path is too long");
        return -1;
    }
    if (g_http_template < 0 && tim_http_download_system_init() != 0) {
        set_error(d, "PS4 network/HTTPS initialization failed");
        return -1;
    }

    d->connection_id = sceHttpCreateConnectionWithURL(g_http_template, d->url, 0);
    if (d->connection_id < 0) {
        set_error(d, "Could not create HTTP connection");
        close_request(d);
        return -1;
    }
    d->request_id = sceHttpCreateRequestWithURL(d->connection_id,
        ORBIS_METHOD_GET, d->url, 0);
    if (d->request_id < 0) {
        set_error(d, "Could not create HTTP request");
        close_request(d);
        return -1;
    }
    rc = sceHttpSendRequest(d->request_id, NULL, 0);
    if (rc < 0) {
        set_error(d, "HTTP request failed; check network and URL");
        close_request(d);
        return -1;
    }
    rc = sceHttpGetStatusCode(d->request_id, &status);
    if (rc < 0 || status < 200 || status >= 300) {
        d->http_status = status;
        set_error(d, status >= 300 && status < 400
            ? "Server redirected the URL; use a direct file URL"
            : "Server rejected the download (HTTP status)");
        close_request(d);
        return -1;
    }
    d->http_status = status;

    rc = sceHttpGetResponseContentLength(d->request_id, &length_result,
                                         &content_length);
    if (rc >= 0 && length_result == ORBIS_HTTP_CONTENTLEN_EXIST)
        d->total_bytes = (uint64_t)content_length;

    /* Do not create arbitrary parent directories or write outside the chosen
       path; the store is expected to pass a writable /data destination. */
    d->output_file = fopen(d->partial_path, "wb");
    if (d->output_file == NULL) {
        set_error(d, "Cannot open destination; check writable path and free space");
        close_request(d);
        return -1;
    }
    d->state = TIM_HTTP_DOWNLOADING;
    return 0;
}

int tim_http_download_step(TimHttpDownload *d, size_t max_bytes) {
    unsigned char buffer[TIM_HTTP_MAX_CHUNK];
    int bytes_read;
    if (d == NULL || d->state != TIM_HTTP_DOWNLOADING ||
        d->request_id < 0 || d->output_file == NULL) return -1;
    if (max_bytes == 0) max_bytes = TIM_HTTP_DEFAULT_CHUNK;
    if (max_bytes > sizeof(buffer)) max_bytes = sizeof(buffer);

    bytes_read = sceHttpReadData(d->request_id, buffer, (uint32_t)max_bytes);
    if (bytes_read < 0) {
        set_error(d, "Network read failed; partial file retained as .part");
        close_output(d);
        close_request(d);
        return -1;
    }
    if (bytes_read == 0) {
        close_output(d);
        close_request(d);
        if (d->total_bytes != 0 && d->received_bytes != d->total_bytes) {
            set_error(d, "Download ended before the expected file size");
            return -1;
        }
        if (rename(d->partial_path, d->destination) != 0) {
            set_error(d, "Could not finalize downloaded file");
            return -1;
        }
        d->state = TIM_HTTP_COMPLETE;
        return 0;
    }
    if (fwrite(buffer, 1, (size_t)bytes_read, d->output_file) != (size_t)bytes_read) {
        set_error(d, "Disk write failed; check free space");
        close_output(d);
        close_request(d);
        return -1;
    }
    d->received_bytes += (uint64_t)bytes_read;
    return 1;
}

void tim_http_download_cancel(TimHttpDownload *d) {
    if (d == NULL) return;
    close_output(d);
    close_request(d);
    if (d->partial_path[0] != '\0') unlink(d->partial_path);
    d->state = TIM_HTTP_CANCELLED;
}

void tim_http_download_system_shutdown(void) {
    if (g_http_template >= 0) {
        sceHttpDeleteTemplate(g_http_template);
        g_http_template = -1;
    }
    if (g_http_context >= 0) {
        sceHttpTerm(g_http_context);
        g_http_context = -1;
    }
    if (g_ssl_ready) {
        sceSslTerm();
        g_ssl_ready = 0;
    }
    if (g_netctl_ready) {
        sceNetCtlTerm();
        g_netctl_ready = 0;
    }
    if (g_net_ready) {
        sceNetTerm();
        g_net_ready = 0;
    }
}
