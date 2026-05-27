#ifndef WAYLAND_STATE_H
#define WAYLAND_STATE_H

#include <stdint.h>
#include <wayland-client-core.h>
#include <xdg-shell.h>
#include <wl_shm_pool.h>

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

typedef struct WaylandState {
    int running;
    struct wl_display *display;
    struct wl_compositor *compositor;
    struct xdg_wm_base *xdg_wm_base;
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct wl_shm *shm;
    OutputInfo **output_infos;
    int output_count;
} WaylandState;

extern WaylandState *g_state;

int wayland_init(int width, int height);
void loop_run(void);
void output_resize(OutputInfo *output);

#endif
