#include "http_download.h"
#include "http_url.h"

#include <stdbool.h>
/* The bundled OpenOrbis SDK headers expose several imported functions as
   unprototyped void stubs. Declare the documented ABI here so return codes
   and arguments are type-checked against the actual runtime API. */
extern int32_t sceNetInit(void);
extern void sceNetTerm(void);
extern int32_t sceNetPoolCreate(const char *name, int32_t size, int32_t flags);
extern void sceNetPoolDestroy(int32_t poolId);
extern int32_t sceSslInit(size_t poolSize);
extern int32_t sceSslTerm(int32_t sslCtxId);
extern int32_t sceHttpInit(int32_t memId, int32_t sslId, size_t poolSize);
extern int32_t sceHttpTerm(int32_t httpCtxId);
extern int32_t sceHttpCreateTemplate(int32_t httpCtxId, const char *userAgent,
                                     int32_t httpVer, int32_t proxy);
extern int32_t sceHttpDeleteTemplate(int32_t templateId);
extern int32_t sceHttpCreateConnectionWithURL(int32_t templateId,
                                               const char *url, bool keepalive);
extern int32_t sceHttpDeleteConnection(int32_t connId);
extern int32_t sceHttpCreateRequestWithURL(int32_t connId, int32_t method,
                                           const char *url, uint64_t contentLength);
extern int32_t sceHttpDeleteRequest(int32_t reqId);
extern int32_t sceHttpSendRequest(int32_t reqId, const void *postData, size_t size);
extern int32_t sceHttpGetStatusCode(int32_t reqId, int32_t *statusCode);
extern int32_t sceHttpGetResponseContentLength(int32_t reqId, int32_t *result,
                                                size_t *contentLength);
extern int32_t sceHttpReadData(int32_t reqId, void *data, uint32_t size);
extern int32_t sceHttpSetAutoRedirect(int32_t id, int32_t isEnable);
extern int32_t sceHttpSetConnectTimeOut(int32_t id, uint32_t usec);
extern int32_t sceHttpSetResolveTimeOut(int32_t id, uint32_t usec);
extern int32_t sceHttpSetSendTimeOut(int32_t id, uint32_t usec);
extern int32_t sceHttpSetRecvTimeOut(int32_t id, uint32_t usec);



#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

/* OpenOrbis currently ships a NetCtl.h that includes a missing sys/stdint.h.
   The exported ABI is simple, so declare these two functions locally. */
extern int32_t sceNetCtlInit(void);
extern void sceNetCtlTerm(void);

/* Stable OpenOrbis enum values; the container SDK omits _types/http.h. */
#define TIM_ORBIS_METHOD_GET 0
#define TIM_ORBIS_HTTP_VERSION_1_1 2
#define TIM_ORBIS_HTTP_CONTENTLEN_EXIST 0
#define TIM_ORBIS_HTTP_CONTENTLEN_NOT_FOUND 1

#define TIM_HTTP_POOL_SIZE (2U * 1024U * 1024U)
#define TIM_HTTP_DEFAULT_CHUNK (32U * 1024U)
#define TIM_HTTP_MAX_CHUNK (64U * 1024U)
/* All sceHttp timeout values are expressed in microseconds. */
#define TIM_HTTP_RESOLVE_TIMEOUT_USEC (15U * 1000U * 1000U)
#define TIM_HTTP_CONNECT_TIMEOUT_USEC (20U * 1000U * 1000U)
#define TIM_HTTP_SEND_TIMEOUT_USEC (30U * 1000U * 1000U)
#define TIM_HTTP_RECV_TIMEOUT_USEC (30U * 1000U * 1000U)

static int g_net_ready = 0;
static int g_netctl_ready = 0;
static int g_net_pool_id = -1;
static int g_ssl_context_id = -1;
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

static int close_output(TimHttpDownload *d) {
    int rc = 0;
    if (d != NULL && d->output_file != NULL) {
        FILE *file = d->output_file;
        d->output_file = NULL;
        /* fclose also flushes buffered writes. Its result must be checked
           before atomically publishing the final filename. */
        if (fclose(file) != 0) rc = -1;
    }
    return rc;
}

