#ifndef Z_UI_H
#define Z_UI_H
#include <stdint.h>

#include "event.h"

typedef struct { int x, y; float z; } Point;
typedef struct { float x, y, z; } vec3;

typedef struct Z_UI_Context Z_UI_Context;

struct Z_UI_Context {
    int running;
    int width;
    int height;
    void *backend_data;
    void (*render)(Z_UI_Context *ctx, uint32_t *pixels, int width, int height);
    void (*destroy)(Z_UI_Context *ctx);
    int wireframe;
    Event event_queue[EVENT_QUEUE_SIZE];
    int event_head;
    int event_tail;
};

void z_ui_init(Z_UI_Context *ctx);
void z_ui_update(Z_UI_Context *ctx, uint32_t *pixels, int width, int height);
void z_ui_event(Z_UI_Context *ctx);
void z_ui_destory(Z_UI_Context *ctx);
void z_ui_set_size(Z_UI_Context *ctx, int width, int height);
void z_ui_exit(Z_UI_Context *ctx);
void z_ui_push_event(Z_UI_Context *ctx, Event *event);

#endif
