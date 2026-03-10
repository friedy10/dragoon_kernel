#ifndef DRAGOON_SNAKE_H
#define DRAGOON_SNAKE_H

void snake_run(void);

/* Non-blocking API for windowed mode */
void snake_init(void);
void snake_key(int key);
int  snake_tick(void);  /* returns 1 if state changed */
void snake_draw(void);  /* draws using gpu_* (respects render target) */

#endif
