#ifndef DRAGOON_TASK_H
#define DRAGOON_TASK_H

#include "types.h"
#include "cap.h"

#define MAX_TASKS       32
#define TASK_STACK_SIZE 16384  /* 16 KB */

/* Task states */
#define TASK_DEAD    0
#define TASK_READY   1
#define TASK_RUNNING 2
#define TASK_BLOCKED 3

struct task_context {
    u64 x19, x20, x21, x22, x23, x24, x25, x26, x27, x28;
    u64 x29;  /* frame pointer */
    u64 x30;  /* link register / return address */
    u64 sp;
};

struct task {
    int id;
    int state;
    char name[32];
    struct task_context ctx;
    u64 stack_base;
    int cap_table[MAX_CAPS_PER_TASK];  /* indices into global cap pool */
    int num_caps;
    int wakeup_reason;  /* 0=normal wake, -1=timeout */
};

void task_init(void);
int task_create(const char *name, void (*entry)(void));
void task_destroy(int task_id);
struct task *task_get(int task_id);
struct task *task_current(void);
int task_current_id(void);
void task_set_current(int id);
int task_count(void);
int task_add_cap(int task_id, int cap_id);

#endif /* DRAGOON_TASK_H */
