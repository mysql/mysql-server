/*****************************************************************************

Copyright (c) 1995, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/******************************************************
The thread local storage

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
UNIV_INTERN
void
thr_local_init(void);
/*================*/
/***********************************************************************
Creates a local storage struct for the calling new thread. */
UNIV_INTERN
void
thr_local_create(void);
/*==================*/
/***********************************************************************
Frees the local storage struct for the specified thread. */
UNIV_INTERN
void
thr_local_free(
/*===========*/
	os_thread_id_t	id);	/* in: thread id */
/***********************************************************************
Gets the slot number in the thread table of a thread. */
UNIV_INTERN
ulint
thr_local_get_slot_no(
/*==================*/
				/* out: slot number */
	os_thread_id_t	id);	/* in: thread id of the thread */
/***********************************************************************
Sets in the local storage the slot number in the thread table of a thread. */
UNIV_INTERN
void
thr_local_set_slot_no(
/*==================*/
	os_thread_id_t	id,	/* in: thread id of the thread */
	ulint		slot_no);/* in: slot number */
/***********************************************************************
Returns pointer to the 'in_ibuf' field within the current thread local
storage. */
UNIV_INTERN
ibool*
thr_local_get_in_ibuf_field(void);
/*=============================*/
			/* out: pointer to the in_ibuf field */

/*************************************************************************
Return local hash table informations. */

ulint
thr_local_hash_cells(void);
/*=======================*/

ulint
thr_local_hash_nodes(void);
/*=======================*/

#ifndef UNIV_NONINL
#include "thr0loc.ic"
#endif

#endif
