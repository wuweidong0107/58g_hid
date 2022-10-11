#ifndef __TASK_H__
#define __TASK_H__

typedef struct task task_t;
typedef void (*task_handler_t)(task_t *task);

typedef void (*task_callback_t)(void *user_data);

struct task {
    task_handler_t handler;
    void *task_data;
    task_callback_t callback;
    void *user_data;
};

#endif