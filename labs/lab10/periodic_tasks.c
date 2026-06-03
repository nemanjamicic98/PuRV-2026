//gcc -Wall periodic_task_posix_timer.c -lrt -o naziv_izlaznog_fajla
// -lrt linkuje librt biblioteku u kojoj se nalaze timer_create i timer_settime

#define _GNU_SOURCE

#include <sys/time.h>
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static sigset_t timer_sigset;

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

int start_periodic_timer(uint64_t offs, int period, int sig_num)
{
    struct itimerspec t;
    struct sigevent sigev;
    timer_t timer;
    int res;

    t.it_value.tv_sec = offs / 1000000;
    t.it_value.tv_nsec = (offs % 1000000) * 1000;
    t.it_interval.tv_sec = period / 1000000;
    t.it_interval.tv_nsec = (period % 1000000) * 1000;
    
    memset(&sigev, 0, sizeof(struct sigevent));
    sigev.sigev_notify = SIGEV_SIGNAL; 
    sigev.sigev_signo = sig_num;
    
    res = timer_create(CLOCK_MONOTONIC, &sigev, &timer);
    if (res < 0)
    {
        perror("Timer Create");
        exit(-1);
    }
    
    return timer_settime(timer, 0 /*TIMER_ABSTIME*/, &t, NULL);
}

int main(int argc, char *argv[])
{
    int res1, res2;
    int received_signal;

    sigemptyset(&timer_sigset);
    sigaddset(&timer_sigset, SIGRTMIN);     // Signal for task1
    sigaddset(&timer_sigset, SIGRTMIN + 1); // Signal for task2
    
    sigprocmask(SIG_BLOCK, &timer_sigset, NULL);

    res1 = start_periodic_timer(60000, 60000, SIGRTMIN);
    if (res1 < 0)
    {
        perror("Start Periodic Timer 1");
        return -1;
    }

    res2 = start_periodic_timer(80000, 80000, SIGRTMIN + 1);
    if (res2 < 0)
    {
        perror("Start Periodic Timer 2");
        return -1;
    }

    while(1)
    {
        sigwait(&timer_sigset, &received_signal);

        if (received_signal == SIGRTMIN) // visi prioritet
        {
            task1();
        }
        else if (received_signal == SIGRTMIN + 1)
        {
            task2();
        }
    }

    return 0;
}