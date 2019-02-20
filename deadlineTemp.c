#define _GNU_SOURCE
#include <unistd.h> // NULL pointer, symbolic const for I/O: SEEK_SET R_OK, type def size_t
		    // gethostname(), fork()
#include <stdio.h> // Standard lib
#include <stdlib.h> // malloc, abs, rand, abort, exit and atexit, system, sort, str to num

#include <string.h>
#include <time.h>

//#include <linux/stdio.h> 
//#include <linux/stdlib.h>
#include <linux/unistd.h>

#include <limits.h> // Tells you limits of your system
#include <asm/types.h>
#include <sys/syscall.h>
#include <sys/mman.h> // Memory mapping, MCL_CURRENT/FUTURE, mlockall, munlockall, shm_open

#include <sched.h> // Set schedule
#include <pthread.h> // Allows for multithreading

#define gettid() syscall(__NR_gettid)

#define SCHED_DEADLINE 6

#ifdef __arm__
#define __NR_sched_setattr	380
#define __NR_sched_getattr	381
#endif

static volatile int done;

struct sched_attr { 
	__u32 size; // Size of this stucture

	__u32 sched_policy; // Sched Policy
	__u64 sched_flags; // SCHED_RESET_ON_FORK -> child no inherit parent privileged sched pol

	// SCHED_NORMAL & SCHED_BATCH
	__s32 sched_nice; // Lower has Priority (-20>19)

	// SCHED_FIFO & SCHED_RR
	__u32 sched_priority; // Higher has Priority (1-99)

	// SCHED_DEADLINE (nsec)
	__u64 sched_runtime; // 
	__u64 sched_deadline; // 
	__u64 sched_period; // 
};

int sched_setattr(pid_t pid, const struct sched_attr *attr, unsigned int flags) {
	return syscall(__NR_sched_setattr, pid, attr, flags);
}

int sched_getattr(pid_t pid, struct sched_attr *attr, unsigned int size, unsigned int flags) {
	return syscall(__NR_sched_getattr, pid, attr, size, flags);
}

__u64 runtime = 10*1000*1000;
long period = 0;
int cycles = 10;

// Object for time left in period
struct period_info{
	struct timespec next_period;
	// struct timespec {
	// 	time_t	tv_sec; // seconds since the great epoch (Jan 1, 1970)
	// 	long	tv_nsec; // nanoseconds
	long period_ns;
};

// Move to period_info next period and tick tv_sec the clock after a second has passed
static void inc_period(struct period_info *pinfo)
{	
	long long int alarm = 0;
	static long long int i = 0;
	alarm =  (pinfo->next_period.tv_nsec)-period*i;
	if(alarm < 0) {
		puts(">>>Deadline NOT MET!!!");
	}

	pinfo->next_period.tv_nsec += pinfo->period_ns;

	// Check if tv_nsec holds a whole second
	while(pinfo->next_period.tv_nsec >= 1000000000) {
		pinfo->next_period.tv_sec++; // Increment tv_sec by 1 second
		pinfo->next_period.tv_nsec -= 1000000000; // Decrement tv_nsec by 1 second
		//i=0;
	}
}

// Setup for period length
static void periodic_task_init(struct period_info *pinfo) {
	pinfo->period_ns = period; // Period size
	clock_gettime(CLOCK_MONOTONIC, &(pinfo->next_period)); // Retrieve time for time_t
	// CLOCK_MONOTONIC cannat be changed
}

struct timespec ts; // Timestamp
struct timespec tp; // Timeprevious

void *run_deadline(void *data) {
	struct sched_attr attr;
	int x = 0;
	int ret;
	unsigned int flags = 0;

	tp.tv_sec = 0;
	tp.tv_nsec = 0;

	printf("deadline thread started [%ld]\n", gettid());

	attr.size = sizeof(attr);
	attr.sched_flags = 0;
	attr.sched_nice = 0;
	attr.sched_priority = 0;

	// This creates a 10ms/30ms reservation
	attr.sched_policy = SCHED_DEADLINE;
	attr.sched_runtime = 10*1000*1000;
	attr.sched_period = attr.sched_deadline = (__u64) deadln;

	puts("Setting sched attr...");
	ret = sched_setattr(0, &attr, flags);
	if (ret<0) {
		done = 0;
		perror("sched_setattr");
		exit(-1);
	}

	puts("Running DEADLINE thread");

	struct period_info pinfo; // Period info
	struct period_info * pinfop = &pinfo;
	periodic_task_init(&pinfo);

	while(!done) {
		clock_gettime(CLOCK_MONOTONIC, &ts);
		printf("Time: %d.%09d sec | ", ts.tv_sec, ts.tv_nsec);
		printf("Dif of %d.%09d sec | ", (ts.tv_sec - tp.tv_sec), (ts.tv_sec - tp.tv_sec));
		printf("Lag of %d", pinfop->next_period.tv_sec - ts.tv_sec);
		printf(".%09d sec", pinfop->next_period.tv_sec - ts.tv_nsec);

		x++;
		if(x==10) {
			done = 1;
		}

		tp.tv_sec = ts.tv_sec;
		tp.tv_nsec = ts.tv_nsec;

		inc_period(pinfop); // Set up period info for next period before sleep

		sched_yield();
	}
	printf("deadline thread dies [%ld]\n", gettid());
	return NULL;
}

int main(void) {
	printf("My process ID: %d\n", getpid());
	printf("My parent's ID: %d\n", getppid());

	pthread_t thread;

	printf("main thread [%ld]\n", gettid());

	pthread_create(&thread, NULL, run_deadline, NULL);

	pthread_join(thread, NULL);

	printf("main dies [%ld]\n", gettid());
	return 0;
}
