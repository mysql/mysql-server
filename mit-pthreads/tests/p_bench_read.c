/* ==== p_bench_read.c ============================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Benchmark reads of /dev/null. Gives a good aprox. of
 *				 syscall times.
 *
 *  1.00 93/08/01 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#define	OK		0
#define	NOTOK  -1

/* ==========================================================================
 * usage();
 */
void usage(void)
{
	printf("p_bench_read [-d?] [-c count] [-s size] [-f file]\n");
    errno = 0;
}

main(int argc, char **argv)
{
	struct timeval starttime, endtime;
	char *infile = "/dev/null";
	int count = 1000000;
	int debug = 0;
	int size = 1;
	int fd;
	int i;

	char word[16384], *word_ptr;

    /* Getopt variables. */
    extern int optind, opterr;
    extern char *optarg;

	pthread_init();

	while ((word[0] = getopt(argc, argv, "c:df:s:?")) != (char)EOF) {
		switch (word[0]) {
		case 'c':
			count = atoi(optarg);
			break;
		case 'd':
			debug++;
			break;
		case 'f':
			infile = optarg;
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

	/* Align buffer boundary to a page boundary */
	word_ptr = (char *)(((size_t) word + 4095) & ~4095);

	if ((fd = open(infile, O_RDONLY)) < OK) {
	  perror ("open");
	  return 1;
	}

	if (gettimeofday(&starttime, NULL)) {
	  perror ("gettimeofday");
	  return 1;
	}

	for (i = 0; i < count; i++) {
		if (read(fd, word_ptr, size) < OK) {
			printf("Error: read\n");
			exit(0);
		}
	}

	if (gettimeofday(&endtime, NULL)) {
	  perror ("gettimeofday");
	  return 1;
	}

	printf("%d reads of %s took %d usecs.\n", count, infile,
		(endtime.tv_sec - starttime.tv_sec) * 1000000 +
		(endtime.tv_usec - starttime.tv_usec));

	return 0;
}
