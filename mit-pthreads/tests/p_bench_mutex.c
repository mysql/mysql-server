/* ==== p_bench_mutex.c =================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Benchmark mutex lock and unlock times
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
	pthread_mutex_t lock;
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

	pthread_mutex_init(&lock, NULL);
	if (gettimeofday(&starttime, NULL)) {
	  perror ("gettimeofday");
	  return 1;
	}
	for (i = 0; i < count; i++) {
		pthread_mutex_lock(&lock);
		pthread_mutex_unlock(&lock);
	}
	if (gettimeofday(&endtime, NULL)) {
	  perror ("gettimeofday");
	  return 1;
	}

	printf("%d mutex locks/unlocks no contention took %d usecs.\n", count, 
		(endtime.tv_sec - starttime.tv_sec) * 1000000 +
		(endtime.tv_usec - starttime.tv_usec));

	return 0;
}
