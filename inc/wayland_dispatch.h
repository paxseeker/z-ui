#ifndef WAYLAND_DISPATCH_H
#define WAYLAND_DISPATCH_H

#include <wayland-client-core.h>

extern struct wl_registry_listener wl_registry_listener;
extern struct wl_output_listener wl_output_listener;
extern struct xdg_surface_listener xdg_surface_listener;
extern struct xdg_toplevel_listener xdg_toplevel_listener;
extern struct wl_keyboard_listener wl_keyboard_listener;
extern struct wl_callback_listener wl_callback_listener;

#endif
