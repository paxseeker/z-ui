#include "z-log.h"
#include "z-ui.h"
#include <stdint.h>

void z_ui_init() {
    z_ui_set_size(800, 600);
}

void z_ui_update(uint32_t *pixels, int width, int height) {
    for (int i = 0; i < 200 && i < height; i++) {
        for (int j = 0; j < 200 && j < width; j++) {
            pixels[i * width + j] = 0xff00ffff;   
        }
    }
}

void z_ui_handle_events() {

}

void z_ui_destory() {

}
