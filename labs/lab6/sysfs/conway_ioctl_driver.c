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

static int32_t value     = 0;   /* primljenih 9 celija spakovanih u bite   */
static int32_t new_state = 0;   /* izracunato novo stanje centralne celije */
static int data_written  = 0;   /* fleg: je li korisnik vec nesto upisao   */

/* -------------------------------------------------------------------
 * Statistika - sadrzi 4 varijable koje mijenja game_of_life.c
 * pisanjem stringa u /dev/etx_device.
 * Sysfs ih izlaze kao zasebne fajlove u /sys/
 * ------------------------------------------------------------------- */
static int stat_generations = 0;  /* broj generacije                 */
static int stat_alive       = 0;  /* ukupno zivih celija             */
static int stat_born        = 0;  /* celija koje su ozivjele         */
static int stat_died        = 0;  /* celija koje su umrle            */

static dev_t            dev;
static struct class    *dev_class;
static struct cdev      etx_cdev;
static struct device  *etx_device; /* cuvam pokazivac za sysfs */

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

/* ===================================================================
 * SYSFS - jedan show() po atributu, svaki vraca jedan broj
 *
 * Nakon insmod, fajlovi ce biti vidljivi na:
 *   /sys/class/etx_class/etx_device/generations
 *   /sys/class/etx_class/etx_device/alive_cells
 *   /sys/class/etx_class/etx_device/born_cells
 *   /sys/class/etx_class/etx_device/died_cells
 *
 * Citanje: cat /sys/class/etx_class/etx_device/generations
 * =================================================================== */

 static ssize_t generations_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", stat_generations);
}
 
static ssize_t alive_cells_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", stat_alive);
}
 
static ssize_t born_cells_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", stat_born);
}
 
static ssize_t died_cells_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", stat_died);
}
 
/*
 * DEVICE_ATTR_RO kreira atribut koji se moze SAMO citati (read-only).
 * Ime funkcije mora biti oblika: <ime>_show
 */
static DEVICE_ATTR_RO(generations);
static DEVICE_ATTR_RO(alive_cells);
static DEVICE_ATTR_RO(born_cells);
static DEVICE_ATTR_RO(died_cells);
 
/* ===================================================================
 * KARAKTER DRIVER - open, release, read, write, ioctl
 * =================================================================== */


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

/* read/write nisu potrebni za IOCTL driver - ostavljam prazne */
static ssize_t etx_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    pr_info("conway: Read function (unused)\n");
    return 0;
}

/*
 * write - game_of_life.c pise string oblika:
 *   "generacije zive rodjene umrle\n"
 *   npr. "5 23 3 7\n"
 *
 * Parsiramo sscanf-om i azuriramo 4 statisticke varijable.
 */
static ssize_t etx_write(struct file *filp, const char __user *buf,
                         size_t len, loff_t *off)
{
    char kbuf[64] = {0};
 
    if (len > sizeof(kbuf) - 1)
        len = sizeof(kbuf) - 1;
 
    if (copy_from_user(kbuf, buf, len)) {
        pr_err("conway: copy_from_user greska u write\n");
        return -EFAULT;
    }
 
    /* Parsiramo 4 broja iz stringa */
    sscanf(kbuf, "%d %d %d %d",
           &stat_generations, &stat_alive, &stat_born, &stat_died);
 
    pr_info("conway: stats: gen=%d alive=%d born=%d died=%d\n",
            stat_generations, stat_alive, stat_born, stat_died);
 
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

    etx_device = device_create(dev_class, NULL, dev, NULL, "etx_device");
    if (IS_ERR(etx_device)) {
        pr_err("conway: ne mogu kreirati device\n");
        goto r_device;
    }

    /*
     * Kreiramo sysfs fajlove.
     * device_create_file() kreira jedan fajl u /sys/class/etx_class/etx_device/
     * Ako nesto ne uspije, brise se ono sto smo vec kreirali.
     */
    if (device_create_file(etx_device, &dev_attr_generations) ||
        device_create_file(etx_device, &dev_attr_alive_cells)  ||
        device_create_file(etx_device, &dev_attr_born_cells)   ||
        device_create_file(etx_device, &dev_attr_died_cells)) {
        pr_err("conway: ne mogu kreirati sysfs atribute\n");
        goto r_sysfs;
    }

    pr_info("conway: driver ucitan, /dev/etx_device kreiran\n");
    pr_info("conway: sysfs: /sys/class/etx_class/etx_device/\n");
    return 0;

    r_sysfs:
        device_remove_file(etx_device, &dev_attr_generations);
        device_remove_file(etx_device, &dev_attr_alive_cells);
        device_remove_file(etx_device, &dev_attr_born_cells);
        device_remove_file(etx_device, &dev_attr_died_cells);
        device_destroy(dev_class, dev);
    r_device:
        class_destroy(dev_class);
    r_class:
        unregister_chrdev_region(dev, 1);
    return -1;
}

static void __exit etx_driver_exit(void)
{
    device_remove_file(etx_device, &dev_attr_generations);
    device_remove_file(etx_device, &dev_attr_alive_cells);
    device_remove_file(etx_device, &dev_attr_born_cells);
    device_remove_file(etx_device, &dev_attr_died_cells);
    
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