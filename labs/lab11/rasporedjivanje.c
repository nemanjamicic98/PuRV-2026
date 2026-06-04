#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sched.h>
#include <time.h>

void task1(void);
void task2(void);
void task3(void);

void add_ms(struct timespec *t, long ms) {
    t->tv_nsec += ms * 1000000L;
    if (t->tv_nsec >= 1000000000L) {
        t->tv_sec += t->tv_nsec / 1000000000L;
        t->tv_nsec %= 1000000000L;
    }
}
//mjerenje vremena i pokretanje 
void run_and_measure_task(int task_id, void (*task_func)(void)) {
    struct timespec start, end;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    task_func();
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double start_sec = start.tv_sec + (start.tv_nsec / 1000000000.0);
    double end_sec = end.tv_sec + (end.tv_nsec / 1000000000.0);
    double duration = (end_sec - start_sec) * 1000.0;
    
    printf("\n -> [Task %d] Pocetak: %.3f s | Kraj: %.3f s | Trajanje: %.2f ms\n", 
           task_id, start_sec, end_sec, duration);
    
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    pid_t pid1, pid2, pid3;
    int pmin = sched_get_priority_min(SCHED_FIFO);
    struct sched_param param;

    // Task 1 - 40ms
    pid1 = fork();
    if (pid1 < 0) {
        perror("Fork za Task 1 neuspjeo");
        return -1;
    }
    if (pid1 == 0) {
        //najkraci period najvisi prioritet
        param.sched_priority = pmin + 30;
        if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
            perror("sched_setscheduler za Task 1 neuspjeo");
            exit(1);
        }

        struct timespec next_period;
        clock_gettime(CLOCK_MONOTONIC, &next_period);

        while (1) {
            run_and_measure_task(1, task1);
            add_ms(&next_period, 40);
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_period, NULL);
        }
        exit(0);
    }

    // Task 2 - 80ms
    pid2 = fork();
    if (pid2 < 0) {
        perror("Fork za Task 2 neuspjeo");
        return -1;
    }
    if (pid2 == 0) {
        param.sched_priority = pmin + 20;
        if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
            perror("sched_setscheduler za Task 2 neuspjeo");
            exit(1);
        }

        struct timespec next_period;
        clock_gettime(CLOCK_MONOTONIC, &next_period);

        while (1) {
            run_and_measure_task(2, task2);
            add_ms(&next_period, 80);
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_period, NULL);
        }
        exit(0);
    }

    // Task 3 - 120ms
    pid3 = fork();
    if (pid3 < 0) {
        perror("Fork za Task 3 neuspjeo");
        return -1;
    }
    if (pid3 == 0) {
        param.sched_priority = pmin + 10;
        if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
            perror("sched_setscheduler za Task 3 neuspjeo");
            exit(1);
        }

        struct timespec next_period;
        clock_gettime(CLOCK_MONOTONIC, &next_period);

        while (1) {
            run_and_measure_task(3, task3);
            add_ms(&next_period, 120);
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_period, NULL);
        }
        exit(0);
    }

    printf(" Svi periodicni realnovremeni procesi su pokrenuti.\n");
    
    wait(NULL);
    wait(NULL);
    wait(NULL);

    return 0;
}