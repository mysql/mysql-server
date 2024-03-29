/*****************************************************************************

Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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

/** @file srv/srv0conc.cc

 InnoDB concurrency manager

 Created 2011/04/18 Sunny Bains
 *******************************************************/

#include <mysql/service_thd_wait.h>
#include <stddef.h>
#include <sys/types.h>

#include "dict0dict.h"
#include "ha_prototypes.h"
#include "row0mysql.h"
#include "srv0srv.h"
#include "trx0trx.h"

/** Number of times a thread is allowed to enter InnoDB within the same
SQL query after it has once got the ticket. */
ulong srv_n_free_tickets_to_enter = 500;

/** Maximum sleep delay (in micro-seconds), value of 0 disables it. */
ulong srv_adaptive_max_sleep_delay = 150000;

ulong srv_thread_sleep_delay = 10000;

/** The following controls how many threads we let inside InnoDB concurrently:
threads waiting for locks are not counted into the number because otherwise
we could get a deadlock. Value of 0 will disable the concurrency check. */

ulong srv_thread_concurrency = 0;

/** Variables tracking the active and waiting threads. */
struct srv_conc_t {
  char pad[ut::INNODB_CACHE_LINE_SIZE];

  /** Number of transactions that have declared_to_be_inside_innodb set.
  It used to be a non-error for this value to drop below zero temporarily.
  This is no longer true. We'll, however, keep the signed datatype to add
  assertions to catch any corner cases that we may have missed. */
  std::atomic<int32_t> n_active;

  /** Number of OS threads waiting in the FIFO for permission to
  enter InnoDB */
  std::atomic<int32_t> n_waiting;
};

/* Control variables for tracking concurrency. */
static srv_conc_t srv_conc;

/** Note that a user thread is entering InnoDB. */
static void srv_enter_innodb_with_tickets(
    trx_t *trx) /*!< in/out: transaction that wants
                to enter InnoDB */
{
  trx->declared_to_be_inside_innodb = true;
  trx->n_tickets_to_enter_innodb = srv_n_free_tickets_to_enter;
}

/** Handle the scheduling of a user thread that wants to enter InnoDB.  Setting
 srv_adaptive_max_sleep_delay > 0 switches the adaptive sleep calibration to
 ON. When set, we want to wait in the queue for as little time as possible.
 However, very short waits will result in a lot of context switches and that
 is also not desirable. When threads need to sleep multiple times we increment
 srv_thread_sleep_delay by one. When we see threads getting a slot without
 waiting and there are no other threads waiting in the queue, we try and reduce
 the wait as much as we can. Currently we reduce it by half each time. If the
 thread only had to wait for one turn before it was able to enter InnoDB we
 decrement it by one. This is to try and keep the sleep time stable around the
 "optimum" sleep time.
 @return InnoDB error code. */
