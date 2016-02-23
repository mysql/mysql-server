/*
   Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <my_global.h>
#include <mysql/plugin.h>
#include <ndbapi/NdbApi.hpp>
#include <portlib/NdbTick.h>
#include <my_sys.h>               // my_sleep.h

/* perform random sleep in the range milli_sleep to 2*milli_sleep */
static inline
void do_retry_sleep(unsigned milli_sleep)
{
  my_sleep(1000*(milli_sleep + 5*(rand()%(milli_sleep/5))));
}


#include "ndb_table_guard.h"

/*
  The lock/unlock functions use the BACKUP_SEQUENCE row in SYSTAB_0

  retry_time == 0 means no retry
  retry_time <  0 means infinite retries
  retry_time >  0 means retries for max 'retry_time' seconds
*/
static NdbTransaction *
gsl_lock_ext(THD *thd, Ndb *ndb, NdbError &ndb_error,
             int retry_time= 10)
{
  ndb->setDatabaseName("sys");
  ndb->setDatabaseSchemaName("def");
  NdbDictionary::Dictionary *dict= ndb->getDictionary();
  Ndb_table_guard ndbtab_g(dict, "SYSTAB_0");
  const NdbDictionary::Table *ndbtab= NULL;
  NdbOperation *op;
  NdbTransaction *trans= NULL;
  int retry_sleep= 50; /* 50 milliseconds, transaction */
  NDB_TICKS start;

  if (retry_time > 0)
  {
    start = NdbTick_getCurrentTicks();
  }
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
  retry:
    if (retry_time == 0)
      goto error_handler;
    if (retry_time > 0)
    {
      const NDB_TICKS now = NdbTick_getCurrentTicks();
      if (NdbTick_Elapsed(start,now).seconds() > (Uint64)retry_time)
        goto error_handler;
    }
    if (trans)
    {
      ndb->closeTransaction(trans);
      trans= NULL;
    }
    do_retry_sleep(retry_sleep);
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

/*
  lock/unlock calls are reference counted, so calls to lock
  must be matched to a call to unlock even if the lock call fails
*/
static int gsl_is_locked_or_queued= 0;
static int gsl_no_locking_allowed= 0;
static native_mutex_t gsl_mutex;

/*
  Indicates if ndb_global_schema_lock module is active/initialized, normally
  turned on/off in ndbcluster_init/deinit with LOCK_plugin held.
*/
static bool gsl_initialized= false;

// NOTE! 'thd_proc_info' is defined in myql/plugin.h but not implemented, only
// a #define available in sql_class.h -> include sql_class.h until
// bug#11844974 has been fixed. 
#include <sql_class.h> 

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
  THD *m_thd;
  const char *m_proc_info;
};


#include "ndb_thd.h"
#include "ndb_thd_ndb.h"
#include "log.h"


extern ulong opt_ndb_extra_logging;

