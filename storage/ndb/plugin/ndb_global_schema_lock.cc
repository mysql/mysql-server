/*
   Copyright (c) 2011, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "storage/ndb/plugin/ndb_global_schema_lock.h"

#include <mutex>

#include "my_dbug.h"
#include "mysql/plugin.h"
#include "sql/debug_sync.h"
#include "sql/mdl.h"
#include "sql/sql_class.h"
#include "sql/sql_thd_internal_api.h"  // thd_query_unsafe
#include "storage/ndb/include/ndbapi/NdbApi.hpp"
#include "storage/ndb/plugin/ndb_ndbapi_errors.h"
#include "storage/ndb/plugin/ndb_sleep.h"
#include "storage/ndb/plugin/ndb_table_guard.h"

/**
 * There is a potential for deadlocks between MDL and GSL locks:
 *
 * A client thread might have acquired an MDL_INTENTIONAL_EXCLUSIVE (IX)
 * lock, and attempt to upgrade this to a MDL_EXCLUSIVE (X) locks, which
 * requires the GSL lock to be taken.
 *
 * However, the GSL lock may already be held by the binlog schema-change
 * coordinator on another mysqld. All participants has to complete
 * the schema change op before the coordinator will release the GSL.
 * As part of that, the participants will request a MDL-X-lock which blocks
 * due to the other client thread holding an MDL-IX-lock. Thus, we
 * have effectively a deadlock between the client thread and the
 * schema change participant.
 *
 * We detect, and break, such deadlock by recording whether we
 * have an active 'IS_SCHEMA_DIST_PARTICIPANT' on this mysqld.
 * Iff another GSL request times-out while there are active
 * schema dist participants, we *assume* we were involved in
 * a deadlock.
 *
 * The MDL code is able to handle such deadlocks by releasing the
 * locks and retry later
 */

static class Ndb_thd_gsl_participant {
  std::mutex m_mutex;
  const THD *m_thd{nullptr};

 public:
  Ndb_thd_gsl_participant &operator=(const THD *thd) {
    std::lock_guard<std::mutex> lock_thd(m_mutex);
    m_thd = thd;
    return *this;
  }
  bool operator!=(const THD *thd) {
    std::lock_guard<std::mutex> lock_thd(m_mutex);
    return m_thd != thd;
  }
} thd_gsl_participant;

static void ndb_set_gsl_participant(THD *thd) { thd_gsl_participant = thd; }

static bool ndb_is_gsl_participant_active() {
  return (thd_gsl_participant != nullptr);
}

/**
 * Another potential scenario for a deadlock between MDL and GSL locks is as
 * follows:
 *
 * A disk data table DDL will try and acquire the following -
 *  - Global read lock of type INTENTION EXCLUSIVE (IX)
 *  - IX lock on the schema
 *  - Shared lock on the table
 *  - Backup lock of type IX
 *  - IX lock on the tablespace
 *  - Upgrade the previously acquired shared lock on the table to an EXCLUSIVE
 *    (X) lock
 *  - The X lock is granted only after the GSL has been acquired
 *
 * A tablespace DDL will try and acquire the following -
 *  - Global read lock of type IX
 *  - X lock on the 'ts1' tablespace
 *  - The X lock is granted only after the GSL has been acquired
 *  - Backup lock of type IX
 *
 * Assume that the table DDL has acquired an IX lock on the tablespace and is
 * waiting for the GSL in order to acquire an X lock on the table. At the same
 * time the tablespace DDL has acquired the GSL and is waiting to acquire an X
 * lock on the tablespace - Deadlock!
 *
 * A very similar deadlock might occur when two DDLs, one on a schema and
 * another on a table from that same schema, are run in parallel. The table DDL
 * has acquired an IX lock on the schema and is waiting for the GSL in order to
 * upgrade the previously acquired shared lock on the table to an X lock. At the
 * same time, the schema DDL has acquired the GSL and is waiting to acquire an
 * X lock on the schema leading to a deadlock.
 *
 * We detect such a deadlock by tracking when the GSL is acquired (and released)
 * during an attempt to obtain an X lock on a tablespace or a schema. When this
 * condition holds true (along with the other 2 conditions specified in
 * gsl_lock_ext() below), we assume that a deadlock has occurred.
 */

// Object to track GSLs acquired through ndbcluster_notify_exclusive_mdl
// for schema and tablespace MDLs
static class Ndb_gsl_for_mdl_guard {
  std::mutex m_gsl_acquired_mutex;  // for m_gsl_acquired
  bool m_gsl_acquired{false};

