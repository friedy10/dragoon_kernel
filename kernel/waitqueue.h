#ifndef DRAGOON_WAITQUEUE_H
#define DRAGOON_WAITQUEUE_H

#include "types.h"

/* Wait queue: linked list of blocked tasks via static pool */

struct wait_queue {
    int head;  /* index into global wq_pool, -1 = empty */
};

/* Initialize a wait queue */
void wq_init(struct wait_queue *wq);

/* Block current task on this wait queue. Returns after wakeup. */
void wq_wait(struct wait_queue *wq);

/* Block with timeout (in timer ticks, 10ms each).
 * Returns 0 on normal wake, -1 on timeout. */
int wq_wait_timeout(struct wait_queue *wq, u64 ticks);

/* Wake the first waiter on this queue */
void wq_wake_one(struct wait_queue *wq);

/* Wake all waiters on this queue */
void wq_wake_all(struct wait_queue *wq);

/* Called from timer IRQ to check for expired timeouts */
void wq_tick(void);

/* Initialize the global wait queue pool */
void wq_pool_init(void);

#endif /* DRAGOON_WAITQUEUE_H */