static
int
ndbcluster_global_schema_lock(THD *thd, bool no_lock_queue,
                              bool report_cluster_disconnected)
{
  if (!gsl_initialized)
    return 0;

  Ndb *ndb= check_ndb_in_thd(thd);
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  NdbError ndb_error;
  if (thd_ndb->check_option(TNO_NO_LOCK_SCHEMA_OP))
    return 0;
  DBUG_ENTER("ndbcluster_global_schema_lock");
  DBUG_PRINT("enter", ("query: '%-.4096s', no_lock_queue: %d",
                       thd_query_unsafe(thd).str, no_lock_queue));
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
    Check that taking the lock is allowed
    - if not allowed to enter lock queue, return if lock exists
    - wait until allowed
    - increase global lock count
  */
  Thd_proc_info_guard proc_info(thd);
  native_mutex_lock(&gsl_mutex);
  /* increase global lock count */
  gsl_is_locked_or_queued++;
  if (no_lock_queue)
  {
    if (gsl_is_locked_or_queued != 1)
    {
      /* Other thread has lock and this thread may not enter lock queue */
      native_mutex_unlock(&gsl_mutex);
      thd_ndb->global_schema_lock_error= -1;
      DBUG_PRINT("exit", ("aborting as lock exists"));
      DBUG_RETURN(-1);
    }
    /* Mark that no other thread may be take lock */
    gsl_no_locking_allowed= 1;
  }
  else
  {
    while (gsl_no_locking_allowed)
    {
      proc_info.set("Waiting for allowed to take ndbcluster global schema lock");
      /* Wait until locking is allowed */
      native_mutex_unlock(&gsl_mutex);
      do_retry_sleep(50);
      if (thd_killed(thd))
      {
        thd_ndb->global_schema_lock_error= -1;
        DBUG_RETURN(-1);
      }
      native_mutex_lock(&gsl_mutex);
    }
  }
  native_mutex_unlock(&gsl_mutex);

  /*
    Take the lock
  */
  proc_info.set("Waiting for ndbcluster global schema lock");
  thd_ndb->global_schema_lock_trans= gsl_lock_ext(thd, ndb, ndb_error, -1);

  DBUG_EXECUTE_IF("sleep_after_global_schema_lock", my_sleep(6000000););

  if (no_lock_queue)
  {
    native_mutex_lock(&gsl_mutex);
    /* Mark that other thread may be take lock */
    gsl_no_locking_allowed= 0;
    native_mutex_unlock(&gsl_mutex);
  }

  if (thd_ndb->global_schema_lock_trans)
  {
    if (opt_ndb_extra_logging > 19)
    {
      sql_print_information("NDB: Global schema lock acquired");
    }

    // Count number of global schema locks taken by this thread
    thd_ndb->schema_locks_count++;
    DBUG_PRINT("info", ("schema_locks_count: %d",
                        thd_ndb->schema_locks_count));

    DBUG_RETURN(0);
  }

  if (ndb_error.code != 4009 || report_cluster_disconnected)
  {
    sql_print_warning("NDB: Could not acquire global schema lock (%d)%s",
                      ndb_error.code, ndb_error.message);
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
  if (!gsl_initialized)
    return 0;

  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  DBUG_ASSERT(thd_ndb != 0);
  if (thd_ndb == 0 ||
      thd_ndb->check_option(TNO_NO_LOCK_SCHEMA_OP))
  {
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

  /*
    Decrease global lock count
  */
  native_mutex_lock(&gsl_mutex);
  gsl_is_locked_or_queued--;
  native_mutex_unlock(&gsl_mutex);

  if (trans)
  {
    thd_ndb->global_schema_lock_trans= NULL;
    NdbError ndb_error;
    if (!gsl_unlock_ext(ndb, trans, ndb_error))
    {
      sql_print_warning("NDB: Releasing global schema lock (%d)%s",
                        ndb_error.code, ndb_error.message);
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_GET_ERRMSG, ER_DEFAULT(ER_GET_ERRMSG),
                          ndb_error.code,
                          ndb_error.message,
                          "ndb. Releasing global schema lock");
      DBUG_RETURN(-1);
    }
    if (opt_ndb_extra_logging > 19)
    {
      sql_print_information("NDB: Global schema lock release");
    }
  }
  DBUG_RETURN(0);
}


static
bool
notify_mdl_lock(THD *thd, bool lock)
{
  DBUG_ENTER("notify_mdl_lock");

  if (lock)
  {
    if (ndbcluster_global_schema_lock(thd, false, true) != 0)
    {
      DBUG_PRINT("error", ("Failed to lock global schema lock"));
      /*
        Ignore error to lock GSL and let execution continue
        until one of ha_ndbcluster's DDL functions use
        Thd_ndb::has_required_global_schema_lock() to verify
        if the GSL is taken or not.
        This allows users to work with non NDB objects although
        failure to lock GSL occurs(for example because connection
        to NDB is not available).
      */
      DBUG_RETURN(false); // Ignore error!
    }
    DBUG_RETURN(false); // OK
  }


  if (ndbcluster_global_schema_unlock(thd) != 0)
  {
    DBUG_PRINT("error", ("Failed to unlock global schema lock"));
    DBUG_RETURN(true); // Error
  }
  DBUG_RETURN(false); // OK
}


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
ndbcluster_notify_alter_table(THD *thd, const MDL_key *mdl_key,
                              ha_notification_type notification)
{
  DBUG_ENTER("ndbcluster_notify_alter_table");
  DBUG_PRINT("enter", ("namespace: '%s', db: '%s', name: '%s'",
                       mdl_namespace_name(mdl_key),
                       mdl_key->db_name(), mdl_key->name()));

  DBUG_ASSERT(notification == HA_NOTIFY_PRE_EVENT ||
              notification == HA_NOTIFY_POST_EVENT);

  const bool result =
      notify_mdl_lock(thd,
                      notification == HA_NOTIFY_PRE_EVENT);
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

  @see notify_exclusive_mdl() in handler.h
*/

static
bool
ndbcluster_notify_exclusive_mdl(THD *thd, const MDL_key *mdl_key,
                                ha_notification_type notification)
{
  DBUG_ENTER("ndbcluster_notify_exclusive_mdl");
  DBUG_PRINT("enter", ("namespace: '%s', db: '%s', name: '%s'",
                       mdl_namespace_name(mdl_key),
                       mdl_key->db_name(), mdl_key->name()));

  DBUG_ASSERT(notification == HA_NOTIFY_PRE_EVENT ||
              notification == HA_NOTIFY_POST_EVENT);

  const bool result =
      notify_mdl_lock(thd,
                      notification == HA_NOTIFY_PRE_EVENT);
  DBUG_RETURN(result);
}


#include "ndb_global_schema_lock.h"

void ndbcluster_global_schema_lock_init(handlerton *hton)
{
  assert(gsl_initialized == false);
  assert(gsl_is_locked_or_queued == 0);
  assert(gsl_no_locking_allowed == 0);
  gsl_initialized= true;
  native_mutex_init(&gsl_mutex, MY_MUTEX_INIT_FAST);

  hton->notify_alter_table = ndbcluster_notify_alter_table;
  hton->notify_exclusive_mdl = ndbcluster_notify_exclusive_mdl;
}


void ndbcluster_global_schema_lock_deinit(void)
{
  assert(gsl_initialized == true);
  assert(gsl_is_locked_or_queued == 0);
  assert(gsl_no_locking_allowed == 0);
  gsl_initialized= false;
  native_mutex_destroy(&gsl_mutex);
}


bool
Thd_ndb::has_required_global_schema_lock(const char* func)
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
  sql_print_error("NDB: programming error, no lock taken while running "
                  "query '%*s' in function '%s'",
                  (int)query.length, query.str, func);
  abort();
  return false;
}


#include "ndb_global_schema_lock_guard.h"

Ndb_global_schema_lock_guard::Ndb_global_schema_lock_guard(THD *thd)
  : m_thd(thd), m_locked(false)
{
}


Ndb_global_schema_lock_guard::~Ndb_global_schema_lock_guard()
{
  if (m_locked)
    ndbcluster_global_schema_unlock(m_thd);
}


int Ndb_global_schema_lock_guard::lock(bool no_lock_queue,
                                       bool report_cluster_disconnected)
{
  /* only one lock call allowed */
  assert(!m_locked);

  /*
    Always set m_locked, even if lock fails. Since the
    lock/unlock calls are reference counted, the number
    of calls to lock and unlock need to match up.
  */
  m_locked= true;

  return ndbcluster_global_schema_lock(m_thd, no_lock_queue,
                                       report_cluster_disconnected);  
}
