#include "torrent_engine.h"

#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

/*
 * Real libtorrent-rasterbar adapter for TIM.
 *
 * The build must provide the PS4-patched libtorrent 2.0.15 package and compile
 * this file with the same ps4_compat.h / Boost.Asio defines used by that package.
 * Progress is copied from libtorrent status; this adapter never fabricates it.
 */
namespace {
namespace lt = libtorrent;

struct TimLibtorrentContext {
    std::unique_ptr<lt::session> session;
};

struct TimLibtorrentHandle {
    lt::torrent_handle handle;
    std::string uri;
    std::string name;
    std::string save_path;
};

static int backend_start(void *opaque, const char *uri, const char *save_path,
                         void **handle_out) {
    if (!opaque || !uri || !*uri || !save_path || !*save_path || !handle_out)
        return -1;
    auto *ctx = static_cast<TimLibtorrentContext *>(opaque);
    if (!ctx->session) return -1;

    lt::add_torrent_params params;
    params.save_path = save_path;
    lt::error_code ec;
    if (std::strncmp(uri, "magnet:?", 8) == 0) {
        params = lt::parse_magnet_uri(uri, ec);
        if (ec) return -1;
        params.save_path = save_path;
    } else {
        /* .torrent URLs are intentionally not treated as magnets. The caller
           must resolve a local .torrent file before passing it to this adapter. */
        return -1;
    }

    auto *item = new (std::nothrow) TimLibtorrentHandle;
    if (!item) return -1;
    item->uri = uri;
    item->save_path = save_path;
    item->handle = ctx->session->add_torrent(std::move(params), ec);
    if (ec || !item->handle.is_valid()) {
        delete item;
        return -1;
    }
    *handle_out = item;
    return 0;
}

static int backend_pause(void *, void *opaque) {
    auto *item = static_cast<TimLibtorrentHandle *>(opaque);
    if (!item || !item->handle.is_valid()) return -1;
    item->handle.pause();
    return 0;
}

static int backend_resume(void *, void *opaque) {
    auto *item = static_cast<TimLibtorrentHandle *>(opaque);
    if (!item || !item->handle.is_valid()) return -1;
    item->handle.resume();
    return 0;
}

static int backend_cancel(void *context, void *opaque) {
    auto *ctx = static_cast<TimLibtorrentContext *>(context);
    auto *item = static_cast<TimLibtorrentHandle *>(opaque);
    if (!ctx || !ctx->session || !item) return -1;
    if (item->handle.is_valid()) ctx->session->remove_torrent(item->handle);
    delete item;
    return 0;
}

static int backend_poll(void *, void *opaque, TimTorrentTask *out) {
    auto *item = static_cast<TimLibtorrentHandle *>(opaque);
    if (!item || !out || !item->handle.is_valid()) return -1;
    lt::torrent_status status = item->handle.status();
    std::memset(out, 0, sizeof(*out));
    std::snprintf(out->source_uri, sizeof(out->source_uri), "%s", item->uri.c_str());
    std::snprintf(out->name, sizeof(out->name), "%s",
                  status.name.empty() ? item->uri.c_str() : status.name.c_str());
    out->received_bytes = status.total_done;
    out->total_bytes = status.total_wanted;
    out->download_rate = status.download_rate;
    out->upload_rate = status.upload_rate;
    out->peers = status.num_peers > 0 ? static_cast<unsigned int>(status.num_peers) : 0;
    out->seeds = status.num_seeds > 0 ? static_cast<unsigned int>(status.num_seeds) : 0;
    if (status.errc) {
        out->state = TIM_TORRENT_ERROR;
        std::snprintf(out->error, sizeof(out->error), "%s", status.errc.message().c_str());
    } else if (status.is_seeding || status.progress >= 1.0f) {
        out->state = TIM_TORRENT_COMPLETE;
    } else if (status.paused) {
        out->state = TIM_TORRENT_PAUSED;
    } else if (!status.has_metadata) {
        out->state = TIM_TORRENT_METADATA;
    } else {
        out->state = TIM_TORRENT_DOWNLOADING;
    }
    return 0;
}
} // namespace

extern "C" void tim_libtorrent_backend_shutdown(void *opaque);

extern "C" int tim_libtorrent_backend_init(TimTorrentBackend *out) {
    if (!out) return -1;
    auto *ctx = new (std::nothrow) TimLibtorrentContext;
    if (!ctx) return -1;
    try {
        lt::settings_pack settings;
        settings.set_int(lt::settings_pack::alert_mask,
            lt::alert_category::error | lt::alert_category::status |
            lt::alert_category::storage);
        settings.set_int(lt::settings_pack::connections_limit, 80);
        settings.set_int(lt::settings_pack::unchoke_slots_limit, 8);
        settings.set_bool(lt::settings_pack::enable_dht, true);
        settings.set_bool(lt::settings_pack::enable_lsd, true);
        settings.set_bool(lt::settings_pack::enable_upnp, false);
        settings.set_bool(lt::settings_pack::enable_natpmp, false);
        ctx->session.reset(new lt::session(settings));
    } catch (...) {
        delete ctx;
        return -1;
    }
    std::memset(out, 0, sizeof(*out));
    out->context = ctx;
    out->start = backend_start;
    out->pause = backend_pause;
    out->resume = backend_resume;
    out->cancel = backend_cancel;
    out->poll = backend_poll;
    out->shutdown = tim_libtorrent_backend_shutdown;
    return 0;
}

extern "C" void tim_libtorrent_backend_shutdown(void *opaque) {
    auto *ctx = static_cast<TimLibtorrentContext *>(opaque);
    if (!ctx) return;
    ctx->session.reset();
    delete ctx;
}
