#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>

#include "init.h"
#include "../../conway.h"

#ifndef COMPUTE_CELL
#define CONWAY_MAGIC 'C'
#define COMPUTE_CELL _IOWR(CONWAY_MAGIC, 1, int[9])
#endif

pthread_t threads[NUM_OF_ROWS][NUM_OF_COLUMNS];
perfect_cell state[NUM_OF_ROWS][NUM_OF_COLUMNS];

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;

int num_finished = 0;
int generation = 0;
int sleep_seconds = 1;
int dev_fd; 

void sync_to_driver(void)
{
    if (write(dev_fd, state, sizeof(state)) < 0) {
        perror("Failed to sync state to driver");
    }
}

void print_cells(void)
{
    perfect_cell temp_state[NUM_OF_ROWS][NUM_OF_COLUMNS];
    
    int fd = open("/dev/conway_dev", O_RDONLY);
    if (fd < 0) {
        perror("print_cells: Failed to open driver");
        return;
    }
    
    if (read(fd, temp_state, sizeof(temp_state)) < 0) {
        perror("print_cells: Failed to read from driver");
    }
    close(fd);

    for (int r = 0; r < NUM_OF_ROWS; r++)
    {
        printf("[");
        for (int c = 0; c < NUM_OF_COLUMNS; c++)
        {
            printf("%c", temp_state[r][c].new_state ? '*' : '.');
            if (c != NUM_OF_COLUMNS - 1)
            {
                printf("|");
            }
        }
        printf("]\n");
    }
    printf("\n");
    fflush(stdout);
}

void evolve(perfect_cell *cell)
{
    int neighbors[9] = {0};
    int i = 0;

    for (int r = -1; r <= 1; r++)
    {
        for (int c = -1; c <= 1; c++)
        {
            int nr = cell->row + r;
            int nc = cell->col + c;

            if (nr < 0 || nr >= NUM_OF_ROWS || nc < 0 || nc >= NUM_OF_COLUMNS)
            {
                neighbors[i++] = 0; 
            }
            else
            {
                neighbors[i++] = state[nr][nc].old_state;
            }
        }
    }

    if (ioctl(dev_fd, COMPUTE_CELL, neighbors) < 0) {
        perror("IOCTL failed compute");
    }

    cell->new_state = neighbors[0];
}

void reset_for_next_generation(void)
{
    for (int r = 0; r < NUM_OF_ROWS; r++)
    {
        for (int c = 0; c < NUM_OF_COLUMNS; c++)
        {
            state[r][c].old_state = state[r][c].new_state;
            state[r][c].is_finished = 0;
        }
    }

    num_finished = 0;
    generation++;
}

void *run(void *arg)
{
    perfect_cell *cell = (perfect_cell *)arg;

    while (1)
    {
        evolve(cell);
        
        pthread_mutex_lock(&mutex);

        cell->is_finished = 1;
        num_finished++;

        int my_generation = generation;

        if (num_finished == NUM_OF_ROWS * NUM_OF_COLUMNS)
        {
            sync_to_driver();
            print_cells();
            sleep(sleep_seconds);

            reset_for_next_generation();

            pthread_cond_broadcast(&condition);
        }
        else
        {
            while (generation == my_generation)
            {
                pthread_cond_wait(&condition, &mutex);
            }
        }

        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}

void start_cells(void)
{
    for (int r = 0; r < NUM_OF_ROWS; r++)
    {
        for (int c = 0; c < NUM_OF_COLUMNS; c++)
        {
            state[r][c].row = r;
            state[r][c].col = c;
            
            if (pthread_create(&threads[r][c], NULL, run, &state[r][c]) != 0)
            {
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }
        }
    }

    for (int r = 0; r < NUM_OF_ROWS; r++)
    {
        for (int c = 0; c < NUM_OF_COLUMNS; c++)
        {
            pthread_join(threads[r][c], NULL);
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        sleep_seconds = atoi(argv[1]);
    }

    dev_fd = open("/dev/conway_dev", O_RDWR);
    if (dev_fd < 0) {
        perror("Failed to open /dev/conway_dev");
        return EXIT_FAILURE;
    }

    printf("Initial state:\n\n");

    if (argc > 2)
    {
        char path[256] = "examples/"; 
        strncat(path, argv[2], sizeof(path) - strlen(path) - 1);
        
        if (init_cells(path)) {
            close(dev_fd);
            return 1;
        }
    }
    else
    {
        if (init_cells("examples/initial.txt")) {
            close(dev_fd);
            return 1;
        }
    }

    sync_to_driver();
    print_cells();

    sleep(sleep_seconds);

    reset_for_next_generation();

    start_cells();

    close(dev_fd);
    return 0;
}