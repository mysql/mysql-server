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
#include <errno.h>

#define OK      0
#define NOTOK  -1

const char buf[] = "abcdefghijklmnopqrstuvwxyz";
char x[sizeof(buf)];
int fd = 1;

/* ==========================================================================
 * usage();
 */
void usage(void)
{
    printf("test_switch [-d?] [-c count]\n");
	printf("count must be between 2 and 26\n");
    errno = 0;
}

void* new_thread(void* arg)
{
	while(1) {
		write (fd, (char *) arg, 1);
		x[(char *)arg - buf] = 1;
	}
	fprintf(stderr, "Compiler error\n");
	exit(1);
}

main(int argc, char **argv)
{
	pthread_t thread;
	int count = 2;
	int debug = 0;
	int eof = 0;
	long i;

	/* Getopt variables. */
	extern int optind, opterr;
	extern char *optarg;

	while (!eof)
	  switch (getopt (argc, argv, "c:d?"))
	    {
	    case EOF:
	      eof = 1;
	      break;
	    case 'd':
	      debug++;
	      break;
	    case 'c':
	      count = atoi(optarg);
	      if ((count > 26) || (count < 2)) {
			  count = 2;
	      }
	      break;
	    case '?':
	      usage();
	      return(OK);
	    default:
	      usage();
	      return(NOTOK);
	    }

	for (i = 0; i < count; i++) {
		if (pthread_create(&thread, NULL, new_thread, (void*)(buf+i))) {
			fprintf (stderr, "error creating new thread %d\n", i);
			exit (1);
		}
	}
#if 0 /* This would cause the program to loop forever, and "make
		 check" would never complete.  */
	pthread_exit (NULL);
	fprintf(stderr, "pthread_exit returned\n");
	exit(1);
#else
	sleep (10);
	for (i = 0; i < count; i++)
		if (x[i] == 0) {
			fprintf (stderr, "thread %d never ran\n", i);
			return 1;
		}
	printf ("\n%s PASSED\n", argv[0]);
	return 0;
#endif
}
