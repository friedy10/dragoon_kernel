#ifndef DRAGOON_BREAKOUT_H
#define DRAGOON_BREAKOUT_H

void breakout_run(void);

/* Non-blocking API for windowed mode */
void breakout_init(void);
void breakout_key(int key);
int  breakout_tick(void);
void breakout_draw(void);

#endif
