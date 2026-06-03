#include "z-log.h"
#include "z-ui.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void draw_pixel(uint32_t *pixels, int width, int height, int x, int y, uint32_t color) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    pixels[y * width + x] = color;
}

static void draw_line(uint32_t *pixels, int width, int height, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        draw_pixel(pixels, width, height, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void draw_triangle_filled(uint32_t *pixels, float *zbuf, int w, int h,
                                  Point p0, Point p1, Point p2, uint32_t color) {
    Point *p[3] = {&p0, &p1, &p2};
    for (int i = 0; i < 2; i++) {
        for (int j = i + 1; j < 3; j++) {
            if (p[i]->y > p[j]->y) {
                Point *tmp = p[i]; p[i] = p[j]; p[j] = tmp;
            }
        }
    }
    if (p[0]->y >= h || p[2]->y < 0) return;
    int y0 = MAX(0, p[0]->y);
    int y2 = MIN(h - 1, p[2]->y);
    if (y0 > y2) return;

    float dy_long = p[2]->y - p[0]->y;
    float dx_long = (p[2]->x - p[0]->x) / dy_long;
    float dz_long = (p[2]->z - p[0]->z) / dy_long;

    if (p[1]->y > y0) {
        float dy_top = p[1]->y - p[0]->y;
        float dx_top = (p[1]->x - p[0]->x) / dy_top;
        float dz_top = (p[1]->z - p[0]->z) / dy_top;
        for (int y = y0; y < p[1]->y && y <= y2; y++) {
            float xa = p[0]->x + dx_long * (y - p[0]->y);
            float za = p[0]->z + dz_long * (y - p[0]->y);
            float xb = p[0]->x + dx_top * (y - p[0]->y);
            float zb = p[0]->z + dz_top * (y - p[0]->y);
            if (xb - xa == 0) continue;

            float x_left = xa < xb ? xa : xb;
            float x_right = xa < xb ? xb : xa;
            int xl = MAX(0, (int)x_left);
            int xr = MIN(w - 1, (int)x_right);
            if (xl > xr) continue;

            float zl = xa < xb ? za : zb;
            float dz = (zb - za) / (xb - xa);
            for (int x = xl; x <= xr; x++) {
                float zval = zl + dz * (x - x_left);
                int idx = y * w + x;
                if (zval < zbuf[idx]) {
                    zbuf[idx] = zval;
                    pixels[idx] = color;
                }
            }
        }
    }

    if (p[2]->y > p[1]->y) {
        float dy_bot = p[2]->y - p[1]->y;
        float dx_bot = (p[2]->x - p[1]->x) / dy_bot;
        float dz_bot = (p[2]->z - p[1]->z) / dy_bot;
        for (int y = MAX(p[1]->y, y0); y <= y2; y++) {
            float xa = p[0]->x + dx_long * (y - p[0]->y);
            float za = p[0]->z + dz_long * (y - p[0]->y);
            float xb = p[1]->x + dx_bot * (y - p[1]->y);
            float zb = p[1]->z + dz_bot * (y - p[1]->y);
            if (xb - xa == 0) continue;

            float x_left = xa < xb ? xa : xb;
            float x_right = xa < xb ? xb : xa;
            int xl = MAX(0, (int)x_left);
            int xr = MIN(w - 1, (int)x_right);
            if (xl > xr) continue;

            float zl = xa < xb ? za : zb;
            float dz = (zb - za) / (xb - xa);
            for (int x = xl; x <= xr; x++) {
                float zval = zl + dz * (x - x_left);
                int idx = y * w + x;
                if (zval < zbuf[idx]) {
                    zbuf[idx] = zval;
                    pixels[idx] = color;
                }
            }
        }
    }
}

static void vec3_rotate_x(vec3 *v, float a) {
    float c = cosf(a), s = sinf(a);
    float y = v->y * c - v->z * s;
    float z = v->y * s + v->z * c;
    v->y = y; v->z = z;
}

static void vec3_rotate_y(vec3 *v, float a) {
    float c = cosf(a), s = sinf(a);
    float x = v->x * c + v->z * s;
    float z = -v->x * s + v->z * c;
    v->x = x; v->z = z;
}

void z_ui_init(Z_UI_Context *ctx) {
    z_ui_set_size(ctx, 800, 600);
}

static void z_ui_handle_event(Z_UI_Context *ctx, Event *ev) {
    switch (ev->type) {
    case EVENT_KEY:
        switch (ev->key.key) {
        case 1:  ctx->running = 0; break;
        case 17: if (ev->key.state) ctx->wireframe = !ctx->wireframe; break;
        }
        break;
    case EVENT_MOUSE_BUTTON:
    case EVENT_MOUSE_MOVE:
        break;
    }
}

void z_ui_event(Z_UI_Context *ctx) {
    while (ctx->event_tail != ctx->event_head) {
        Event ev = ctx->event_queue[ctx->event_tail];
        ctx->event_tail = (ctx->event_tail + 1) % EVENT_QUEUE_SIZE;
        z_ui_handle_event(ctx, &ev);
    }
}

void z_ui_update(Z_UI_Context *ctx, uint32_t *pixels, int width, int height) {
    z_ui_event(ctx);
    memset(pixels, 0, width * height * 4);

    static float *zbuf = NULL;
    static int zbuf_w = 0, zbuf_h = 0;
    if (zbuf_w != width || zbuf_h != height) {
        free(zbuf);
        zbuf = malloc(width * height * sizeof(float));
        zbuf_w = width;
        zbuf_h = height;
    }
    for (int i = 0; i < width * height; i++) zbuf[i] = INFINITY;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    float t = ts.tv_sec + ts.tv_nsec * 1e-9f;

    vec3 verts[42] = {
        {0.000000, 1.000000, 0.000000},
        {0.6000, 0.6000, 0.0000}, {0.4243, 0.6000, 0.4243},
        {0.0000, 0.6000, 0.6000}, {-0.4243, 0.6000, 0.4243},
        {-0.6000, 0.6000, 0.0000}, {-0.4243, 0.6000, -0.4243},
        {0.0000, 0.6000, -0.6000}, {0.4243, 0.6000, -0.4243},
        {0.8000, 0.2000, 0.0000}, {0.5657, 0.2000, 0.5657},
        {0.0000, 0.2000, 0.8000}, {-0.5657, 0.2000, 0.5657},
        {-0.8000, 0.2000, 0.0000}, {-0.5657, 0.2000, -0.5657},
        {0.0000, 0.2000, -0.8000}, {0.5657, 0.2000, -0.5657},
        {0.8000, -0.2000, 0.0000}, {0.5657, -0.2000, 0.5657},
        {0.0000, -0.2000, 0.8000}, {-0.5657, -0.2000, 0.5657},
        {-0.8000, -0.2000, 0.0000}, {-0.5657, -0.2000, -0.5657},
        {0.0000, -0.2000, -0.8000}, {0.5657, -0.2000, -0.5657},
        {0.6000, -0.6000, 0.0000}, {0.4243, -0.6000, 0.4243},
        {0.0000, -0.6000, 0.6000}, {-0.4243, -0.6000, 0.4243},
        {-0.6000, -0.6000, 0.0000}, {-0.4243, -0.6000, -0.4243},
        {0.0000, -0.6000, -0.6000}, {0.4243, -0.6000, -0.4243},
        {0.000000, -1.000000, 0.000000}
    };

    int faces[64][3] = {
        {0,1,2},{0,2,3},{0,3,4},{0,4,5},{0,5,6},{0,6,7},{0,7,8},{0,8,1},
        {1,2,10},{1,10,9},{2,3,11},{2,11,10},{3,4,12},{3,12,11},{4,5,13},{4,13,12},
        {5,6,14},{5,14,13},{6,7,15},{6,15,14},{7,8,16},{7,16,15},{8,1,9},{8,9,16},
        {9,10,18},{9,18,17},{10,11,19},{10,19,18},{11,12,20},{11,20,19},{12,13,21},{12,21,20},
        {13,14,22},{13,22,21},{14,15,23},{14,23,22},{15,16,24},{15,24,23},{16,9,17},{16,17,24},
        {17,18,26},{17,26,25},{18,19,27},{18,27,26},{19,20,28},{19,28,27},{20,21,29},{20,29,28},
        {21,22,30},{21,30,29},{22,23,31},{22,31,30},{23,24,32},{23,32,31},{24,17,25},{24,25,32},
        {33,25,26},{33,26,27},{33,27,28},{33,28,29},{33,29,30},{33,30,31},{33,31,32},{33,32,25}
    };

    int cx = width / 2, cy = height / 2;
    float scale = MIN(width, height) * 0.3f;
    float fov = scale, dist = 3.5f;

    Point proj[42];
    for (int i = 0; i < 42; i++) {
        vec3 v = verts[i];
        vec3_rotate_x(&v, t * 0.7f);
        vec3_rotate_y(&v, t);
        float z = v.z + dist;
        proj[i].x = (int)(v.x * fov / z + cx);
        proj[i].y = (int)(v.y * fov / z + cy);
        proj[i].z = z;
    }

    uint32_t base_color = 0xff88ccff;
    for (int i = 0; i < 64; i++) {
        int *f = faces[i];

        float avg_z = (proj[f[0]].z + proj[f[1]].z + proj[f[2]].z) / 3.0f;
        float shade = 1.0f - (avg_z - 2.5f) / 2.0f;
        if (shade < 0.2f) shade = 0.2f;

        uint8_t r = ((base_color >> 16) & 0xff) * shade;
        uint8_t g = ((base_color >> 8) & 0xff) * shade;
        uint8_t b = (base_color & 0xff) * shade;
        uint32_t color = (0xff << 24) | (r << 16) | (g << 8) | b;

        if (ctx->wireframe) {
            draw_line(pixels, width, height, proj[f[0]].x, proj[f[0]].y, proj[f[1]].x, proj[f[1]].y, 0xff88ccff);
            draw_line(pixels, width, height, proj[f[1]].x, proj[f[1]].y, proj[f[2]].x, proj[f[2]].y, 0xff88ccff);
            draw_line(pixels, width, height, proj[f[2]].x, proj[f[2]].y, proj[f[0]].x, proj[f[0]].y, 0xff88ccff);
        } else {
            draw_triangle_filled(pixels, zbuf, width, height,
                                 proj[f[0]], proj[f[1]], proj[f[2]], color);
        }
    }
}

void z_ui_destory(Z_UI_Context *ctx) {
    (void)ctx;
}
