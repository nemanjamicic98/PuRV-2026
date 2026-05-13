#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

#define MAGIC_NO 'k'
struct cell_data {
    int neighbors[9];
    int result;
};
#define IOCTL_SET_AND_GET _IOWR(MAGIC_NO, 1, struct cell_data)
#define IOCTL_INC_GEN _IO(MAGIC_NO, 2) 

static int major;
static unsigned int total_alive = 0;
static unsigned int total_born = 0;
static unsigned int total_died = 0;
static unsigned int generations = 0;

static ssize_t total_alive_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "%u\n", total_alive);
}
static ssize_t total_born_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "%u\n", total_born);
}
static ssize_t total_died_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "%u\n", total_died);
}
static ssize_t generations_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "%u\n", generations);
}

static struct kobj_attribute alive_attr = __ATTR_RO(total_alive);
static struct kobj_attribute born_attr = __ATTR_RO(total_born);
static struct kobj_attribute died_attr = __ATTR_RO(total_died);
static struct kobj_attribute gen_attr = __ATTR_RO(generations);

static struct attribute *life_attrs[] = {
    &alive_attr.attr, &born_attr.attr, &died_attr.attr, &gen_attr.attr, NULL,
};
static struct attribute_group attr_group = { .attrs = life_attrs };
static struct kobject *life_kobj;

static long life_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct cell_data cd;
    
    if (cmd == IOCTL_INC_GEN) {
        generations++;
        return 0;
    }

    if (cmd != IOCTL_SET_AND_GET) return -EINVAL;
    if (copy_from_user(&cd, (struct cell_data *)arg, sizeof(cd))) return -EFAULT;

    int alive_neighbors = 0;
    int i;
    for (i = 0; i < 8; i++) if (cd.neighbors[i] == 1) alive_neighbors++;

    int current_state = cd.neighbors[8];
    if (current_state == 1) {
        cd.result = (alive_neighbors == 2 || alive_neighbors == 3) ? 1 : 0;
        if (cd.result == 0) total_died++;
    } else {
        cd.result = (alive_neighbors == 3) ? 1 : 0;
        if (cd.result == 1) total_born++;
    }

    if (copy_to_user((struct cell_data *)arg, &cd, sizeof(cd))) return -EFAULT;
    return 0;
}

static struct file_operations fops = { .unlocked_ioctl = life_ioctl, .open = (void*)0 };

static int __init life_init(void) {
    major = register_chrdev(0, "life_service", &fops);
    life_kobj = kobject_create_and_add("life_stats", kernel_kobj);
    sysfs_create_group(life_kobj, &attr_group);
    return 0;
}

static void __exit life_exit(void) {
    kobject_put(life_kobj);
    unregister_chrdev(major, "life_service");
}

module_init(life_init);
module_exit(life_exit);
MODULE_LICENSE("GPL");
