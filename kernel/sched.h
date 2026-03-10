#ifndef DRAGOON_SCHED_H
#define DRAGOON_SCHED_H

void sched_init(void);
void schedule(void);
void sched_yield(void);
void sched_start(void) __attribute__((noreturn));

#endif /* DRAGOON_SCHED_H */
