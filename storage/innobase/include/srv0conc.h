/*****************************************************************************

Copyright (c) 2011, Oracle and/or its affiliates. All Rights Reserved.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

Portions of this file contain modifications contributed and copyrighted
by Percona Inc.. Those modifications are
gratefully acknowledged and are described briefly in the InnoDB
documentation. The contributions by Percona Inc. are incorporated with
their permission, and subject to the conditions contained in the file
COPYING.Percona.

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

/**************************************************//**
@file srv/srv0conc.h

InnoDB concurrency manager header file

Created 2011/04/18 Sunny Bains
*******************************************************/

#ifndef srv_conc_h
#define srv_conc_h

/** We are prepared for a situation that we have this many threads waiting for
a semaphore inside InnoDB. innobase_start_or_create_for_mysql() sets the
value. */

extern	ulint	srv_max_n_threads;

/** The following controls how many threads we let inside InnoDB concurrently:
threads waiting for locks are not counted into the number because otherwise
we could get a deadlock. MySQL creates a thread for each user session, and
semaphore contention and convoy problems can occur withput this restriction.
Value 10 should be good if there are less than 4 processors + 4 disks in the
computer. Bigger computers need bigger values. Value 0 will disable the
concurrency check. */

extern ulong	srv_thread_concurrency;

/** Number of transactions that have declared_to_be_inside_innodb set.
It used to be a non-error for this value to drop below zero temporarily.
This is no longer true. We'll, however, keep the lint datatype to add
assertions to catch any corner cases that we may have missed. */

extern	lint	srv_conc_n_threads;

/*********************************************************************//**
Initialise the concurrency management data structures */
void
srv_conc_init(void);
/*===============*/

/*********************************************************************//**
Free the concurrency management data structures */
void
srv_conc_free(void);
/*===============*/

/*********************************************************************//**
Puts an OS thread to wait if there are too many concurrent threads
(>= srv_thread_concurrency) inside InnoDB. The threads wait in a FIFO queue. */
UNIV_INTERN
void
srv_conc_enter_innodb(
/*==================*/
	trx_t*	trx);		/*!< in: transaction object associated
				with the thread */

/*********************************************************************//**
This lets a thread enter InnoDB regardless of the number of threads inside
InnoDB. This must be called when a thread ends a lock wait. */
UNIV_INTERN
void
srv_conc_force_enter_innodb(
/*========================*/
	trx_t*	trx);		/*!< in: transaction object associated with
				the thread */

/*********************************************************************//**
This must be called when a thread exits InnoDB in a lock wait or at the
end of an SQL statement. */
UNIV_INTERN
void
srv_conc_force_exit_innodb(
/*=======================*/
	trx_t*	trx);		/*!< in: transaction object associated with
				the thread */

/*********************************************************************//**
This must be called when a thread exits InnoDB. */
UNIV_INTERN
void
srv_conc_exit_innodb(
/*=================*/
	trx_t*	trx);		/*!< in: transaction object associated with
				the thread */

/*********************************************************************//**
Get the count of threads waiting inside InnoDB. */
UNIV_INTERN
ulint
srv_conc_get_waiting_threads(void);
/*==============================*/

#endif /* srv_conc_h */
