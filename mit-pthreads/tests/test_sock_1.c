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
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_attr_t attr;

#define MESSAGE5 "This should be message #5"
#define MESSAGE6 "This should be message #6"

void * sock_connect(void* arg)
{
	char buf[1024];
	int fd, tmp;

	/* Ensure sock_read runs first */
	if (pthread_mutex_lock(&mutex)) {
		printf("Error: sock_connect:pthread_mutex_lock()\n");
		exit(1);
	}

	a_sout.sin_addr.s_addr = htonl(0x7f000001); /* loopback */

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Error: sock_connect:socket()\n");
		exit(1);
	}

	printf("This should be message #2\n");
	if (connect(fd, (struct sockaddr *) &a_sout, sizeof(a_sout)) < 0) {
		printf("Error: sock_connect:connect()\n");
		exit(1);
	}
	close(fd);
		
	if (pthread_mutex_unlock(&mutex)) {
		printf("Error: sock_connect:pthread_mutex_lock()\n");
		exit(1);
	}

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Error: sock_connect:socket()\n");
		exit(1);
	}

	printf("This should be message #3\n");

	if (connect(fd, (struct sockaddr *) &a_sout, sizeof(a_sout)) < 0) {
		printf("Error: sock_connect:connect()\n");
		exit(1);
	}

	/* Ensure sock_read runs again */
	pthread_yield();
	pthread_yield();
	pthread_yield();
	pthread_yield();
	if (pthread_mutex_lock(&mutex)) {
		printf("Error: sock_connect:pthread_mutex_lock()\n");
		exit(1);
	}

	if ((tmp = read(fd, buf, 1024)) <= 0) {
		printf("Error: sock_connect:read() == %d\n", tmp);
		exit(1);
	}
	write(fd, MESSAGE6, sizeof(MESSAGE6));
	printf("%s\n", buf);
	close(fd);
}

extern struct fd_table_entry ** fd_table;
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

	if (pthread_mutex_unlock(&mutex)) {
		printf("Error: sock_accept:pthread_mutex_lock()\n");
		exit(1);
	}

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
	
	if (pthread_mutex_lock(&mutex)) {
		printf("Error: sock_accept:pthread_mutex_lock()\n");
		exit(1);
	}
	close(fd);

	a_sin_size = sizeof(a_sin);
	printf("This should be message #4\n");
	if ((fd = accept(a_fd, &a_sin, &a_sin_size)) < 0) {
		printf("Error: sock_accept:accept()\n");
		exit(1);
	}

	if (pthread_mutex_unlock(&mutex)) {
		printf("Error: sock_accept:pthread_mutex_lock()\n");
		exit(1);
	}

	/* Setup a write thread */
	if (pthread_create(&thread, &attr, sock_write, &fd)) {
		printf("Error: sock_accept:pthread_create(sock_write)\n");
		exit(1);
	}
	if ((tmp = read(fd, buf, 1024)) <= 0) {
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

	pthread_init(); 
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	/* Ensure sock_read runs first */
	if (pthread_mutex_lock(&mutex)) {
		printf("Error: main:pthread_mutex_lock()\n");
		exit(1);
	}

	if (pthread_attr_init(&attr)) {
		printf("Error: main:pthread_attr_init()\n");
		exit(1);
	}
	if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO)) {
		printf("Error: main:pthread_attr_setschedpolicy()\n");
		exit(1);
	}
	if (pthread_create(&thread, &attr, sock_accept, (void *)0xdeadbeaf)) {
		printf("Error: main:pthread_create(sock_accept)\n");
		exit(1);
	}
	if (pthread_create(&thread, &attr, sock_connect, (void *)0xdeadbeaf)) {
		printf("Error: main:pthread_create(sock_connect)\n");
		exit(1);
	}
	printf("initial thread %lx going to sleep\n", pthread_self());
	sleep(10);
	printf("done sleeping\n");
	return 0;
}
