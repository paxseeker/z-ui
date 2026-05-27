#include "wayland_dispatch.h"

#include "wayland_state.h"
#include "z-log.h"
//#include <libavutil/time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

extern WaylandState *g_state;

void frame_done(void *data, struct wl_callback *wl_callback, uint32_t time) {
    (void)wl_callback;
    (void)time;
    if (data) {
        OutputInfo *output = (OutputInfo *)data;
        if (output->frame_callback) {
            wl_callback_destroy(output->frame_callback);
            output->frame_callback = NULL;
        }
    }
}

struct wl_callback_listener wl_callback_listener = {
    .done = frame_done,
};
// ================ xdg_toplevel_listener ===================
void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel,
                            int32_t width, int32_t height,
                            struct wl_array *states) {
    for (int i = 0; i < g_state->output_count; i++) {
        OutputInfo *output = g_state->output_infos[i];
        if (output->toplevel == xdg_toplevel) {
            output->pending_width = width;
            output->pending_height = height;
            return;
        }
    }
}
void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    g_state->running = 0;
}
void xdg_toplevel_configure_bounds(void *data,
                                   struct xdg_toplevel *xdg_toplevel,
                                   int32_t width, int32_t height) {}
void xdg_toplevel_wm_capabilities(void *data, struct xdg_toplevel *xdg_toplevel,
                                  struct wl_array *capabilities) {}

struct xdg_toplevel_listener xdg_toplevel_listener = {
    .close = xdg_toplevel_close,
    .configure = xdg_toplevel_configure,
    .configure_bounds = xdg_toplevel_configure_bounds,
    .wm_capabilities = xdg_toplevel_wm_capabilities,
};

// ================ wl_keyboard_listener ===================

void keymap(void *data, struct wl_keyboard *wl_keyboard, uint32_t format,
            int32_t fd, uint32_t size) {
    INFO("keymap format:%d size:%d", format, size);
}

void enter(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial,
           struct wl_surface *surface, struct wl_array *keys) {
    INFO("enter");
}

void leave(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial,
           struct wl_surface *surface) {
    INFO("leave");
}

void key(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial,
         uint32_t time, uint32_t key, uint32_t state) {
    INFO("key %d state %d", key, state);
    switch (key) {
    case 1:
        g_state->running = 0;
        break;
    }
}

void modifiers(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial,
               uint32_t mods_depressed, uint32_t mods_latched,
               uint32_t mods_locked, uint32_t group) {
    INFO("modifiers %d %d %d %d", mods_depressed, mods_latched, mods_locked,
         group);
}

void repeat_info(void *data, struct wl_keyboard *wl_keyboard, int32_t rate,
                 int32_t delay) {}

struct wl_keyboard_listener wl_keyboard_listener = {
    .enter = enter,
    .key = key,
    .keymap = keymap,
    .leave = leave,
    .modifiers = modifiers,
    .repeat_info = repeat_info,
};

//

// ================ xdg_surface_listener ===================

void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                           uint32_t serial) {
    for (int i = 0; i < g_state->output_count; i++) {
        OutputInfo *output = g_state->output_infos[i];
        if (output->xdg_surface == xdg_surface) {
            xdg_surface_ack_configure(xdg_surface, serial);
            if (output->pending_width && output->pending_height) {
                if (output->width != output->pending_width || output->height != output->pending_height) {
                    output->width = output->pending_width;
                    output->height = output->pending_height;
                    output_resize(output);
                } else {
                    DEBUG("configure with same size, skip resize");
                }
                output->pending_width = 0;
                output->pending_height = 0;
            }
            output->is_configured = 1;
            DEBUG("output %s is configured", output->name);
        }
    }
}

struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

// ================ wl_register_listener ===================

void global(void *data, struct wl_registry *wl_registry, uint32_t name,
            const char *interface, uint32_t version) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        g_state->compositor = wl_registry_bind(
            wl_registry, name, &wl_compositor_interface, version);
        INFO("wl_compositor_interface binded");
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        g_state->shm =
            wl_registry_bind(wl_registry, name, &wl_shm_interface, version);
        INFO("wl_shm_interface binded");
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        OutputInfo *output_info = (OutputInfo *)calloc(1, sizeof(OutputInfo));
        output_info->output =
            wl_registry_bind(wl_registry, name, &wl_output_interface, version);
        wl_output_add_listener(output_info->output, &wl_output_listener,
                               g_state);
        g_state->output_infos =
            realloc(g_state->output_infos,
                    sizeof(struct output_info *) * (g_state->output_count + 1));
        g_state->output_count++;
        g_state->output_infos[g_state->output_count - 1] = output_info;
        INFO("find output, current_output_count: %d", g_state->output_count);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        g_state->xdg_wm_base = wl_registry_bind(
            wl_registry, name, &xdg_wm_base_interface, version);
        INFO("xdg_wm_base_interface binded");
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        g_state->seat =
            wl_registry_bind(wl_registry, name, &wl_seat_interface, version);
        INFO("wl_seat_interface binded");
    }
}

void global_remove(void *data, struct wl_registry *wl_registry, uint32_t name) {
}

struct wl_registry_listener wl_registry_listener = {
    .global = global, .global_remove = global_remove};

// ================ wl_output_listener ==================================

void geometry(void *data, struct wl_output *wl_output, int32_t x, int32_t y,
              int32_t physical_width, int32_t physical_height, int32_t subpixel,
              const char *make, const char *model, int32_t transform) {
    for (int i = 0; i < g_state->output_count; i++) {
        if (g_state->output_infos[i]->output == wl_output) {
        }
    }
}

void mode(void *data, struct wl_output *wl_output, uint32_t flags,
          int32_t width, int32_t height, int32_t refresh) {
    for (int i = 0; i < g_state->output_count; i++) {
        if (g_state->output_infos[i]->output == wl_output) {
            g_state->output_infos[i]->width = width;
            g_state->output_infos[i]->height = height;
            g_state->output_infos[i]->refresh_rate = refresh;
            INFO("wl_output mode event: %dx%d", width, height);
            return;
        }
    }
}
void done(void *data, struct wl_output *wl_output) {}
void scale(void *data, struct wl_output *wl_output, int32_t factor) {
    for (int i = 0; i < g_state->output_count; i++) {
        if (g_state->output_infos[i]->output == wl_output) {
            g_state->output_infos[i]->scale = factor;
            return;
        }
    }
}
void name(void *data, struct wl_output *wl_output, const char *name) {
    for (int i = 0; i < g_state->output_count; i++) {
        if (g_state->output_infos[i]->output == wl_output) {
            g_state->output_infos[i]->name = strdup(name);
            INFO("wl_output name event: %s", name);
            return;
        }
    }
}
void description(void *data, struct wl_output *wl_output,
                 const char *description) {
    for (int i = 0; i < g_state->output_count; i++) {
        if (g_state->output_infos[i]->output == wl_output) {
            g_state->output_infos[i]->description = strdup(description);
            INFO("wl_output description event: %s", description);
        }
    }
}
struct wl_output_listener wl_output_listener = {
    .done = done,
    .description = description,
    .geometry = geometry,
    .name = name,
    .scale = scale,
    .mode = mode,
};
