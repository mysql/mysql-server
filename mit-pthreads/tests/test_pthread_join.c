/* ==== test_pthread_join.c =================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test pthread_join(). Run this after test_create()
 *
 *  1.23 94/05/04 proven
 *      -Started coding this file.
 */

#define PTHREAD_KERNEL
#include <pthread.h>
#include <stdio.h>

/* This thread yields so the creator has a live thread to wait on */
void* new_thread_1(void * new_buf)
{
	int i;

	sprintf((char *)new_buf, "New thread %%d stack at %x\n", &i);
	pthread_yield();
	return(new_buf);
	PANIC();
}

/* This thread doesn't yield so the creator has a dead thread to wait on */
void* new_thread_2(void * new_buf)
{
	int i;

	sprintf((char *)new_buf, "New thread %%d stack at %x\n", &i);
	return(new_buf);
	PANIC();
}

main()
{
	char buf[256], *status;
	pthread_t thread;
	int debug = 1;
	int i = 0;

	pthread_init(); 

	printf("Original thread stack at %x\n", &i);
	if (pthread_create(&thread, NULL, new_thread_1, (void *)buf) == OK) {
		if (pthread_join(thread, (void **)(&status)) == OK) {
			if (debug) { printf(status, ++i); }
		} else {
			printf("ERROR: Joining with new thread #1.\n");
			printf("FAILED: test_pthread_join\n");
			exit(1);
		} 
	} else {
		printf("ERROR: 	Creating new thread #1\n");
		printf("FAILED: test_pthread_join\n");
		exit(2);
	}


	/* Now have the created thread finishing before the join. */
	if (pthread_create(&thread, NULL, new_thread_2, (void *)buf) == OK){
		pthread_yield();
		if (pthread_join(thread, (void **)(&status)) == OK) {
			if (debug) { printf(status, ++i); }
		} else {
			printf("ERROR: Joining with new thread #2.\n");
			printf("FAILED: test_pthread_join\n");
			exit(1);
		} 
	} else {
		printf("ERROR: 	Creating new thread #2\n");
		printf("FAILED: test_pthread_join\n");
		exit(2);
	}
	printf("test_pthread_join PASSED\n");
	pthread_exit(NULL);
}