 public:
  void gsl_acquired() {
    std::lock_guard<std::mutex> lock_gsl_acquired(m_gsl_acquired_mutex);
    m_gsl_acquired = true;
  }

  void gsl_released() {
    std::lock_guard<std::mutex> lock_gsl_acquired(m_gsl_acquired_mutex);
    m_gsl_acquired = false;
  }

  bool is_gsl_acquired() {
    std::lock_guard<std::mutex> lock_gsl_acquired(m_gsl_acquired_mutex);
    return m_gsl_acquired;
  }
} ndb_gsl_for_mdl_guard;

/*
  The lock/unlock functions use the BACKUP_SEQUENCE row in SYSTAB_0

  In case retry = true, the function will retry infinitely or until the THD
  is killed or a GSL / MDL deadlock is detected/assumed. In the last case a
  timeout error (266) is returned. If retry = false, then the function attempts
  to acquire GSL only once and returns.

  Returns a NdbTransaction owning the gsl-lock if it was taken. NULL is returned
  if failed to take lock. Returned NdbError will then contain the error code if
  lock failed due to some NdbError. If there is no error code set, lock was
  rejected by lock manager, likely due to deadlock.
*/
static NdbTransaction *gsl_lock_ext(THD *thd, Ndb *ndb, NdbError &ndb_error,
                                    bool retry, bool no_wait) {
  while (true) {
    /*
      while loop to control the behaviour of the attempt to lock the row.
      - Temporary errors are dealt with by closing the transaction (if
        applicable) and continuing from the beginning of the loop if retry is
        set to true. A fresh attempt to acquire the GSL occurs after a random
        sleep. If retry = false, even temporary errors are handled as described
        in the next point
      - Other errors are handled by setting ndb_error, closing the transaction
        (if applicable), and returning nullptr
      - A pointer to the NdbTransaction is returned in case of success
    */

    // Get table from dictionary
    Ndb_table_guard ndbtab_g(ndb, "sys", "SYSTAB_0");
    const NdbDictionary::Table *ndbtab = ndbtab_g.get_table();
    if (ndbtab == nullptr) {
      if (ndb->getDictionary()->getNdbError().status ==
              NdbError::TemporaryError &&
          retry) {
        ndb_trans_retry_sleep();
        continue;
      }
      ndb_error = ndb->getDictionary()->getNdbError();
      return nullptr;
    }

    // Start NDB transaction
    NdbTransaction *trans = ndb->startTransaction();
    if (trans == nullptr) {
      ndb_error = ndb->getNdbError();
      return nullptr;
    }

    // Get NDB operation
    NdbOperation *op = trans->getNdbOperation(ndbtab);
    if (op == nullptr) {
      if (trans->getNdbError().status == NdbError::TemporaryError && retry) {
        ndb->closeTransaction(trans);
        ndb_trans_retry_sleep();
        continue;
      }
      ndb_error = trans->getNdbError();
      ndb->closeTransaction(trans);
      return nullptr;
    }

    // Read the tuple
    if (op->readTuple(NdbOperation::LM_Exclusive)) {
      ndb_error = trans->getNdbError();
      ndb->closeTransaction(trans);
      return nullptr;
    }

    // Set the 'NoWait' option if the caller has requested to do so
    if (no_wait && op->setNoWait()) {
      ndb_error = trans->getNdbError();
      ndb->closeTransaction(trans);
      return nullptr;
    }

    // Attempt to lock the tuple where SYSKEY_0 = NDB_BACKUP_SEQUENCE
    if (op->equal("SYSKEY_0", NDB_BACKUP_SEQUENCE)) {
      ndb_error = trans->getNdbError();
      ndb->closeTransaction(trans);
      return nullptr;
    }

    // Execute transaction
    if (trans->execute(NdbTransaction::NoCommit) == 0) {
      /*
        The transaction is successful but still check if the operation has
        failed since the abort mode is set to AO_IgnoreError. Error 635
        is the expected error when no_wait has been set and the row could not
        be locked immediately
      */
      if (trans->getNdbError().code == 635) {
        ndb_error = trans->getNdbError();
        ndb->closeTransaction(trans);
        return nullptr;
      }
      /*
        Transaction executed successfully i.e. GSL has been obtained. The
        transaction will eventually be closed in the gsl_unlock_ext() function
      */
      return trans;
    }

    if (trans->getNdbError().status != NdbError::TemporaryError ||
        thd_killed(thd)) {
      ndb_error = trans->getNdbError();
      ndb->closeTransaction(trans);
      return nullptr;
    }

    /*
      Check for MDL / GSL deadlock. A deadlock is assumed if:
      1)  ::execute failed with a timeout error.
      2a) There already is another THD being an participant in a schema distr.
          operation (which implies that the coordinator already held the GSL
                                  OR
      2b) The GSL has already been acquired for a pending exclusive MDL on a
          namespace. It's highly likely that there are two DDL statements
          competing for a lock on the same namespace
      3)  This THD holds a lock being waited for by another THD

      Note: If we incorrectly assume a deadlock above, the caller
      will still either retry indefinitely as today, (notify_alter),
      or now be able to release locks gotten so far and retry later.
    */
    if (trans->getNdbError().code == 266 &&           // 1)
        (ndb_is_gsl_participant_active() ||           // 2a)
         ndb_gsl_for_mdl_guard.is_gsl_acquired()) &&  // 2b)
        thd->mdl_context.has_locks_waited_for()) {    // 3)
      ndb_error = trans->getNdbError();
      ndb->closeTransaction(trans);
      return nullptr;
    }

    assert(trans->getNdbError().status == NdbError::TemporaryError);
    if (!retry) {
      ndb_error = trans->getNdbError();
      ndb->closeTransaction(trans);
      return nullptr;
    }
    // Sleep and then retry
    ndb->closeTransaction(trans);
    ndb_trans_retry_sleep();
  }

  // This should be unreachable code
  assert(false);
  return nullptr;
}

