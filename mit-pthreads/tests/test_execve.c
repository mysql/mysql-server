/* ==== test_execve.c ============================================================
 * Copyright (c) 1994 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test execve() and dup2() calls.
 *
 *  1.00 94/04/29 proven
 *      -Started coding this file.
 */

#define PTHREAD_KERNEL
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

extern char **environ;
char *argv[] = {
	"/bin/echo",
	"This message should be displayed after the execve system call",
	NULL
};

char * should_succeed = "This line should be displayed\n";
char * should_fail = "Error: This line should NOT be displayed\n";

main()
{
	pthread_t thread;
	int fd;

	pthread_init(); 

	printf("This is the first message\n");
	if (isatty(1)) {
		if ((fd = open(ttyname(1), O_RDWR)) < OK) {
			printf("Error: opening tty\n");
			exit(1);
		}
	} else {
		printf("Error: stdout not a tty\n");
		exit(1);
	}

	printf("This output is necessary to set the stdout fd to NONBLOCKING\n");

	/* do a dup2 */
	dup2(fd, 1);
	write(1, should_succeed, (size_t)strlen(should_succeed));
	machdep_sys_write(1, should_fail, strlen(should_fail));

	if (execve(argv[0], argv, environ) < OK) {
		printf("Error: execve\n");
		exit(1);
	}
	PANIC();
}
