#include "wayland_state.h"
#include "sys/types.h"
#include "wayland_dispatch.h"
#include "wl_shm_pool.h"
#include "z-log.h"
#include "z-ui.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <time.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

WaylandState *g_state = NULL;

void creat_xdg_surface();


int wayland_state_new(int width, int height) {
    g_state = malloc(sizeof(WaylandState));
    g_state->running = 1;
    struct wl_display *display = wl_display_connect(NULL);
    if (!display) {
        ERROR("Failed to create wayland display");
        return -1;
    }
    g_state->display = display;

    struct wl_registry *registry = wl_display_get_registry(display);
    if (!registry) {
        ERROR("Failed to get wayland registry");
        return -1;
    }
    wl_registry_add_listener(registry, &wl_registry_listener, g_state);
    wl_display_roundtrip(display);
    wl_display_roundtrip(display);
 
    g_state->keyboard = wl_seat_get_keyboard(g_state->seat);
    wl_keyboard_add_listener(g_state->keyboard, &wl_keyboard_listener, NULL);

    creat_xdg_surface();
    wl_display_roundtrip(display);
    wl_display_roundtrip(display);

    if (width && height) {
        for (int i = 0; i < g_state->output_count; i++) {
            OutputInfo* info = g_state->output_infos[i];
            info->width = width;
            info->height = height;
        }
    }
    return 0;
}

void wayland_state_free(WaylandState *state) {
    if (!state) return;
    for (int i = 0; i < state->output_count; i++) {
        if (state->output_infos[i]->frame_callback) {
            wl_callback_destroy(state->output_infos[i]->frame_callback);
            state->output_infos[i]->frame_callback = NULL;
        }
    }
    if (state->display)
        wl_display_disconnect(state->display);
}

void creat_xdg_surface() {
    for (int i = 0; i < g_state->output_count; i++) {
        OutputInfo* info = g_state->output_infos[i];
        info->surface = wl_compositor_create_surface(g_state->compositor);
        info->xdg_surface = xdg_wm_base_get_xdg_surface(g_state->xdg_wm_base, info->surface);
        info->toplevel = xdg_surface_get_toplevel(info->xdg_surface);
        xdg_surface_add_listener(info->xdg_surface, &xdg_surface_listener, NULL);
        xdg_toplevel_add_listener(info->toplevel, &xdg_toplevel_listener, NULL);

        wl_surface_commit(info->surface);
    }
}

void output_resize(OutputInfo *output) {
    if (output->shm_pool) {
        shm_pool_destroy(output->shm_pool);
        output->shm_pool = NULL;
    }
    wl_shm_pool_init(g_state, output, 3);
}

#define TARGET_FRAME_NS 16666667L

void loop_run(void) {
    struct timespec last = {0};
    int frame_count = 0;
    int epfd = epoll_create1(0);
    struct epoll_event ev = {
        .events = EPOLLIN,
        .data.fd = wl_display_get_fd(g_state->display),
    };
    epoll_ctl(epfd, EPOLL_CTL_ADD, ev.data.fd, &ev);

    while (g_state->running) {
        struct timespec frame_start;
        clock_gettime(CLOCK_MONOTONIC, &frame_start);

        for (int i = 0; i < g_state->output_count; i++) {
            OutputInfo *info = g_state->output_infos[i];
            ShmBuffer *buf = get_shm_buffer(info->shm_pool);
            if (!buf) continue;
            uint32_t *pixels = buf->data;
            z_ui_update(pixels, info->width, info->height);
            wl_surface_damage(info->surface, 0, 0, info->width, info->height);
            wl_surface_attach(info->surface, buf->buffer, 0, 0);
            mark_busy(info->shm_pool, buf->buffer);
            wl_surface_commit(info->surface);
        }

        frame_count++;
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

        wl_display_flush(g_state->display);
        struct epoll_event events[1];
        if (epoll_wait(epfd, events, 1, 0) > 0) {
            wl_display_dispatch(g_state->display);
        }

        clock_gettime(CLOCK_MONOTONIC, &now);
        long frame_ns = (now.tv_sec - frame_start.tv_sec) * 1000000000L + (now.tv_nsec - frame_start.tv_nsec);
        if (frame_ns < TARGET_FRAME_NS) {
            struct timespec rem = {0, TARGET_FRAME_NS - frame_ns};
            clock_nanosleep(CLOCK_MONOTONIC, 0, &rem, NULL);
        }
    }
    close(epfd);
    wayland_state_free(g_state);
}

int wayland_init(int width, int height) {
    return wayland_state_new(width, height);
}


