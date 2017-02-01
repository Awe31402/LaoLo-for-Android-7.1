#define ALOG_TAG "FregHalStub"

#include <hardware/hardware.h>
#include <hardware/freg.h>

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <cutils/log.h>
#include <cutils/atomic.h>

#define DEVICE_NAME "/dev/freg"
#define MODULE_NAME "freg"
#define MODULE_AUTHOR "awe"

/*device open and close interface*/
static int freg_device_open(const struct hw_module_t* module,
        const char* id,
        struct hw_device_t **device);
static int freg_device_close(struct hw_device_t* device);

/*fake register read and write interface*/
static int freg_get_val(struct freg_device_t *dev, int* val);
static int freg_set_val(struct freg_device_t *dev, int val);

/*define variable of module operation metrhod struct*/
static struct hw_module_methods_t freg_module_method = {
    .open = freg_device_open,
};

struct freg_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = 1,
        .hal_api_version = 0,
        .id = FREG_HARDWARE_MODULE_ID,
        .name = FREG_HARDWARE_DEVICE_ID,
        .author = MODULE_AUTHOR,
        .methods = &freg_module_method,
    }
};

static int freg_device_open(const struct hw_module_t *module,
        const char* id,
        struct hw_device_t **device)
{
    struct freg_device_t* dev;

    /* if device id is not compatibale,
     * return -EFAULT*/
    if (!!strcmp(id, FREG_HARDWARE_DEVICE_ID))
        return -EFAULT;

    dev = (struct freg_device_t*) malloc(sizeof(struct freg_device_t));

    if (!dev) {
        ALOGE("Failed to alloc space for freg_device_t.");
        return -EFAULT;
    }

    memset(dev, 0, sizeof(struct freg_device_t));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*) module;
    dev->common.close = freg_device_close;
    dev->set_val = freg_set_val;
    dev->get_val = freg_get_val;

    dev->fd = open(DEVICE_NAME, O_RDWR);
    if (dev->fd == -1) {
        ALOGE("Failed to open /dev/freg -- %s",
                strerror(errno));
        free(dev);
        return -EFAULT;
    }

    *device = &dev->common;
    ALOGI("Open device /dev/freg success");
    return 0;
}

static int freg_device_close(struct hw_device_t* device)
{
    struct freg_device_t* freg_device = (struct freg_device_t*) device;
    if (freg_device) {
        close(freg_device->fd);
        free(freg_device);
    }
    return 0;
}

int freg_set_val(struct freg_device_t* dev, int val)
{
    if (!dev) {
        ALOGE("NULL device pointer");
        return -EFAULT;
    }

    ALOGI("Set value %d to /dev/freg", val);
    write(dev->fd, &val, sizeof(int));

    return 0;
}

int freg_get_val(struct freg_device_t* dev, int* val)
{
    if (!dev) {
        ALOGE("NULL device pointer");
        return -EFAULT;
    }

    if (!val) {
        ALOGE("NULL output int pointer");
        return -EFAULT;
    }

    read(dev->fd, val, sizeof(int));

    ALOGI("Get value %d from /dev/freg", *val);
    return 0;
}
