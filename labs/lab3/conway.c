#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "conway.h"
#include "init.h"

perfect_cell matrix[NUM_OF_ROWS][NUM_OF_COLUMNS];

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;

int num_finished = 0;
int generation = 0;
int sleep_seconds = 1;

void print_cells(void)
{
    for (int r = 0; r < NUM_OF_ROWS; r++)
    {
        printf("[");
        for (int c = 0; c < NUM_OF_COLUMNS; c++)
        {
            printf("%c", matrix[r][c].is_alive ? '*' : '.');
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

int can_start(perfect_cell *cell)
{
    if (cell->is_finished)
    {
        return 0;
    }

    if (cell->row == 0 && cell->column == 0)
    {
        return 1;
    }

    if (cell->row == 0)
    {
        return matrix[cell->row][cell->column - 1].is_finished;
    }

    if (cell->column == 0)
    {
        return matrix[cell->row - 1][cell->column].is_finished;
    }

    return matrix[cell->row - 1][cell->column].is_finished &&
           matrix[cell->row][cell->column - 1].is_finished;
}

void evolve(perfect_cell *cell)
{
    int count = 0;

    for (int r = -1; r <= 1; r++)
    {
        for (int c = -1; c <= 1; c++)
        {
            if (r == 0 && c == 0)
            {
                continue;
            }

            int nr = cell->row + r;
            int nc = cell->column + c;

            if (nr < 0 || nr >= NUM_OF_ROWS || nc < 0 || nc >= NUM_OF_COLUMNS)
            {
                continue;
            }

            perfect_cell *neighbor = &matrix[nr][nc];

            if (neighbor->is_finished)
            {
                count += neighbor->old_state;
            }
            else
            {
                count += neighbor->is_alive;
            }
        }
    }

    if (cell->is_alive)
    {
        if (count < 2 || count > 3)
        {
            cell->is_alive = 0;
        }
    }
    else
    {
        if (count == 3)
        {
            cell->is_alive = 1;
        }
    }
}

void reset_for_next_generation(void)
{
    for (int r = 0; r < NUM_OF_ROWS; r++)
    {
        for (int c = 0; c < NUM_OF_COLUMNS; c++)
        {
            matrix[r][c].old_state = matrix[r][c].is_alive;
            matrix[r][c].is_finished = 0;
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
        pthread_mutex_lock(&mutex);

        while (!can_start(cell))
        {
            pthread_cond_wait(&condition, &mutex);
        }

        evolve(cell);

        cell->is_finished = 1;
        num_finished++;

        pthread_cond_broadcast(&condition);

        int my_generation = generation;

        if (num_finished == TOTAL_CELLS)
        {
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
            if (pthread_create(&matrix[r][c].thread, NULL, run, &matrix[r][c]) != 0)
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
            pthread_join(matrix[r][c].thread, NULL);
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        sleep_seconds = atoi(argv[1]);
    }

    printf("Initial state:\n\n");

    char *file_name;
    if (argc > 2)
    {
        init_cells(argv[2]);            
    }
    else
    {
        init_cells("initial.txt");
    }
    print_cells();

    start_cells();

    return 0;
}