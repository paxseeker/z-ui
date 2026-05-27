#include "z-log.h"
#include "z-ui.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void draw_pixel(uint32_t *pixels, int width, int height, int x, int y, uint32_t color) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    pixels[y * width + x] = color;
}

static void draw_line(uint32_t *pixels, int width, int height, Point p0, Point p1, uint32_t color) {
    int x0 = p0.x, y0 = p0.y;
    int x1 = p1.x, y1 = p1.y;
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

void z_ui_init() {
    z_ui_set_size(800, 600);
}

void z_ui_update(uint32_t *pixels, int width, int height) {
    memset(pixels, 0, width * height * 4);

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    float t = ts.tv_sec + ts.tv_nsec * 1e-9f;

    vec3 verts[8] = {
        {-1, -1, -1}, {1, -1, -1}, {1, -1, 1}, {-1, -1, 1},
        {-1, 1, -1}, {1, 1, -1}, {1, 1, 1}, {-1, 1, 1},
    };
    int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7},
    };

    int cx = width / 2, cy = height / 2;
    float scale = MIN(width, height) * 0.3f;
    float fov = scale, dist = 3.5f;

    Point proj[8];
    for (int i = 0; i < 8; i++) {
        vec3 v = verts[i];
        vec3_rotate_x(&v, t * 0.7f);
        vec3_rotate_y(&v, t);
        float z = v.z + dist;
        proj[i].x = (int)(v.x * fov / z + cx);
        proj[i].y = (int)(v.y * fov / z + cy);
    }

    uint32_t color = 0xff88ccff;
    for (int i = 0; i < 12; i++) {
        draw_line(pixels, width, height, proj[edges[i][0]], proj[edges[i][1]], color);
    }
}

void z_ui_handle_events() {
}

void z_ui_destory() {
}
