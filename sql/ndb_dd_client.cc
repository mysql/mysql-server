/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "ndb_dd_client.h"

#include "sql_class.h"      // Using THD
#include "mdl.h"            // MDL_*


Ndb_dd_client::~Ndb_dd_client()
{
  // Automatically release any acquired MDL locks
  if (m_mdl_locks_acquired)
    mdl_locks_release();
  assert(!m_mdl_locks_acquired);

  // Automatically restore the option_bits in THD if they have
  // been modified
  if (m_save_option_bits)
    m_thd->variables.option_bits = m_save_option_bits;
}


bool
Ndb_dd_client::mdl_locks_acquire(const char* schema_name,
                           const char* table_name)
{
  MDL_request_list mdl_requests;
  MDL_request schema_request;
  MDL_request mdl_request;
  MDL_REQUEST_INIT(&schema_request,
                   MDL_key::SCHEMA, schema_name, "", MDL_INTENTION_EXCLUSIVE,
                   MDL_TRANSACTION);
  MDL_REQUEST_INIT(&mdl_request,
                   MDL_key::TABLE, schema_name, table_name, MDL_SHARED,
                   MDL_TRANSACTION);

  mdl_requests.push_front(&schema_request);
  mdl_requests.push_front(&mdl_request);

  if (m_thd->mdl_context.acquire_locks(&mdl_requests,
                                       m_thd->variables.lock_wait_timeout))
  {
    return false;
  }

  // Remember that MDL locks where acquired
  m_mdl_locks_acquired = true;

  return true;
}


bool
Ndb_dd_client::mdl_locks_acquire_exclusive(const char* schema_name,
                                     const char* table_name)
{
  MDL_request_list mdl_requests;
  MDL_request schema_request;
  MDL_request mdl_request;

  MDL_REQUEST_INIT(&schema_request,
                   MDL_key::SCHEMA, schema_name, "", MDL_INTENTION_EXCLUSIVE,
                   MDL_TRANSACTION);
  MDL_REQUEST_INIT(&mdl_request,
                   MDL_key::TABLE, schema_name, table_name, MDL_EXCLUSIVE,
                   MDL_TRANSACTION);

  mdl_requests.push_front(&schema_request);
  mdl_requests.push_front(&mdl_request);

  if (m_thd->mdl_context.acquire_locks(&mdl_requests,
                                       m_thd->variables.lock_wait_timeout))
  {
    return false;
  }

  // Remember that MDL locks where acquired
  m_mdl_locks_acquired = true;

  return true;
}


void Ndb_dd_client::mdl_locks_release()
{
  m_thd->mdl_context.release_transactional_locks();
  m_mdl_locks_acquired = false;
}

void Ndb_dd_client::disable_autocommit()
{
  /*
    Implementation details from which storage the DD uses leaks out
    and the user of these functions magically need to turn auto commit
    off.

    I.e as in sql_table.cc, execute_ddl_log_recovery()
     'Prevent InnoDB from automatically committing InnoDB transaction
      each time data-dictionary tables are closed after being updated.'
  */

  // Don't allow empty bits as zero is used as indicator
  // to restore the saved bits
  assert(m_thd->variables.option_bits);
  m_save_option_bits = m_thd->variables.option_bits;

  m_thd->variables.option_bits&= ~OPTION_AUTOCOMMIT;
  m_thd->variables.option_bits|= OPTION_NOT_AUTOCOMMIT;
}
