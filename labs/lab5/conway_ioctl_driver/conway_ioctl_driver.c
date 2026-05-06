/***************************************************************************//**
*  \file       conway_ioctl_driver.c
*
*  \details    IOCTL driver that receives 9 Conway cell states,
               calculates the next state of the center cell,
               and returns it to user space.
*
*  \author     Natasa Miljevic
*
*  \Tested with Linux raspberrypi 6.12.47+rpt-rpi-v7
*
*******************************************************************************/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include<linux/slab.h>                 //kmalloc()
#include<linux/uaccess.h>              //copy_to/from_user()
#include <linux/ioctl.h>
#include <linux/err.h>

 
#define WR_VALUE _IOW('a','a',int32_t*)
#define RD_VALUE _IOR('a','b',int32_t*)

#define ALIVE 1
#define DEAD 0

int32_t value = 0;      // ovdje cuvamo 9 celija spakovanih u bite
int32_t new_state = 0;  // novo stanje centralne celije
int data_written = 0;   // da li je user nesto prvo upisao

dev_t dev = 0;
static struct class *dev_class;
static struct cdev etx_cdev;

/*
** Function Prototypes
*/
static int      __init etx_driver_init(void);
static void     __exit etx_driver_exit(void);
static int      etx_open(struct inode *inode, struct file *file);
static int      etx_release(struct inode *inode, struct file *file);
static ssize_t  etx_read(struct file *filp, char __user *buf, size_t len,loff_t * off);
static ssize_t  etx_write(struct file *filp, const char *buf, size_t len, loff_t * off);
static long     etx_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

static void determine_cell_new_state(void);

/*
** File operation sturcture
*/
static struct file_operations fops =
{
        .owner          = THIS_MODULE,
        .read           = etx_read,
        .write          = etx_write,
        .open           = etx_open,
        .unlocked_ioctl = etx_ioctl,
        .release        = etx_release,
};

/*
** This function will be called when we open the Device file
*/
static int etx_open(struct inode *inode, struct file *file)
{
        pr_info("Device File Opened...!!!\n");
        return 0;
}

/*
** This function will be called when we close the Device file
*/
static int etx_release(struct inode *inode, struct file *file)
{
        pr_info("Device File Closed...!!!\n");
        return 0;
}

/*
** This function will be called when we read the Device file
*/
static ssize_t etx_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
        pr_info("Read Function\n");
        return 0;
}

/*
** This function will be called when we write the Device file
*/
static ssize_t etx_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
        pr_info("Write function\n");
        return len;
}

/*
** This function will be called when we write IOCTL on the Device file
*/
static long etx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
         switch(cmd) {
                case WR_VALUE:
                        if( copy_from_user(&value ,(int32_t*) arg, sizeof(value)) )
                        {
                                pr_err("Data Write : Err!\n");
                                return -EFAULT;
                        }
                        data_written = 1;
                        pr_info("Received cells = 0x%x\n", value);
                        determine_cell_new_state();
                        break;
                case RD_VALUE:
                        if(!data_written)
                        {
                                pr_err("Read requested before write!\n");
                                return -EINVAL;
                        }
                        if( copy_to_user((int32_t*) arg, &new_state, sizeof(new_state)) )
                        {
                                pr_err("Data Read : Err!\n");
                                return -EFAULT;
                        }
                        pr_info("Returned new state = %d\n", new_state);
                        break;
                default:
                        pr_info("Default\n");
                        break;
        }
        return 0;
}

static void determine_cell_new_state(void)
{
        int alive_neighbour_count = 0;

        /*
                Raspored bita:

                bit 8  bit 7  bit 6
                bit 5  bit 4  bit 3
                bit 2  bit 1  bit 0
        */

        uint8_t neighbour1 = (value >> 8) & 0x1;
        uint8_t neighbour2 = (value >> 7) & 0x1;
        uint8_t neighbour3 = (value >> 6) & 0x1;
        uint8_t neighbour4 = (value >> 5) & 0x1;
        uint8_t cell       = (value >> 4) & 0x1;
        uint8_t neighbour5 = (value >> 3) & 0x1;
        uint8_t neighbour6 = (value >> 2) & 0x1;
        uint8_t neighbour7 = (value >> 1) & 0x1;
        uint8_t neighbour8 = value & 0x1;

        alive_neighbour_count = neighbour1 + neighbour2 + neighbour3 + neighbour4 +
                                neighbour5 + neighbour6 + neighbour7 + neighbour8;
        
        if(cell == ALIVE)
        {
                if (alive_neighbour_count == 2 || alive_neighbour_count == 3)
                        new_state = ALIVE;
                else
                        new_state = DEAD;
        }
        else
        {
                if (alive_neighbour_count == 3)
                        new_state = ALIVE;
                else
                        new_state = DEAD;
        }

        pr_info("Cell = %d, alive_neighbour_count = %d, new_state = %d\n", cell, alive_neighbour_count, new_state);

}
 
/*
** Module Init function
*/
static int __init etx_driver_init(void)
{
        /*Allocating Major number*/
        if((alloc_chrdev_region(&dev, 0, 1, "etx_Dev")) <0){
                pr_err("Cannot allocate major number\n");
                return -1;
        }
        pr_info("Major = %d Minor = %d \n",MAJOR(dev), MINOR(dev));
 
        /*Creating cdev structure*/
        cdev_init(&etx_cdev,&fops);
 
        /*Adding character device to the system*/
        if((cdev_add(&etx_cdev,dev,1)) < 0){
            pr_err("Cannot add the device to the system\n");
            goto r_class;
        }
 
        /*Creating struct class*/
        if(IS_ERR(dev_class = class_create("etx_class"))){
            pr_err("Cannot create the struct class\n");
            goto r_class;
        }
 
        /*Creating device*/
        if(IS_ERR(device_create(dev_class,NULL,dev,NULL,"etx_device"))){
            pr_err("Cannot create the Device 1\n");
            goto r_device;
        }
        pr_info("Device Driver Insert...Done!!!\n");
        return 0;
 
r_device:
        class_destroy(dev_class);
r_class:
        unregister_chrdev_region(dev,1);
        return -1;
}

/*
** Module exit function
*/
static void __exit etx_driver_exit(void)
{
        device_destroy(dev_class,dev);
        class_destroy(dev_class);
        cdev_del(&etx_cdev);
        unregister_chrdev_region(dev, 1);
        pr_info("Device Driver Remove...Done!!!\n");
}
 
module_init(etx_driver_init);
module_exit(etx_driver_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("EmbeTronicX <embetronicx@gmail.com>");
MODULE_DESCRIPTION("Simple Linux device driver (IOCTL)");
MODULE_VERSION("1.5");
