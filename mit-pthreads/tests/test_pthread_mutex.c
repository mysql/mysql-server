/* ==== test_pthread_cond.c =========================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test pthread_cond(). Run this after test_create()
 *
 *  1.23 94/05/04 proven
 *      -Started coding this file.
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>

#define	OK		0
#define NOTOK	-1

int contention_variable;

void * thread_contention(void * arg)
{
	pthread_mutex_t * mutex = arg;

	if (pthread_mutex_lock(mutex)) {
		printf("pthread_mutex_lock() ERROR\n");
		pthread_exit(NULL);
	}

	if (contention_variable != 1) {
		printf("contention_variable != 1 ERROR\n");
		pthread_exit(NULL);
	}
	contention_variable = 2;
	
	if (pthread_mutex_unlock(mutex)) {
		printf("pthread_mutex_unlock() ERROR\n");
		pthread_exit(NULL);
	}
	pthread_exit(NULL);
}

int test_contention_lock(pthread_mutex_t * mutex)
{
	pthread_t thread;

	printf("test_contention_lock()\n");

	if (pthread_mutex_lock(mutex)) {
		printf("pthread_mutex_lock() ERROR\n");
		return(NOTOK);
	}
	contention_variable = 0;

	if (pthread_create(&thread, NULL, thread_contention, mutex)) {
		printf("pthread_create() FAILED\n");
		exit(2);
	}

	pthread_yield();

	contention_variable = 1;

	if (pthread_mutex_unlock(mutex)) {
		printf("pthread_mutex_unlock() ERROR\n");
		return(NOTOK);
	}

	if (pthread_mutex_lock(mutex)) {
		printf("pthread_mutex_lock() ERROR\n");
		return(NOTOK);
	}

	if (contention_variable != 2) {
		printf("contention_variable != 2 ERROR\n");
		return(NOTOK);
	}

	if (pthread_mutex_unlock(mutex)) {
		printf("pthread_mutex_unlock() ERROR\n");
		return(NOTOK);
	}

	return(OK);
}

int test_nocontention_lock(pthread_mutex_t * mutex)
{
	printf("test_nocontention_lock()\n");
	if (pthread_mutex_lock(mutex)) {
		printf("pthread_mutex_lock() ERROR\n");
		return(NOTOK);
	}
	if (pthread_mutex_unlock(mutex)) {
		printf("pthread_mutex_unlock() ERROR\n");
		return(NOTOK);
	}
	return(OK);
}

int test_debug_double_lock(pthread_mutex_t * mutex)
{
	printf("test_debug_double_lock()\n");
	if (pthread_mutex_lock(mutex)) {
		printf("pthread_mutex_lock() ERROR\n");
		return(NOTOK);
	}
	if (pthread_mutex_lock(mutex) != EDEADLK) {
		printf("double lock error not detected ERROR\n");
		return(NOTOK);
	}
	if (pthread_mutex_unlock(mutex)) {
		printf("pthread_mutex_unlock() ERROR\n");
		return(NOTOK);
	}
	return(OK);
}

int test_debug_double_unlock(pthread_mutex_t * mutex)
{
	printf("test_debug_double_unlock()\n");
	if (pthread_mutex_lock(mutex)) {
		printf("pthread_mutex_lock() ERROR\n");
		return(NOTOK);
	}
	if (pthread_mutex_unlock(mutex)) {
		printf("pthread_mutex_unlock() ERROR\n");
		return(NOTOK);
	}
	if (pthread_mutex_unlock(mutex) != EPERM) {
		printf("double unlock error not detected ERROR\n");
		return(NOTOK);
	}
	return(OK);
}

int test_nocontention_trylock(pthread_mutex_t * mutex)
{
	printf("test_nocontention_trylock()\n");
	if (pthread_mutex_trylock(mutex)) {
		printf("pthread_mutex_trylock() ERROR\n");
		return(NOTOK);
	}
	if (pthread_mutex_unlock(mutex)) {
		printf("pthread_mutex_unlock() ERROR\n");
		return(NOTOK);
	}
	return(OK);
}

int test_mutex_static(void)
{
	pthread_mutex_t mutex_static = PTHREAD_MUTEX_INITIALIZER;

	printf("test_mutex_static()\n");
	if (test_nocontention_lock(&mutex_static) ||
	  test_contention_lock(&mutex_static)) {
		return(NOTOK);
	}
	return(OK);
}

int test_mutex_fast(void)
{
	pthread_mutex_t mutex_fast; 

	printf("test_mutex_fast()\n");
	if (pthread_mutex_init(&mutex_fast, NULL)) {
		printf("pthread_mutex_init() ERROR\n");
		return(NOTOK);
	}
	if (test_nocontention_lock(&mutex_fast) ||
	  test_contention_lock(&mutex_fast)) {
		return(NOTOK);
	}
	if (pthread_mutex_destroy(&mutex_fast)) {
		printf("pthread_mutex_destroy() ERROR\n");
		return(NOTOK);
	}
	return(OK);
}

int test_mutex_debug()
{
	pthread_mutexattr_t mutex_debug_attr; 
	pthread_mutex_t mutex_debug; 

	printf("test_mutex_debug()\n");
	pthread_mutexattr_init(&mutex_debug_attr);
	pthread_mutexattr_settype(&mutex_debug_attr, PTHREAD_MUTEXTYPE_DEBUG);

	if (pthread_mutex_init(&mutex_debug, &mutex_debug_attr)) {
		printf("pthread_mutex_init() ERROR\n");
		return(NOTOK);
	}
	if (test_nocontention_lock(&mutex_debug) ||
	  test_contention_lock(&mutex_debug) ||
	  test_debug_double_lock(&mutex_debug) ||
	  test_debug_double_unlock(&mutex_debug)) {
		return(NOTOK);
	}
	if (pthread_mutex_destroy(&mutex_debug)) {
		printf("pthread_mutex_destroy() ERROR\n");
		return(NOTOK);
	}
	return(OK);
}

main()
{
	pthread_init();

	printf("test_pthread_mutex START\n");

	if (test_mutex_static() || test_mutex_fast() || test_mutex_debug()) { 
		printf("test_pthread_mutex FAILED\n");
		exit(1);
	}

	printf("test_pthread_mutex PASSED\n");
	exit(0);
}

