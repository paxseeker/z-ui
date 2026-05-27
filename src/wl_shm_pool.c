#define _GNU_SOURCE
#include "wl_shm_pool.h"
#include "wayland_state.h"
#include "z-log.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client-protocol.h>

static void buffer_release(void *data, struct wl_buffer *wl_buffer) {
    mark_idle((ShmPool *)data, wl_buffer);
}

static struct wl_buffer_listener shm_buffer_listener = {
    .release = buffer_release,
};

int create_shm_buffer(ShmPool *shm_pool) {
    for (int i = 0; i < shm_pool->buffer_count; i++) {
        shm_pool->shm_buffers[i] =
            (ShmBuffer *)malloc(sizeof(ShmBuffer));

        if (!shm_pool->shm_buffers[i]) {
            ERROR("malloc shm_buffer error");
            return 1;
        }

        int stride = shm_pool->stride;
        int aligned_ssize = shm_pool->ssize;
        int offset = aligned_ssize * i;
        shm_pool->shm_buffers[i]->buffer = wl_shm_pool_create_buffer(
            shm_pool->wl_shm_pool, offset, shm_pool->buffer_width,
            shm_pool->buffer_height, stride, WL_SHM_FORMAT_ARGB8888);

        if (!shm_pool->shm_buffers[i]->buffer) {
            ERROR("create buffer error - compositor rejected buffer size");
            return 1;
        }

        wl_buffer_add_listener(shm_pool->shm_buffers[i]->buffer,
                               &shm_buffer_listener, shm_pool);

        shm_pool->shm_buffers[i]->busy = 0;
        void *ret = mmap(NULL, shm_pool->ssize, PROT_READ | PROT_WRITE,
                         MAP_SHARED, shm_pool->fd, offset);
        if (ret == MAP_FAILED) {
            ERROR("mmap error: %s", strerror(errno));
            return 1;
        }
        shm_pool->shm_buffers[i]->data = ret;
    }

    return 0;
}

int wl_shm_pool_init(WaylandState *state,
                     OutputInfo *output_info, int buffer_count) {
    INFO("begine shm pool init");
    ShmPool *shm_pool =
        (ShmPool *)malloc(sizeof(ShmPool));
    if (!shm_pool) {
        ERROR("malloc shm_pool error");
        return 1;
    }

    shm_pool->shm_buffers = (ShmBuffer **)malloc(
        sizeof(ShmBuffer *) * buffer_count);
    if (!shm_pool->shm_buffers) {
        ERROR("malloc shm_buffers error");
        free(shm_pool);
        return 1;
    }

    // Initialize shm_buffers to NULL
    for (int i = 0; i < buffer_count; i++) {
        shm_pool->shm_buffers[i] = NULL;
    }

    shm_pool->buffer_count = buffer_count;
    shm_pool->buffer_width = output_info->width;
    shm_pool->buffer_height = output_info->height;

    INFO("buffer width %d, height %d, buffer count %d", shm_pool->buffer_width,
         shm_pool->buffer_height, shm_pool->buffer_count);

    struct wl_shm *shm = state->shm;
    int stride = output_info->width * 4;
    int aligned_ssize = (stride * output_info->height + 4095) & ~4095;
    int64_t pool_size = (int64_t)aligned_ssize * buffer_count;

    shm_pool->ssize = aligned_ssize;
    shm_pool->stride = stride;

    int fd = memfd_create("zpaper-shm-pool", MFD_CLOEXEC);
    if (fd < 0) {
        ERROR("memfd_create error: %s", strerror(errno));
        free(shm_pool->shm_buffers);
        free(shm_pool);
        return 1;
    }

    if (ftruncate(fd, pool_size) < 0) {
        ERROR("ftruncate error: %s", strerror(errno));
        close(fd);
        free(shm_pool->shm_buffers);
        free(shm_pool);
        return 1;
    }

    shm_pool->wl_shm_pool = wl_shm_create_pool(shm, fd, pool_size);
    if (!shm_pool->wl_shm_pool) {
        ERROR("create shm pool error");
        close(fd);
        free(shm_pool->shm_buffers);
        free(shm_pool);
        return 1;
    }
    shm_pool->fd = fd;
    shm_pool->state = state;

    output_info->shm_pool = shm_pool;

    int ret = create_shm_buffer(shm_pool);
    if (ret) {
        ERROR("create shm buffer error");
        // Cleanup partially created buffers
        if (shm_pool->shm_buffers) {
            for (int i = 0; i < buffer_count; i++) {
                if (shm_pool->shm_buffers[i]) {
                    if (shm_pool->shm_buffers[i]->data) {
                        munmap(shm_pool->shm_buffers[i]->data, shm_pool->ssize);
                    }
                    if (shm_pool->shm_buffers[i]->buffer) {
                        wl_buffer_destroy(shm_pool->shm_buffers[i]->buffer);
                    }
                    free(shm_pool->shm_buffers[i]);
                }
            }
            free(shm_pool->shm_buffers);
        }
        if (shm_pool->wl_shm_pool) {
            wl_shm_pool_destroy(shm_pool->wl_shm_pool);
        }
        close(fd);
        free(shm_pool);
        return 1;
    }
    INFO("shm pool init successful");
    return 0;
}
ShmBuffer *get_shm_buffer(ShmPool *pool) {
    if (!pool) {
        ERROR("pool is null");
        return NULL;
    }
    for (int i = 0; i < pool->buffer_count; i++) {
        if (!pool->shm_buffers[i]->busy) {
            return pool->shm_buffers[i];
        }
    }
    WARN("All buffer busy");
    return NULL;
}