static bool gsl_unlock_ext(Ndb *ndb, NdbTransaction *trans,
                           NdbError &ndb_error) {
  if (trans->execute(NdbTransaction::Commit)) {
    ndb_error = trans->getNdbError();
    ndb->closeTransaction(trans);
    return false;
  }
  ndb->closeTransaction(trans);
  return true;
}

class Thd_proc_info_guard {
 public:
  Thd_proc_info_guard(THD *thd) : m_thd(thd), m_proc_info(nullptr) {}
  void set(const char *message) {
    const char *old = thd_proc_info(m_thd, message);
    if (!m_proc_info) {
      // Save the original on first change
      m_proc_info = old;
    }
  }
  ~Thd_proc_info_guard() {
    if (m_proc_info) thd_proc_info(m_thd, m_proc_info);
  }

 private:
  THD *const m_thd;
  const char *m_proc_info;
};

#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_thd.h"
#include "storage/ndb/plugin/ndb_thd_ndb.h"

/*
  lock/unlock calls are reference counted, so calls to lock
  must be matched to a call to unlock if the lock call succeeded
*/
static int ndbcluster_global_schema_lock(THD *thd,
                                         bool report_cluster_disconnected,
                                         bool record_gsl, bool *victimized) {
  Ndb *ndb = check_ndb_in_thd(thd);
  if (ndb == nullptr) {
    return -1;
  }

  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  NdbError ndb_error;
  *victimized = false;

  if (thd_ndb->check_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT)) {
    ndb_set_gsl_participant(thd);
    return 0;
  }
  DBUG_TRACE;

  if (thd_ndb->global_schema_lock_count) {
    // Remember that GSL was locked if requested
    if (record_gsl) ndb_gsl_for_mdl_guard.gsl_acquired();

    if (thd_ndb->global_schema_lock_trans)
      thd_ndb->global_schema_lock_trans->refresh();
    else
      assert(thd_ndb->global_schema_lock_error != 0);
    thd_ndb->global_schema_lock_count++;
    DBUG_PRINT("exit", ("global_schema_lock_count: %d",
                        thd_ndb->global_schema_lock_count));
    return 0;
  }
  assert(thd_ndb->global_schema_lock_count == 0);
  thd_ndb->global_schema_lock_count = 1;
  thd_ndb->global_schema_lock_error = 0;
  DBUG_PRINT("exit", ("global_schema_lock_count: %d",
                      thd_ndb->global_schema_lock_count));

  /*
    Take the lock
  */
  Thd_proc_info_guard proc_info(thd);
  proc_info.set("Waiting for ndbcluster global schema lock");
  thd_ndb->global_schema_lock_trans =
      gsl_lock_ext(thd, ndb, ndb_error, true /* retry */, false /* no_wait */);

  if (DBUG_EVALUATE_IF("sleep_after_global_schema_lock", true, false)) {
    ndb_milli_sleep(6000);
  }

  if (thd_ndb->global_schema_lock_trans) {
    ndb_log_verbose(19, "Global schema lock acquired");

    // Count number of global schema locks taken by this thread
    thd_ndb->schema_locks_count++;
    thd_ndb->global_schema_lock_count = 1;
    DBUG_PRINT("info", ("schema_locks_count: %d", thd_ndb->schema_locks_count));

    // Remember that GSL was locked if requested
    if (record_gsl) ndb_gsl_for_mdl_guard.gsl_acquired();

    // Sync point used when testing global schema lock concurrency
    DEBUG_SYNC(thd, "ndb_global_schema_lock_acquired");

    return 0;
  }
  // Else, didn't get GSL: Deadlock or failure from NDB

  /**
   * If GSL request failed due to cluster failure,
   * we consider the lock granted, else GSL request failed.
   */
  if (ndb_error.code != NDB_ERR_CLUSTER_FAILURE) {
    assert(thd_ndb->global_schema_lock_count == 1);
    // This reset triggers the special case in ndbcluster_global_schema_unlock()
    thd_ndb->global_schema_lock_count = 0;
  }

  if (ndb_error.code == 266)  // Deadlock resolution
  {
    ndb_log_info(
        "Failed to acquire global schema lock due to deadlock resolution");
    *victimized = true;
  } else if (ndb_error.code != NDB_ERR_CLUSTER_FAILURE ||
             report_cluster_disconnected) {
    if (ndb_thd_is_background_thread(thd)) {
      // Don't push any warning when background thread fail to acquire GSL
    } else {
      thd_ndb->push_ndb_error_warning(ndb_error);
      thd_ndb->push_warning("Could not acquire global schema lock");
    }
  }
  thd_ndb->global_schema_lock_error = ndb_error.code ? ndb_error.code : -1;
  return -1;
}

