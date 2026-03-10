#ifndef DRAGOON_TETRIS_H
#define DRAGOON_TETRIS_H

void tetris_run(void);

/* Non-blocking API for windowed mode */
void tetris_init(void);
void tetris_key(int key);
int  tetris_tick(void);
void tetris_draw(void);

#endif
