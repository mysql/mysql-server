/* ==== p_bench_semaphore.c =================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Benchmark semaphore Test and Set/ CLear times
 *
 *  1.00 93/11/08 proven
 *      -Started coding this file.
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>

#define	OK		0
#define	NOTOK  -1

/* ==========================================================================
 * usage();
 */
void usage(void)
{
	printf("getopt [-d?] [-c count]\n");
    errno = 0;
}

main(int argc, char **argv)
{
	struct timeval starttime, endtime;
	semaphore lock = SEMAPHORE_CLEAR;
	semaphore *lock_addr;
	int count = 1000000;
	int debug = 0;
	int i;

	char word[256];

    /* Getopt variables. */
    extern int optind, opterr;
    extern char *optarg;

	pthread_init();

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

	lock_addr = &lock;
	if (gettimeofday(&starttime, NULL)) {
		perror ("gettimeofday");
		return 1;
	}
	for (i = 0; i < count; i++) {
		if (SEMAPHORE_TEST_AND_SET(lock_addr)) {
			printf("Semaphore already locked error\n");
			return 1;
		}
		SEMAPHORE_RESET(lock_addr);
	}
	if (gettimeofday(&endtime, NULL)) {
		perror ("gettimeofday");
		return 1;
	}

	printf("%d locks/unlocks of a semaphore took %d usecs.\n", count, 
		(endtime.tv_sec - starttime.tv_sec) * 1000000 +
		(endtime.tv_usec - starttime.tv_usec));

	return 0;
}