static int ndbcluster_global_schema_unlock(THD *thd, bool record_gsl) {
  Thd_ndb *thd_ndb = get_thd_ndb(thd);
  if (unlikely(thd_ndb == nullptr)) {
    return 0;
  }

  if (thd_ndb->check_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT)) {
    ndb_set_gsl_participant(nullptr);
    return 0;
  }

  if (thd_ndb->global_schema_lock_error != NDB_ERR_CLUSTER_FAILURE &&
      thd_ndb->global_schema_lock_count == 0) {
    // Special case to handle unlock after failure to acquire GSL due to
    // any error other than cluster failure.
    // - when cluster failure occurs the lock is granted anyway and the lock
    //   count is not reset, thus unlock() should be called.
    // - for other errors the lock is not granted, lock count is reset and
    // the exact same error code is returned. Thus it's impossible to know
    // that there is actually no need to call unlock. Fix by allowing unlock
    // without doing anything since the trans is already closed.
    assert(thd_ndb->global_schema_lock_trans == nullptr);
    thd_ndb->global_schema_lock_count++;
  }

  Ndb *ndb = thd_ndb->ndb;
  DBUG_TRACE;
  NdbTransaction *trans = thd_ndb->global_schema_lock_trans;
  // Don't allow decrementing from zero
  assert(thd_ndb->global_schema_lock_count > 0);
  thd_ndb->global_schema_lock_count--;
  DBUG_PRINT("exit", ("global_schema_lock_count: %d",
                      thd_ndb->global_schema_lock_count));
  assert(ndb != nullptr);
  if (ndb == nullptr) {
    return 0;
  }
  assert(trans != nullptr || thd_ndb->global_schema_lock_error != 0);
  if (thd_ndb->global_schema_lock_count != 0) {
    return 0;
  }
  thd_ndb->global_schema_lock_error = 0;

  if (trans) {
    thd_ndb->global_schema_lock_trans = nullptr;

    // Remember that GSL has been released
    if (record_gsl) ndb_gsl_for_mdl_guard.gsl_released();

    NdbError ndb_error;
    if (!gsl_unlock_ext(ndb, trans, ndb_error)) {
      ndb_log_warning("Failed to release global schema lock, error: (%d)%s",
                      ndb_error.code, ndb_error.message);
      thd_ndb->push_ndb_error_warning(ndb_error);
      thd_ndb->push_warning("Failed to release global schema lock");
      return -1;
    }

    ndb_log_verbose(19, "Global schema lock release");
  }
  return 0;
}

bool ndb_gsl_lock(THD *thd, bool lock, bool record_gsl, bool *victimized) {
  DBUG_TRACE;

  if (lock) {
    if (ndbcluster_global_schema_lock(thd, true, record_gsl, victimized) != 0) {
      DBUG_PRINT("error", ("Failed to lock global schema lock"));
      return true;  // Error
    }

    return false;  // OK
  }

  *victimized = false;
  if (ndbcluster_global_schema_unlock(thd, record_gsl) != 0) {
    DBUG_PRINT("error", ("Failed to unlock global schema lock"));
    return true;  // Error
  }

  return false;  // OK
}