static dberr_t srv_conc_enter_innodb_with_atomics(
    trx_t *trx) /*!< in/out: transaction that wants
                to enter InnoDB */
{
  ulint n_sleeps = 0;
  bool notified_mysql = false;

  ut_a(!trx->declared_to_be_inside_innodb);

  for (;;) {
    ulint sleep_in_us;

    if (srv_thread_concurrency == 0) {
      if (notified_mysql) {
        srv_conc.n_waiting.fetch_sub(1, std::memory_order_relaxed);

        thd_wait_end(trx->mysql_thd);
      }

      return DB_SUCCESS;
    }

    if (srv_conc.n_active.load(std::memory_order_relaxed) <
        (int32_t)srv_thread_concurrency) {
      /* Check if there are any free tickets. */
      const auto n_active =
          srv_conc.n_active.fetch_add(1, std::memory_order_acquire) + 1;

      if (n_active <= (int32_t)srv_thread_concurrency) {
        srv_enter_innodb_with_tickets(trx);

        if (notified_mysql) {
          srv_conc.n_waiting.fetch_sub(1, std::memory_order_relaxed);

          thd_wait_end(trx->mysql_thd);
        }

        if (srv_adaptive_max_sleep_delay > 0) {
          if (srv_thread_sleep_delay > 20 && n_sleeps == 1) {
            --srv_thread_sleep_delay;
          }

          if (srv_conc.n_waiting.load(std::memory_order_relaxed) == 0) {
            srv_thread_sleep_delay >>= 1;
          }
        }

        return DB_SUCCESS;
      }

      /* Since there were no free seats, we relinquish
      the overbooked ticket. */
      srv_conc.n_active.fetch_sub(1, std::memory_order_release);
    }

    if (!notified_mysql) {
      srv_conc.n_waiting.fetch_add(1, std::memory_order_relaxed);

      thd_wait_begin(trx->mysql_thd, THD_WAIT_USER_LOCK);

      notified_mysql = true;
    }

    DEBUG_SYNC_C("user_thread_waiting");
    trx->op_info = "sleeping before entering InnoDB";

    sleep_in_us = srv_thread_sleep_delay;

    /* Guard against overflow when adaptive sleep delay is on. */

    if (srv_adaptive_max_sleep_delay > 0 &&
        sleep_in_us > srv_adaptive_max_sleep_delay) {
      sleep_in_us = srv_adaptive_max_sleep_delay;
      srv_thread_sleep_delay = static_cast<ulong>(sleep_in_us);
    }

    std::this_thread::sleep_for(std::chrono::microseconds(sleep_in_us));

    trx->op_info = "";

    ++n_sleeps;

    if (srv_adaptive_max_sleep_delay > 0 && n_sleeps > 1) {
      ++srv_thread_sleep_delay;
    }

    if (trx_is_interrupted(trx)) {
      if (notified_mysql) {
        srv_conc.n_waiting.fetch_sub(1, std::memory_order_relaxed);

        thd_wait_end(trx->mysql_thd);
      }
      return DB_INTERRUPTED;
    }
  }
}

/** Note that a user thread is leaving InnoDB code. */
static void srv_conc_exit_innodb_with_atomics(
    trx_t *trx) /*!< in/out: transaction */
{
  trx->n_tickets_to_enter_innodb = 0;
  trx->declared_to_be_inside_innodb = false;
  srv_conc.n_active.fetch_sub(1, std::memory_order_release);
}

dberr_t srv_conc_enter_innodb(row_prebuilt_t *prebuilt) {
  trx_t *trx = prebuilt->trx;

#ifdef UNIV_DEBUG
  {
    btrsea_sync_check check(trx->has_search_latch);

    ut_ad(!sync_check_iterate(check));
  }
#endif /* UNIV_DEBUG */

  return srv_conc_enter_innodb_with_atomics(trx);
}

/** This lets a thread enter InnoDB regardless of the number of threads inside
 InnoDB. This must be called when a thread ends a lock wait. */
void srv_conc_force_enter_innodb(trx_t *trx) /*!< in: transaction object
                                             associated with the thread */
{
#ifdef UNIV_DEBUG
  {
    btrsea_sync_check check(trx->has_search_latch);

    ut_ad(!sync_check_iterate(check));
  }
#endif /* UNIV_DEBUG */

  if (!srv_thread_concurrency) {
    return;
  }

  ut_ad(srv_conc.n_active.load(std::memory_order_relaxed) >= 0);

  srv_conc.n_active.fetch_add(1, std::memory_order_acquire);

  trx->n_tickets_to_enter_innodb = 1;
  trx->declared_to_be_inside_innodb = true;
}

/** This must be called when a thread exits InnoDB in a lock wait or at the
 end of an SQL statement. */
void srv_conc_force_exit_innodb(trx_t *trx) /*!< in: transaction object
                                            associated with the thread */
{
  if ((trx->mysql_thd != nullptr &&
       thd_is_replication_slave_thread(trx->mysql_thd)) ||
      trx->declared_to_be_inside_innodb == false) {
    return;
  }

  srv_conc_exit_innodb_with_atomics(trx);

#ifdef UNIV_DEBUG
  {
    btrsea_sync_check check(trx->has_search_latch);

    ut_ad(!sync_check_iterate(check));
  }
#endif /* UNIV_DEBUG */
}

/** Get the count of threads waiting inside InnoDB. */
int32_t srv_conc_get_waiting_threads(void) {
  return srv_conc.n_waiting.load(std::memory_order_relaxed);
}

/** Get the count of threads active inside InnoDB. */
int32_t srv_conc_get_active_threads(void) {
  return srv_conc.n_active.load(std::memory_order_relaxed);
}