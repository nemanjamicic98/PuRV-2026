// gcc -Wall -Wextra periodic_tasks_abs_time.c -o periodic_tasks -pthread -lrt
// ./periodic_tasks

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define NSEC_PER_SEC        1000000000ULL // 1 s je 1 000 000 000 ns

#define FIRST_ACTIVATION_US 100000ULL     // prva aktivacija nakon 100 ms
#define TASK1_PERIOD_US      60000ULL     // 60 ms
#define TASK2_PERIOD_US      80000ULL     // 80 ms

void task1(void)
{
    int i, j;
    for (i = 0; i < 4; i++) 
    {
        for (j = 0; j < 1000; j++);
        printf("1");
        fflush(stdout);
    }
}

void task2(void)
{
    int i, j;
    for (i = 0; i < 6; i++) 
    {
        for (j = 0; j < 10000; j++);
        printf("2");
        fflush(stdout);
    }
}

// struktura koja opisuje task
typedef struct 
{
    const char* name;
    uint64_t period_us;
    struct timespec next_activation; // sljedeci apsolutni trenutak kada task treba da se probudi
    struct timespec start_time;      // vrijeme prve planirane aktivacije
    void (*body)(void);              // funkcija koju task izvrsava
} periodic_task_data_t;

static inline void timespec_add_us(struct timespec *t, uint64_t d)
{
    d *= 1000;
    d += t->tv_nsec;
    while (d >= NSEC_PER_SEC)
    {
        d -= NSEC_PER_SEC;
		t->tv_sec += 1;
    }
    t->tv_nsec = d;
}

// koliko ms je proslo izmedju 2 vremena
static double elapsed_ms(struct timespec start, struct timespec now)
{
    return (now.tv_sec - start.tv_sec) * 1000.0 + (now.tv_nsec - start.tv_nsec) / 1000000.0;
}

void wait_next_activation(periodic_task_data_t *task)
{
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &task->next_activation, NULL); // spavaj do sljedeceg planiranog vremena
    timespec_add_us(&task->next_activation, task->period_us); // nakon budjenja, izracunaj kad ce biti sljedeca aktivacija
}

static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *periodic_thread(void *arg)
{
    periodic_task_data_t *task; // podaci o konkretnom tasku
    struct timespec now; // stvarno vrijeme nakon budjenja
    int activation = 1; // brojac aktivacija

    task = (periodic_task_data_t *)arg;

    while(1)
    {
        wait_next_activation(task);

        if (clock_gettime(CLOCK_REALTIME, &now) != 0) // stvarno vrijeme budjenja
        {
            perror("clock_gettime");
            return NULL;
        }

        pthread_mutex_lock(&print_mutex);
            printf("%s | aktivacija %2d | vrijeme = %7.2f ms\n", task->name, activation, elapsed_ms(task->start_time, now));
            fflush(stdout);

            task->body();

            printf("\n");
            fflush(stdout);
        pthread_mutex_unlock(&print_mutex);

        activation++;
    }
    return NULL;
}

int main(void)
{
    pthread_t thread1;
    pthread_t thread2;

    periodic_task_data_t task_data1;
    periodic_task_data_t task_data2;

    struct timespec first_activation; // zajednicki prvi trenutak aktivacije

    if (clock_gettime(CLOCK_REALTIME, &first_activation) != 0) // trenutno stvarno vrijeme
    {
        perror("clock_gettime");
        return 1;
    }

    timespec_add_us(&first_activation, FIRST_ACTIVATION_US); // programu se ostavlja vrijeme da napravi obe niti prije nego pocnu sa stvarim radom
                                                             // prva aktivacija oba taska bice 100 ms nakon pokretanja programa
    task_data1.name = "TASK 1";
    task_data1.period_us = TASK1_PERIOD_US;
    task_data1.next_activation = first_activation;
    task_data1.start_time = first_activation;
    task_data1.body = task1;

    task_data2.name = "TASK 2";
    task_data2.period_us = TASK2_PERIOD_US;
    task_data2.next_activation = first_activation;
    task_data2.start_time = first_activation;
    task_data2.body = task2;

    pthread_create(&thread1, NULL, periodic_thread, &task_data1);
    pthread_create(&thread2, NULL, periodic_thread, &task_data2);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    return 0;
}
