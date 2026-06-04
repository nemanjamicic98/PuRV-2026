#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>

static sigset_t sigset; //set signala

void wait_next_activation(void)
{
    int dummy;
    // ceka signal iz seta i u dummy upisuje
    sigwait(&sigset, &dummy); 
}

int start_periodic_timer(uint64_t offs, int period)
{
    struct itimerval t;

    t.it_value.tv_sec = offs / 1000000;
    t.it_value.tv_usec = offs % 1000000;
    t.it_interval.tv_sec = period / 1000000;
    t.it_interval.tv_usec = period % 1000000;
    
    // signal(SIGALRM, sighand);
    sigemptyset(&sigset); //inicijalizacija
    sigaddset(&sigset, SIGALRM); // ubacivanje SIGALRM u set
    sigprocmask(SIG_BLOCK, &sigset, NULL); // podesavanje koji se skup signala prati

    return setitimer(ITIMER_REAL, &t, NULL);
}
//task 1 sa periodom 60ms
void task1(void)
{
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 1000; j++);
        printf("1");
        fflush(stdout);
    }
}
//task 2 sa periodom 80ms
void task2(void)
{
    int i, j;
    for (i = 0; i < 6; i++) {
        for (j = 0; j < 10000; j++);
        printf("2");
        fflush(stdout);
    }
}

int main(int argc, char *argv[])
{
    int res;
    uint64_t ticks = 0;
    //prva aktivacija nakon 20
    res = start_periodic_timer(20000, 20000);
    if (res < 0)
    {
        perror("Start Periodic Timer");
        return -1;
    }

    while(1)
    {
        wait_next_activation();
        ticks++;
        //60ms/20ms 
        if (ticks % 3 == 0)
        {
            task1();
        }
        //80/20
        if (ticks % 4 == 0)
        {
            task2();
        }
    }

    return 0;
}