/* ==== p_bench_mutex.c =================================================
 * Copyright (c) 1993-1995 by Chris Provenzano, proven@athena.mit.edu
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
	printf("p_bench_yield [-d?] \\\n");
	printf("\t[-c count] \\\n");
	printf("\t[-C thread count] \\\n");
	printf("\t[-O optimization level]\n");
    errno = 0;
}

void *yield(void * arg)
{
	int i, * count;

	count = (int *)arg;
	for (i = 0; i < *count; i++) {
		pthread_yield();
	}
	return(NULL);
}

main(int argc, char **argv)
{
	struct timeval starttime, endtime;
	pthread_mutex_t lock;
	pthread_attr_t attr;
	pthread_t thread_id;
	int thread_count = 1;
	int optimization = 0;
	int count = 1000000;
	int i, debug = 0;

	char word[256];

    /* Getopt variables. */
    extern int optind, opterr;
    extern char *optarg;

	pthread_init();

	while ((word[0] = getopt(argc, argv, "C:O:c:d?")) != (char)EOF) {
		switch (word[0]) {
		case 'C':
			thread_count = atoi(optarg);
			break;
		case 'O':
			optimization = atoi(optarg);
			break;
		case 'c':
			count = atoi(optarg);
			break;
		case 'd':
			debug++;
			break;
		case '?':
			usage();
			return(OK);
		default:
			usage();
			return(NOTOK);
		}
	}

	pthread_attr_init(&attr);
    if (optimization > 0) {
		pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
	}
    if (optimization > 1) {
		pthread_attr_setfloatstate(&attr, PTHREAD_NOFLOAT);
	}

	pthread_mutex_init(&lock, NULL);
	if (gettimeofday(&starttime, NULL)) {
		perror ("gettimeofday");
		return 1;
	}
	for (i = 1; i < thread_count; i++) {
		if (pthread_create(&thread_id, &attr, yield, &count)) {
			perror ("pthread_create");
			return 1;
		}
		if (pthread_detach(thread_id)) {
			perror ("pthread_detach");
			return 1;
		}
	}
	if (pthread_create(&thread_id, &attr, yield, &count)) {
		perror ("pthread_create");
		return 1;
	}
	if (pthread_join(thread_id, NULL)) {
		perror ("pthread_join");
		return 1;
	}
	if (gettimeofday(&endtime, NULL)) {
		perror ("gettimeofday");
		return 1;
	}

	printf("%d pthread_yields took %d usecs.\n", count, 
		(endtime.tv_sec - starttime.tv_sec) * 1000000 +
		(endtime.tv_usec - starttime.tv_usec));

	return 0;
}
