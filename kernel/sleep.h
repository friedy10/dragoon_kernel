#ifndef DRAGOON_SLEEP_H
#define DRAGOON_SLEEP_H

#include "types.h"

/* Initialize sleep subsystem */
void sleep_init(void);

/* Sleep for the given number of timer ticks (10ms each) */
void sleep_ticks(u64 ticks);

/* Sleep for the given number of milliseconds (rounded to 10ms ticks) */
void sleep_ms(u64 ms);

#endif /* DRAGOON_SLEEP_H */
