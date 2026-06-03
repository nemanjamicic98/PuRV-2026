// Kompajlirati sa:
// gcc thread_template.c -o thread_template -pthread -lrt -Wall -Wextra
//
// Pokrenuti sa jednim CPU jezgrom i RT privilegijama:
// sudo taskset -c 0 ./thread_template

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>      
#include <unistd.h>      
#include <malloc.h>
#include <sys/time.h>       
#include <sys/resource.h>  
#include <pthread.h>
#include <limits.h>
#include <sched.h>          
#include <time.h>   

#define PRE_ALLOCATION_SIZE (100 * 1024 * 1024) /* 100 MB pagefault free buffer */
#define MY_STACK_SIZE       (100 * 1024)        /* 100 kB dodatak za stek */


// LOW aktivno koristi dijeljeni resurs
// MEDIUM aktivno koristi CPU, ali ne koristi mutex niti shared_val
#define LOW_BUSY_SECONDS     4
#define MEDIUM_BUSY_SECONDS  4

static pthread_mutex_t mtx;
static int shared_val = 0;

typedef struct {
	const char *name;
	int sleep_time;
	int priority;
	int additional_stack_size;
	int do_log;
	int shared_val_increment;
} params;

// =================================================================

static void setprio(int prio, int sched)
{
	struct sched_param param;
	param.sched_priority = prio;
	if (sched_setscheduler(0, sched, &param) < 0)
		perror("sched_setscheduler");
}

void show_new_pagefault_count(const char *logtext, const char *allowed_maj, const char *allowed_min)
{
	static int last_majflt = 0, last_minflt = 0;
	struct rusage usage;

	getrusage(RUSAGE_SELF, &usage);

	printf("%-30.30s: Pagefaults, Major:%ld (Allowed %s), "
	       "Minor:%ld (Allowed %s)\n",
	       logtext,
	       usage.ru_majflt - last_majflt, allowed_maj,
	       usage.ru_minflt - last_minflt, allowed_min);

	last_majflt = usage.ru_majflt;
	last_minflt = usage.ru_minflt;
}

static void prove_thread_stack_use_is_safe(int stacksize, int do_log)
{
	volatile char buffer[stacksize];
	int i;

	for (i = 0; i < stacksize; i += sysconf(_SC_PAGESIZE))
		buffer[i] = i;
	
	(void)buffer[0];

	if (do_log)
		show_new_pagefault_count("Caused by using thread stack", "0", "0");
}

static void error(int at)
{
	fprintf(stderr, "Some error occurred at %d\n", at);
	exit(1);
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
	char *buffer;
	buffer = malloc(size);
	if (buffer == NULL)
		error(14);
	for (i = 0; i < size; i += sysconf(_SC_PAGESIZE))
		buffer[i] = 0;
	free(buffer);
}

// =================================================================

static void sleep_for_seconds(int seconds)
{
	struct timespec ts;
	ts.tv_sec = seconds;
	ts.tv_nsec = 0;

	if (seconds <= 0)
		return;
		
	if (clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL) != 0)
		error(11);
}

static void busy_cpu_seconds(int seconds) // aktivni posao, nit stvarno trosi CPU
{
	struct timespec start; // cpu vrijeme niti na pocetku rada
	struct timespec now; // trenutno cpu vrijeme niti tokom rada
	double elapsed; // koliko vremena je proslo od start do now
	volatile unsigned long something = 0;
	unsigned long i;

	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start) != 0) // pamti pocetno cpu vrijeme trenutne niti
		error(12);

	// aktivni posao koji se ponavlja dok nit ne potrosi trazeni broj sekundi cpu vremena
	do {
		for (i = 0; i < 100000UL; i++)
			something += i;

		if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now) != 0)
			error(13);

		elapsed = (double)(now.tv_sec - start.tv_sec) +
			  (double)(now.tv_nsec - start.tv_nsec) / 1000000000.0;

		} while (elapsed < seconds);
}

static void *resource_thread_fn(void *args)
{
	params *local_args = (params *)args;

	setprio(local_args->priority, SCHED_RR);

	prove_thread_stack_use_is_safe(MY_STACK_SIZE + local_args->additional_stack_size, local_args->do_log); // priprema steka

	printf("%s: spavam %d sekundi prije svog posla.\n", local_args->name, local_args->sleep_time);
	fflush(stdout);
	sleep_for_seconds(local_args->sleep_time);
	
	// zavrseno je pocetno cekanje
	printf("%s: probudila sam se i pokusavam zakljucati mutex.\n", local_args->name);
	fflush(stdout);

	if (pthread_mutex_lock(&mtx))
		error(3);

	printf("%s: dobila sam mutex. shared_val = %d\n", local_args->name, shared_val);
	fflush(stdout);

	// provjerava da li je trenutna nit LOW, a ona je u main dobila minimalni prioritet 
	if (local_args->priority == sched_get_priority_min(SCHED_RR)) 
	{
		printf("%s: aktivno koristim dijeljeni resurs %d sekunde CPU vremena.\n", local_args->name, LOW_BUSY_SECONDS);
		fflush(stdout);

		busy_cpu_seconds(LOW_BUSY_SECONDS); // LOW aktivno trosi CPU dok je mutex zakljucan
	}

	shared_val += local_args->shared_val_increment;

	printf("%s: shared_val += %d, sada je shared_val = %d\n", local_args->name, local_args->shared_val_increment, shared_val);
	fflush(stdout);

	printf("%s: sada oslobadjam mutex.\n", local_args->name);
	fflush(stdout);

	if (pthread_mutex_unlock(&mtx))
		error(4);

	printf("%s: zavrsavam.\n", local_args->name);
	fflush(stdout);

	return NULL;
}

