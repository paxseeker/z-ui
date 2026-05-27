#ifndef Z_UI_H
#define Z_UI_H
#include <stdint.h>

typedef struct {
    int width, height;
    int running;
} Z_UI_Context;


void z_ui_init();
void z_ui_update(uint32_t *pixels, int width, int height);
void z_ui_handle_events();
void z_ui_destory();
void z_ui_set_size(int width, int height);
void z_ui_exit();


#endif
