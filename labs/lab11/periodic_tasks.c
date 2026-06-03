//gcc -Wall periodic_task_posix_timer.c -lrt -o naziv_izlaznog_fajla
// -lrt linkuje librt biblioteku u kojoj se nalaze timer_create i timer_settime

#define _GNU_SOURCE

#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sched.h>
#include <string.h>
#include <sys/wait.h>

void task1(void)
{
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 1000; j++);
        printf("1");
        fflush(stdout);
    }
}

void task2(void)
{
    int i, j;
    for (i = 0; i < 6; i++) {
        for (j = 0; j < 10000; j++);
        printf("2");
        fflush(stdout);
    }
}

void task3(void)
{
    int i, j;
    for (i = 0; i < 6; i++) {
        for (j = 0; j < 100000; j++);
        printf("3");
        fflush(stdout);
    }
}

int start_periodic_timer(uint64_t period_us)
{
    struct itimerspec t;
    struct sigevent sigev;
    timer_t timer;

    t.it_value.tv_sec = period_us / 1000000;
    t.it_value.tv_nsec = (period_us % 1000000) * 1000;
    t.it_interval.tv_sec = period_us / 1000000;
    t.it_interval.tv_nsec = (period_us % 1000000) * 1000;
    
    memset(&sigev, 0, sizeof(struct sigevent));
    sigev.sigev_notify = SIGEV_SIGNAL; 
    sigev.sigev_signo = SIGALRM; 
    
    if (timer_create(CLOCK_MONOTONIC, &sigev, &timer) < 0) {
        perror("Timer Create");
        return -1;
    }
    
    return timer_settime(timer, 0, &t, NULL);
}

void run_task(void (*task_func)(void), int period_ms, int priority)
{
    struct sched_param param;
    sigset_t timer_sigset;
    int received_signal;

    param.sched_priority = priority;
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        perror("Greska: sched_setscheduler");
        printf("NAPOMENA: Morate pokrenuti program sa 'sudo' da biste koristili SCHED_FIFO!\n");
        exit(-1);
    }

    sigemptyset(&timer_sigset);
    sigaddset(&timer_sigset, SIGALRM);
    sigprocmask(SIG_BLOCK, &timer_sigset, NULL);

    if (start_periodic_timer((uint64_t)period_ms * 1000) < 0) {
        exit(-1);
    }

    while (1) {
        sigwait(&timer_sigset, &received_signal);
        task_func();
    }
}

int main(int argc, char *argv[])
{
    pid_t pid1, pid2, pid3;

    int prio1 = 90;
    int prio2 = 80;
    int prio3 = 70; 

    pid1 = fork();
    if (pid1 == 0) {
        run_task(task1, 40, prio1);
        exit(0);
    }

    pid2 = fork();
    if (pid2 == 0) {
        run_task(task2, 80, prio2);
        exit(0);
    }

    pid3 = fork();
    if (pid3 == 0) {
        run_task(task3, 120, prio3);
        exit(0);
    }

    wait(NULL);
    wait(NULL);
    wait(NULL);

    return 0;
}