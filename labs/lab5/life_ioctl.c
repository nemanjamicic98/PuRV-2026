#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>

#define MAGIC_NO 'k'
#define IOCTL_SET_AND_GET _IOWR(MAGIC_NO, 1, struct cell_data)

struct cell_data {
    int neighbors[9]; 
    int result;
};

static int major;
static int data_written = 0;

static long life_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct cell_data cd;
    
    if (cmd != IOCTL_SET_AND_GET) return -EINVAL;
    if (copy_from_user(&cd, (struct cell_data *)arg, sizeof(cd))) return -EFAULT;

    data_written = 1;

    int alive_neighbors = 0;
    for (int i = 0; i < 8; i++) {
        if (cd.neighbors[i] == 1) alive_neighbors++;
    }

    int current_state = cd.neighbors[8];
    if (current_state == 1) {
        cd.result = (alive_neighbors == 2 || alive_neighbors == 3) ? 1 : 0;
    } else {
        cd.result = (alive_neighbors == 3) ? 1 : 0;
    }

    if (copy_to_user((struct cell_data *)arg, &cd, sizeof(cd))) return -EFAULT;
    return 0;
}

static struct file_operations fops = { .unlocked_ioctl = life_ioctl };

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
