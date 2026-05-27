#ifndef EVENT_H
#define EVENT_H

typedef enum {
    ON_CLICK,
    ON_DOUBLE_CLICK,
    ON_FCOUS
} Event_Type;

typedef struct {
    Event_Type event_type;
} Event ;

#endif

