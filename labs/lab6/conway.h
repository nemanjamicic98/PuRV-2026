#ifndef CONWAY_IOCTL_H
#define CONWAY_IOCTL_H

#include <linux/ioctl.h>

// Use a unique magic number for your driver, 'C' is just an example
#define CONWAY_MAGIC 'C' 

// We are passing an array of 9 ints
#define COMPUTE_CELL _IOWR(CONWAY_MAGIC, 1, int[9])

#define NUM_OF_ROWS 10
#define NUM_OF_COLUMNS 10

typedef struct perfect_cell {
    int row;
    int col;

    char new_state;
    char old_state;

    char is_finished;
} perfect_cell;

#endif
