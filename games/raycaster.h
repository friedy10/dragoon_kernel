#ifndef DRAGOON_RAYCASTER_H
#define DRAGOON_RAYCASTER_H

void raycaster_run(void);

/* Non-blocking API for windowed mode */
void raycaster_init(void);
void raycaster_key(int key);
int  raycaster_tick(void);
void raycaster_draw(void);

#endif
