/***************************************************************************//**
*  \file       conway_driver.c
*
*  \details    Conway Game of Life character driver with IOCTL and procfs.
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
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/string.h>

#define HEIGHT 10
#define WIDTH  10
#define ALIVE 1
#define DEAD 0

#define PROC_NAME "conway_stats"

struct cells_table
{
     char cells[HEIGHT][WIDTH];   
};

struct cell_position
{
        int row;
        int column;
};

#define IOCTL_SET_CELL_STATES   _IOW('a','a', struct cells_table *) 
#define IOCTL_EVOLVE_CELL       _IOW('a', 'b', struct cell_position *)
#define IOCTL_FINISH_GENERATION _IO('a','c')
#define IOCTL_GET_CELL_STATES   _IOR('a','d', struct cells_table *)

static char current_states[HEIGHT][WIDTH];
static char next_states[HEIGHT][WIDTH];

static int cells_table_loaded = 0;

static int alive_cells_count = 0;
static int born_cells = 0;
static int died_cells = 0;
static int generation_count = 0;

static DEFINE_MUTEX(conway_lock);

dev_t dev = 0;
static struct class *dev_class;
static struct cdev conway_device;

/*
** Function Prototypes
*/
static int      __init conway_driver_init(void);
static void     __exit conway_driver_exit(void);
static int      conway_open(struct inode *inode, struct file *file);
static int      conway_release(struct inode *inode, struct file *file);
static ssize_t  conway_read(struct file *filp, char __user *buf, size_t len,loff_t * off);
static ssize_t  conway_write(struct file *filp, const char __user *buf, size_t len, loff_t * off);
static long     conway_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int      determine_cell_new_state(int cell_state, int alive_neighbour_count);

/*
** File operation sturcture
*/
static struct file_operations fops =
{
        .owner          = THIS_MODULE,
        .read           = conway_read,
        .write          = conway_write,
        .open           = conway_open,
        .unlocked_ioctl = conway_ioctl,
        .release        = conway_release,
};

/*
** This function will be called when we open the Device file
*/
static int conway_open(struct inode *inode, struct file *file)
{
        pr_info("conway_driver: device opened\n");
        return 0;
}

/*
** This function will be called when we close the Device file
*/
static int conway_release(struct inode *inode, struct file *file)
{
        pr_info("conway_driver: device closed\n");
        return 0;
}

/*
** This function will be called when we read the Device file
*/
static ssize_t conway_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
        pr_info("conway_driver: read called\n");
        return 0;
}

/*
** This function will be called when we write the Device file
*/
static ssize_t conway_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
        pr_info("conway_driver: write called\n");
        return len;
}

int alive_neighbours(int row, int column)
{
    int alive_neighbour_count = 0;

    if (row == 0)
    {
        if (column == 0)
            alive_neighbour_count = current_states[row][column+1] + current_states[row+1][column] + current_states[row+1][column+1];
        else if (column == (WIDTH - 1))
            alive_neighbour_count = current_states[row][WIDTH-2] + current_states[row+1][WIDTH-2] + current_states[row+1][WIDTH-1];
        else
            alive_neighbour_count = current_states[row][column-1] + current_states[row][column+1] + current_states[row+1][column-1] +
                                    current_states[row+1][column] + current_states[row+1][column+1];
    }
    else if (row == (HEIGHT-1))
    {
        if (column == 0)
            alive_neighbour_count = current_states[row][column+1] + current_states[row-1][column] + current_states[row-1][column+1];
        else if (column == (WIDTH - 1))
            alive_neighbour_count = current_states[row][WIDTH-2] + current_states[row-1][WIDTH-2] + current_states[row-1][WIDTH-1];
        else
            alive_neighbour_count = current_states[row][column-1] + current_states[row][column+1] + current_states[row-1][column-1] +
                                    current_states[row-1][column] + current_states[row-1][column+1];
    }
    else if (row != 0 && row != (HEIGHT-1) && column == 0)
        alive_neighbour_count = current_states[row-1][column] + current_states[row-1][column+1] + current_states[row][column+1] +
                                current_states[row+1][column] + current_states[row+1][column+1];
    else if (row != 0 && row != (HEIGHT-1) && column == (WIDTH-1))
        alive_neighbour_count = current_states[row-1][column] + current_states[row-1][column-1] + current_states[row][column-1] +
                                current_states[row+1][column] + current_states[row+1][column-1];
    else
        alive_neighbour_count = current_states[row][column-1] + current_states[row][column+1] + 
                                current_states[row-1][column-1] + current_states[row-1][column] + current_states[row-1][column+1] +
                                current_states[row+1][column-1] + current_states[row+1][column] + current_states[row+1][column+1];
    return alive_neighbour_count;
}

int determine_cell_new_state(int cell_state, int alive_neighbour_count)
{
    int cell_new_state = 0;

    if (cell_state)
    {
        if (alive_neighbour_count == 2 || alive_neighbour_count == 3)
            cell_new_state = 1;
        else
            cell_new_state = 0;
    }
    else
    {
        if(alive_neighbour_count == 3)
            cell_new_state = 1;
        else
            cell_new_state = 0;
    }

    return cell_new_state;
}

void update_initial_alive_count(void)
{
        int i,j;
        alive_cells_count = 0;
        for (i = 0; i < HEIGHT; i++) 
        {
                for (j = 0; j < WIDTH; j++) 
                {
                        if (current_states[i][j] == ALIVE)
                                alive_cells_count++;
                }
        }      
}

