/***************************************************************************//**
*  \file       main.c
*
*  \details    Userspace Conway Game of Life application using pthreads and IOCTL.
*
*  \author     Natasa Miljevic
*
*  \Tested with Linux raspberrypi 6.12.47+rpt-rpi-v7
*
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <pthread.h>
 
#define HEIGHT 10
#define WIDTH  10

#define DEVICE_PATH "/dev/conway_device"
#define PROC_PATH   "/proc/conway_stats"

struct cells_table
{
     char cells[HEIGHT][WIDTH];   
};

struct cell_position
{
        int row;
        int column;
};

#define IOCTL_SET_CELL_STATES   _IOW('a', 'a', struct cells_table *) 
#define IOCTL_EVOLVE_CELL       _IOW('a', 'b', struct cell_position *)
#define IOCTL_FINISH_GENERATION _IO ('a', 'c')
#define IOCTL_GET_CELL_STATES   _IOR('a', 'd', struct cells_table *)

int fd = -1;

pthread_t conway_threads[HEIGHT][WIDTH];
typedef struct
{
    int row;
    int column;
} thread_arguments;
thread_arguments arguments[HEIGHT][WIDTH];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static int current_generation = 0;
static int completed_cells = 0;
static int finished_generation[HEIGHT][WIDTH];

int generation_started = 0;
int simulation_finished = 0;

struct cells_table initial_table = {
                                        .cells = {
                                                    {0,0,0,0,0,0,0,0,0,0},
                                                    {0,0,0,0,0,0,0,0,0,0},
                                                    {0,0,0,0,0,0,0,0,0,0},
                                                    {0,0,0,0,1,0,0,0,0,0},
                                                    {0,0,0,0,1,0,0,0,0,0},
                                                    {0,0,0,0,1,0,0,0,0,0},
                                                    {0,0,0,0,0,0,0,0,0,0},
                                                    {0,0,0,0,0,0,0,0,0,0},
                                                    {0,0,0,0,0,0,0,0,0,0},
                                                    {0,0,0,0,0,0,0,0,0,0}
                                                }
                                   };

void print_cell_states(char states_matrix[HEIGHT][WIDTH])
{
    for (int i = 0; i < HEIGHT; i++)
    {
        for(int j = 0; j < WIDTH; j++)
        {   
            if (states_matrix[i][j] == 1)
                printf(" + ");
            else
                printf(" . ");
        }
        printf("\n");
    }
    printf("\n");
}

int predecessors_finished(int row, int column, int generation)
{
    // ako postoji lijevi susjed i nije zavrsio mutaciju za ovu generaciju
    if (column - 1 >= 0 && finished_generation[row][column - 1] != generation)
        return 0;
    // ako postoji gore-lijevi susjed i nije zavrsio
    if (row - 1 >= 0 && column - 1 >= 0 && finished_generation[row - 1][column - 1] != generation)
        return 0;
    // ako postoji gore susjed i nije zavrsio
    if (row - 1 >= 0 && finished_generation[row - 1][column] != generation)
        return 0;
    // ako postoji gore-desni susjed i nije zavrsio
    if (row - 1 >= 0 && column + 1 < WIDTH && finished_generation[row - 1][column + 1] != generation)
        return 0;

    return 1;
}


void* evolve_into_new_generation(void* args)
{
    thread_arguments* cell = (thread_arguments*) args;
    int row = cell->row;
    int column = cell->column;

    while(1)
    {
        pthread_mutex_lock(&mutex);

         while (!simulation_finished &&
              (!generation_started ||
               finished_generation[row][column] == current_generation ||
               !predecessors_finished(row, column, current_generation)))
        {
            pthread_cond_wait(&cond, &mutex);
        }

        if (simulation_finished)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }

        pthread_mutex_unlock(&mutex);

        // celija moze da mutira
        // stvarno stanje celije je u drajveru
        // nit salje samo poziciju celije

        struct cell_position pos;
        pos.row = row;
        pos.column = column;

        if (ioctl(fd, IOCTL_EVOLVE_CELL, &pos) < 0)
        {
            perror("IOCTL_EVOLVE_CELL failed");
        }

        pthread_mutex_lock(&mutex);

        finished_generation[row][column] = current_generation;
        completed_cells++;

        pthread_cond_broadcast(&cond);

        pthread_mutex_unlock(&mutex);

    }

    return NULL;
}

int get_cells_table_from_driver(struct cells_table * table)
{
    if(ioctl(fd, IOCTL_GET_CELL_STATES, table) < 0)
    {
        perror("IOCTL_GET_CELL_STATES failed");
        return -1;
    }

    return 0;
}

static void print_stats_from_proc(void)
{
    FILE *fp;
    char line[128];

    fp = fopen(PROC_PATH, "r");
    if (!fp) {
        perror("Cannot open /proc/conway_stats");
        return;
    }

    printf("Statistics from procfs:\n");

    while (fgets(line, sizeof(line), fp))
        printf("%s", line);

    printf("\n");

    fclose(fp);
}

void initialize_finished_generation(void)
{
    int i, j;

    for (i = 0; i < HEIGHT; i++)
    {
        for (j = 0; j < WIDTH; j++)
        {
            finished_generation[i][j] = -1;
        }
    }
}

void create_threads(void)
{
    int i, j;

    for (i = 0; i < HEIGHT; i++)
    {
        for (j = 0; j < WIDTH; j++)
        {
            arguments[i][j].row = i;
            arguments[i][j].column = j;

            if (pthread_create(&conway_threads[i][j],
                               NULL,
                               evolve_into_new_generation,
                               &arguments[i][j]) != 0)
            {
                perror("pthread_create failed");
                exit(EXIT_FAILURE);
            }
        }
    }
}

void join_threads(void)
{
    int i, j;

    for (i = 0; i < HEIGHT; i++)
    {
        for (j = 0; j < WIDTH; j++)
        {
            pthread_join(conway_threads[i][j], NULL);
        }
    }
}

static int run_one_generation(void)
{
    pthread_mutex_lock(&mutex);

        completed_cells = 0;
        initialize_finished_generation();
        generation_started = 1;
        pthread_cond_broadcast(&cond);
        while (completed_cells < HEIGHT * WIDTH)
        {
            pthread_cond_wait(&cond, &mutex);
        }
        generation_started = 0;
    pthread_mutex_unlock(&mutex);

    if (ioctl(fd, IOCTL_FINISH_GENERATION) < 0)
    {
        perror("IOCTL_FINISH_GENERATION failed");
        return -1;
    }

    pthread_mutex_lock(&mutex);
        current_generation++;
        pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);

    return 0;
}

void stop_threads(void)
{
    pthread_mutex_lock(&mutex);
        simulation_finished = 1;
        pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
}

int main()
{
    char command[32];

    struct cells_table table;

    fd = open("/dev/conway_device", O_RDWR);
    if(fd < 0) 
    {
        printf("Cannot open device file...\n");
        return -1;
    }

    if(ioctl(fd, IOCTL_SET_CELL_STATES, &initial_table) < 0) 
    {
        perror("IOCTL_SET_CELL_STATES failed");
        close(fd);
        return -1;
    }

    initialize_finished_generation();

    create_threads();

    while (1) 
    {
        system("clear");

        if (get_cells_table_from_driver(&table) < 0)
            break;

        printf("Current matrix, generation %d:\n", current_generation);
        print_cell_states(table.cells);

        print_stats_from_proc();

        printf("Press Enter for next generation, or q + Enter to quit: ");

        if (!fgets(command, sizeof(command), stdin))
            break;

        if (command[0] == 'q' || command[0] == 'Q')
            break;

        if (run_one_generation() < 0)
            break;
    }

    stop_threads();
    join_threads();

    close(fd);

    printf("Program finished.\n");

    return 0;

}
