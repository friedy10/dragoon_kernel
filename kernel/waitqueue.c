/*
 * Dragoon Microkernel - Wait Queue
 *
 * Generic wait queue for blocking tasks with optional timeout.
 * Uses a static pool of entries (no dynamic allocation).
 * Timer calls wq_tick() each 10ms to check for expired timeouts.
 */
#include "waitqueue.h"
#include "task.h"
#include "timer.h"
#include "sched.h"
#include "printf.h"

#define WQ_POOL_SIZE 64

struct wq_entry {
    int task_id;
    u64 timeout_tick;   /* absolute tick to expire, 0 = no timeout */
    int next;           /* next entry index in queue, -1 = end */
    int in_use;
    struct wait_queue *owner;  /* which queue this entry belongs to */
};

static struct wq_entry wq_pool[WQ_POOL_SIZE];

void wq_pool_init(void)
{
    for (int i = 0; i < WQ_POOL_SIZE; i++) {
        wq_pool[i].in_use = 0;
        wq_pool[i].next = -1;
        wq_pool[i].owner = (void *)0;
    }
}

void wq_init(struct wait_queue *wq)
{
    wq->head = -1;
}

static int pool_alloc(void)
{
    for (int i = 0; i < WQ_POOL_SIZE; i++) {
        if (!wq_pool[i].in_use) {
            wq_pool[i].in_use = 1;
            wq_pool[i].next = -1;
            return i;
        }
    }
    return -1;
}

static void pool_free(int idx)
{
    if (idx >= 0 && idx < WQ_POOL_SIZE) {
        wq_pool[idx].in_use = 0;
        wq_pool[idx].owner = (void *)0;
    }
}

/* Add entry to tail of queue */
static void queue_append(struct wait_queue *wq, int idx)
{
    wq_pool[idx].next = -1;
    if (wq->head < 0) {
        wq->head = idx;
    } else {
        int cur = wq->head;
        while (wq_pool[cur].next >= 0)
            cur = wq_pool[cur].next;
        wq_pool[cur].next = idx;
    }
}

/* Remove entry from queue by index */
static void queue_remove(struct wait_queue *wq, int idx)
{
    if (wq->head < 0) return;

    if (wq->head == idx) {
        wq->head = wq_pool[idx].next;
        return;
    }

    int prev = wq->head;
    while (prev >= 0 && wq_pool[prev].next != idx)
        prev = wq_pool[prev].next;

    if (prev >= 0)
        wq_pool[prev].next = wq_pool[idx].next;
}

void wq_wait(struct wait_queue *wq)
{
    int tid = task_current_id();
    if (tid < 0) return;

    int idx = pool_alloc();
    if (idx < 0) {
        kprintf("[wq] pool exhausted!\n");
        return;
    }

    wq_pool[idx].task_id = tid;
    wq_pool[idx].timeout_tick = 0;
    wq_pool[idx].owner = wq;
    queue_append(wq, idx);

    struct task *t = task_get(tid);
    if (t) {
        t->wakeup_reason = 0;
        t->state = TASK_BLOCKED;
    }

    schedule();
}

int wq_wait_timeout(struct wait_queue *wq, u64 ticks)
{
    int tid = task_current_id();
    if (tid < 0) return -1;

    int idx = pool_alloc();
    if (idx < 0) {
        kprintf("[wq] pool exhausted!\n");
        return -1;
    }

    wq_pool[idx].task_id = tid;
    wq_pool[idx].timeout_tick = timer_get_ticks() + ticks;
    wq_pool[idx].owner = wq;
    queue_append(wq, idx);

    struct task *t = task_get(tid);
    if (t) {
        t->wakeup_reason = 0;
        t->state = TASK_BLOCKED;
    }

    schedule();

    /* After waking up, check reason */
    if (t)
        return t->wakeup_reason;
    return -1;
}

void wq_wake_one(struct wait_queue *wq)
{
    if (wq->head < 0) return;

    int idx = wq->head;
    wq->head = wq_pool[idx].next;

    struct task *t = task_get(wq_pool[idx].task_id);
    if (t && t->state == TASK_BLOCKED) {
        t->wakeup_reason = 0;
        t->state = TASK_READY;
    }

    pool_free(idx);
}

void wq_wake_all(struct wait_queue *wq)
{
    while (wq->head >= 0) {
        int idx = wq->head;
        wq->head = wq_pool[idx].next;

        struct task *t = task_get(wq_pool[idx].task_id);
        if (t && t->state == TASK_BLOCKED) {
            t->wakeup_reason = 0;
            t->state = TASK_READY;
        }

        pool_free(idx);
    }
}

void wq_tick(void)
{
    u64 now = timer_get_ticks();

    for (int i = 0; i < WQ_POOL_SIZE; i++) {
        if (!wq_pool[i].in_use) continue;
        if (wq_pool[i].timeout_tick == 0) continue;
        if (now < wq_pool[i].timeout_tick) continue;

        /* Timeout expired — wake the task */
        struct task *t = task_get(wq_pool[i].task_id);
        if (t && t->state == TASK_BLOCKED) {
            t->wakeup_reason = -1;  /* timeout */
            t->state = TASK_READY;
        }

        /* Remove from its owner queue */
        if (wq_pool[i].owner)
            queue_remove(wq_pool[i].owner, i);

        pool_free(i);
    }
}
