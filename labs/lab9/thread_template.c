// Kompajlirati sa 'gcc thread_template.c -lpthread -lrt -Wall'

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h> 
#include <unistd.h> 
#include <malloc.h>
#include <sys/time.h> 
#include <sys/resource.h> 
#include <pthread.h>
#include <limits.h>

#define PRE_ALLOCATION_SIZE (100*1024*1024) /* 100MB pagefault free buffer */
#define MY_STACK_SIZE       (100*1024)      /* 100 kB dodatak za stek */

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static int shared_val = 0;

// Struktura za prosledjivanje parametara programskim nitima
typedef struct {
    int sleep_seconds;
    int priority;
    int stack_size_add;
    int do_log;
    int increment_val;
    int thread_id;
} thread_params_t;

static void setprio(int prio, int sched)
{
    struct sched_param param;
    param.sched_priority = prio;
    if (sched_setscheduler(0, sched, &param) < 0)
        perror("sched_setscheduler");
}

void show_new_pagefault_count(const char* logtext, 
              const char* allowed_maj,
              const char* allowed_min)
{
    static int last_majflt = 0, last_minflt = 0;
    struct rusage usage;

    getrusage(RUSAGE_SELF, &usage);

    printf("%-30.30s: Pagefaults, Major:%ld (Allowed %s), " \
           "Minor:%ld (Allowed %s)\n", logtext,
           usage.ru_majflt - last_majflt, allowed_maj,
           usage.ru_minflt - last_minflt, allowed_min);

    last_majflt = usage.ru_majflt; 
    last_minflt = usage.ru_minflt;
}

static void prove_thread_stack_use_is_safe(int stacksize, int do_log)
{
    volatile char buffer[stacksize];
    int i;

    for (i = 0; i < stacksize; i += sysconf(_SC_PAGESIZE)) {
        buffer[i] = i;
    }
    
    // DODATO: Utisava warning kompajlera "unused-but-set-variable"
    (void)buffer[0];

    if(do_log)
        show_new_pagefault_count("Caused by using thread stack", "0", "0");
}

/*************************************************************/
/* Funkcija programske niti sa resursom */
static void *resource_thread_fn(void *args)
{
    thread_params_t *params = (thread_params_t *)args;
    struct timespec ts;
    ts.tv_sec = params->sleep_seconds;
    ts.tv_nsec = 0;

    setprio(params->priority, SCHED_RR);

    if (params->do_log) {
        printf("[Thread %d - Prio %d] RT-thread (Resource), stacksize=%i\n", 
               params->thread_id, params->priority, MY_STACK_SIZE + params->stack_size_add);
        show_new_pagefault_count("Caused by creating thread", ">=0", ">=0");
    }

    prove_thread_stack_use_is_safe(MY_STACK_SIZE + params->stack_size_add, params->do_log);

    // Zauzimanje resursa
    printf("[Thread %d] Pokusava da zauzme mutex...\n", params->thread_id);
    pthread_mutex_lock(&mtx);
    printf("[Thread %d] Zauzeo mutex. Inkrementuje shared_val i spava %d sekundi.\n", 
           params->thread_id, params->sleep_seconds);
    
    shared_val += params->increment_val;

    /* Spavanje sa zakljucanim mutexom - simulacija rada sa resursom */
    clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL);

    printf("[Thread %d] Otpusta mutex. shared_val=%i\n", params->thread_id, shared_val);
    pthread_mutex_unlock(&mtx);

    return NULL;
}

/*************************************************************/
/* Funkcija programske niti bez resursa */
static void *non_res_thread_fn(void *args)
{
    thread_params_t *params = (thread_params_t *)args;
    struct timespec ts;
    ts.tv_sec = params->sleep_seconds;
    ts.tv_nsec = 0;

    setprio(params->priority, SCHED_RR);

    if (params->do_log) {
        printf("[Thread %d - Prio %d] RT-thread (Non-Resource), stacksize=%i\n", 
               params->thread_id, params->priority, MY_STACK_SIZE + params->stack_size_add);
        show_new_pagefault_count("Caused by creating thread", ">=0", ">=0");
    }

    prove_thread_stack_use_is_safe(MY_STACK_SIZE + params->stack_size_add, params->do_log);

    // Samo simulacija rada koja trosi vreme/spava, bez blokiranja na mutexu
    printf("[Thread %d] Zapocinje rad (bez mutexa) i spava %d sekundi...\n", 
           params->thread_id, params->sleep_seconds);
    clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL);
    printf("[Thread %d] Zavrsio rad (bez mutexa).\n", params->thread_id);

    return NULL;
}

