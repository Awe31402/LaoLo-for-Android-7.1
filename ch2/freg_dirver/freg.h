#ifndef FREG_H
#define FREG_H

#include <linux/cdev.h>
#include <linux/semaphore.h>

#define DEBUG
#define FREG_DEVICE_NODE_NAME "freg"
#define FREG_DEVICE_FILE_NAME "freg"
#define FREG_DEVICE_PROC_NAME "freg"
#define FREG_DEVICE_CLASS_NAME "freg"

struct fake_reg_dev {
    int val;
    struct semaphore sem;
    struct cdev dev;
};

#endif
