/* ==== test_create.c ============================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test pthread_create() and pthread_exit() calls.
 *
 *  1.00 93/08/03 proven
 *      -Started coding this file.
 */

#define PTHREAD_KERNEL
#include <pthread.h>
#include <stdio.h>

void* new_thread(void* arg)
{
	int i;

	printf("New thread was passed arg address %x\n", arg);
	printf("New thread stack at %x\n", &i);
	return(NULL);
	PANIC();
}

main()
{
	pthread_t thread;
	int i;

	printf("Original thread stack at %x\n", &i);
	if (pthread_create(&thread, NULL, new_thread, (void *)0xdeadbeef)) {
		printf("Error: creating new thread\n");
	}
	pthread_exit(NULL);
	PANIC();
}
