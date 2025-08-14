#include <stdlib.h>
#include <hal.h>
#include "Task/thread.h"

typedef enum {
    HAL_TASK_REG,
    HAL_TASK_UNREG
} hal_task_type_t;

typedef struct hal_async_task {
    hal_task_type_t op;
    hal_descriptor_t desc;
    uint64_t parent_id;
    uint64_t id;
    hal_async_cb_t cb;
    void *ctx;
} hal_async_task_t;

static hal_async_task_t *current_task;

static void hal_async_worker(void) {
    hal_async_task_t *t = current_task;
    uint64_t id = 0;
    int status = 0;

    if (t->op == HAL_TASK_REG) {
        id = hal_register(&t->desc, t->parent_id);
        status = id ? 0 : -1;
    } else {
        status = hal_unregister(t->id);
        id = t->id;
    }

    if (t->cb) {
        t->cb(id, status, t->ctx);
    }

    free(t);
}

static int start_async(hal_async_task_t *task) {
    current_task = task;
    thread_t *thr = thread_create(hal_async_worker);
    return thr ? 0 : -1;
}

void hal_register_async(const hal_descriptor_t *desc, uint64_t parent_id,
                        hal_async_cb_t cb, void *ctx) {
    if (!desc) {
        if (cb) cb(0, -1, ctx);
        return;
    }

    hal_async_task_t *t = malloc(sizeof(*t));
    if (!t) {
        if (cb) cb(0, -1, ctx);
        return;
    }

    t->op = HAL_TASK_REG;
    t->desc = *desc;
    t->parent_id = parent_id;
    t->cb = cb;
    t->ctx = ctx;

    if (start_async(t) != 0) {
        if (cb) cb(0, -1, ctx);
        free(t);
    }
}

void hal_unregister_async(uint64_t id, hal_async_cb_t cb, void *ctx) {
    hal_async_task_t *t = malloc(sizeof(*t));
    if (!t) {
        if (cb) cb(id, -1, ctx);
        return;
    }

    t->op = HAL_TASK_UNREG;
    t->id = id;
    t->cb = cb;
    t->ctx = ctx;

    if (start_async(t) != 0) {
        if (cb) cb(id, -1, ctx);
        free(t);
    }
}

