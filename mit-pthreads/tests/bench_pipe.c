/* ==== bench_pipe.c ============================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Benchmark reads of /dev/null. Gives a good aprox. of
 *				 syscall times.
 *
 *  1.00 93/08/01 proven
 *      -Started coding this file.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#define OK 0
#define NOTOK -1

/* ==========================================================================
 * usage();
 */
void usage(void)
{
	printf("bench_pipe [-d?] [-c count]\n");
    errno = 0;
}

main(int argc, char **argv)
{
	struct timeval starttime, endtime;
	char buf[1];
	int count = 1000;
	int debug = 0;
	int fd0[2];
	int fd1[2];
	int i;

	char word[256];

    /* Getopt variables. */
    extern int optind, opterr;
    extern char *optarg;

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

	if ((pipe(fd0) < OK) || (pipe(fd1) < OK)) {
		printf("Error: pipe\n");
		exit(0);
	}

	switch (fork()) {
	case NOTOK:
		printf("Error: fork\n");
		exit(0);
	case OK: /* Child */
		for (i = 0; i < count; i++) {
			if (read(fd1[0], buf, 1) < OK) {
				printf("Error: child read\n");
				exit(0);
			}
			if (write(fd0[1], buf, 1) < OK) {
				printf("Error: child write\n");
				exit(0);
			}
		}
		exit(0);
		break;
	default:
		break;
	}

	if (gettimeofday(&starttime, NULL)) {
		printf("Error: gettimeofday\n");
		exit(0);
	}
	count --;
	if (write(fd1[1], buf, 1) < OK) {
		perror("first parent write");
		exit(0);
	}
	for (i = 0; i < count; i++) {
		if (read(fd0[0], buf, 1) < OK) {
			printf("Error: parent read\n");
			exit(0);
		}
		if (write(fd1[1], buf, 1) < OK) {
			printf("Error: parent write\n");
			exit(0);
		}
	}
	if (gettimeofday(&endtime, NULL)) {
		printf("Error: gettimeofday\n");
		exit(0);
	}

	printf("%d ping pong tests took %d usecs.\n", count, 
		(endtime.tv_sec - starttime.tv_sec) * 1000000 +
		(endtime.tv_usec - starttime.tv_usec));
}
