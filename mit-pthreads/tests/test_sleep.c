/* ==== test_switch.c ============================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test context switch functionality.
 *
 *  1.00 93/08/04 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <stdio.h>

const char buf[] = "abcdefghijklimnopqrstuvwxyz";
int fd = 1;

void* new_thread(void* arg)
{
	int i;

	for (i = 0; i < 10; i++) {
		write(fd, buf + (long) arg, 1);
		sleep(1);
	}
}

main()
{
	pthread_t thread;
	int count = 2;
	long i;

	pthread_init();

	printf("Going to sleep\n");
	sleep(10);
	printf("Done sleeping\n");

	for(i = 0; i < count; i++) {
		if (pthread_create(&thread, NULL, new_thread, (void *) i)) {
			printf("error creating new thread %d\n", i);
		}
	}
	pthread_exit(NULL);
	fprintf(stderr, "pthread_exit returned\n");
	exit(1);
}
