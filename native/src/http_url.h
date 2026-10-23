#ifndef TIM_HTTP_URL_H
#define TIM_HTTP_URL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Accept only absolute HTTP(S) URLs with a non-empty host and no control
 * characters. This is validation, not a guarantee that the server is trusted. */
int tim_http_url_is_valid(const char *url);

#ifdef __cplusplus
}
#endif
#endif
