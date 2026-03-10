#ifndef DRAGOON_TIMER_H
#define DRAGOON_TIMER_H

#include "types.h"

void timer_init(void);
void timer_init_secondary(void);
u64 timer_get_ticks(void);

#endif /* DRAGOON_TIMER_H */
