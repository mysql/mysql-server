/* ==== bench_read.c ============================================================
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

#define OK		0
#define NOTOK	-1
/* ==========================================================================
 * usage();
 */
void usage(void)
{
	printf("getopt [-d?] [-c count] [-s size]\n");
    errno = 0;
}

main(int argc, char **argv)
{
	struct timeval starttime, endtime;
	int count = 1000000;
	int debug = 0;
	int size = 1;
	int fd;
	int i;

	char word[8192];

    /* Getopt variables. */
    extern int optind, opterr;
    extern char *optarg;

	while ((word[0] = getopt(argc, argv, "s:c:d?")) != (char)EOF) {
		switch (word[0]) {
		case 'd':
			debug++;
			break;
		case 'c':
			count = atoi(optarg);
			break;
		case 's':
			if ((size = atoi(optarg)) > 8192) {
				size = 8192;
			}
			break;
		case '?':
			usage();
			return(OK);
		default:
			usage();
			return(NOTOK);
		}
	}

	if ((fd = open("/netbsd", O_RDONLY)) < OK) {
		printf("Error: open\n");
		exit(0);
	}

	if (gettimeofday(&starttime, NULL)) {
		printf("Error: gettimeofday\n");
		exit(0);
	}
	for (i = 0; i < count; i++) {
		if (read(fd, word, size) < OK) {
			printf("Error: read\n");
			exit(0);
		}
	}
	if (gettimeofday(&endtime, NULL)) {
		printf("Error: gettimeofday\n");
		exit(0);
	}

	printf("%d reads of /netbsd took %d usecs.\n", count, 
		(endtime.tv_sec - starttime.tv_sec) * 1000000 +
		(endtime.tv_usec - starttime.tv_usec));
}
