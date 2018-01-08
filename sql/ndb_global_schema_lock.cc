/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <mysql/plugin.h>

#include "my_dbug.h"
#include "my_sys.h"               // my_sleep.h
#include "mysql/plugin.h"
#include "sql/ndb_sleep.h"
#include "sql/sql_class.h"
#include "sql/sql_thd_internal_api.h" // thd_query_unsafe
#include "storage/ndb/include/ndbapi/NdbApi.hpp"
#include "storage/ndb/include/portlib/NdbTick.h"


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

extern mysql_mutex_t ndbcluster_mutex;
static const THD *thd_gsl_participant= NULL; 

static void ndb_set_gsl_participant(const THD *thd)
{
  mysql_mutex_lock(&ndbcluster_mutex);
  thd_gsl_participant= thd;
  mysql_mutex_unlock(&ndbcluster_mutex);
}

static bool ndb_is_gsl_participant_active()
{
  mysql_mutex_lock(&ndbcluster_mutex);
  const bool state= (thd_gsl_participant != NULL);
  mysql_mutex_unlock(&ndbcluster_mutex);
  return state;
}

#include "sql/ndb_table_guard.h"

/*
  The lock/unlock functions use the BACKUP_SEQUENCE row in SYSTAB_0

  The function will retry infintely or until the THD is killed or a 
  GSL / MDL deadlock is detected/assumed. In the later case a
  timeout error (266) is returned. 

  Returns a NdbTransaction owning the gsl-lock if it was taken.
  NULL is returned if failed to take lock. Returned NdbError
  will then contain the error code if lock failed due
  to some NdbError. If there are no error code set, lock
  was rejected by lock manager, likely due to deadlock. 
*/
static NdbTransaction *
gsl_lock_ext(THD *thd, Ndb *ndb, NdbError &ndb_error)
{
  ndb->setDatabaseName("sys");
  ndb->setDatabaseSchemaName("def");
  NdbDictionary::Dictionary *dict= ndb->getDictionary();
  Ndb_table_guard ndbtab_g(dict, "SYSTAB_0");
  const NdbDictionary::Table *ndbtab= NULL;
  NdbOperation *op;
  NdbTransaction *trans= NULL;

  while (1)
  {
    if (!ndbtab)
    {
      if (!(ndbtab= ndbtab_g.get_table()))
      {
        if (dict->getNdbError().status == NdbError::TemporaryError)
          goto retry;
        ndb_error= dict->getNdbError();
        goto error_handler;
      }
    }

    trans= ndb->startTransaction();
    if (trans == NULL)
    {
      ndb_error= ndb->getNdbError();
      goto error_handler;
    }

    op= trans->getNdbOperation(ndbtab);
    op->readTuple(NdbOperation::LM_Exclusive);
    op->equal("SYSKEY_0", NDB_BACKUP_SEQUENCE);

    if (trans->execute(NdbTransaction::NoCommit) == 0)
      break;

    if (trans->getNdbError().status != NdbError::TemporaryError)
      goto error_handler;
    else if (thd_killed(thd))
      goto error_handler;

    /**
     * Check for MDL / GSL deadlock. A deadlock is assumed if:
     *  1) ::execute failed with a timeout error.
     *  2) There already is another THD being an participant
     *     in a schema distr. operation (which implies that
     *     the coordinator already held the GSL.
     *  3) This THD holds a lock being waited for by another THD
     *
     * Note: If we incorrectly assume a deadlock above, the calle
     * will still either retry indefinitely as today, (notify_alter),
     * or now be able to release locks gotten so far and retry later.
     */
    if (trans->getNdbError().code == 266 &&     // 1)
        ndb_is_gsl_participant_active()  &&     // 2)
        thd->mdl_context.has_locks_waited_for())// 3)
      goto error_handler;

  retry:
    if (trans)
    {
      ndb->closeTransaction(trans);
      trans= NULL;
    }

    const unsigned retry_sleep= 50; /* 50 milliseconds, transaction */
    ndb_retry_sleep(retry_sleep);
  }
  return trans;

 error_handler:
  if (trans)
  {
    ndb_error= trans->getNdbError();
    ndb->closeTransaction(trans);
  }
  return NULL;
}


static bool
gsl_unlock_ext(Ndb *ndb, NdbTransaction *trans,
               NdbError &ndb_error)
{
  if (trans->execute(NdbTransaction::Commit))
  {
    ndb_error= trans->getNdbError();
    ndb->closeTransaction(trans);
    return false;
  }
  ndb->closeTransaction(trans);
  return true;
}


