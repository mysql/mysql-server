/* ==== test_fork.c ============================================================
 * Copyright (c) 1994 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test fork() and dup2() calls.
 *
 *  1.00 94/04/29 proven
 *      -Started coding this file.
 */

#define PTHREAD_KERNEL
#include <pthread.h>
#include <stdio.h>
#include <fcntl.h>

main()
{
	pthread_t thread;
	int flags, pid;

	pthread_init(); 

	if (((flags = machdep_sys_fcntl(1, F_GETFL, NULL)) >= OK) && 
	  (flags & __FD_NONBLOCK | O_NDELAY)) {
		machdep_sys_fcntl(1, F_SETFL, flags & (~__FD_NONBLOCK | O_NDELAY));
	}
	printf("parent process %d\n", getpid());

	switch(pid = fork()) {
	case OK:
		exit(OK);
		break;
	case NOTOK:
		printf("fork() FAILED\n");
		exit(2);
		break;
	default:
		if ((flags = machdep_sys_fcntl(1, F_GETFL, NULL)) >= OK) {
			if (flags & (__FD_NONBLOCK | O_NDELAY)) {
				printf("fd flags not set to BLOCKING ERROR\n");
				printf("test_fork FAILED\n");
				exit(1);
				break;
			}
			printf("The stdout fd was set to BLOCKING\n");
			printf("child process %d\n", pid);
			flags = machdep_sys_fcntl(1, F_GETFL, NULL);
			if (flags & (__FD_NONBLOCK | O_NDELAY)) {
				printf("The stdout fd was reset to O_NDELAY\n");
			} else {
				printf("Error: the stdout fd was not reset\n");
				printf("test_fork FAILED\n");
				exit(1);
			}
		}
		break;
	}

	printf("test_fork PASSED\n");
	pthread_exit(NULL);
}
