/* ==== test_sock_1.c =========================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test pthread_create() and pthread_exit() calls.
 *
 *  1.00 93/08/03 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

struct sockaddr_in a_sout;

#define MESSAGE5 "This should be message #5"
#define MESSAGE6 "This should be message #6"

void * sock_write(void* arg)
{
	int fd = *(int *)arg;

	write(fd, MESSAGE5, sizeof(MESSAGE5));
	return(NULL);
}

void * sock_accept(void* arg)
{
	pthread_t thread;
	struct sockaddr a_sin;
	int a_sin_size, a_fd, fd, tmp;
	short port;
	char buf[1024];

	port = 3276;
	a_sout.sin_family = AF_INET;
	a_sout.sin_port = htons(port);
	a_sout.sin_addr.s_addr = INADDR_ANY;

	if ((a_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Error: sock_accept:socket()\n");
		exit(1);
	}

	while (bind(a_fd, (struct sockaddr *) &a_sout, sizeof(a_sout)) < 0) {
		if (errno == EADDRINUSE) { 
			a_sout.sin_port = htons((++port));
			continue;
		}
		printf("Error: sock_accept:bind()\n");
		exit(1);
	}

	if (listen(a_fd, 2)) {
		printf("Error: sock_accept:listen()\n");
		exit(1);
	}
		
	a_sin_size = sizeof(a_sin);
	printf("This should be message #1\n");
	if ((fd = accept(a_fd, &a_sin, &a_sin_size)) < 0) {
		printf("Error: sock_accept:accept()\n");
		exit(1);
	}
	close(fd); 
	sleep(1);

	a_sin_size = sizeof(a_sin);
	memset(&a_sin, 0, sizeof(a_sin));
	printf("This should be message #4\n");
	if ((fd = accept(a_fd, &a_sin, &a_sin_size)) < 0) {
		printf("Error: sock_accept:accept()\n");
		exit(1);
	}

	/* Setup a write thread */
	if (pthread_create(&thread, NULL, sock_write, &fd)) {
		printf("Error: sock_accept:pthread_create(sock_write)\n");
		exit(1);
	}
	if ((tmp = read(fd, buf, 1024)) <= 0) {
		tmp = read(fd, buf, 1024);
		printf("Error: sock_accept:read() == %d\n", tmp);
		exit(1);
	}
	printf("%s\n", buf);
	close(fd);
}

main()
{
	pthread_t thread;
	int i;

	switch(fork()) {
	case -1:
		printf("Error: main:fork()\n");
		break;
	case 0:
		execl("test_sock_2a", "test_sock_2a", "fork okay", NULL);
	default:
		break;
	}

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	if (pthread_create(&thread, NULL, sock_accept, (void *)0xdeadbeaf)) {
		printf("Error: main:pthread_create(sock_accept)\n");
		exit(1);
	}
	pthread_exit(NULL);
}