// NOTE! 'thd_proc_info' is defined in myql/plugin.h but not implemented, only
// a #define available in sql_class.h -> include sql_class.h until
// bug#11844974 has been fixed. 
#include "sql/sql_class.h" 

class Thd_proc_info_guard
{
public:
  Thd_proc_info_guard(THD *thd)
   : m_thd(thd), m_proc_info(NULL) {}
  void set(const char* message)
  {
    const char* old= thd_proc_info(m_thd, message);
    if (!m_proc_info)
    {
      // Save the original on first change
      m_proc_info = old;
    }
  }
  ~Thd_proc_info_guard()
  {
    if (m_proc_info)
      thd_proc_info(m_thd, m_proc_info);
  }
private:
  THD* const m_thd;
  const char *m_proc_info;
};


#include "sql/derror.h"
#include "sql/ndb_log.h"
#include "sql/ndb_thd.h"
#include "sql/ndb_thd_ndb.h"

/*
  lock/unlock calls are reference counted, so calls to lock
  must be matched to a call to unlock if the lock call succeeded
*/
static
int
ndbcluster_global_schema_lock(THD *thd,
                              bool report_cluster_disconnected,
                              bool *victimized)
{
  Ndb *ndb= check_ndb_in_thd(thd);
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  NdbError ndb_error;
  *victimized= false;

  if (thd_ndb->check_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT))
  {
    ndb_set_gsl_participant(thd);
    return 0;
  }
  DBUG_ENTER("ndbcluster_global_schema_lock");

  if (thd_ndb->global_schema_lock_count)
  {
    if (thd_ndb->global_schema_lock_trans)
      thd_ndb->global_schema_lock_trans->refresh();
    else
      DBUG_ASSERT(thd_ndb->global_schema_lock_error != 0);
    thd_ndb->global_schema_lock_count++;
    DBUG_PRINT("exit", ("global_schema_lock_count: %d",
                        thd_ndb->global_schema_lock_count));
    DBUG_RETURN(0);
  }
  DBUG_ASSERT(thd_ndb->global_schema_lock_count == 0);
  thd_ndb->global_schema_lock_count= 1;
  thd_ndb->global_schema_lock_error= 0;
  DBUG_PRINT("exit", ("global_schema_lock_count: %d",
                      thd_ndb->global_schema_lock_count));


  /*
    Take the lock
  */
  Thd_proc_info_guard proc_info(thd);
  proc_info.set("Waiting for ndbcluster global schema lock");
  thd_ndb->global_schema_lock_trans= gsl_lock_ext(thd, ndb, ndb_error);

  if (DBUG_EVALUATE_IF("sleep_after_global_schema_lock", true, false))
  {
    ndb_milli_sleep(6000);
  }

  if (thd_ndb->global_schema_lock_trans)
  {
    ndb_log_verbose(19, "Global schema lock acquired");

    // Count number of global schema locks taken by this thread
    thd_ndb->schema_locks_count++;
    DBUG_PRINT("info", ("schema_locks_count: %d",
                        thd_ndb->schema_locks_count));

    DBUG_RETURN(0);
  }
  // Else, didn't get GSL: Deadlock or failure from NDB

  /**
   * If GSL request failed due to no cluster connection (4009),
   * we consider the lock granted, else GSL request failed.
   */
  if (ndb_error.code != 4009)  //No cluster connection
  {
    DBUG_ASSERT(thd_ndb->global_schema_lock_count == 1);
    thd_ndb->global_schema_lock_count= 0;
  }

  if (ndb_error.code == 266)  //Deadlock resolution
  {
    ndb_log_info("Failed to acquire global schema lock due to deadlock resolution");
    *victimized= true;
  }
  else if (ndb_error.code != 4009 || report_cluster_disconnected)
  {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_GET_ERRMSG, ER_DEFAULT(ER_GET_ERRMSG),
                        ndb_error.code, ndb_error.message,
                        "NDB. Could not acquire global schema lock");
  }
  thd_ndb->global_schema_lock_error= ndb_error.code ? ndb_error.code : -1;
  DBUG_RETURN(-1);
}


