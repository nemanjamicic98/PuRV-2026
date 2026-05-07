#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/err.h>

#define WR_VALUE _IOW('a', 'a', int32_t*)   /* korisnik salje 9 celija */
#define RD_VALUE _IOR('a', 'b', int32_t*)   /* korisnik prima novo stanje */

#define ALIVE 1
#define DEAD  0

static int32_t value     = 0;   /* primljenih 9 celija spakovanih u bite */
static int32_t new_state = 0;   /* izracunato novo stanje centralne celije */
static int data_written  = 0;   /* zastava: je li korisnik vec nesto upisao */

static dev_t            dev;
static struct class    *dev_class;
static struct cdev      etx_cdev;

static int  __init etx_driver_init(void);
static void __exit etx_driver_exit(void);
static int  etx_open(struct inode *inode, struct file *file);
static int  etx_release(struct inode *inode, struct file *file);
static ssize_t etx_read(struct file *filp, char __user *buf, size_t len, loff_t *off);
static ssize_t etx_write(struct file *filp, const char *buf, size_t len, loff_t *off);
static long etx_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static void determine_cell_new_state(void);

static struct file_operations fops = {
    .owner          = THIS_MODULE,
    .read           = etx_read,
    .write          = etx_write,
    .open           = etx_open,
    .unlocked_ioctl = etx_ioctl,
    .release        = etx_release,
};

static int etx_open(struct inode *inode, struct file *file)
{
    pr_info("conway: Device File Opened\n");
    return 0;
}

static int etx_release(struct inode *inode, struct file *file)
{
    data_written = 0;
    pr_info("conway: Device File Closed, state reset\n");
    return 0;
}

/* read/write nisu potrebni za IOCTL driver — ostavljam prazne */
static ssize_t etx_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    pr_info("conway: Read function (unused)\n");
    return 0;
}

static ssize_t etx_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
    pr_info("conway: Write function (unused)\n");
    return len;
}

/*
 * GOL logika: raspakuj bite iz value, prebroji zive susjede, primijeni pravila.
 *
 * Raspored bita u value:
 *   bit8=NW  bit7=N   bit6=NE
 *   bit5=W   bit4=C   bit3=E      C = centralna celija
 *   bit2=SW  bit1=S   bit0=SE
 */
static void determine_cell_new_state(void)
{
    int alive_count = 0;

    uint8_t nw = (value >> 8) & 0x1;
    uint8_t n  = (value >> 7) & 0x1;
    uint8_t ne = (value >> 6) & 0x1;
    uint8_t w  = (value >> 5) & 0x1;
    uint8_t c  = (value >> 4) & 0x1;   /* centralna celija */
    uint8_t e  = (value >> 3) & 0x1;
    uint8_t sw = (value >> 2) & 0x1;
    uint8_t s  = (value >> 1) & 0x1;
    uint8_t se = (value >> 0) & 0x1;

    alive_count = nw + n + ne + w + e + sw + s + se;  /* bez centralne */

    /* Pravila igre zivota */
    if (c == ALIVE) {
        new_state = (alive_count == 2 || alive_count == 3) ? ALIVE : DEAD;
    } else {
        new_state = (alive_count == 3) ? ALIVE : DEAD;
    }

    pr_info("conway: centralna=%d, susjeda=%d, novo_stanje=%d\n",
            c, alive_count, new_state);
}

static long etx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {

    case WR_VALUE:
        /* Primamo 9 celija spakovanih u int32_t */
        if (copy_from_user(&value, (int32_t *)arg, sizeof(value))) {
            pr_err("conway: copy_from_user greska\n");
            return -EFAULT;
        }
        data_written = 1;
        pr_info("conway: primljeno value=0x%x\n", value);
        determine_cell_new_state();  /* izracunaj odmah pri upisu */
        break;

    case RD_VALUE:
        /* Detektujemo: citanje bez prethodnog pisanja */
        if (!data_written) {
            pr_err("conway: READ bez prethodnog WRITE!\n");
            return -EINVAL;
        }
        if (copy_to_user((int32_t *)arg, &new_state, sizeof(new_state))) {
            pr_err("conway: copy_to_user greska\n");
            return -EFAULT;
        }
        
        pr_info("conway: vraca new_state=%d\n", new_state);
        break;

    default:
        pr_info("conway: nepoznata komanda\n");
        break;
    }

    return 0;
}

static int __init etx_driver_init(void)
{
    if (alloc_chrdev_region(&dev, 0, 1, "etx_Dev") < 0) {
        pr_err("conway: ne mogu alocirati major broj\n");
        return -1;
    }
    pr_info("conway: Major=%d Minor=%d\n", MAJOR(dev), MINOR(dev));

    cdev_init(&etx_cdev, &fops);

    if (cdev_add(&etx_cdev, dev, 1) < 0) {
        pr_err("conway: ne mogu dodati device\n");
        goto r_class;
    }

    if (IS_ERR(dev_class = class_create("etx_class"))) {
        pr_err("conway: ne mogu kreirati klasu\n");
        goto r_class;
    }

    if (IS_ERR(device_create(dev_class, NULL, dev, NULL, "etx_device"))) {
        pr_err("conway: ne mogu kreirati device\n");
        goto r_device;
    }

    pr_info("conway: driver ucitan, /dev/etx_device kreiran\n");
    return 0;

    r_device:
        class_destroy(dev_class);
    r_class:
        unregister_chrdev_region(dev, 1);

    return -1;
}

static void __exit etx_driver_exit(void)
{
    device_destroy(dev_class, dev);
    class_destroy(dev_class);
    cdev_del(&etx_cdev);
    unregister_chrdev_region(dev, 1);
    pr_info("conway: driver uklonjen\n");
}

module_init(etx_driver_init);
module_exit(etx_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dejan <dejan.tomic.dev@gmail.com>");
MODULE_DESCRIPTION("Conway Game of Life IOCTL driver");
MODULE_VERSION("1.5");