static void *non_res_thread_fn(void *args)
{
	params *local_args = (params *)args;

	setprio(local_args->priority, SCHED_RR);

	prove_thread_stack_use_is_safe(MY_STACK_SIZE + local_args->additional_stack_size, local_args->do_log);

	printf("%s: spavam %d sekundi prije svog posla.\n", local_args->name, local_args->sleep_time);
	fflush(stdout);

	sleep_for_seconds(local_args->sleep_time);

	printf("%s: probudila sam se; ne koristim mutex niti shared_val.\n", local_args->name);
	printf("%s: aktivno radim %d sekunde CPU vremena.\n", local_args->name, MEDIUM_BUSY_SECONDS);
	fflush(stdout);

	busy_cpu_seconds(MEDIUM_BUSY_SECONDS);

	printf("%s: zavrsila sam svoj posao.\n", local_args->name);
	fflush(stdout);

	return NULL;
}

static pthread_t start_rt_thread(void *(*thread_fn)(void *), params *thread_args)
{
	pthread_t thread;
	pthread_attr_t attr;

	if (pthread_attr_init(&attr))
		error(1);

	// odredjuje se velicina steka nove niti
	if (pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + MY_STACK_SIZE + thread_args->additional_stack_size))
		error(2);

	if (pthread_create(&thread, &attr, thread_fn, thread_args))
		error(5);

	if (pthread_attr_destroy(&attr))
		error(6);

	return thread;
}

int main()
{
	pthread_t low_thread;
	pthread_t high_thread;
	pthread_t medium_thread;

	params low_priority = 
	{
		.name = "LOW",
		.sleep_time = 1, // spava najmanje i prva dolazi na red
		.priority = sched_get_priority_min(SCHED_RR), // dobija najmanji rt prioritet
		.additional_stack_size = 0, // ne trazi dodatni stek iznad osnovnog
		.do_log = 0, // ne ispisuje pagefault info kad priprema svoj stek
		.shared_val_increment = 1 // povecava zajednicku promjenljivu za 1
	};

	params high_priority = 
	{
		.name = "HIGH",
		.sleep_time = 2,
		.priority = sched_get_priority_max(SCHED_RR), // 99
		.additional_stack_size = 0,
		.do_log = 0,
		.shared_val_increment = 100
	};

	params medium_priority = 
	{
		.name = "MEDIUM",
		.sleep_time = 3,
		.priority = (sched_get_priority_min(SCHED_RR) + sched_get_priority_max(SCHED_RR)) / 2,
		.additional_stack_size = 0,
		.do_log = 0,
		.shared_val_increment = 0
	};

	// =================================================================

	show_new_pagefault_count("Initial count", ">=0", ">=0");

	configure_malloc_behavior();

	show_new_pagefault_count("mlockall() generated", ">=0", ">=0");

	reserve_process_memory(PRE_ALLOCATION_SIZE);

	show_new_pagefault_count("malloc() and touch generated", ">=0", ">=0");

	reserve_process_memory(PRE_ALLOCATION_SIZE);

	show_new_pagefault_count("2nd malloc() and use generated", "0", "0");

	printf("\n\nLook at the output of ps -leyf, and see that the "
		"RSS is now about %d [MB]\n", PRE_ALLOCATION_SIZE / (1024 * 1024));
	
	// =================================================================

	if (pthread_mutex_init(&mtx, NULL))
		error(7);

	printf("\nDemonstracija inverzije prioriteta:\n");
	printf("LOW:    sleep_time = %d, priority = %d\n", low_priority.sleep_time, low_priority.priority);
	printf("HIGH:   sleep_time = %d, priority = %d\n", high_priority.sleep_time, high_priority.priority);
	printf("MEDIUM: sleep_time = %d, priority = %d\n\n", medium_priority.sleep_time, medium_priority.priority);
	fflush(stdout);

	low_thread = start_rt_thread(resource_thread_fn, &low_priority);
	high_thread = start_rt_thread(resource_thread_fn, &high_priority);
	medium_thread = start_rt_thread(non_res_thread_fn, &medium_priority);

	if (pthread_join(low_thread, NULL))
		error(8);

	if (pthread_join(high_thread, NULL))
		error(9);

	if (pthread_join(medium_thread, NULL))
		error(10);

	printf("\nSve niti su zavrsile.\n");
	printf("Konacna vrijednost shared_val = %d\n", shared_val);
	printf("Ocekivana vrijednost je 101: LOW dodaje 1, HIGH dodaje 100.\n");

	if (pthread_mutex_destroy(&mtx))
		error(15);

	return 0;
}