bool Thd_ndb::has_required_global_schema_lock(const char *func) const {
  if (global_schema_lock_error) {
    // An error occurred while locking, either because
    // no connection to cluster or another user has locked
    // the lock -> ok, but caller should not allow to continue
    return false;
  }

  if (global_schema_lock_trans) {
    global_schema_lock_trans->refresh();
    return true;  // All OK
  }

  // No attempt at taking global schema lock has been done, neither
  // error or trans set -> programming error
  LEX_CSTRING query = thd_query_unsafe(m_thd);
  ndb_log_error(
      "programming error, no lock taken while running "
      "query '%*s' in function '%s'",
      (int)query.length, query.str, func);
  abort();
  return false;
}

#include "storage/ndb/plugin/ndb_global_schema_lock_guard.h"

Ndb_global_schema_lock_guard::Ndb_global_schema_lock_guard(THD *thd)
    : m_thd(thd), m_locked(false), m_try_locked(false) {}

Ndb_global_schema_lock_guard::~Ndb_global_schema_lock_guard() {
  if (m_try_locked)
    unlock();
  else if (m_locked)
    ndbcluster_global_schema_unlock(m_thd, false /* record_gsl */);
}

/**
 * Set a Global Schema Lock.
 * May fail due to either Ndb Cluster failure, or due to being
 * 'victimized' as part of deadlock resolution. In the later case we
 * retry the GSL locking.
 */
int Ndb_global_schema_lock_guard::lock(void) {
  /* only one lock call allowed */
  assert(!m_locked);

  /*
    Always set m_locked, even if lock fails. Since the
    lock/unlock calls are reference counted, the number
    of calls to lock and unlock need to match up.
  */
  m_locked = true;
  bool victimized = false;
  bool ret;
  do {
    ret = ndbcluster_global_schema_lock(m_thd, false, false /* record_gsl */,
                                        &victimized);
    if (ret && thd_killed(m_thd)) {
      // Failed to acuire GSL and THD is killed -> give up!
      break;  // Terminate loop
    }
  } while (victimized);

  return ret;
}

bool Ndb_global_schema_lock_guard::try_lock(void) {
  /*
    Always set m_locked, even if lock fails. Since the lock/unlock calls are
    reference counted, the number of calls to lock and unlock need to match up.
  */
  m_locked = true;
  m_try_locked = true;
  Thd_ndb *thd_ndb = get_thd_ndb(m_thd);
  // Check if this thd has acquired GSL already
  if (thd_ndb->global_schema_lock_count) return false;

  thd_ndb->global_schema_lock_error = 0;

  Ndb *ndb = check_ndb_in_thd(m_thd);
  NdbError ndb_error;
  // Attempt to take the GSL with no retry and no waiting
  thd_ndb->global_schema_lock_trans =
      gsl_lock_ext(m_thd, ndb, ndb_error, false, /* retry */
                   true /* no_wait */);

  if (thd_ndb->global_schema_lock_trans != nullptr) {
    ndb_log_verbose(19, "Global schema lock acquired");

    // Count number of global schema locks taken by this thread
    thd_ndb->schema_locks_count++;
    thd_ndb->global_schema_lock_count = 1;
    DBUG_PRINT("info", ("schema_locks_count: %d", thd_ndb->schema_locks_count));

    return true;
  }
  thd_ndb->global_schema_lock_error = ndb_error.code ? ndb_error.code : -1;
  return false;
}

bool Ndb_global_schema_lock_guard::unlock() {
  // This function should only be called in conjunction with try_lock()
  assert(m_try_locked);

  Thd_ndb *thd_ndb = get_thd_ndb(m_thd);
  if (unlikely(thd_ndb == nullptr)) {
    return true;
  }

  Ndb *ndb = thd_ndb->ndb;
  if (ndb == nullptr) {
    return true;
  }
  NdbTransaction *trans = thd_ndb->global_schema_lock_trans;
  thd_ndb->global_schema_lock_error = 0;
  if (trans != nullptr) {
    thd_ndb->global_schema_lock_trans = nullptr;
    thd_ndb->global_schema_lock_count = 0;

    NdbError ndb_error;
    if (!gsl_unlock_ext(ndb, trans, ndb_error)) {
      ndb_log_warning("Failed to release global schema lock, error: (%d)%s",
                      ndb_error.code, ndb_error.message);
      thd_ndb->push_ndb_error_warning(ndb_error);
      thd_ndb->push_warning("Failed to release global schema lock");
      return false;
    }
    ndb_log_verbose(19, "Global schema lock release");
  }
  return true;
}
