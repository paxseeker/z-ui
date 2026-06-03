#ifndef BACKEND_H
#define BACKEND_H

typedef struct Z_UI_Context Z_UI_Context;

typedef struct Backend {
    const char *name;
    int  (*init)(Z_UI_Context *ctx);
    void (*run)(Z_UI_Context *ctx);
    void (*cleanup)(Z_UI_Context *ctx);
} Backend;

extern const Backend wayland_backend;

#endif