static
int
ndbcluster_global_schema_unlock(THD *thd)
{
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  DBUG_ASSERT(thd_ndb != 0);
  if (unlikely(thd_ndb == NULL))
  {
    return 0;
  }
  else if (thd_ndb->check_option(Thd_ndb::IS_SCHEMA_DIST_PARTICIPANT))
  {
    ndb_set_gsl_participant(NULL);
    return 0;
  }
  Ndb *ndb= thd_ndb->ndb;
  DBUG_ENTER("ndbcluster_global_schema_unlock");
  NdbTransaction *trans= thd_ndb->global_schema_lock_trans;
  thd_ndb->global_schema_lock_count--;
  DBUG_PRINT("exit", ("global_schema_lock_count: %d",
                      thd_ndb->global_schema_lock_count));
  DBUG_ASSERT(ndb != NULL);
  if (ndb == NULL)
  {
    DBUG_RETURN(0);
  }
  DBUG_ASSERT(trans != NULL || thd_ndb->global_schema_lock_error != 0);
  if (thd_ndb->global_schema_lock_count != 0)
  {
    DBUG_RETURN(0);
  }
  thd_ndb->global_schema_lock_error= 0;

  if (trans)
  {
    thd_ndb->global_schema_lock_trans= NULL;
    NdbError ndb_error;
    if (!gsl_unlock_ext(ndb, trans, ndb_error))
    {
      ndb_log_warning("Failed to release global schema lock, error: (%d)%s",
                      ndb_error.code, ndb_error.message);
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_GET_ERRMSG, ER_DEFAULT(ER_GET_ERRMSG),
                          ndb_error.code,
                          ndb_error.message,
                          "ndb. Releasing global schema lock");
      DBUG_RETURN(-1);
    }

    ndb_log_verbose(19, "Global schema lock release");
  }
  DBUG_RETURN(0);
}


static
bool
notify_mdl_lock(THD *thd, bool lock, bool *victimized)
{
  DBUG_ENTER("notify_mdl_lock");

  if (lock)
  {
    if (ndbcluster_global_schema_lock(thd, true, victimized) != 0)
    {
      DBUG_PRINT("error", ("Failed to lock global schema lock"));
      /*
        If not 'victimized' in order to avoid deadlocks:
 
        Ignore error to lock GSL and let execution continue
        until one of ha_ndbcluster's DDL functions use
        Thd_ndb::has_required_global_schema_lock() to verify
        if the GSL is taken or not.
        This allows users to work with non NDB objects although
        failure to lock GSL occurs(for example because connection
        to NDB is not available).

        Victimized failures are handled immediately by MDL
        releasing, and later retrying the locks.
      */
      DBUG_RETURN(*victimized == true); // Ignore error if not 'victimized'
    }
    DBUG_RETURN(false); // OK
  }

  *victimized= false;
  if (ndbcluster_global_schema_unlock(thd) != 0)
  {
    DBUG_PRINT("error", ("Failed to unlock global schema lock"));
    DBUG_RETURN(true); // Error
  }
  DBUG_RETURN(false); // OK
}


#ifndef DBUG_OFF
static
const char*
mdl_namespace_name(const MDL_key* mdl_key)
{
  switch(mdl_key->mdl_namespace())
  {
  case MDL_key::GLOBAL:
    return "GLOBAL";
  case MDL_key::SCHEMA:
    return "SCHEMA";
  case MDL_key::TABLESPACE:
    return "TABLESPACE";
  case MDL_key::TABLE:
    return "TABLE";
  case MDL_key::FUNCTION:
    return "FUNCTION";
  case MDL_key::PROCEDURE:
    return "PROCEDURE";
  case MDL_key::TRIGGER:
    return "TRIGGER";
  case MDL_key::EVENT:
    return "EVENT";
  default:
    return "<unknown>";
  }
}
#endif


/**
  Callback handling the notification of ALTER TABLE start and end
  on the given key. The function locks or unlocks the GSL thus
  preventing concurrent modification to any other object in
  the cluster.

  @param thd                Thread context.
  @param mdl_key            MDL key identifying table which is going to be
                            or was ALTERed.
  @param notification_type  Indicates whether this is pre-ALTER TABLE or
                            post-ALTER TABLE notification.

  @note This is an additional notification that spans the duration
        of the whole ALTER TABLE thus avoiding the need for an expensive
        abort of the ALTER late in the process when upgrade to X
        metadata lock happens.

  @note This callback is called in addition to notify_exclusive_mdl()
        which means that during an ALTER TABLE we will get two different
        calls to take and release GSL.

  @see notify_alter_table() in handler.h
*/

