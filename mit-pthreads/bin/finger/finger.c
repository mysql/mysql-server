/* ==== finger.c ============================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Tony Nardo of the Johns Hopkins University/Applied Physics Lab.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Chris Provenzano,
 *	the University of California, Berkeley and its contributors.
 * 4. Neither the name of Chris Provenzano, the University nor the names of
 *	  its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO, THE REGENTS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *  1.00 93/08/26 proven
 *      -Pthread redesign of this file.
 *
 *	1.10 95/02/11 proven
 *		-Now that gethostbyname works ....
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 @(#) Copyright (c) 1993, 1995 Chris Provenzano.\n\
 @(#) Copyright (c) 1995 Greg Stark.\n\
 All rights reserved.\n";
#endif /* not lint */

#include <pthreadutil.h>
#include <sys/param.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void *netfinger();

void usage(int eval)
{
	fprintf(stderr,
	  "usage: finger [-lps] [-c <net_count>] [-t|T <timeout>] [-f <filename>] [login ...]\n");
	exit(eval);
}

/*
 * These globals are set initialy and then are only read. 
 * They do not need mutexes.
 */
int thread_time = 0, program_timeout = 0, lflag = 0;
pthread_tad_t parse_file_tad;
pthread_tad_t netfinger_tad;

void * timeout_thread(void * arg)
{
	sleep(program_timeout);
	exit(0);
}

void * signal_thread(void * arg)
{
	int sig;
	sigset_t  program_signals;
	sigemptyset(&program_signals);
	sigaddset(&program_signals, SIGINT);
	sigwait(&program_signals, &sig);
	exit(0);
}

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

void * parse_file(void * arg) 
{
	char hostname[MAXHOSTNAMELEN];
	char * filename = arg;
	pthread_atexit_t atexit_id;
	pthread_attr_t attr;
	pthread_t thread_id;
    char * thread_arg;
	FILE * fp;
	int len;

	netsetupwait();

	/* Parse the file and create a thread per connection */
	if ((fp = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "Can't open file %s\n", filename);
		pthread_exit(NULL);
	}
	pthread_atexit_add(&atexit_id, fclose_nrv, fp);

	if (pthread_attr_init(&attr)) {
		fprintf(stderr, "Error: Can't initialize thread attributes\n");
		exit(2);
	}
	pthread_atexit_add(&atexit_id, pthread_attr_destroy_nrv, &attr);

	while (fgets(hostname, MAXHOSTNAMELEN, fp)) {
		if ((thread_arg = (char *)malloc(len = strlen(hostname))) == NULL) {
			fprintf(stderr, "Error: out of memory\n");
			exit(2);
		}

		hostname[len - 1] = '\0';
		strcpy(thread_arg, hostname);
		pthread_attr_setcleanup(&attr, free, thread_arg);
		if (pthread_tad_create(&netfinger_tad, &thread_id, NULL, 
							   netfinger, thread_arg)) {
			fprintf(stderr, "Error: pthread_tad_create() netfinger_tad.\n");
			exit(2);
		}
	}
	pthread_exit(NULL);
}

main(int argc, char **argv)
{
	pthread_atexit_t atexit_id;
	pthread_t thread_id;
    int max_count = 0;
	char ch;

	/* getopt variables */
	extern char *optarg;
	extern int optind;

	/* Setup tad for parse_file() threads */
	if (pthread_tad_init(&parse_file_tad, max_count)) {
		fprintf(stderr,"Error: couldn't create parse_file() TAD.\n");
		exit(1);
	}

	while ((ch = getopt(argc, argv, "c:f:t:T:ls")) != (char)EOF)
		switch(ch) {
		case 't':	/* Time to let each thread run */
			if ((thread_time = atoi(optarg)) <= 0) {
				usage(1);
			}
			break;	
		case 'T':	/* Time to let entire program run */
			if ((program_timeout = atoi(optarg)) <= 0) {
				usage(1);
			}
			break;	
		case 'f': 	/* Parse file for list of places to finger */
			if (pthread_tad_create(&parse_file_tad, &thread_id, NULL, 
								   parse_file, optarg)) {
				fprintf(stderr,"Error: pthread_tad_create() parse_file_tad.\n");
				exit(1);
			}
			break;	
		case 'c':
			max_count = atoi(optarg);
			break;
		case 'l': 	/* long format */
			lflag = 1;		
			break;
		case 's': 	/* short format */
			lflag = 0;		
			break;
		case '?':
			usage(0);
		default:
			usage(1);
		}

	/* The rest of the argumants are hosts */
	argc -= optind;
	argv += optind;

	/* Setup timeout thread, if there is one */
	if (program_timeout) {
		if (pthread_create(&thread_id, NULL, timeout_thread, NULL)) {
			fprintf(stderr,"Error: couldn't create program_timeout() thread\n");
			exit(1);
		}
	}

	/* Setup cleanup thread for signals */
	if (pthread_create(&thread_id, NULL, signal_thread, NULL)) {
		fprintf(stderr,"Error: couldn't create signal_timeout() thread\n");
		exit(1);
	}

	/* Setup tad for netfinger() threads */
	if (pthread_tad_init(&netfinger_tad, max_count)) {
		fprintf(stderr,"Error: couldn't create netfinger() TAD.\n");
		exit(1);
	}

	/* Setup the net and let everyone run */
	netsetup();

	while (*argv) {
		if (pthread_tad_create(&netfinger_tad, &thread_id, NULL, 
							   netfinger, *argv)) {
			fprintf(stderr, "Error: pthread_tad_create() netfinger_tad.\n");
			exit(2);
		}
		argv++;
	}
	pthread_tad_wait(&parse_file_tad, 0);
	pthread_tad_wait(&netfinger_tad, 0);
	exit(0);
}

