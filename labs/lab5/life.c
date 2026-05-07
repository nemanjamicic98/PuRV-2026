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


int board[SIZE][SIZE];
int next_board[SIZE][SIZE];
sem_t cell_sem[SIZE][SIZE];
sem_t gen_done_sem;
sem_t start_sem[SIZE][SIZE];
int is_running = 1;
int fd;

typedef struct {
    int r, c;
} ThreadData;

void* cell_func(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    int r = data->r;
    int c = data->c;

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
                int nr = r + i;
                int nc = c + j;
                if (nr >= 0 && nr < SIZE && nc >= 0 && nc < SIZE) {
                    if (nr < r || (nr == r && nc < c)) {
                        cd.neighbors[n_idx++] = next_board[nr][nc];
                    } else {
                        cd.neighbors[n_idx++] = board[nr][nc];
                    }
                } else {
                    cd.neighbors[n_idx++] = 0;
                }
            }
        }
        cd.neighbors[8] = board[r][c];

        if (ioctl(fd, IOCTL_SET_AND_GET, &cd) < 0) {
            perror("IOCTL Error");
        }
        next_board[r][c] = cd.result;

        int signal_count = 0;
        if (r < SIZE - 1) signal_count++;
        if (c < SIZE - 1) signal_count++;
        for (int i = 0; i < signal_count; i++) sem_post(&cell_sem[r][c]);

        if (r == SIZE - 1 && c == SIZE - 1) sem_post(&gen_done_sem);
    }
    return NULL;
}

void draw_board(int g) {
    printf("\033[H\033[J");
    printf("Generation: %d\n", g);
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            printf(board[i][j] ? "O " : ". ");
        }
        printf("\n");
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Vrijeme spavanja: %s <ms>\n", argv[0]);
        return 1;
    }

    fd = open("/dev/life_service", O_RDWR);
    if (fd < 0) {
        perror("Greska pri otvaranju /dev/life_service.");
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

    int gen_count = 0;
    while (is_running) {
        draw_board(gen_count++);
        usleep(sleep_time);

        for (int i = 0; i < SIZE; i++)
            for (int j = 0; j < SIZE; j++)
                sem_post(&start_sem[i][j]);

        sem_wait(&gen_done_sem);

        int has_changed = 0;
        for (int i = 0; i < SIZE; i++) {
            for (int j = 0; j < SIZE; j++) {
                if (board[i][j] != next_board[i][j]) has_changed = 1;
                board[i][j] = next_board[i][j];

                while (sem_trywait(&cell_sem[i][j]) == 0);
            }
        }

        if (!has_changed) {
            printf("\nStabilno stanje je postignuto. Game over.\n");
            is_running = 0;
        }
        
        if (gen_count > 1000) is_running = 0;
    }

    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            sem_post(&start_sem[i][j]); 
            pthread_join(threads[i][j], NULL);
            sem_destroy(&cell_sem[i][j]);
            sem_destroy(&start_sem[i][j]);
        }
    }
    sem_destroy(&gen_done_sem);
    close(fd);

    return 0;
}