int mark_busy(ShmPool *pool, struct wl_buffer *buffer) {
    if (!pool || !pool->shm_buffers || !buffer) {
        return -1;
    }
    for (int i = 0; i < pool->buffer_count; i++) {
        if (pool->shm_buffers[i] && pool->shm_buffers[i]->buffer == buffer) {
            pool->shm_buffers[i]->busy = 1;
            return 0;
        }
    }
    return -1;
}

int mark_idle(ShmPool *pool, struct wl_buffer *buffer) {
    if (!pool || !pool->shm_buffers || !buffer) {
        return -1;
    }
    for (int i = 0; i < pool->buffer_count; i++) {
        if (pool->shm_buffers[i] && pool->shm_buffers[i]->buffer == buffer) {
            pool->shm_buffers[i]->busy = 0;
            return 0;
        }
    }
    return -1;
}

void shm_pool_unmap_buffers(ShmPool *pool) {
    if (!pool || !pool->shm_buffers) {
        return;
    }

    for (int i = 0; i < pool->buffer_count; i++) {
        ShmBuffer *buf = pool->shm_buffers[i];
        if (buf && buf->data) {
            munmap(buf->data, pool->ssize);
            buf->data = NULL;
        }
    }
}

void shm_pool_destroy(ShmPool *pool) {
    if (!pool) {
        return;
    }

    INFO("shm pool destroy");

    if (pool->shm_buffers) {
        for (int i = 0; i < pool->buffer_count; i++) {
            ShmBuffer *buf = pool->shm_buffers[i];
            if (buf) {
                if (buf->data) {
                    munmap(buf->data, pool->ssize);
                    buf->data = NULL;
                }
                if (buf->buffer) {
                    wl_buffer_destroy(buf->buffer);
                    buf->buffer = NULL;
                }
                free(buf);
                pool->shm_buffers[i] = NULL;
            }
        }
        free(pool->shm_buffers);
        pool->shm_buffers = NULL;
    }

    if (pool->wl_shm_pool) {
        wl_shm_pool_destroy(pool->wl_shm_pool);
        pool->wl_shm_pool = NULL;
    }

    if (pool->fd >= 0) {
        close(pool->fd);
        pool->fd = -1;
    }

    free(pool);
    pool = NULL;
}
