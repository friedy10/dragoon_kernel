#ifndef _LINUX_FS_H
#define _LINUX_FS_H

#include <linux/types.h>

struct file;
struct inode;

struct file_operations {
    int (*open)(struct inode *, struct file *);
    int (*release)(struct inode *, struct file *);
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
};

int register_chrdev(unsigned int major, const char *name,
                    const struct file_operations *fops);
void unregister_chrdev(unsigned int major, const char *name);

#endif /* _LINUX_FS_H */
