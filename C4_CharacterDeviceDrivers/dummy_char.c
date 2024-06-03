#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/ioctl.h>
#include "dummy_ioctl.h"

#define DUMMY_DEVICE_NAME   "dummy_char"
#define DUMMY_CLASS         "dummy_char_class"
#define DUMMY_FRAME_SIZE    12

struct class    *dummy_class;
struct cdev     dummy_cdev;
dev_t           dev_num;

struct dummy_data_frame {
    struct cdev cdev;
    unsigned char *data;
    int frame_size;
};

struct mutex cdev_lock;

static int dummy_open(struct inode *inode, struct file *filp) {
    unsigned int min = iminor(inode);

    struct dummy_data_frame *data_frm = NULL;
    data_frm = container_of(inode->i_cdev, struct dummy_data_frame, cdev);
    data_frm->frame_size = DUMMY_FRAME_SIZE;

    if (min < 0) {
        pr_err("Device not found\n");
        return -ENODEV;
    }

    /* Prepare the buffer if the device is opened for the first time */
    if (!data_frm->data) {
        data_frm->data = kzalloc(data_frm->frame_size, GFP_KERNEL);
        
        if (!data_frm->data) {
            pr_err("Open: memory allocation failed\n");
            return -ENOMEM;
        }
    }
    filp->private_data = data_frm;
    pr_info("Open device successfully\n");
    return 0;
}

static int dummy_release(struct inode *inode, struct file *filp) {
    struct dummy_data_frame *data_frm = NULL;
    data_frm = container_of(inode->i_cdev, struct dummy_data_frame, cdev);
    
    mutex_lock(&cdev_lock);
    if (data_frm->data) {
        kfree(data_frm->data);
        data_frm->data = NULL;
    }
    filp->private_data = NULL;
    mutex_unlock(&cdev_lock);
    pr_info("Release device successfully\n");
    return 0;
}

ssize_t dummy_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    struct dummy_data_frame *data_frm = filp->private_data;
    unsigned char *p_write = NULL;

    if (*f_pos >= data_frm->frame_size)
        return -EINVAL;
    if (*f_pos + count > data_frm->frame_size)
        count = data_frm->frame_size - *f_pos;
    p_write = data_frm->data + *f_pos;
    
    if (copy_from_user(p_write, buf, count) != 0)
        return -EFAULT;

    *f_pos += count;
    pr_info("Write %zu bytes to device\n", count);
    filp->private_data = data_frm;
    int i = 0;
    while (i < data_frm->frame_size) {
        pr_info("%c ", data_frm->data[i]);
        i++;
    }
    return count;
}

ssize_t dummy_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
    struct dummy_data_frame *data_frm = filp->private_data;
    char* p_read = NULL;
    int ret;
    if (*f_pos >= data_frm->frame_size)
        return -EINVAL;

    if (*f_pos + count > data_frm->frame_size)
        count = data_frm->frame_size - *f_pos;
    p_read = data_frm->data + *f_pos;

    ret = copy_to_user(buf, p_read, count);
    if (ret < 0)
        return -EFAULT;
    *f_pos += count;
    pr_info("Data read: %s\n", buf);
    return ret;
}

loff_t dummy_llseek(struct file *filp, loff_t offset, int whence) {
    loff_t newpos;
    struct dummy_data_frame *data_frm = filp->private_data;
    switch (whence) {
    case SEEK_SET:
        newpos = offset;
        break;
    case SEEK_CUR:
        newpos = filp->f_pos + offset;
        break;
    case SEEK_END:
        newpos = data_frm->frame_size + offset;
        break;
    default:
        return -EINVAL;
    }

    if (newpos < 0)
        return -EINVAL;
    filp->f_pos = newpos;
    pr_info("LLESK: newpos = %lld\n", filp->f_pos);
    return newpos;
}

static long dummy_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    struct dummy_data_frame *data_frm = filp->private_data;
    switch (cmd) {
        case DUMMY_CLEAR:
            memset(data_frm->data, 0, data_frm->frame_size);
            filp->f_pos = 0;
            pr_info("DUMMY_CLEAR: %s\n", data_frm->data);
            break;
        case DUMMY_RESIZE:
            krealloc(data_frm->data, arg, GFP_KERNEL);
            data_frm->frame_size = arg;
            pr_info("DUMMY_RESIZE: %d\n", data_frm->frame_size);
            break;
        case DUMMY_GETSIZE:
            copy_to_user((int*)arg, &(data_frm->frame_size), sizeof(int));
            break;
        default:
            return -ENOTTY;
    }
    return 0;
}

static const struct file_operations dummy_fops = {
    .owner =    THIS_MODULE,
    .open =     dummy_open,
    .release =  dummy_release,
    .read =     dummy_read,
    .write =    dummy_write,
    .llseek =   dummy_llseek,
    .unlocked_ioctl = dummy_ioctl,
};

static int __init dummy_init(void) {
    /* Request the kernel for DUMMY device */
    alloc_chrdev_region(&dev_num, 0, 1, DUMMY_DEVICE_NAME);
    pr_info("%s major number = %d\n", DUMMY_DEVICE_NAME, MAJOR(dev_num));

    /* Create device's class, visible in /sys/class */
    dummy_class = class_create(THIS_MODULE, DUMMY_CLASS);
    
    /* Tie file operations to the cdev */
    cdev_init(&dummy_cdev, &dummy_fops);
    dummy_cdev.owner = THIS_MODULE;

    /* Make the device line for the users to access */
    cdev_add(&dummy_cdev, dev_num, 1);

    device_create(dummy_class,
                  NULL,             /* no parant device */
                  dev_num,
                  NULL,             /* no additional data */
                  DUMMY_DEVICE_NAME);

    mutex_init(&cdev_lock);
    pr_info("%s module loaded\n", DUMMY_DEVICE_NAME);
    return 0;
}

static void __exit dummy_exit(void) {
    cdev_del(&dummy_cdev);
    device_destroy(dummy_class, dev_num);
    class_destroy(dummy_class);
    unregister_chrdev_region(dev_num, 1);
    pr_info("%s module unloaded\n", DUMMY_DEVICE_NAME);
}

module_init(dummy_init);
module_exit(dummy_exit);
MODULE_AUTHOR("Phuc Le <lethanhphuc150501@gmail.com>");
MODULE_DESCRIPTION("Dummy character driver");
MODULE_LICENSE("GPL");