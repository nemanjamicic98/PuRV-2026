#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define HEIGHT 10
#define WIDTH  10

// pocetno stanje igre : 0 mrtva celija, 1 ziva celija
char current_states[HEIGHT][WIDTH] = { 0,0,0,0,0,0,0,0,0,0,
                                       0,1,1,0,0,0,0,0,0,0,
                                       0,1,1,0,0,1,0,0,0,0,
                                       0,0,0,0,0,1,0,0,0,0,
                                       0,0,0,0,0,1,0,0,0,0,
                                       0,0,0,0,0,0,0,1,0,0,
                                       0,0,0,1,1,1,0,0,0,0,
                                       0,0,0,0,0,0,0,0,0,0,
                                       0,0,0,0,0,0,1,1,1,0,
                                       0,0,0,0,0,0,0,0,0,0 };


// oscilator
/*char current_states[HEIGHT][WIDTH] = {
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
};*/

// kosnica
/*char current_states[HEIGHT][WIDTH] =  {
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,1,1,0,0,0,0,0,0},
    {0,0,1,1,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,1,1,0,0,0},
    {0,0,0,0,1,0,0,1,0,0},
    {0,0,0,0,1,0,0,1,0,0},
    {0,0,0,0,0,1,1,0,0,0},
    {0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0}
};*/

char next_states[HEIGHT][WIDTH] = {0}; // ovdje se upisuje sljedeca generacija

pthread_t conway_threads [HEIGHT][WIDTH]; // matrica niti gdje je svaka nit jedna celija

typedef struct
{
    int row;
    int column;
} thread_arguments;

thread_arguments arguments[HEIGHT][WIDTH];

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER; // ceka se da celije viseg prioriteta zavrse sa mutacijama

int current_generation = 0; // redni broj trenutne generacije
int completed_cells = 0; // broj celija koje su zavrsile mutaciju za sljedecu generaciju
int finished_generation[HEIGHT][WIDTH]; // za koju generaciju je pojedinacna celija zavrsila mutaciju

int simulation_finished = 0;

// funkcija za ispis matrice
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

// funkcija koja broji koliko zivih susjeda ima celija
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

// funkcija u kojoj se odredjuje stanje celije u sljedecoj generaciji
// ako je celija ziva, sa 2 ili 3 ziva susjeda, ostaje ziva
// ako je celija ziva, sa manje od 2 ziva susjeda, umire (underpopulation)
// ako je celija ziva, sa vise od 3 ziva susjeda, umire (overpopulation)
// ako je celija mrtva, sa tacno 3 ziva susjeda, postaje ziva (reproduction), inace ostaje mrtva
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

// funkcija daje odgovor moze li celija da pocne mutaciju za novu generaciju
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

// 
void* evolve_into_new_generation(void* args)
{
    thread_arguments* cell = (thread_arguments*) args;
    int row = cell->row;
    int column = cell->column;

    while(1)
    {
        // nit ce cekati dok god traje simulacija i
        // nit(celija) koja je vec zavrsila mutaciju za ovu generaciju pokusava opet ili 
        // prethodnici nisu zavrsili svoje mutacije
        pthread_mutex_lock(&mutex);
            while (!simulation_finished && (finished_generation[row][column] == current_generation || !predecessors_finished(row, column, current_generation)))
                pthread_cond_wait(&cond, &mutex); // nit se "uspava"

                if (simulation_finished)
                {
                    pthread_mutex_unlock(&mutex);
                    break;
                }
        pthread_mutex_unlock(&mutex);   

        // simulacija nije zavrsena i celija smije da mutira
        int alive_neighbour_count = alive_neighbours(row, column);
        next_states[row][column] = determine_cell_new_state(current_states[row][column], alive_neighbour_count);

        pthread_mutex_lock(&mutex);
            finished_generation[row][column] = current_generation; 
            completed_cells++;
            pthread_cond_broadcast(&cond); // bude se ostale niti kad se nesto promijeni
        pthread_mutex_unlock(&mutex);
    }
}

void copy_next_to_current(void)
{
    for (int i = 0; i < HEIGHT; i++)
    {
        for (int j = 0; j < WIDTH; j++)
        {
            current_states[i][j] = next_states[i][j];
        }
    }
}

// funkcija koja provjerava da li su se mutacije stabilizovale
int generations_are_same(void)
{
    int i, j;
    for(i = 0; i < HEIGHT; i++)
        for(j = 0; j < WIDTH; j++)
            if(current_states[i][j] != next_states[i][j])
                return 0; // barem jedna razlika, nije se stabilizovalo
    return 1;
}

int main(int argc, char *argv[])
{
    int sleep_time = 0;
    
    for (int i = 0; i < HEIGHT; i++)
        for (int j = 0; j < WIDTH; j++)
            finished_generation[i][j] = -1; // nijedna celija nije zavrsila nijednu generaciju

    if (argc > 1)
    {
        int ms = atoi(argv[1]);
        if(ms > 0)
            sleep_time = ms * 1000;
        else
        {
            printf("Invalid sleep time.\n");
            return -1;
        }
    }
    else
    {
        printf("Enter sleep time.\n");
        return -1;
    }

    system("clear");
    printf("Initial generation:\n");
    print_cell_states(current_states); // pocetna generacija
    usleep(sleep_time);

    for (int i = 0; i < HEIGHT; i++)
    {
        for (int j = 0; j < WIDTH; j++)
        {
            arguments[i][j].row = i;
            arguments[i][j].column = j;
            pthread_create(&conway_threads[i][j], NULL, evolve_into_new_generation, &arguments[i][j]);
        }
    }

   while (1)
    {
        pthread_mutex_lock(&mutex);

            // dok se ne zavrsi svih 100 niti, main spava
            while (completed_cells < HEIGHT * WIDTH)
            {
                pthread_cond_wait(&cond, &mutex);
            }

        pthread_mutex_unlock(&mutex);

        system("clear");
        printf("Generation %d:\n", current_generation + 1);
        print_cell_states(next_states);

        if (generations_are_same()) // simulacija se stabilizovala
        {
            pthread_mutex_lock(&mutex);
                simulation_finished = 1; 
                pthread_cond_broadcast(&cond); // da bi sve niti vidjele da je simulacija gotova
            pthread_mutex_unlock(&mutex);

            for (int i = 0; i < HEIGHT; i++)
            {
                for (int j = 0; j < WIDTH; j++)
                {
                    pthread_join(conway_threads[i][j], NULL);
                }
            }

            printf("Simulation finished. Generations stabilized.\n");

            return 0;
        }

        // ako se simulacija nije stabilizovala

        copy_next_to_current();

        usleep(sleep_time);

        pthread_mutex_lock(&mutex);
            completed_cells = 0; // resetovanje broja zavrsenih celija za ovu generaciju
            current_generation++; // prelazi se na sljedecu generaciju
        pthread_cond_broadcast(&cond); // posto je pocela nova generacija, niti treba da provjere ko moze da mutira

        pthread_mutex_unlock(&mutex);
    }

    return 0;
}
