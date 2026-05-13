#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define SIZE 10
#define MAGIC_NO 'k'

struct cell_data { 
    int neighbors[9]; 
    int result; 
};

#define IOCTL_SET_AND_GET _IOWR(MAGIC_NO, 1, struct cell_data)
#define IOCTL_INC_GEN _IO(MAGIC_NO, 2)

int board[SIZE][SIZE];
int next_board[SIZE][SIZE];
sem_t cell_sem[SIZE][SIZE], gen_done_sem, start_sem[SIZE][SIZE];
int is_running = 1, fd;

typedef struct { int r, c; } ThreadData;

void* cell_func(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    int r = data->r, c = data->c;

    while (is_running) {
        sem_wait(&start_sem[r][c]); 
        if (!is_running) break;

        if (r > 0) sem_wait(&cell_sem[r-1][c]);
        if (c > 0) sem_wait(&cell_sem[r][c-1]);

        struct cell_data cd;
        int n_idx = 0;

        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                if (i == 0 && j == 0) continue;
                int nr = r + i, nc = c + j;
                if (nr >= 0 && nr < SIZE && nc >= 0 && nc < SIZE) {
                    cd.neighbors[n_idx++] = (nr < r || (nr == r && nc < c)) ? next_board[nr][nc] : board[nr][nc];
                } else {
                    cd.neighbors[n_idx++] = 0;
                }
            }
        }
        cd.neighbors[8] = board[r][c]; 

        ioctl(fd, IOCTL_SET_AND_GET, &cd);
        next_board[r][c] = cd.result;

        if (r < SIZE - 1) sem_post(&cell_sem[r][c]);
        if (c < SIZE - 1) sem_post(&cell_sem[r][c]);

        if (r == SIZE - 1 && c == SIZE - 1) sem_post(&gen_done_sem);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Vrijeme spavanja: %s <ms>\n", argv[0]);
        return 1;
    }

    fd = open("/dev/life_service", O_RDWR);
    if (fd < 0) {
        perror("Greska pri otvaranju drajvera");
        return 1;
    }

    int sleep_time = atoi(argv[1]) * 1000;
    srand(time(NULL));
    pthread_t threads[SIZE][SIZE];
    ThreadData info[SIZE][SIZE];
    sem_init(&gen_done_sem, 0, 0);

    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            sem_init(&cell_sem[i][j], 0, 0);
            sem_init(&start_sem[i][j], 0, 0);
            board[i][j] = rand() % 2;
            info[i][j].r = i; 
            info[i][j].c = j;
            pthread_create(&threads[i][j], NULL, cell_func, &info[i][j]);
        }
    }

    int gen_count = 1;
    while (is_running) {
        usleep(sleep_time);

        for (int i = 0; i < SIZE; i++)
            for (int j = 0; j < SIZE; j++) sem_post(&start_sem[i][j]);

        sem_wait(&gen_done_sem);

        ioctl(fd, IOCTL_INC_GEN);

        printf("\033[H\033[J"); 
        printf("Generation: %d\n", gen_count++);

        int has_changed = 0;
        for (int i = 0; i < SIZE; i++) {
            for (int j = 0; j < SIZE; j++) {
                if (board[i][j] != next_board[i][j]) has_changed = 1;
                board[i][j] = next_board[i][j];
                printf(board[i][j] ? "O " : ". ");
                
                while (sem_trywait(&cell_sem[i][j]) == 0);
            }
            printf("\n");
        }

        if (!has_changed) {
            printf("Stabilno stanje je postignuto. Game over.\n");
            is_running = 0;
            for (int i = 0; i < SIZE; i++)
                for (int j = 0; j < SIZE; j++) sem_post(&start_sem[i][j]);
        }
    }

    for (int i = 0; i < SIZE; i++)
        for (int j = 0; j < SIZE; j++) pthread_join(threads[i][j], NULL);

    close(fd);
    return 0;
}