int tim_http_download_system_init(void) {
    int rc;
    if (g_http_template >= 0) return 0;

    /* Match the PS4 initialization order and use IDs returned by each
       subsystem. Hard-coding 1 for the HTTP net/SSL IDs is not reliable. */
    rc = sceNetCtlInit();
    if (rc < 0) goto fail;
    g_netctl_ready = 1;

    rc = sceNetInit();
    if (rc < 0) goto fail;
    g_net_ready = 1;

    g_net_pool_id = sceNetPoolCreate("TIM_HTTP_NET_POOL",
                                     (int32_t)TIM_HTTP_POOL_SIZE, 0);
    if (g_net_pool_id < 0) goto fail;

    g_ssl_context_id = sceSslInit(TIM_HTTP_POOL_SIZE);
    if (g_ssl_context_id < 0) {
        g_ssl_context_id = -1;
        goto fail;
    }

    g_http_context = sceHttpInit(g_net_pool_id, g_ssl_context_id,
                                 TIM_HTTP_POOL_SIZE);
    if (g_http_context < 0) goto fail;

    g_http_template = sceHttpCreateTemplate(g_http_context,
        "TIM-PS4-Downloader/0.2", TIM_ORBIS_HTTP_VERSION_1_1, 0);
    if (g_http_template < 0) goto fail;

    /* Many ordinary download hosts redirect from short links/CDNs. Enable
       the SDK redirect handler and bound each network phase so a dead host
       does not leave the user staring at a frozen screen indefinitely. */
    (void)sceHttpSetAutoRedirect(g_http_template, 1);
    (void)sceHttpSetResolveTimeOut(g_http_template, TIM_HTTP_RESOLVE_TIMEOUT_USEC);
    (void)sceHttpSetConnectTimeOut(g_http_template, TIM_HTTP_CONNECT_TIMEOUT_USEC);
    (void)sceHttpSetSendTimeOut(g_http_template, TIM_HTTP_SEND_TIMEOUT_USEC);
    (void)sceHttpSetRecvTimeOut(g_http_template, TIM_HTTP_RECV_TIMEOUT_USEC);
    return 0;

fail:
    tim_http_download_system_shutdown();
    return -1;
}

int tim_http_download_start(TimHttpDownload *d, const char *url,
                            const char *destination) {
    int rc, length_result = TIM_ORBIS_HTTP_CONTENTLEN_NOT_FOUND;
    size_t content_length = 0;
    int status = 0;
    if (d == NULL) return -1;
    memset(d, 0, sizeof(*d));
    d->connection_id = -1;
    d->request_id = -1;
    if (!tim_http_url_is_valid(url)) {
        set_error(d, "Invalid URL; enter an absolute HTTP(S) URL");
        return -1;
    }
    if (destination == NULL || destination[0] != '/' ||
        strlen(destination) >= sizeof(d->destination)) {
        set_error(d, "Invalid destination path");
        return -1;
    }
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
    (void)sceHttpSetConnectTimeOut(d->connection_id, TIM_HTTP_CONNECT_TIMEOUT_USEC);
    d->request_id = sceHttpCreateRequestWithURL(d->connection_id,
        TIM_ORBIS_METHOD_GET, d->url, 0);
    if (d->request_id < 0) {
        set_error(d, "Could not create HTTP request");
        close_request(d);
        return -1;
    }
    (void)sceHttpSetSendTimeOut(d->request_id, TIM_HTTP_SEND_TIMEOUT_USEC);
    (void)sceHttpSetRecvTimeOut(d->request_id, TIM_HTTP_RECV_TIMEOUT_USEC);
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
    if (rc >= 0 && length_result == TIM_ORBIS_HTTP_CONTENTLEN_EXIST)
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
        if (close_output(d) != 0) {
            close_request(d);
            set_error(d, "Disk flush failed; partial file retained as .part");
            return -1;
        }
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
    if (g_ssl_context_id >= 0) {
        sceSslTerm(g_ssl_context_id);
        g_ssl_context_id = -1;
    }
    if (g_net_pool_id >= 0) {
        sceNetPoolDestroy(g_net_pool_id);
        g_net_pool_id = -1;
    }
    if (g_net_ready) {
        sceNetTerm();
        g_net_ready = 0;
    }
    if (g_netctl_ready) {
        sceNetCtlTerm();
        g_netctl_ready = 0;
    }
}
