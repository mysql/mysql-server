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

void* new_thread(void * new_buf)
{
	int i;

	for (i = 0; i < 10; i++) {
		pthread_yield();
	}
	printf("test_preemption PASSED\n");
	exit(0);
}

main()
{
	pthread_t thread;
	int error;

	printf("test_preemption START\n");

	if (pthread_create(&thread, NULL, new_thread, NULL)) {
		printf("pthread_create failed\n");
		exit(2);
	}

	while(1);
	exit(1);
}