/*************************************************************/

static void error(int at)
{
    fprintf(stderr, "Some error occured at %d\n", at);
    exit(1);
}

// Genericka funkcija za pokretanje niti sa zadanom funkcijom i parametrima
static pthread_t start_rt_thread(void *(*thread_func)(void *), thread_params_t *params)
{
    pthread_t thread;
    pthread_attr_t attr;

    if (pthread_attr_init(&attr))
        error(1);
    
    // Inicijalizacija memorije potrebne za stek (osnovni + dodatni)
    if (pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + MY_STACK_SIZE + params->stack_size_add))
        error(2);
    
    pthread_create(&thread, &attr, thread_func, (void *)params);
    return thread;
}

static void configure_malloc_behavior(void)
{
    if (mlockall(MCL_CURRENT | MCL_FUTURE))
        perror("mlockall failed:");

    mallopt(M_TRIM_THRESHOLD, -1);
    mallopt(M_MMAP_MAX, 0);
}

static void reserve_process_memory(int size)
{
    int i;
    char *buffer = malloc(size);

    // DODATO: Sigurnosna provjera
    if (buffer == NULL) {
        perror("malloc nije uspio! Vjerovatno memlock limit");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < size; i += sysconf(_SC_PAGESIZE)) {
        buffer[i] = 0;
    }
    free(buffer);
}

int main(int argc, char *argv[])
{
    show_new_pagefault_count("Initial count", ">=0", ">=0");
    configure_malloc_behavior();
    show_new_pagefault_count("mlockall() generated", ">=0", ">=0");
    reserve_process_memory(PRE_ALLOCATION_SIZE);
    show_new_pagefault_count("malloc() and touch generated", ">=0", ">=0");
    reserve_process_memory(PRE_ALLOCATION_SIZE);
    show_new_pagefault_count("2nd malloc() and use generated", "0", "0");

    printf("\n--- POCETAK SCENARIJA INVERZIJE PRIORITETA ---\n\n");

    // Parametri za niti
    // Thread 1: Nizak prioritet, koristi resurs
    thread_params_t p_low = {
        .sleep_seconds = 6, 
        .priority = 10, 
        .stack_size_add = 0, 
        .do_log = 0, 
        .increment_val = 1,
        .thread_id = 1
    };
    
    // Thread 2: Visok prioritet, koristi resurs
    thread_params_t p_high = {
        .sleep_seconds = 2, 
        .priority = 30, 
        .stack_size_add = 1024, 
        .do_log = 0, 
        .increment_val = 5,
        .thread_id = 2
    };

    // Thread 3: Srednji prioritet, NE koristi resurs
    thread_params_t p_medium = {
        .sleep_seconds = 4, 
        .priority = 20, 
        .stack_size_add = 2048, 
        .do_log = 0, 
        .increment_val = 0,
        .thread_id = 3
    };

    // 1. Pokrecemo nit NIZAK prioritet. Ona ce zauzeti mutex i spavati dugo.
    pthread_t t_low = start_rt_thread(resource_thread_fn, &p_low);
    usleep(500000); // 0.5s pauza osigurava da Thread 1 prvi stigne do mutexa

    // 2. Pokrecemo nit VISOK prioritet. Ona ce preempt-ovati (prekinuti) Thread 1, 
    // doci do mutexa i zablokirati se, jer ga Thread 1 drzi.
    pthread_t t_high = start_rt_thread(resource_thread_fn, &p_high);
    usleep(500000); // 0.5s pauza

    // 3. Pokrecemo nit SREDNJI prioritet. Ona ce preempt-ovati Thread 1 
    // (koja se jedina trenutno moze izvrsavati jer Thread 2 ceka mutex).
    // Thread 3 ne trazi mutex, pa ce se uspesno izvrsavati i blokirati Thread 1 na duze vreme,
    // samim tim, Thread 3 de facto usporava i Thread 2 (visokog prioriteta). Ovo je inverzija!
    pthread_t t_medium = start_rt_thread(non_res_thread_fn, &p_medium);

    // Cekamo da se sve zavrse
    pthread_join(t_low, NULL);
    pthread_join(t_high, NULL);
    pthread_join(t_medium, NULL);

    printf("\nKrajnja vrednost deljenog resursa: %d\n", shared_val);
    printf("Press <ENTER> to exit\n");
    getchar();

    return 0;
}