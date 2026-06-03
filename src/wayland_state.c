#include "wayland_state.h"
#include "sys/types.h"
#include "wayland_dispatch.h"
#include "wl_shm_pool.h"
#include "z-log.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

static void create_xdg_surface(WaylandState *ws);

WaylandState *wayland_state_new(int width, int height) {
    WaylandState *ws = calloc(1, sizeof(WaylandState));
    if (!ws) return NULL;

    struct wl_display *display = wl_display_connect(NULL);
    if (!display) {
        ERROR("Failed to create wayland display");
        free(ws);
        return NULL;
    }
    ws->display = display;

    struct wl_registry *registry = wl_display_get_registry(display);
    if (!registry) {
        ERROR("Failed to get wayland registry");
        wl_display_disconnect(display);
        free(ws);
        return NULL;
    }
    wl_registry_add_listener(registry, &wl_registry_listener, ws);
    wl_display_roundtrip(display);
    wl_display_roundtrip(display);

    ws->keyboard = wl_seat_get_keyboard(ws->seat);
    wl_keyboard_add_listener(ws->keyboard, &wl_keyboard_listener, ws);

    create_xdg_surface(ws);
    wl_display_roundtrip(display);
    wl_display_roundtrip(display);

    if (width && height && ws->output_count > 0) {
        ws->output_infos[0]->width = width;
        ws->output_infos[0]->height = height;
    }
    return ws;
}

void wayland_state_free(WaylandState *ws) {
    if (!ws) return;
    for (int i = 0; i < ws->output_count; i++) {
        OutputInfo *info = ws->output_infos[i];
        if (!info) continue;
        if (info->frame_callback) {
            wl_callback_destroy(info->frame_callback);
        }
        if (info->shm_pool) {
            shm_pool_destroy(info->shm_pool);
        }
    }
    if (ws->display)
        wl_display_disconnect(ws->display);
    free(ws);
}

static void create_xdg_surface(WaylandState *ws) {
    if (ws->output_count == 0) return;
    OutputInfo* info = ws->output_infos[0];
    info->surface = wl_compositor_create_surface(ws->compositor);
    info->xdg_surface = xdg_wm_base_get_xdg_surface(ws->xdg_wm_base, info->surface);
    info->toplevel = xdg_surface_get_toplevel(info->xdg_surface);
    xdg_surface_add_listener(info->xdg_surface, &xdg_surface_listener, ws);
    xdg_toplevel_add_listener(info->toplevel, &xdg_toplevel_listener, ws);

    wl_surface_commit(info->surface);
}

void output_resize(WaylandState *ws, OutputInfo *output) {
    if (output->shm_pool) {
        shm_pool_destroy(output->shm_pool);
        output->shm_pool = NULL;
    }
    wl_shm_pool_init(ws->shm, output, 3);
}

static void request_frame(OutputInfo *info) {
    if (info->frame_callback) {
        wl_callback_destroy(info->frame_callback);
    }
    info->frame_callback = wl_surface_frame(info->surface);
    wl_callback_add_listener(info->frame_callback, &wl_callback_listener, info);
}

static void render_output(WaylandState *ws, OutputInfo *info) {
    ShmBuffer *buf = get_shm_buffer(info->shm_pool);
    if (!buf) return;

    uint32_t *pixels = buf->data;
    if (ws->render) ws->render(ws->render_data, pixels, info->width, info->height);

    wl_surface_damage(info->surface, 0, 0, info->width, info->height);
    wl_surface_attach(info->surface, buf->buffer, 0, 0);
    mark_busy(info->shm_pool, buf->buffer);

    request_frame(info);
    wl_surface_commit(info->surface);
}

void loop_run(WaylandState *ws) {
    for (int i = 0; i < ws->output_count; i++) {
        OutputInfo *info = ws->output_infos[i];
        if (!info->surface) continue;
        info->frame_callback = NULL;
        if (info->shm_pool) {
            render_output(ws, info);
        }
    }
    wl_display_flush(ws->display);

    int frame_count = 0;
    struct timespec last = {0};

    while (ws->running && *ws->running) {
        wl_display_dispatch(ws->display);

        for (int i = 0; i < ws->output_count; i++) {
            OutputInfo *info = ws->output_infos[i];
            if (!info->surface) continue;
            if (info->frame_callback == NULL && info->shm_pool) {
                render_output(ws, info);
                frame_count++;
            }
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (last.tv_sec == 0) {
            last = now;
        }
        double elapsed = (now.tv_sec - last.tv_sec) + (now.tv_nsec - last.tv_nsec) / 1e9;
        if (elapsed >= 1.0) {
            INFO("FPS: %d", (int)(frame_count / elapsed));
            frame_count = 0;
            last = now;
        }
    }
}
