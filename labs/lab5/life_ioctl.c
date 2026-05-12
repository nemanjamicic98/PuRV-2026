#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>

#define MAGIC_NO 'k'
#define WR_VALUE _IOW(MAGIC_NO, 'a', int32_t*)
#define RD_VALUE _IOR(MAGIC_NO, 'b', int32_t*)

static int major;
static int32_t value = 0;
static int32_t new_state = 0;
static int data_written = 0;

static void determine_cell_new_state(void) {
    int alive_neighbour_count = 0;

    int b8 = (value >> 8) & 0x1;
    int b7 = (value >> 7) & 0x1;
    int b6 = (value >> 6) & 0x1;
    int b5 = (value >> 5) & 0x1;
    int cell = (value >> 4) & 0x1;
    int b3 = (value >> 3) & 0x1;
    int b2 = (value >> 2) & 0x1;
    int b1 = (value >> 1) & 0x1;
    int b0 = value & 0x1;

    alive_neighbour_count = b8 + b7 + b6 + b5 + b3 + b2 + b1 + b0;

    if (cell == 1) {
        new_state = (alive_neighbour_count == 2 || alive_neighbour_count == 3) ? 1 : 0;
    } else {
        new_state = (alive_neighbour_count == 3) ? 1 : 0;
    }
}

static long life_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    switch(cmd) {
        case WR_VALUE:
            if (copy_from_user(&value, (int32_t*)arg, sizeof(value))) {
                return -EFAULT;
            }
            data_written = 1;
            determine_cell_new_state();
            break;
        case RD_VALUE:
            if (!data_written) {
                return -EINVAL;
            }
            if (copy_to_user((int32_t*)arg, &new_state, sizeof(new_state))) {
                return -EFAULT;
            }
            data_written = 0;
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = life_ioctl
};

static int __init life_init(void) {
    major = register_chrdev(0, "life_service", &fops);
    return 0;
}

static void __exit life_exit(void) {
    unregister_chrdev(major, "life_service");
}

module_init(life_init);
module_exit(life_exit);
MODULE_LICENSE("GPL");