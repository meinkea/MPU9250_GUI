#include <limits.h> // Tells you limits of your system
#include <pthread.h> // Allows for multithreading
#include <sched.h> // Set schedule
#include <stdio.h> // Standard lib
#include <stdlib.h> // malloc, abs, rand, abort, exit and atexit, system, sort, str to num
#include <sys/mman.h> // Memory mapping, MCL_CURRENT/FUTURE, mlockall, munlockall, shm_open


long long int period = 1;
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

// As the name implies
static void wait_rest_of_period(struct period_info *pinfo) {
	inc_period(pinfo); // Set up period info for next period before sleep
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &pinfo->next_period, NULL); // Do a sleep
	// clock,
	// flags,
	// sleep till (w/ TIMER_ABSTIME),
	// time remaining if interrupted by signal
}

void *thread_func(void *data) {
	
	struct timespec rinfo; // Resolution info 
	int i = clock_getres(CLOCK_MONOTONIC, &rinfo);

	printf("Time: %d.%09d sec\n", rinfo.tv_sec, rinfo.tv_nsec);

	if(-1 == i) {
		perror("Can't get clock resolution...\n  clock_getres failed\n");
		return NULL;
	}

	printf("The resolution of the pi is %d.%09d\n\n", rinfo.tv_sec, rinfo.tv_nsec);

	struct period_info pinfo; // Period info
	struct period_info * pinfop = &pinfo;
	periodic_task_init(&pinfo);

	printf("         0.%09lld\n", period);

	i=0;
	while(i<cycles) {
		// The real time task is done here
		// do_rt_task();
		printf("Time: %d.%09d sec\n", pinfop->next_period.tv_sec, pinfop->next_period.tv_nsec);
		

		// Wait for rest of period.
		wait_rest_of_period(&pinfo);
		++i;
	}
	
	//puts("Do RT specific stuff here");
	return NULL;
}


int main(int argc, char* argv[]) {
	struct sched_param param; 
	// Description:
	//   Contains scheduling parameters required for each supported scheduling policy
	// 
	// Always contains:
	//   int sched_priority ~ thread's execution scheduling priority (1 to 99)
	//
	pthread_attr_t attr;
	pthread_t thread;
	int ret;

	// Lock memory
	if(mlockall(MCL_CURRENT|MCL_FUTURE)==1) {
		// mlockall() is called to lock the processes's virtual address space into RAM
		//   MCL_CURRENT ~ Lock all pages currently mapped into process's address space
		//   MCL_FUTURE  ~
		//
		printf("mlockall failed: %m\n");
		exit(-2);
	}

	// Initialize pthread attributes (default values)
	ret = pthread_attr_init(&attr);
	if (ret) {
		printf("init pthread attributes failed\n");
		goto out;
	}

	// Set a specific stack size
	ret = pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN);
	if (ret) {
		printf("pthread setstacksize failed\n");
		goto out;
	}

	// Set scheduler policy and priority of pthread
	ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
	if (ret) {
		printf("pthread setschedpolicy failed\n");
		goto out;
	}	
	param.sched_priority = 80;
	ret = pthread_attr_setschedparam(&attr, &param);
	if (ret) {
		printf("pthread setschedparam failed\n");
		goto out;
	}
	// Use scheduling parameters of attr
        ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
	if (ret) {
		printf("pthread setinheritsched failed\n");
		goto out;
	}

	// Create a pthread with specified attributes
	 ret = pthread_create(&thread, &attr, thread_func, NULL);
	 if (ret) {
		 printf("create pthread failed\n");
		 goto out;
	}
	
	// Join the thread and wait until it is done
	ret = pthread_join(thread, NULL);
	if (ret)
		printf("join pthread failed: %m\n");

out:
	return ret;

}

