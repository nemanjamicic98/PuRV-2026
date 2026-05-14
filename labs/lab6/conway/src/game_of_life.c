#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>

#define NUM_OF_ROWS 10
#define NUM_OF_COLUMNS 10

#define NUM_OF_GENERATIONS 30

/* Putanja do drajvera */
#define DRIVER_PATH "/dev/etx_device"

/* -------------------------------------------------------------------
 * Statistika - azurira se u evolve(), salje se u driver
 * ------------------------------------------------------------------- */
static int stat_alive = 0;   /* zive celije nakon evolucije     */
static int stat_born  = 0;   /* celije koje su ozivjele         */
static int stat_died  = 0;   /* celije koje su umrle            */

typedef struct Cell {

    int row;
    int column;
    bool is_current_state_alive;
    bool is_next_state_alive;
    
    pthread_t thread;
    sem_t sem_start_gen;
    sem_t sem_done;
    
} Cell;

Cell cells[NUM_OF_ROWS][NUM_OF_COLUMNS];
sem_t sem_gen_finished;
volatile bool is_simulation_finished = false;
pthread_mutex_t exit_mutex = PTHREAD_MUTEX_INITIALIZER;

int threads_exited = 0;
int num_of_created_threads = 0;
int num_of_completed_generations = 0;

void* run(void* arg);
void calculate_next_state(Cell *cell);
void evolve();
void write_stats_to_driver(void);
void set_values();
void print_cells(int);
int get_num_of_alive_neighbours(int row, int column);
void clear_screen();

void* run(void* arg) {

    Cell *cell = (Cell*) arg;

    int r = cell->row;
    int c = cell->column;
    
    while(true) {

        sem_wait(&cell->sem_start_gen);

        if (is_simulation_finished) {
            break;
        }

        if(r > 0) {
            sem_wait(&cells[r-1][c].sem_done);
        }

        if(c > 0) {
            sem_wait(&cells[r][c-1].sem_done);
        }

        calculate_next_state(cell);

        int num_to_post = 0;
        if (r < NUM_OF_ROWS - 1) num_to_post++;
        if (c < NUM_OF_COLUMNS - 1) num_to_post++;

        for(int i = 0; i < num_to_post; i++) {
            sem_post(&cell->sem_done);
        }

        if (r == NUM_OF_ROWS - 1 && c == NUM_OF_COLUMNS - 1) {
            sem_post(&sem_gen_finished);
        }

    }

    pthread_mutex_lock(&exit_mutex);
        threads_exited++;
    pthread_mutex_unlock(&exit_mutex);

    return NULL;
}

void calculate_next_state(Cell *cell) {

    int alive_neighbours = get_num_of_alive_neighbours(cell->row, cell->column);

    if (cell->is_current_state_alive) {
            if (alive_neighbours == 2 || alive_neighbours == 3) {
                cell->is_next_state_alive = true;
            } else {
                cell->is_next_state_alive = false;
            }
        }

        else {
            if (alive_neighbours == 3) {
                cell->is_next_state_alive = true;
            } else {
                cell->is_next_state_alive = false;
            }
        }
}

/*
 * evolve - prebaci next u current i izracunaj statistiku.
 *
 * Ova funkcija sada:
 *   1. Poredi current vs next da bi nasla born/died
 *   2. Prebaci next -> current (kao prije)
 *   3. Broji ukupno zivih
 */
void evolve() {

    stat_alive = 0;
    stat_born  = 0;
    stat_died  = 0;

    for(int r = 0; r < NUM_OF_ROWS; r++) {
            for(int c = 0; c < NUM_OF_COLUMNS; c++) {

                bool current = cells[r][c].is_current_state_alive;
                bool next    = cells[r][c].is_next_state_alive;

                /* Detektuj promjene stanja */
                if (!current && next) stat_born++;   /* bila mrtva, postala ziva */
                if (current && !next) stat_died++;   /* bila ziva, postala mrtva */
 
                /* Prebaci stanje */
                cells[r][c].is_current_state_alive = next;
 
                if (next)
                    stat_alive++;

                while(sem_trywait(&cells[r][c].sem_done) == 0);
            }
        }
}

/*
 * write_stats_to_driver - pise statistiku u /dev/etx_device.
 *
 * Driver ocekuje string oblika: "generacije zive rodjene umrle\n"
 */
void write_stats_to_driver(void)
{
    FILE *f = fopen(DRIVER_PATH, "w");
    if (f == NULL) {
        printf("Upozorenje: nije moguce otvoriti %s za pisanje statistike\n",
               DRIVER_PATH);
        return;
    }
    fprintf(f, "%d %d %d %d\n",
            num_of_completed_generations,
            stat_alive,
            stat_born,
            stat_died);
    fclose(f);
}

