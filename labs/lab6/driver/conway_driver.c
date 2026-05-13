#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/vmalloc.h>
#include <linux/atomic.h>    

#include "../conway.h"

int major;
perfect_cell cell_states[NUM_OF_ROWS][NUM_OF_COLUMNS];

static DEFINE_MUTEX(conway_lock);

static unsigned long current_gen = 0;
static unsigned long total_alive = 0;
static unsigned long died_last_gen = 0;
static unsigned long born_last_gen = 0;

static atomic_t device_opened = ATOMIC_INIT(0);

static struct proc_dir_entry *conway_proc_entry;

static int conway_open(struct inode *inode, struct file *file)
{
    atomic_inc(&device_opened);
    return 0;
}

static int conway_release(struct inode *inode, struct file *file)
{
    if (atomic_dec_and_test(&device_opened)) {
        mutex_lock(&conway_lock);
        
        current_gen = 0;
        total_alive = 0;
        died_last_gen = 0;
        born_last_gen = 0;
        
        mutex_unlock(&conway_lock);
        pr_info("conway_dev - All processes closed. Generation stats reset.\n");
    }
    return 0;
}

static long conway_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int values[9];
    int local_result; 
    int count = 0;
    int i;

    switch (cmd)
    {
        case COMPUTE_CELL: 
            if (copy_from_user(values, (int __user *)arg, 9 * sizeof(int)))
            {
                pr_err("conway_driver - Error copying data from userspace!\n");
                return -EFAULT;
            }

            for(i = 0; i < 9; i++)
            {
                if (i == 4) continue;
                count += values[i];
            }

            if (values[4])
            {
                local_result = (count < 2 || count > 3) ? 0 : 1;
            }
            else
            {
                local_result = (count == 3) ? 1 : 0;
            }
            
            if (copy_to_user((int __user *)arg, &local_result, sizeof(int))) {
                return -EFAULT;
            }
            break;

        default:
            return -ENOTTY;
    }

    return 0;
}

static ssize_t conway_read(struct file *filp, char __user *buf, size_t count, loff_t *off)
{
    size_t len = sizeof(cell_states);
    ssize_t ret;

    if (*off >= len)
        return 0;

    if (count > len - *off)
        count = len - *off;

    if (mutex_lock_interruptible(&conway_lock))
        return -ERESTARTSYS;

    if (copy_to_user(buf, (char *)cell_states + *off, count)) {
        ret = -EFAULT;
    } else {
        *off += count;
        ret = count;
    }

    mutex_unlock(&conway_lock);
    return ret;
}

static ssize_t conway_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
    ssize_t ret;
    int r, c;
    unsigned long new_alive = 0, died = 0, born = 0;
    
    perfect_cell (*new_states)[NUM_OF_COLUMNS];

    if (len != sizeof(cell_states)) {
        pr_err("Pogrešna veličina podataka!\n");
        return -EINVAL;
    }

    new_states = vmalloc(sizeof(cell_states));
    if (!new_states)
        return -ENOMEM;

    if (copy_from_user(new_states, buf, sizeof(cell_states))) {
        pr_err("Greška prilikom kopiranja iz user space-a\n");
        vfree(new_states);
        return -EFAULT;
    }

    if (mutex_lock_interruptible(&conway_lock)) {
        vfree(new_states);
        return -ERESTARTSYS;
    }

    for (r = 0; r < NUM_OF_ROWS; r++) {
        for (c = 0; c < NUM_OF_COLUMNS; c++) {
            int old_alive = cell_states[r][c].new_state;
            int new_alive_cell = new_states[r][c].new_state;

            if (new_alive_cell)
                new_alive++;

            if (old_alive && !new_alive_cell)
                died++;
            else if (!old_alive && new_alive_cell)
                born++;
        }
    }

    current_gen++;
    total_alive = new_alive;
    died_last_gen = died;
    born_last_gen = born;

    memcpy(cell_states, new_states, sizeof(cell_states));
    ret = len;

    mutex_unlock(&conway_lock);
    vfree(new_states);
    
    return ret;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = conway_open,
    .release = conway_release,
    .unlocked_ioctl = conway_ioctl,
    .read = conway_read,
    .write = conway_write
};

static int conway_proc_show(struct seq_file *m, void *v)
{
    mutex_lock(&conway_lock);
    seq_printf(m, "Current Generation: %lu\n", current_gen);
    seq_printf(m, "Alive Cells: %lu\n", total_alive);
    seq_printf(m, "Died Since Last Gen: %lu\n", died_last_gen);
    seq_printf(m, "Born Since Last Gen: %lu\n", born_last_gen);
    mutex_unlock(&conway_lock);
    
    return 0;
}

static int conway_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, conway_proc_show, NULL);
}

static const struct proc_ops conway_proc_ops = {
    .proc_open    = conway_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static int __init conway_init(void)
{
    major = register_chrdev(0, "conway_dev", &fops);
    if (major < 0) {
        pr_err("conway_dev - Error registering chrdev\n");
        return major;
    }

    conway_proc_entry = proc_create("conway_stats", 0444, NULL, &conway_proc_ops);
    if (!conway_proc_entry) {
        pr_err("conway_dev - Error creating proc entry\n");
        unregister_chrdev(major, "conway_dev");
        return -ENOMEM;
    }

    pr_info("conway_dev - Major Device Number: %d\n", major);
    pr_info("conway_dev - Procfs entry created at /proc/conway_stats\n");
    return 0;
}

static void __exit conway_exit(void)
{
    if (conway_proc_entry)
        remove_proc_entry("conway_stats", NULL);

    unregister_chrdev(major, "conway_dev");
    pr_info("conway_dev - Unregistered character device\n");
}

module_init(conway_init);
module_exit(conway_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Filip");