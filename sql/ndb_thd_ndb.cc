/*
   Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/ndb_thd_ndb.h"

#include "my_dbug.h"
#include "mysqld_error.h"
#include "mysql/plugin.h"          // thd_get_thread_id
#include "sql/ndb_log.h"
#include "sql/ndb_thd.h"
#include "sql/sql_error.h"

/*
  Default value for max number of transactions createable against NDB from
  the handler. Should really be 2 but there is a transaction to much allocated
  when lock table is used, and one extra to used for global schema lock.
*/
static const int MAX_TRANSACTIONS= 4;


Thd_ndb*
Thd_ndb::seize(THD* thd)
{
  DBUG_ENTER("seize_thd_ndb");

  Thd_ndb* thd_ndb= new Thd_ndb(thd);
  if (thd_ndb == NULL)
    DBUG_RETURN(NULL);

  if (thd_ndb->ndb->init(MAX_TRANSACTIONS) != 0)
  {
    DBUG_PRINT("error", ("Ndb::init failed, error: %d  message: %s",
                         thd_ndb->ndb->getNdbError().code,
                         thd_ndb->ndb->getNdbError().message));
    
    delete thd_ndb;
    thd_ndb= NULL;
  }
  else
  {
    thd_ndb->ndb->setCustomData64(thd_get_thread_id(thd));
  }
  DBUG_RETURN(thd_ndb);
}


void
Thd_ndb::release(Thd_ndb* thd_ndb)
{
  DBUG_ENTER("release_thd_ndb");
  delete thd_ndb;
  DBUG_VOID_RETURN;
}


bool
Thd_ndb::recycle_ndb(void)
{
  DBUG_ENTER("recycle_ndb");
  DBUG_PRINT("enter", ("ndb: 0x%lx", (long)ndb));

  DBUG_ASSERT(global_schema_lock_trans == NULL);
  DBUG_ASSERT(trans == NULL);

  delete ndb;
  if ((ndb= new Ndb(connection, "")) == NULL)
  {
    DBUG_PRINT("error",("failed to allocate Ndb object"));
    DBUG_RETURN(false);
  }

  if (ndb->init(MAX_TRANSACTIONS) != 0)
  {
    delete ndb;
    ndb= NULL;
    DBUG_PRINT("error", ("Ndb::init failed, %d  message: %s",
                         ndb->getNdbError().code,
                         ndb->getNdbError().message));
    DBUG_RETURN(false);
  }
  else
  {
    ndb->setCustomData64(thd_get_thread_id(m_thd));
  }

  /* Reset last commit epoch for this 'session'. */
  m_last_commit_epoch_session = 0;

  /* Update m_connect_count to avoid false failures of ::valid_ndb() */
  m_connect_count= connection->get_connect_count();

  DBUG_RETURN(true);
}


bool
Thd_ndb::valid_ndb(void) const
{
  // The ndb object should be valid as long as a
  // global schema lock transaction is ongoing
  if (global_schema_lock_trans)
    return true;

  // The ndb object should be valid as long as a
  // transaction is ongoing
  if (trans)
    return true;

  if (unlikely(m_connect_count != connection->get_connect_count()))
    return false;

  return true;
}


void
Thd_ndb::init_open_tables()
{
  count= 0;
  m_error= false;
  open_tables.clear();
}


bool
Thd_ndb::check_option(Options option) const
{
  return (options & option);
}


void
Thd_ndb::set_option(Options option)
{
  options |= option;
}


/*
  Used for every additional row operation, to update the guesstimate
  of pending bytes to send, and to check if it is now time to flush a batch.
*/

bool
Thd_ndb::add_row_check_if_batch_full(uint size)
{
  if (m_unsent_bytes == 0)
    free_root(&m_batch_mem_root, MY_MARK_BLOCKS_FREE);

  uint unsent= m_unsent_bytes;
  unsent+= size;
  m_unsent_bytes= unsent;
  return unsent >= m_batch_size;
}


bool
Thd_ndb::check_trans_option(Trans_options option) const
{
  return (trans_options & option);
}


void
Thd_ndb::set_trans_option(Trans_options option)
{
#ifndef DBUG_OFF
  if (check_trans_option(TRANS_TRANSACTIONS_OFF))
    DBUG_PRINT("info", ("Disabling transactions"));
  if (check_trans_option(TRANS_INJECTED_APPLY_STATUS))
    DBUG_PRINT("info", ("Statement has written to ndb_apply_status"));
  if (check_trans_option(TRANS_NO_LOGGING))
    DBUG_PRINT("info", ("Statement is not using logging"));
#endif
  trans_options |= option;
}


void
Thd_ndb::reset_trans_options(void)
{
  DBUG_PRINT("info", ("Resetting trans_options"));
  trans_options = 0;
}


/*
  Push to THD's condition stack

  @param severity    Severity of the pushed condition
  @param code        Error code to use for the pushed condition
  @param[in]  fmt    printf-like format string
  @param[in]  args   Arguments
*/
static void push_condition(THD* thd,
                           Sql_condition::enum_severity_level severity,
                           uint code, const char* fmt, va_list args)
  MY_ATTRIBUTE((format(printf, 4, 0)));

static void push_condition(THD* thd,
                           Sql_condition::enum_severity_level severity,
                           uint code, const char* fmt, va_list args) {
  DBUG_ASSERT(fmt);

  // Assemble the message
  char msg_buf[512];
  vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);

  push_warning(thd, severity, code, msg_buf);

  // Workaround problem where Ndb_local_connection can't access the
  // warnings produced when running a SQL query, instead detect
  // if this a binlog thread and print the warning also to log.
  // NOTE! This can be removed when BUG#27507543 has been implemented
  // and instead log these warnings in a more controlled/selective manner
  // in Ndb_local_connection.
  if (ndb_thd_is_binlog_thread(thd)) {
    ndb_log_warning("%s", msg_buf);
  }
}

void Thd_ndb::push_warning(const char* fmt, ...) const {
  const uint code = ER_GET_ERRMSG;
  va_list args;
  va_start(args, fmt);
  push_condition(m_thd, Sql_condition::SL_WARNING, code, fmt, args);
  va_end(args);
}

void Thd_ndb::push_warning(uint code, const char* fmt, ...) const {
  va_list args;
  va_start(args, fmt);
  push_condition(m_thd, Sql_condition::SL_WARNING, code, fmt, args);
  va_end(args);
}
