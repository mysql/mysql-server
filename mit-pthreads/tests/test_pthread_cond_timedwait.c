/* ==== test_pthread_cond.c =========================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test pthread_cond(). Run this after test_create()
 *
 *  1.23 94/05/04 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <stdio.h>
#include <errno.h>

#ifndef ETIME
#define ETIME ETIMEDOUT  
#endif

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void* thread_1(void * new_buf)
{
	pthread_mutex_lock(&mutex);
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
}

void* thread_2(void * new_buf)
{
	sleep(1);
	pthread_mutex_lock(&mutex);
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
}

main()
{
	struct timespec abstime = { 0, 0 };
	struct timeval curtime;
	pthread_t thread;
	int error;

	pthread_init(); 
	printf("pthread_cond_timedwait START\n");

	pthread_mutex_lock(&mutex);
	gettimeofday(&curtime, NULL);
	abstime.tv_sec = curtime.tv_sec + 5; 

	/* Test a condition timeout */
	if (pthread_cond_timedwait(&cond, &mutex, &abstime) != ETIME) {
		printf("pthread_cond_timedwait failed to timeout\n");
		printf("pthread_cond_timedwait FAILED\n");
		pthread_mutex_unlock(&mutex);
		exit(1);
	}
	printf("Got first timeout ok\n");	/* Added by monty */
	/* Test a normal condition signal */
	if (pthread_create(&thread, NULL, thread_1, NULL)) {
		printf("pthread_create failed\n");
		exit(2);
	}

	abstime.tv_sec = curtime.tv_sec + 10; 
	if (pthread_cond_timedwait(&cond, &mutex, &abstime)) {
		printf("pthread_cond_timedwait #1 timedout\n");
		printf("pthread_cond_timedwait FAILED\n");
		pthread_mutex_unlock(&mutex);
		exit(1);
	}

	/* Test a normal condition signal after a sleep */
	if (pthread_create(&thread, NULL, thread_2, NULL)) {
		printf("pthread_create failed\n");
		exit(2);
	}

	pthread_yield();

	abstime.tv_sec = curtime.tv_sec + 10; 
	if (pthread_cond_timedwait(&cond, &mutex, &abstime)) {
		printf("pthread_cond_timedwait #2 timedout\n");
		printf("pthread_cond_timedwait FAILED\n");
		pthread_mutex_unlock(&mutex);
		exit(1);
	}

	printf("pthread_cond_timedwait PASSED\n");
	pthread_mutex_unlock(&mutex);
	exit(0);
}
