// gcc procesi.c -o procesi -Wall -Wextra -lrt
// sudo taskset -c 0 ./procesi

#include <errno.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define NSEC_PER_SEC          1000000000ULL

#define FIRST_ACTIVATION_US   100000ULL   // prva aktivacija nakon 100 ms

#define TASK1_PERIOD_US        40000ULL   // 40 ms
#define TASK2_PERIOD_US        80000ULL   // 80 ms
#define TASK3_PERIOD_US       120000ULL   // 120 ms

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

void task3(void)
{
    int i, j;

    for (i = 0; i < 6; i++)
    {
        for (j = 0; j < 100000; j++);

        printf("3");
        fflush(stdout);
    }
}

typedef struct
{
    const char *name;
    uint64_t period_us;
    int priority;
    struct timespec next_activation;
    struct timespec start_time;
    void (*body)(void);
} periodic_process_data_t;

static void timespec_add_us(struct timespec *t, uint64_t us)
{
    uint64_t ns = us * 1000ULL + (uint64_t)t->tv_nsec;

    while (ns >= NSEC_PER_SEC)
    {
        ns -= NSEC_PER_SEC;
        t->tv_sec++;
    }

    t->tv_nsec = (long)ns;
}

static double elapsed_ms(struct timespec start, struct timespec now)
{
    return (now.tv_sec - start.tv_sec) * 1000.0 + (now.tv_nsec - start.tv_nsec) / 1000000.0;
}

static int wait_next_activation(periodic_process_data_t *task)
{
    int ret;

    do
    {
        ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &task->next_activation, NULL);
    } while (ret == EINTR);

    timespec_add_us(&task->next_activation, task->period_us);

    return 0;
}

static int set_fifo_priority(int priority)
{
    struct sched_param param;

    param.sched_priority = priority;

    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1)
    {
        perror("sched_setscheduler");
        return -1;
    }

    return 0;
}

static int periodic_process(periodic_process_data_t *task)
{
    struct timespec now;
    int activation = 0;

    if (set_fifo_priority(task->priority) != 0)
    {
        return -1;
    }

    printf("%s pokrenut: PID = %ld, period = %llu ms, prioritet = %d\n", task->name, (long)getpid(), (unsigned long long)(task->period_us / 1000ULL), task->priority);
    fflush(stdout);

    while (1)
    {
        if (wait_next_activation(task) != 0)
        {
            return -1;
        }

        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        {
            perror("clock_gettime");
            return -1;
        }

        printf("%s | PID = %ld | aktivacija %2d | vrijeme = %7.2f ms | ", task->name, (long)getpid(), activation, elapsed_ms(task->start_time, now));
        fflush(stdout);

        task->body();

        printf("\n");
        fflush(stdout);

        activation++;
    }

    return 0;
}

int main(void)
{
    pid_t children[3];
    int i;
    int status;
    struct timespec first_activation;

    periodic_process_data_t tasks[3] =
    {
        {
            .name = "TASK 1",
            .period_us = TASK1_PERIOD_US,
            .priority = 30,
            .body = task1
        },
        {
            .name = "TASK 2",
            .period_us = TASK2_PERIOD_US,
            .priority = 20,
            .body = task2
        },
        {
            .name = "TASK 3",
            .period_us = TASK3_PERIOD_US,
            .priority = 10,
            .body = task3
        }
    };

    if (clock_gettime(CLOCK_MONOTONIC, &first_activation) != 0) // CLOCK_MONOTONIC nije osjetljiv na promjenu sistemskog sata
    {
        perror("clock_gettime");
        return -1;
    }

    timespec_add_us(&first_activation, FIRST_ACTIVATION_US);

    for (i = 0; i < 3; i++)
    {
        tasks[i].next_activation = first_activation;
        tasks[i].start_time = first_activation;
    }

    for (i = 0; i < 3; i++)
    {
        children[i] = fork();

        if (children[i] < 0)
        {
            perror("fork");
            return -1;
        }

        if (children[i] == 0)
        {
            return periodic_process(&tasks[i]);
        }
    }

    printf("Parent PID = %ld je kreirao tri periodicna procesa.\n", (long)getpid());
    fflush(stdout);

    for (i = 0; i < 3; i++)
    {
        waitpid(children[i], &status, 0);
    }

    return 0;
}
