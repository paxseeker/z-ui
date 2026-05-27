#ifndef WL_SHM_POOL_H
#define WL_SHM_POOL_H

#include <stdint.h>
#include <wayland-client-protocol.h>

typedef struct WaylandState WaylandState;
typedef struct OutputInfo OutputInfo;

typedef struct {
    struct wl_buffer *buffer;
    int busy;
    uint32_t *data;
} ShmBuffer;

typedef struct ShmPool {
    ShmBuffer **shm_buffers;
    struct wl_shm_pool *wl_shm_pool;
    int buffer_count;
    int buffer_width;
    int buffer_height;
    int64_t ssize;
    int stride;
    int fd;
    WaylandState *state;
} ShmPool;

int wl_shm_pool_init(WaylandState *state, OutputInfo *output_info,
                     int buffer_count);

ShmBuffer *get_shm_buffer(ShmPool *pool);

int mark_busy(ShmPool *pool, struct wl_buffer *buffer);

int mark_idle(ShmPool *pool, struct wl_buffer *buffer);

void shm_pool_destroy(ShmPool *pool);

void shm_pool_unmap_buffers(ShmPool *pool);

#endif
