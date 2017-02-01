#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/seq_file.h>

#include "freg.h"

static int freg_major = 0;
static int freg_minor = 0;

static struct class* freg_class = NULL;
static struct fake_reg_dev* freg_dev = NULL;

static int freg_open(struct inode* inode, struct file* file)
{
    struct fake_reg_dev* dev;
    dump_stack();
    dev = container_of(inode->i_cdev, struct fake_reg_dev, dev);
    file->private_data = dev;

    return 0;
}

static int freg_release(struct inode* inode, struct file* file)
{
    dump_stack();
    return 0;
}

static ssize_t freg_read(struct file* file, char __user *buf,
        size_t count, loff_t* fpos)
{
    ssize_t err = 0;
    struct fake_reg_dev *dev = file->private_data;

    dump_stack();
    if (down_interruptible(&dev->sem)) {
        return -ERESTARTSYS;
    }

    if (count < sizeof(dev->val))
        goto out;

    if (copy_to_user(buf, &dev->val, sizeof(dev->val))) {
        err = -EFAULT;
        goto out;
    }

    err = sizeof(dev->val);
out:
    up(&dev->sem);
    return err;
}

static ssize_t freg_write(struct file* file, const char __user *buf,
        size_t count, loff_t* fpos)
{
    ssize_t err = 0;
    struct fake_reg_dev *dev = file->private_data;

    dump_stack();
    if (down_interruptible(&dev->sem)) {
        return -ERESTARTSYS;
    }

    if (count < sizeof(dev->val))
        goto out;

    if (copy_from_user(&dev->val, buf, sizeof(dev->val))) {
        err = -EFAULT;
        goto out;
    }

    err = sizeof(dev->val);
out:
    up(&dev->sem);
    return err;
}

static struct file_operations freg_fops = {
    .owner = THIS_MODULE,
    .open = freg_open,
    .release = freg_release,
    .read = freg_read,
    .write = freg_write,
};

#ifdef DEBUG
static int freg_proc_show(struct seq_file *f, void* v)
{
    if (freg_dev) {
        seq_printf(f, "%d\n", freg_dev->val);
    }
    return 0;
}

static int freg_proc_open(struct inode* inode, struct file* file)
{
    return single_open(file, freg_proc_show, NULL);
}

static struct file_operations freg_proc_fops = {
    .open = freg_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static void freg_create_proc(void)
{
    struct proc_dir_entry* entry;
    entry = proc_create(FREG_DEVICE_PROC_NAME, 0, NULL, &freg_proc_fops);
    if (!entry) {
        printk(KERN_ALERT "proc create failed\n");
    }
}
#endif

static ssize_t __freg_get_val(struct fake_reg_dev* dev,
        char* buf)
{
    int val = 0;

    if (down_interruptible(&dev->sem)) {
        return -ERESTARTSYS;
    }
    val = dev->val;
    up(&dev->sem);

    return sprintf(buf, "%d\n", val);
}

static ssize_t freg_val_show(struct device* dev,
        struct device_attribute *attr, char* buf)
{
    struct fake_reg_dev* hdev
        = (struct fake_reg_dev*) dev_get_drvdata(dev);
    return __freg_get_val(hdev, buf);
}

static ssize_t __freg_set_val(struct fake_reg_dev *dev,
        const char* buf, size_t count)
{
    int val = 0;

    val = simple_strtol(buf, NULL, 10);
    if (down_interruptible(&dev->sem)) {
        return -ERESTARTSYS;
    }
    dev->val = val;
    up(&dev->sem);

    return count;
}

static ssize_t freg_val_store(struct device* dev,
        struct device_attribute *attr, const char* buf, size_t count)
{
    struct fake_reg_dev* hdev
        = (struct fake_reg_dev*) dev_get_drvdata(dev);
    return __freg_set_val(hdev, buf, count);
}

static int __freg_setup_dev(struct fake_reg_dev* dev)
{
    int err;
    dev_t devno = MKDEV(freg_major, freg_minor);

    memset(dev, 0, sizeof(struct fake_reg_dev));
    cdev_init(&dev->dev, &freg_fops);

    err = cdev_add(&dev->dev, devno, 1);
    if (err) {
        return err;
    }

    sema_init(&dev->sem, 1);
    dev->val = 0;

    return 0;
}

/*devfs file attribute*/
static DEVICE_ATTR(val, 0660, freg_val_show, freg_val_store);

static int __init freg_init(void)
{
    int err = -1;
    dev_t dev = 0;

    struct device *temp = NULL;

    printk(KERN_INFO "freg init\n");

    dump_stack();
    /*allocate char device region*/
    err = alloc_chrdev_region(&dev, 0, 1, FREG_DEVICE_NODE_NAME);
    if (err < 0) {
        printk(KERN_ALERT "Failed to alloc char dev region\n");
        goto failed;
    }

    freg_major = MAJOR(dev);
    freg_minor = MINOR(dev);

    printk(KERN_INFO "register freg device with major %d, minor %d",
            freg_major, freg_minor);

    /*allocate fake_reg_dev data struct*/
    freg_dev = kmalloc(sizeof(struct fake_reg_dev), GFP_KERNEL);

    if (unlikely(!freg_dev)) {
        printk(KERN_ALERT "failed to kmalloc\n");
        goto unregister;
    }

    err = __freg_setup_dev(freg_dev);
    if (err) {
        printk(KERN_ALERT "failed to setup freg device\n");
        goto cleanup;
    }

    /* create 'freg' directory under /sys/class */
    freg_class = class_create(THIS_MODULE, FREG_DEVICE_CLASS_NAME);
    if (unlikely(IS_ERR(freg_class))) {
        err = PTR_ERR(freg_class);
        printk(KERN_ALERT "failed to create freg device class\n");
        goto destroy_cdev;
    }

    /*
     * create device file 'freg' under /sys/class/freg and /dev
     */
    temp = device_create(freg_class, NULL, dev, NULL, FREG_DEVICE_FILE_NAME);

    if (unlikely(IS_ERR(temp))) {
        err = PTR_ERR(temp);
        printk(KERN_ALERT "failed to create freg device\n");
        goto destroy_class;
    }

    /*
     * create attribute file 'val' under
     * /sys/class/freg/freg/
     */
    err = device_create_file(temp, &dev_attr_val);
    if (err < 0) {
        printk(KERN_ALERT "failed to create device file");
        goto destroy_device;
    }

    dev_set_drvdata(temp, freg_dev);

#ifdef DEBUG
    freg_create_proc();
#endif
    printk(KERN_INFO "freg device init success\n");
    return 0;

destroy_device:
    device_destroy(freg_class, dev);
destroy_class:
    class_destroy(freg_class);
destroy_cdev:
    cdev_del(&(freg_dev->dev));
cleanup:
    kfree(freg_dev);
unregister:
    unregister_chrdev_region(MKDEV(freg_major, freg_minor), 1);
failed:
    return err;
}

static void __exit freg_exit(void)
{
    dev_t devno = MKDEV(freg_major, freg_minor);
    printk(KERN_INFO "freg exit\n");

    if (freg_class) {
        device_destroy(freg_class, devno);
        class_destroy(freg_class);
    }

    if (freg_dev) {
        cdev_del(&freg_dev->dev);
        kfree(freg_dev);
    }

    unregister_chrdev_region(devno, 1);
}

MODULE_LICENSE("Dual BSD/GPL");
module_init(freg_init);
module_exit(freg_exit);
