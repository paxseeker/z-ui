#ifndef WAYLAND_STATE_H
#define WAYLAND_STATE_H

#include <stdint.h>
#include <wayland-client-protocol.h>
#include <xdg-shell.h>
#include "event.h"
#include "wl_shm_pool.h"

typedef struct OutputInfo {
    struct wl_output *output;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *toplevel;
    struct wl_callback *frame_callback;
    int width;
    int height;
    int pending_width;
    int pending_height;
    int refresh_rate;
    int is_configured;
    int scale;
    char *name;
    char *description;
    ShmPool *shm_pool;
} OutputInfo;

typedef void (*render_fn)(void *userdata, uint32_t *pixels, int width, int height);
typedef void (*event_fn)(void *userdata, Event *ev);

typedef struct WaylandState {
    struct wl_display *display;
    struct wl_compositor *compositor;
    struct xdg_wm_base *xdg_wm_base;
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct wl_shm *shm;
    OutputInfo **output_infos;
    int output_count;
    int *running;
    void *render_data;
    render_fn render;
    void *event_data;
    event_fn on_event;
} WaylandState;

WaylandState *wayland_state_new(int width, int height);
void loop_run(WaylandState *ws);
void wayland_state_free(WaylandState *ws);
void output_resize(WaylandState *ws, OutputInfo *output);

#endif
