/******************************************************
The wait array used in synchronization primitives

(c) 1995 Innobase Oy

Created 9/5/1995 Heikki Tuuri
*******************************************************/

#ifndef sync0arr_h
#define sync0arr_h

#include "univ.i"
#include "ut0lst.h"
#include "ut0mem.h"
#include "os0thread.h"

typedef struct sync_cell_struct        	sync_cell_t;
typedef struct sync_array_struct	sync_array_t;

#define SYNC_ARRAY_OS_MUTEX	1
#define SYNC_ARRAY_MUTEX	2

/***********************************************************************
Creates a synchronization wait array. It is protected by a mutex
which is automatically reserved when the functions operating on it
are called. */

sync_array_t*
sync_array_create(
/*==============*/
				/* out, own: created wait array */
	ulint	n_cells,	/* in: number of cells in the array
				to create */
	ulint	protection);	/* in: either SYNC_ARRAY_OS_MUTEX or
				SYNC_ARRAY_MUTEX: determines the type
				of mutex protecting the data structure */
/**********************************************************************
Frees the resources in a wait array. */

void
sync_array_free(
/*============*/
	sync_array_t*	arr);	/* in, own: sync wait array */
/**********************************************************************
Reserves a wait array cell for waiting for an object.
The event of the cell is reset to nonsignalled state. */

void
sync_array_reserve_cell(
/*====================*/
        sync_array_t*	arr,	/* in: wait array */
        void*   	object, /* in: pointer to the object to wait for */
        ulint		type,	/* in: lock request type */
        char*		file,	/* in: file where requested */
        ulint		line,	/* in: line where requested */
        ulint*   	index); /* out: index of the reserved cell */
/**********************************************************************
This function should be called when a thread starts to wait on
a wait array cell. In the debug version this function checks
if the wait for a semaphore will result in a deadlock, in which
case prints info and asserts. */

void
sync_array_wait_event(
/*==================*/
        sync_array_t*	arr,	/* in: wait array */
        ulint   	index);  /* in: index of the reserved cell */
/**********************************************************************
Frees the cell. NOTE! sync_array_wait_event frees the cell
automatically! */

void
sync_array_free_cell(
/*=================*/
	sync_array_t*	arr,	/* in: wait array */
        ulint    	index);  /* in: index of the cell in array */
/**************************************************************************
Looks for the cells in the wait array which refer
to the wait object specified,
and sets their corresponding events to the signaled state. In this
way releases the threads waiting for the object to contend for the object.
It is possible that no such cell is found, in which case does nothing. */

void
sync_array_signal_object(
/*=====================*/
	sync_array_t*	arr,	/* in: wait array */
	void*		object);/* in: wait object */
/**************************************************************************
If the wakeup algorithm does not work perfectly at semaphore relases,
this function will do the waking (see the comment in mutex_exit). This
function should be called about every 1 second in the server. */

void
sync_arr_wake_threads_if_sema_free(void);
/*====================================*/
/**************************************************************************
Prints warnings of long semaphore waits to stderr. Currently > 120 sec. */

void
sync_array_print_long_waits(void);
/*=============================*/
/************************************************************************
Validates the integrity of the wait array. Checks
that the number of reserved cells equals the count variable. */

void
sync_array_validate(
/*================*/
	sync_array_t*	arr);	/* in: sync wait array */
/**************************************************************************
Prints info of the wait array. */

void
sync_array_print_info(
/*==================*/
	sync_array_t*	arr);	/* in: wait array */


#ifndef UNIV_NONINL
#include "sync0arr.ic"
#endif

#endif
