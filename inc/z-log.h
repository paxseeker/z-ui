/*
 * author paxseeker 2026-04-24
 * a logger write in c
 * if you want to output log to a file, define LOG_FILE_PATH
 */

#ifndef Z_LOG_H
#define Z_LOG_H

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef QUEUE_SIZE
#define QUEUE_SIZE 32
#endif

#define Z_LOG_DEBUG 0
#define Z_LOG_INFO 1
#define Z_LOG_WARN 2
#define Z_LOG_ERROR 3



#ifndef Z_LOG_DEF
#define Z_LOG_DEF
#endif

typedef struct {
    int level;
    char *msg;
    const char *file;
    int line;
} z_log;

Z_LOG_DEF int z_log_init(void);
Z_LOG_DEF int z_log_exit(void);
Z_LOG_DEF int send_log(z_log *log);

#define INFO(x, ...)                                                           \
    do {                                                                       \
        char buf[255];                                                         \
        snprintf(buf, sizeof(buf), x, ##__VA_ARGS__);                          \
        z_log *_log = (z_log *)malloc(sizeof(z_log));                          \
        _log->level = Z_LOG_INFO;                                              \
        _log->msg = strdup(buf);                                               \
        _log->file = __FILE__;                                                 \
        _log->line = __LINE__;                                                 \
        send_log(_log);                                                        \
    } while (0)

#define DEBUG(x, ...)                                                          \
    do {                                                                       \
        char buf[255];                                                         \
        snprintf(buf, sizeof(buf), x, ##__VA_ARGS__);                          \
        z_log *_log = (z_log *)malloc(sizeof(z_log));                          \
        _log->level = Z_LOG_DEBUG;                                             \
        _log->msg = strdup(buf);                                               \
        _log->file = __FILE__;                                                 \
        _log->line = __LINE__;                                                 \
        send_log(_log);                                                        \
    } while (0)

#define WARN(x, ...)                                                           \
    do {                                                                       \
        char buf[255];                                                         \
        snprintf(buf, sizeof(buf), x, ##__VA_ARGS__);                          \
        z_log *_log = (z_log *)malloc(sizeof(z_log));                          \
        _log->level = Z_LOG_WARN;                                              \
        _log->msg = strdup(buf);                                               \
        _log->file = __FILE__;                                                 \
        _log->line = __LINE__;                                                 \
        send_log(_log);                                                        \
    } while (0)

#define ERROR(x, ...)                                                          \
    do {                                                                       \
        char buf[255];                                                         \
        snprintf(buf, sizeof(buf), x, ##__VA_ARGS__);                          \
        z_log *_log = (z_log *)malloc(sizeof(z_log));                          \
        _log->level = Z_LOG_ERROR;                                             \
        _log->msg = strdup(buf);                                               \
        _log->file = __FILE__;                                                 \
        _log->line = __LINE__;                                                 \
        send_log(_log);                                                        \
    } while (0)

#define Z_LOG_INIT() z_log_init();
#define Z_LOG_EXIT() z_log_exit();


#ifdef Z_LOG_IMPLEMENTATION

typedef struct {
    z_log *log_queue[QUEUE_SIZE];
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int head, tail;
    int count;
} z_log_queue;

z_log_queue *queue = NULL;
atomic_int log_running = 0;
pthread_t log_thread;
int Z_LOG_LEVEL;


#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"

#define GET_LOG_LEVEL(x)                                                       \
    x == Z_LOG_INFO    ? "[INFO]"                                              \
    : x == Z_LOG_DEBUG ? "[DEBUG]"                                             \
    : x == Z_LOG_WARN  ? "[WARN]"                                              \
                       : "[ERROR]"

#define LOG_COLOR(x)                                                           \
    x == Z_LOG_INFO    ? COLOR_GREEN                                           \
    : x == Z_LOG_DEBUG ? COLOR_BLUE                                            \
    : x == Z_LOG_WARN  ? COLOR_YELLOW                                          \
                       : COLOR_RED

#ifdef LOG_FILE_PATH
static FILE *log_file = NULL;
#endif

void z_log_println(z_log *log) {
    time_t now = time(NULL);
    char timestr[32];
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(stderr, "[%s] ", timestr);
    fprintf(stderr, "%s", LOG_COLOR(log->level));
    fprintf(stderr, "%-8s", GET_LOG_LEVEL(log->level));
    const char *short_file = strrchr(log->file, '/');
    if (!short_file) {
        short_file = log->file;
    } else {
        short_file++;
    }
    fprintf(stderr, COLOR_RESET " %s:%d %s\n", short_file, log->line, log->msg);
    fflush(stderr);
#ifdef LOG_FILE_PATH
    if (log_file) {
        fprintf(log_file, "[%s] %-8s %s:%d %s\n", timestr,
                GET_LOG_LEVEL(log->level), log->file, log->line, log->msg);
        fflush(log_file);
    }
#endif
}

void *log_thread_handle(void *argv) {
    z_log_queue *queue = (z_log_queue *)argv;
    while (1) {
        pthread_mutex_lock(&queue->mutex);
        if (queue->count == 0) {
            if (atomic_load(&log_running) == 0) {
                pthread_mutex_unlock(&queue->mutex);
                return NULL;
            }
            pthread_cond_wait(&queue->cond, &queue->mutex);
        }

        if (queue->count == 0 && atomic_load(&log_running) == 0) {
            pthread_mutex_unlock(&queue->mutex);
            return NULL;
        }

        z_log *log = queue->log_queue[queue->head];
        queue->head = (queue->head + 1) % QUEUE_SIZE;
        queue->count--;
        pthread_mutex_unlock(&queue->mutex);

        z_log_println(log);
        free(log->msg);
        free(log);
    }
    return NULL;
}

int send_log(z_log *log) {
    if (log->level < Z_LOG_LEVEL) {
        return 0;
    }
    pthread_mutex_lock(&queue->mutex);
    if (queue->count >= QUEUE_SIZE) {
        free(log->msg);
        free(log);
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }
    queue->log_queue[queue->tail] = log;
    queue->tail = (queue->tail + 1) % QUEUE_SIZE;
    queue->count++;
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

int get_log_level(const char* env_log_level) {
    if (strcmp(env_log_level, "debug") == 0) {
        return Z_LOG_DEBUG;
    } else if (strcmp(env_log_level, "info") == 0) {
        return Z_LOG_INFO;
    } else if (strcmp(env_log_level, "warn") == 0) {
        return Z_LOG_WARN;
    } else if (strcmp(env_log_level, "error") == 0) {
        return Z_LOG_ERROR;
    }
    return Z_LOG_INFO;
}

int z_log_init(void) {
    char* env_log_level = getenv("Z_LOG_LEVEL");
    
    int env_log_level_int = env_log_level ? get_log_level(env_log_level) : Z_LOG_INFO;
    Z_LOG_LEVEL = env_log_level_int;

    atomic_store(&log_running, 1);
    queue = (z_log_queue *)malloc(sizeof(z_log_queue));
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);

#ifdef LOG_FILE_PATH
    log_file = fopen(LOG_FILE_PATH, "a");
#endif

    if (pthread_create(&log_thread, NULL, log_thread_handle, queue) != 0) {
        return -1;
    }
    return 0;
}

int z_log_exit(void) {
    atomic_store(&log_running, 0);
    pthread_cond_broadcast(&queue->cond);
    pthread_join(log_thread, NULL);
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
#ifdef LOG_FILE_PATH
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
#endif
    free(queue);
    return 0;
}

void write_log(char *msg) {
    z_log *log = (z_log *)malloc(sizeof(z_log));
    log->level = Z_LOG_INFO;
    log->msg = msg;
    log->file = __FILE__;
    log->line = __LINE__;
    if (send_log(log) != 0) {
        free(log);
    }
}
#ifdef __cplusplus
}
#endif

#endif
#endif
