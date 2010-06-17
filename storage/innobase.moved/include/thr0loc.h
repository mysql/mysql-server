/******************************************************
The thread local storage

(c) 1995 Innobase Oy

Created 10/5/1995 Heikki Tuuri
*******************************************************/

/* This module implements storage private to each thread,
a capability useful in some situations like storing the
OS handle to the current thread, or its priority. */

#ifndef thr0loc_h
#define thr0loc_h

#include "univ.i"
#include "os0thread.h"

/********************************************************************
Initializes the thread local storage module. */

void
thr_local_init(void);
/*================*/
/***********************************************************************
Creates a local storage struct for the calling new thread. */

void
thr_local_create(void);
/*==================*/
/***********************************************************************
Frees the local storage struct for the specified thread. */

void
thr_local_free(
/*===========*/
	os_thread_id_t	id);	/* in: thread id */
/***********************************************************************
Gets the slot number in the thread table of a thread. */

ulint
thr_local_get_slot_no(
/*==================*/
				/* out: slot number */
	os_thread_id_t	id);	/* in: thread id of the thread */
/***********************************************************************
Sets in the local storage the slot number in the thread table of a thread. */

void
thr_local_set_slot_no(
/*==================*/
	os_thread_id_t	id,	/* in: thread id of the thread */
	ulint		slot_no);/* in: slot number */
/***********************************************************************
Returns pointer to the 'in_ibuf' field within the current thread local
storage. */

ibool*
thr_local_get_in_ibuf_field(void);
/*=============================*/
			/* out: pointer to the in_ibuf field */

#ifndef UNIV_NONINL
#include "thr0loc.ic"
#endif

#endif