void finish_generation(void)
{
        int i, j;
        int current_alive = 0;
        born_cells = 0;
        died_cells = 0;

        for (i = 0; i < HEIGHT; i++) 
        {
                for (j = 0; j < WIDTH; j++)
                {
                        if (current_states[i][j] == DEAD && next_states[i][j] == ALIVE)
                                born_cells++;

                        if (current_states[i][j] == ALIVE && next_states[i][j] == DEAD)
                                died_cells++;

                        if (next_states[i][j] == ALIVE)
                                current_alive++;

                        current_states[i][j] = next_states[i][j];
                }
        }

        alive_cells_count = current_alive;
        generation_count++;
}

ssize_t conway_proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
        char kbuf[256];
        int len;

        if(*ppos > 0)
                return 0;
        
        mutex_lock(&conway_lock);
                len = snprintf(kbuf, sizeof(kbuf), 
                                "Total alive cells: %d\n"
                                "Born cells: %d\n"
                                "Died cells: %d\n"
                                "Generation count: %d\n",
                                alive_cells_count,
                                born_cells,
                                died_cells,
                                generation_count);
        mutex_unlock(&conway_lock);
     
        if(copy_to_user(buf, kbuf, len))
                return -EFAULT;
        
        *ppos = len;

        return len;
}

static const struct proc_ops conway_proc_ops = 
{
        .proc_read = conway_proc_read,
};



/*
** This function will be called when we write IOCTL on the Device file
*/
static long conway_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

        struct cells_table table;
        struct cell_position pos;
        int alive_neighbour_count;

        switch(cmd)
        {
                case IOCTL_SET_CELL_STATES:
                        if(copy_from_user(&table, (struct cells_table __user *)arg,sizeof(table)))
                                return -EFAULT;
                        mutex_lock(&conway_lock);
                                memcpy(current_states, table.cells, sizeof(current_states));
                                memset(next_states, 0, sizeof(next_states));

                                cells_table_loaded = 1;
                                born_cells = 0;
                                died_cells = 0;
                                generation_count = 0;
                                update_initial_alive_count();

                        mutex_unlock(&conway_lock);

                        pr_info("conway_driver: initial table set\n");
                        break;
                
                case IOCTL_EVOLVE_CELL:
                        if(copy_from_user(&pos, (struct cell_position __user *)arg, sizeof(pos)))
                                return -EFAULT;
                        if(pos.row < 0 || pos.row >= HEIGHT || pos.column < 0 || pos.column >= WIDTH)
                                return -EINVAL;
                        
                        mutex_lock(&conway_lock);
                                if(!cells_table_loaded)
                                {
                                        mutex_unlock(&conway_lock);
                                        return -EINVAL;
                                }

                                alive_neighbour_count = alive_neighbours(pos.row, pos.column);
                                next_states[pos.row][pos.column] = 
                                determine_cell_new_state(current_states[pos.row][pos.column], 
                                                         alive_neighbour_count);
                        mutex_unlock(&conway_lock);
                        break;

                case IOCTL_FINISH_GENERATION:
                        mutex_lock(&conway_lock);
                                if(!cells_table_loaded)
                                {
                                        mutex_unlock(&conway_lock);
                                        return -EINVAL;
                                }
                                finish_generation();
                        mutex_unlock(&conway_lock);

                        pr_info("conway_driver: generation mutations finished\n");
                        break;
                
                case IOCTL_GET_CELL_STATES:
                        mutex_lock(&conway_lock);
                                if(!cells_table_loaded)
                                {
                                        mutex_unlock(&conway_lock);
                                        return -EINVAL;
                                }
                                memcpy(table.cells, current_states, sizeof(current_states));
                        mutex_unlock(&conway_lock);
                        
                        if(copy_to_user((struct cells_table __user *)arg, &table, sizeof(table)))
                                return -EFAULT;
                        break;

                default:
                        pr_info("conway_driver: unknown ioctl command\n");
                        break;
        }
        return 0;
}

 
/*
** Module Init function
*/
static int __init conway_driver_init(void)
{
        /*Allocating Major number*/
        if((alloc_chrdev_region(&dev, 0, 1, "conway_Dev")) <0){
                pr_err("Cannot allocate major number\n");
                return -1;
        }
        pr_info("Major = %d Minor = %d \n",MAJOR(dev), MINOR(dev));
 
        /*Creating cdev structure*/
        cdev_init(&conway_device,&fops);
 
        /*Adding character device to the system*/
        if((cdev_add(&conway_device,dev,1)) < 0){
            pr_err("Cannot add the device to the system\n");
            goto r_class;
        }
 
        /*Creating struct class*/
        if(IS_ERR(dev_class = class_create("conway_class"))){
            pr_err("Cannot create the struct class\n");
            goto r_class;
        }
 
        /*Creating device*/
        if(IS_ERR(device_create(dev_class,NULL,dev,NULL,"conway_device"))){
            pr_err("Cannot create the Device 1\n");
            goto r_device;
        }

        if (!proc_create(PROC_NAME, 0444, NULL, &conway_proc_ops)) 
        {
                pr_err("conway_driver: cannot create proc entry\n");
                goto r_proc;
        }

        pr_info("Device Driver Insert...Done!!!\n");
        return 0;

        r_proc:
                device_destroy(dev_class, dev);
        r_device:
                class_destroy(dev_class);
        r_class:
                cdev_del(&conway_device);
                unregister_chrdev_region(dev,1);
                return -1;
}

/*
** Module exit function
*/
static void __exit conway_driver_exit(void)
{
        remove_proc_entry(PROC_NAME, NULL);
        device_destroy(dev_class,dev);
        class_destroy(dev_class);
        cdev_del(&conway_device);
        unregister_chrdev_region(dev, 1);
        pr_info("Device Driver Remove...Done!!!\n");
}
 
module_init(conway_driver_init);
module_exit(conway_driver_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Natasa Miljevic");
MODULE_DESCRIPTION("Conway Game of Life character driver with IOCTL and procfs");
MODULE_VERSION("1.0");
