#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include "conway_driver.h"

int result;
int major;

static long conway_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int values[9];
    switch (cmd)
    {
        case WR_VALUE:
            if (copy_from_user(values, (int __user *)arg, 9 * sizeof(int)))
            {
                pr_err("conway_driver - Error copying data from userspace!\n");
                return -EFAULT;
            }

            int count = 0;
            for(int i = 0; i < 9; i++)
            {
                if (i == 4)
                {
                    continue;
                }
                count += values[i];
            }

            if (values[4])
            {
                if (count < 2 || count > 3)
                {
                    result = 0;
                }
                else
                {
                    result = 1;
                }
            }
            else
            {
                if (count == 3)
                {
                    result = 1;
                }
                else
                {
                    result = 0;
                }
            }
            
            break;
        case RD_VALUE:
            if (copy_to_user((int __user *)arg, &result, sizeof(int))) {
                return -EFAULT;
            }

            break;
        default:
            return -ENOTTY;
            break;
    }

    return 0;
}

static struct file_operations fops = {
    .unlocked_ioctl = conway_ioctl
};

static int __init conway_init(void)
{
    major = register_chrdev(0, "conway_dev", &fops);
    if (major < 0) {
        pr_err("conway_dev - Error registering chrdev\n");
        return major;
    }

    pr_info("conway_dev - Major Device Number: %d\n", major);

    return 0;
}

static void __exit conway_exit(void)
{
    unregister_chrdev(major, "conway_dev");
    pr_info("cpu_cdev - Unregistered character device\n");
}

module_init(conway_init);
module_exit(conway_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Filip");