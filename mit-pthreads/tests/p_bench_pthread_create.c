/* ==== p_bench_pthread_create.c =============================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Benchmark mutex lock and unlock times
 *
 *  1.00 93/11/08 proven
 *      -Started coding this file.
 */

#define PTHREAD_KERNEL

#include <errno.h>
#include <pthread.h>
#include <stdio.h>

extern pthread_attr_t pthread_attr_default;

/* ==========================================================================
 * new_thread();
 */
void * new_thread(void * arg)
{
	PANIC();
}

/* ==========================================================================
 * usage();
 */
void usage(void)
{
	printf("p_bench_getpid [-d?] [-c count]\n");
    errno = 0;
}

main(int argc, char **argv)
{
	struct timeval starttime, endtime;
	pthread_mutex_t lock;
	pthread_t thread_id;
	int count = 10000;
	int debug = 0;
	int i;

	char word[256];

    /* Getopt variables. */
    extern int optind, opterr;
    extern char *optarg;

	pthread_init();
	/* Shut timer off */
	machdep_unset_thread_timer(NULL);
	pthread_attr_default.stackaddr_attr = &word;

	while ((word[0] = getopt(argc, argv, "c:d?")) != (char)EOF) {
		switch (word[0]) {
		case 'd':
			debug++;
			break;
		case 'c':
			count = atoi(optarg);
			break;
		case '?':
			usage();
			return(OK);
		default:
			usage();
			return(NOTOK);
		}
	}

	if (gettimeofday(&starttime, NULL)) {
	  perror ("gettimeofday");
	  return 1;
	}
	for (i = 0; i < count; i++) {
		if (pthread_create(&thread_id, & pthread_attr_default, new_thread, NULL)) {
			printf("Bad pthread create routine\n");
			exit(1);
		}
	}
	if (gettimeofday(&endtime, NULL)) {
	  perror ("gettimeofday");
	  return 1;
	}

	printf("%d getpid calls took %d usecs.\n", count, 
		(endtime.tv_sec - starttime.tv_sec) * 1000000 +
		(endtime.tv_usec - starttime.tv_usec));

	return 0;
}
