#include "z-ui.h"

#define Z_LOG_IMPLEMENTATION
#include "z-log.h"
#include "wayland_state.h"
#include <stdlib.h>

static Z_UI_Context *z_ui_ctx;

int main(int argc, char **argv) {
    Z_LOG_INIT();
    z_ui_ctx = malloc(sizeof(Z_UI_Context));
    z_ui_init();
    wayland_init(z_ui_ctx->width, z_ui_ctx->height);
    loop_run();
    z_ui_destory();
    Z_LOG_EXIT();
}

void z_ui_set_size(int width, int height) {
    z_ui_ctx->width = width;
    z_ui_ctx->height = height;
}

void z_ui_exit() {
    z_ui_ctx->running = 0;
}

