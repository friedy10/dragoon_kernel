#ifndef DRAGOON_COMPAT_H
#define DRAGOON_COMPAT_H

/* Linux compat server entry point (runs as kernel task) */
void linux_compat_server(void);

/* Register a platform device for driver matching */
int compat_register_platform_device(void *pdev_ptr);

#endif /* DRAGOON_COMPAT_H */