void set_values() {

    int start_r = 2;
    int start_c = 1;
    
    for(int row = 0; row < NUM_OF_ROWS; row++) {
        for(int column = 0; column < NUM_OF_COLUMNS; column++) {
            
            cells[row][column].row = row;
            cells[row][column].column = column;
            cells[row][column].is_current_state_alive = false;
            cells[row][column].is_next_state_alive = false;

            //----------------------------------------------------------------
            // 1) Oscillator patter
            //if (column == 1 && (row >= 3 && row <= 5)) {
            //    cells[row][column].is_current_state_alive = true;
            //}
            //
            //if (row == 5 && (column >= 5 && column <= 7)) {
            //    cells[row][column].is_current_state_alive = true;
            //}
            //
            //if (row == 4 && (column >= 6 && column <= 8)) {
            //    cells[row][column].is_current_state_alive = true;
            //}

            // ----------------------------------------------------------------
            // 2) Cool pattern

            //if ((row == 0 && column == 0) || (row == 1 && column == 1) || (row == 1 && column == 2) || (row == 2 && column == 0) || (row == 2 && column == 1)) {
            //    cells[row][column].is_current_state_alive = true;
            //}
            
            // ----------------------------------------------------------------
            // 3) Really cool pattern - matrix has to be at least 15x15

            //int r = row;
            //int c = column;
            //
            //if ((r==1 || r==6 || r==8 || r==13) &&
            //    (c==3||c==4||c==5||c==9||c==10||c==11)) {
            //    cells[r][c].is_current_state_alive = true;
            //}
            //
            //if ((r==3||r==4||r==5||r==9||r==10||r==11) &&
            //    (c==1||c==6||c==8||c==13)) {
            //    cells[r][c].is_current_state_alive = true;
            //}
            
            // ----------------------------------------------------------------
            // 4) Random cells alive pattern

            //if((rand() % (10 + 1)) % 2 == 0) {
            //    cells[row][column].is_current_state_alive = true;
            //}

            // ----------------------------------------------------------------
            // 5) All cells alive pattern
            //cells[row][column].is_current_state_alive = true;

            // ----------------------------------------------------------------
            
            // 2) Pattern to test different num of alive cells between gens
            // 2.1)

            //if ((row == 0 && column == 0) || (row == 1 && column == 1) || (row == 1 && column == 2) || (row == 2 && column == 0) || (row == 2 && column == 1) || (row == 2 && column == 2) || (row == 2 && column == 3)) {
            //    cells[row][column].is_current_state_alive = true;
            //}
            
            // ----------------------------------------------------------------
            // 2.2)
            
            if ( (row == start_r && (column == start_c + 1 || column == start_c + 4)) ||
                 (row == start_r + 1 && column == start_c) ||
                 (row == start_r + 2 && (column == start_c || column == start_c + 4)) ||
                 (row == start_r + 3 && (column >= start_c && column <= start_c + 3)) ) 
            {
                cells[row][column].is_current_state_alive = true;
            }

            // ----------------------------------------------------------------

            sem_init(&cells[row][column].sem_start_gen, 0, 0);
            sem_init(&cells[row][column].sem_done, 0, 0);

            pthread_mutex_lock(&exit_mutex);
                num_of_created_threads++;
            pthread_mutex_unlock(&exit_mutex);

            pthread_create(&cells[row][column].thread, NULL, run, &cells[row][column]);
        }
    }
}

void print_cells(int num_of_gen) {

    for(int r = 0; r < NUM_OF_ROWS; r++) {
        for(int c = 0; c < NUM_OF_COLUMNS; c++) {
            printf("[%s] ", cells[r][c].is_current_state_alive ? "*" : " ");
        }
        printf("\n");
    }

    if(num_of_gen == 0) {
        printf("Za prvu generaciju nema statistike!\n");
        return;
    }

    /* Ispisi i statistiku ispod grida */
    printf("\nGeneracija: %d | Zive: %d | Rodjene: %d | Umrle: %d\n",
           num_of_completed_generations, stat_alive, stat_born, stat_died);

    fflush(stdout);

}

int get_num_of_alive_neighbours(int row, int column) {

    int alive_neighbours = 0;

    for(int r = row - 1; r <= row + 1; r++) {
        for(int c = column - 1; c <= column + 1; c++) {
            if (r >= 0 && r < NUM_OF_ROWS && c >= 0 && c < NUM_OF_COLUMNS) {
                if (cells[r][c].is_current_state_alive) {
                    alive_neighbours++;
                }
            }
        }
    }

    if (cells[row][column].is_current_state_alive) {
        alive_neighbours--;
    }

    return alive_neighbours;
}

void clear_screen() {
    system("clear");
}

int main() {

    int sleep_time;

    printf("Unesite vrijeme spavanja (ms): ");
    scanf("%d", &sleep_time);

    sem_init(&sem_gen_finished, 0, 0);
    set_values();

    for(int gen = 0; gen < NUM_OF_GENERATIONS; gen++) {

        //clear_screen();
        printf("\n");
        print_cells(gen);
        usleep(sleep_time * 1000);

        if(gen == NUM_OF_GENERATIONS - 1) {
            num_of_completed_generations++;
            //clear_screen();
            printf("\n");
            write_stats_to_driver();  /* pisi i za zadnju generaciju */
            print_cells(gen);
            break;
        }

        for(int r = 0; r < NUM_OF_ROWS; r++) {
            for(int c = 0; c < NUM_OF_COLUMNS; c++) {
                sem_post(&cells[r][c].sem_start_gen);
            }
        }

        sem_wait(&sem_gen_finished);

        evolve();
        num_of_completed_generations++;

        write_stats_to_driver();  /* pisi statistiku u driver */
    }

    is_simulation_finished = true;

    for(int r = 0; r < NUM_OF_ROWS; r++)
        for(int c = 0; c < NUM_OF_COLUMNS; c++)
            sem_post(&cells[r][c].sem_start_gen);

    for(int row = 0; row < NUM_OF_ROWS; row++) {
        for(int column = 0; column < NUM_OF_COLUMNS; column++) {
            pthread_join(cells[row][column].thread, NULL);
        }
    }

    for(int r = 0; r < NUM_OF_ROWS; r++) {
        for(int c = 0; c < NUM_OF_COLUMNS; c++) {
            sem_destroy(&cells[r][c].sem_start_gen);
            sem_destroy(&cells[r][c].sem_done);
        }
    }

    sem_destroy(&sem_gen_finished);

    printf("\n");
    printf("Simulation finished!\n");
    printf("Total threads created: %d\n", num_of_created_threads);
    printf("Total threads exited: %d\n", threads_exited);
    printf("Total generations completed: %d\n", num_of_completed_generations);

    return 0;
}