/*****************************************************************************

Copyright (c) 2011, 2018, Oracle and/or its affiliates. All Rights Reserved.

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
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/srv0conc.h

 InnoDB concurrency manager header file

 Created 2011/04/18 Sunny Bains
 *******************************************************/

#ifndef srv_conc_h
#define srv_conc_h

/** We are prepared for a situation that we have this many threads waiting for
a semaphore inside InnoDB. innobase_start_or_create_for_mysql() sets the
value. */

extern ulint srv_max_n_threads;

/** The following controls how many threads we let inside InnoDB concurrently:
threads waiting for locks are not counted into the number because otherwise
we could get a deadlock. Value of 0 will disable the concurrency check. */

extern ulong srv_thread_concurrency;

struct row_prebuilt_t;
/** Puts an OS thread to wait if there are too many concurrent threads
 (>= srv_thread_concurrency) inside InnoDB. The threads wait in a FIFO queue.
 @param[in,out]	prebuilt	row prebuilt handler */
void srv_conc_enter_innodb(row_prebuilt_t *prebuilt);

/** This lets a thread enter InnoDB regardless of the number of threads inside
 InnoDB. This must be called when a thread ends a lock wait. */
void srv_conc_force_enter_innodb(
    trx_t *trx); /*!< in: transaction object associated with
                 the thread */

/** This must be called when a thread exits InnoDB in a lock wait or at the
 end of an SQL statement. */
void srv_conc_force_exit_innodb(
    trx_t *trx); /*!< in: transaction object associated with
                 the thread */

/** Get the count of threads waiting inside InnoDB. */
ulint srv_conc_get_waiting_threads(void);

/** Get the count of threads active inside InnoDB. */
ulint srv_conc_get_active_threads(void);

#endif /* srv_conc_h */
