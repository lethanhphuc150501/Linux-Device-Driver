#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/mod_devicetable.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/string.h>
#include <asm/uaccess.h>

#define GPIO_NAME       "gpio-expander"
#define DATA_SZ         1024

static struct class *pcf_class = NULL;
static unsigned int major;

struct pcf_dev {
    unsigned char *data;
    struct i2c_client *client;
    struct cdev cdev;
    int cur_ptr;
};

int pcf_open(struct inode *inode, struct file *filp) {
    struct pcf_dev *dev = NULL;
    dev = container_of(inode->i_cdev, struct pcf_dev, cdev);

    if (dev == NULL) {
        pr_err("Container_of did not found any valid data\n");
        return -ENODEV;
    }

    dev->cur_ptr = 0;
    filp->private_data = dev;

    dev->data = (unsigned char *) kzalloc(DATA_SZ, GFP_KERNEL);
    if (dev->data == NULL) {
        pr_err("Out of memory\n");
        return -ENOMEM;
    }
    return 0;
}

int pcf_release(struct inode *inode, struct file *filp) {
    struct pcf_dev *dev = filp->private_data;
    if (dev->data != NULL) {
        kfree(dev->data);
        dev->data = NULL;
    }
    dev->cur_ptr = 0;
    return 0;
}

ssize_t pcf_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
    struct pcf_dev *dev = filp->private_data;
    int _reg_addr;
    u8 reg_addr[2];
    struct i2c_msg msg[2];
    int ret;

    _reg_addr = dev->cur_ptr;
    reg_addr[0] = (u8) (_reg_addr >> 8);
    reg_addr[1] = (u8) (_reg_addr & 0xff);

    
    msg[0].addr = dev->client->addr;
    msg[0].flags = 0;
    msg[0].len = 2;
    msg[0].buf = reg_addr;

    msg[1].addr = dev->client->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].len = count;
    msg[1].buf = dev->data;

    if (i2c_transfer(dev->client->adapter, msg, 2) < 0)
        pr_err("i2c_transfer failed\n");
    
    ret = copy_to_user(buf, dev->data, count);
    if (ret != 0) {
        return -EIO;
    }

    dev->cur_ptr += count;
    return ret;
}

ssize_t pcf_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    struct pcf_dev *dev = filp->private_data;
    int _reg_addr;
    unsigned char tmp[2];
    struct i2c_msg msg[2];

    if (copy_from_user(dev->data, buf, count) != 0) {
        return -EFAULT;
    }

    _reg_addr = dev->cur_ptr;
    tmp[0] = (u8) (_reg_addr >> 8);
    tmp[1] = (u8) (_reg_addr & 0xff);

    msg[0].addr = dev->client->addr;
    msg[0].flags = 0;
    msg[0].len = 2;
    msg[0].buf = tmp;

    msg[1].addr = dev->client->addr;
    msg[1].flags = 0;
    msg[1].len = count;
    msg[1].buf = dev->data;

    if (i2c_transfer(dev->client->adapter, msg, 2) < 0) {
        pr_err("i2c_transfer failed\n");
        return -1;
    }
    return count;
}

struct file_operations pcf_fops = {
	.owner =    THIS_MODULE,
    .open =     pcf_open,
    .release =  pcf_release,
    .read =     pcf_read,
    .write =    pcf_write,
};

static int pcf8574_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    int err = 0;
    dev_t devno = 0;
    struct pcf_dev *pcf8574_device = NULL;
    struct device *device = NULL;
    struct i2c_msg msg[2];
    unsigned char data[5];
    u8 reg_addr[2];

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
        return -EIO;
    
    reg_addr[0] =  0x00;
    reg_addr[1] =  0x00;

    msg[0].addr = client->addr;
    msg[0].flags = 0;
    msg[0].len = 2;
    msg[0].buf = reg_addr;

    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].len = 5;
    msg[1].buf = data;

    if (i2c_transfer(client->adapter, msg, 2) < 0)
        pr_err("i2c transfer failed\n");
    
    err = alloc_chrdev_region(&devno, 0, 1, GPIO_NAME);         // Every time probe occurs, create a new major number
    if (err < 0) {
        pr_err("alloc_chrdev_region() failed for %s\n", GPIO_NAME);
        return err;
    }
    major = MAJOR(devno);

    pcf_class = class_create(THIS_MODULE, GPIO_NAME);           // Every time probe occurs, only one class is created --> if class_create() is called more than 1 time, it returns a pointer to the existed class
    if (IS_ERR(pcf_class)) {
        err = PTR_ERR(pcf_class);
        unregister_chrdev_region(MKDEV(major, 0), 1);
        return err;
    }

    pcf8574_device = (struct pcf_dev*) kzalloc(sizeof(struct pcf_dev), GFP_KERNEL);
    if (!pcf8574_device) {
        err = -ENOMEM;
        class_destroy(pcf_class);
        unregister_chrdev_region(MKDEV(major, 0), 1);
        return err;
    }
    pcf8574_device->client = client;
    pcf8574_device->data = NULL;
    
    cdev_init(&pcf8574_device->cdev, &pcf_fops);
    pcf8574_device->cdev.owner = THIS_MODULE;

    err = cdev_add(&pcf8574_device->cdev, devno, 1);
    if (err) {
        pr_err("Error while trying to add %s", GPIO_NAME);
        kfree(pcf8574_device);
        class_destroy(pcf_class);
        unregister_chrdev_region(MKDEV(major, 0), 1);
        return err;
    }

    device = device_create(pcf_class, NULL, devno, NULL, GPIO_NAME);
    if (IS_ERR(device)) {
        err = PTR_ERR(device);
        pr_err("failure while trying to create %s device", GPIO_NAME);
        cdev_del(&pcf8574_device->cdev);
        kfree(pcf8574_device);
        class_destroy(pcf_class);
        unregister_chrdev_region(MKDEV(major, 0), 1);
        return err;
    }

    i2c_set_clientdata(client, pcf8574_device);
    return 0;
}

static int pcf8574_remove(struct i2c_client *client) {
    struct pcf_dev *pcf8574_device = i2c_get_clientdata(client);
    device_destroy(pcf_class, MKDEV(major, 0));
    cdev_del(&pcf8574_device->cdev);
    kfree(pcf8574_device);
    class_destroy(pcf_class);
    unregister_chrdev_region(MKDEV(major, 0), 1);
    pr_info("pcf8574_remove successfully\n");
    return 0;
}

static const struct of_device_id pcf8574_ids[] = {
    { .compatible = "nxp,pcf8574", },
    {},
};

static const struct i2c_device_id pcf8574_id[] = {
    {"pcf8574", 0},
    {},
};

MODULE_DEVICE_TABLE(of, pcf8574_ids);
MODULE_DEVICE_TABLE(i2c, pcf8574_id);

static struct i2c_driver pcf8574_drv = {
    .probe = pcf8574_probe,
    .remove = pcf8574_remove,
    .driver = {
        .owner = THIS_MODULE,
        .name = "pcf8574",
        .of_match_table = of_match_ptr(pcf8574_ids),
    },
    .id_table = pcf8574_id,
};

module_i2c_driver(pcf8574_drv);

MODULE_AUTHOR("Phuc Le <phuc.le-thanh@banvien.com.vn>");
MODULE_LICENSE("GPL");