static
bool
ndbcluster_notify_alter_table(THD *thd,
                              const MDL_key *mdl_key MY_ATTRIBUTE((unused)),
                              ha_notification_type notification)
{
  DBUG_ENTER("ndbcluster_notify_alter_table");
  DBUG_PRINT("enter", ("namespace: '%s', db: '%s', name: '%s'",
                       mdl_namespace_name(mdl_key),
                       mdl_key->db_name(), mdl_key->name()));

  DBUG_ASSERT(notification == HA_NOTIFY_PRE_EVENT ||
              notification == HA_NOTIFY_POST_EVENT);

  bool victimized= false;
  bool result;
  do
  {
    result =
      notify_mdl_lock(thd,
                      notification == HA_NOTIFY_PRE_EVENT, &victimized);
  }
  while (victimized);
  DBUG_RETURN(result);
}


/**
  Callback handling the notification about acquisition or after
  release of exclusive metadata lock on object represented by
  key. The function locks or unlocks the GSL thus preventing
  concurrent modification to any other object in the cluster

  @param thd                Thread context.
  @param mdl_key            MDL key identifying object on which exclusive
                            lock is to be acquired/was released.
  @param notification_type  Indicates whether this is pre-acquire or
                            post-release notification.
  @param victimized        'true' if locking failed as we were choosen
                            as a victim in order to avoid possible deadlocks.

  @see notify_exclusive_mdl() in handler.h
*/

static
bool
ndbcluster_notify_exclusive_mdl(THD *thd,
                                const MDL_key *mdl_key MY_ATTRIBUTE((unused)),
                                ha_notification_type notification,
                                bool *victimized)
{
  DBUG_ENTER("ndbcluster_notify_exclusive_mdl");
  DBUG_PRINT("enter", ("namespace: '%s', db: '%s', name: '%s'",
                       mdl_namespace_name(mdl_key),
                       mdl_key->db_name(), mdl_key->name()));

  DBUG_ASSERT(notification == HA_NOTIFY_PRE_EVENT ||
              notification == HA_NOTIFY_POST_EVENT);

  const bool result =
      notify_mdl_lock(thd,
                      notification == HA_NOTIFY_PRE_EVENT, victimized);
  DBUG_RETURN(result);
}


#include "sql/ndb_global_schema_lock.h"

void ndbcluster_global_schema_lock_init(handlerton *hton)
{
  hton->notify_alter_table = ndbcluster_notify_alter_table;
  hton->notify_exclusive_mdl = ndbcluster_notify_exclusive_mdl;
}


void ndbcluster_global_schema_lock_deinit(handlerton* hton)
{
  hton->notify_alter_table = NULL;
  hton->notify_exclusive_mdl = NULL;
}


bool
Thd_ndb::has_required_global_schema_lock(const char* func) const
{
  if (global_schema_lock_error)
  {
    // An error occured while locking, either because
    // no connection to cluster or another user has locked
    // the lock -> ok, but caller should not allow to continue
    return false;
  }

  if (global_schema_lock_trans)
  {
    global_schema_lock_trans->refresh();
    return true; // All OK
  }

  // No attempt at taking global schema lock has been done, neither
  // error or trans set -> programming error
  LEX_CSTRING query= thd_query_unsafe(m_thd);
  ndb_log_error("programming error, no lock taken while running "
                "query '%*s' in function '%s'",
                (int)query.length, query.str, func);
  abort();
  return false;
}


#include "sql/ndb_global_schema_lock_guard.h"

Ndb_global_schema_lock_guard::Ndb_global_schema_lock_guard(THD *thd)
  : m_thd(thd), m_locked(false)
{
}


Ndb_global_schema_lock_guard::~Ndb_global_schema_lock_guard()
{
  if (m_locked)
    ndbcluster_global_schema_unlock(m_thd);
}

/**
 * Set a Global Schema Lock.
 * May fail due to either Ndb Cluster failure, or due to being
 * 'victimized' as part of deadlock resolution. In the later case we
 * retry the GSL locking.
 */
int Ndb_global_schema_lock_guard::lock(void)
{
  /* only one lock call allowed */
  assert(!m_locked);

  /*
    Always set m_locked, even if lock fails. Since the
    lock/unlock calls are reference counted, the number
    of calls to lock and unlock need to match up.
  */
  m_locked= true;
  bool victimized= false;
  bool ret;
  do
  {
    ret= ndbcluster_global_schema_lock(m_thd, false, &victimized);
  }
  while (victimized);

  return ret;
}
