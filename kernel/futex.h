#ifndef DRAGOON_FUTEX_H
#define DRAGOON_FUTEX_H

#include "types.h"

/* Initialize futex subsystem */
void futex_init(void);

/* Wait on a futex. If *addr == expected, block until woken.
 * Returns 0 on wake, -1 if *addr != expected (no block). */
int futex_wait(u32 *addr, u32 expected);

/* Wait on a futex with timeout (in timer ticks, 10ms each).
 * Returns 0 on wake, -1 on timeout or value mismatch. */
int futex_wait_timeout(u32 *addr, u32 expected, u64 ticks);

/* Wake up to count waiters on the futex at addr.
 * Returns number of tasks woken. */
int futex_wake(u32 *addr, int count);

#endif /* DRAGOON_FUTEX_H */
