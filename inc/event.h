#ifndef EVENT_H
#define EVENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EVENT_QUEUE_SIZE 64

typedef enum {
    EVENT_KEY,
    EVENT_MOUSE_BUTTON,
    EVENT_MOUSE_MOVE,
} Event_Type;

typedef struct {
    Event_Type type;
    union {
        struct { uint32_t key; uint32_t state; } key;
        struct { int x; int y; uint32_t button; uint32_t state; } mouse_button;
        struct { int x; int y; } mouse_move;
    };
} Event;

#ifdef __cplusplus
}
#endif

#endif

