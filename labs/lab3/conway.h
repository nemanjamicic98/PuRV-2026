#ifndef CONWAY_H
#define CONWAY_H

#include <pthread.h>

#define NUM_OF_ROWS 10
#define NUM_OF_COLUMNS 10
#define TOTAL_CELLS (NUM_OF_ROWS * NUM_OF_COLUMNS)

typedef struct perfect_cell
{
    pthread_t thread;
    int row;
    int column;

    int is_alive;
    int old_state;

    int is_finished;
} perfect_cell;

#endif