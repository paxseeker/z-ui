#include "backend.h"
#include "wayland_state.h"
#include "z-ui.h"

static void render_cb(void *userdata, uint32_t *pixels, int width, int height) {
    Z_UI_Context *ctx = userdata;
    if (ctx->render) ctx->render(ctx, pixels, width, height);
}

static void event_cb(void *userdata, Event *ev) {
    Z_UI_Context *ctx = userdata;
    z_ui_push_event(ctx, ev);
    z_ui_event(ctx);
}

static int init(Z_UI_Context *ctx) {
    WaylandState *ws = wayland_state_new(ctx->width, ctx->height);
    if (!ws) return -1;
    ctx->backend_data = ws;
    return 0;
}

static void run(Z_UI_Context *ctx) {
    WaylandState *ws = ctx->backend_data;
    ws->running = &ctx->running;
    ws->render = render_cb;
    ws->render_data = ctx;
    ws->on_event = event_cb;
    ws->event_data = ctx;
    loop_run(ws);
}

static void cleanup(Z_UI_Context *ctx) {
    wayland_state_free(ctx->backend_data);
    ctx->backend_data = NULL;
}

const Backend wayland_backend = {
    .name = "wayland",
    .init = init,
    .run = run,
    .cleanup = cleanup,
};
