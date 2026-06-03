#include "z-ui.h"

#define Z_LOG_IMPLEMENTATION
#include "z-log.h"
#include "backend.h"
#include <stdlib.h>
#include <string.h>

void z_ui_push_event(Z_UI_Context *ctx, Event *event) {
    int next = (ctx->event_head + 1) % EVENT_QUEUE_SIZE;
    if (next == ctx->event_tail) return;
    ctx->event_queue[ctx->event_head] = *event;
    ctx->event_head = next;
}

int main(int argc, char **argv) {
    Z_LOG_INIT();
    Z_UI_Context *ctx = malloc(sizeof(Z_UI_Context));
    memset(ctx, 0, sizeof(Z_UI_Context));
    ctx->running = 1;
    ctx->wireframe = 1;

    ctx->render = z_ui_update;
    ctx->destroy = z_ui_destory;
    z_ui_init(ctx);

    const Backend *backend = &wayland_backend;
    if (backend->init(ctx) != 0) {
        ERROR("backend init failed");
        return 1;
    }
    backend->run(ctx);
    backend->cleanup(ctx);

    if (ctx->destroy) ctx->destroy(ctx);
    free(ctx);
    Z_LOG_EXIT();
}

void z_ui_set_size(Z_UI_Context *ctx, int width, int height) {
    ctx->width = width;
    ctx->height = height;
}

void z_ui_exit(Z_UI_Context *ctx) {
    ctx->running = 0;
}
