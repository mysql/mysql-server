/******************************************************
A fast mutex for interprocess synchronization.
mutex_t can be used only within single process,
but ip mutex also between processes.

(c) 1995 Innobase Oy

Created 9/30/1995 Heikki Tuuri
*******************************************************/

#ifndef sync0ipm_h
#define sync0ipm_h

#include "univ.i"
#include "os0sync.h"
#include "sync0sync.h"

typedef struct ip_mutex_hdl_struct	ip_mutex_hdl_t;
typedef struct ip_mutex_struct		ip_mutex_t;

/* NOTE! The structure appears here only for the compiler to
know its size. Do not use its fields directly!
The structure used in a fast implementation of
an interprocess mutex. */

struct ip_mutex_struct {
	mutex_t		mutex;		/* Ordinary mutex struct */
	ulint		waiters;	/* This field is set to 1 if
					there may be waiters */
};

/* The performance of the ip mutex in NT depends on how often
a thread has to suspend itself waiting for the ip mutex
to become free. The following variable counts system calls
involved. */

extern ulint	ip_mutex_system_call_count;

/**********************************************************************
Creates, or rather, initializes
an ip mutex object in a specified shared memory location (which must be
appropriately aligned). The ip mutex is initialized in the reset state.
NOTE! Explicit destroying of the ip mutex with ip_mutex_free
is not recommended
as the mutex resides in shared memory and we cannot make sure that
no process is currently accessing it. Therefore just use
ip_mutex_close to free the operating system event and mutex. */

ulint
ip_mutex_create(
/*============*/
					/* out: 0 if succeed */
	ip_mutex_t*	ip_mutex,	/* in: pointer to shared memory */
	char*		name,		/* in: name of the ip mutex */
	ip_mutex_hdl_t** handle);	/* out, own: handle to the
					created mutex; handle exists
					in the private address space of
					the calling process */
/**********************************************************************
NOTE! Using this function is not recommended. See the note
on ip_mutex_create. Destroys an ip mutex */

void
ip_mutex_free(
/*==========*/
	ip_mutex_hdl_t*	handle);		/* in, own: ip mutex handle */
/**********************************************************************
Opens an ip mutex object in a specified shared memory location.
Explicit closing of the ip mutex with ip_mutex_close is necessary to
free the operating system event and mutex created, and the handle. */

ulint
ip_mutex_open(
/*==========*/
					/* out: 0 if succeed */
	ip_mutex_t*	ip_mutex,	/* in: pointer to shared memory */
	char*		name,		/* in: name of the ip mutex */
	ip_mutex_hdl_t** handle);	/* out, own: handle to the
					opened mutex */
/**********************************************************************
Closes an ip mutex. */

void
ip_mutex_close(
/*===========*/
	ip_mutex_hdl_t*	handle);	/* in, own: ip mutex handle */
/******************************************************************
Reserves an ip mutex. */
UNIV_INLINE
ulint
ip_mutex_enter(
/*===========*/
					/* out: 0 if success, 
					SYNC_TIME_EXCEEDED if timeout */
	ip_mutex_hdl_t*	ip_mutex_hdl,	/* in: pointer to ip mutex handle */
	ulint		time);		/* in: maximum time to wait, in
					microseconds, or 
					SYNC_INFINITE_TIME */
/******************************************************************
Releases an ip mutex. */
UNIV_INLINE
void
ip_mutex_exit(
/*==========*/
	ip_mutex_hdl_t*	ip_mutex_hdl);	/* in: pointer to ip mutex handle */



#ifndef UNIV_NONINL
#include "sync0ipm.ic"
#endif

#endif
