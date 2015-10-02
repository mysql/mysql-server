/*
   Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "transaction_info.h"
#include "mysys_err.h"
#include "sql_class.h"

Transaction_ctx::Transaction_ctx()
  : m_savepoints(NULL), m_xid_state(), m_changed_tables(NULL),
    last_committed(0), sequence_number(0),
    m_rpl_transaction_ctx(), m_transaction_write_set_ctx()
{
  memset(&m_scope_info, 0, sizeof(m_scope_info));
  memset(&m_flags, 0, sizeof(m_flags));
  init_sql_alloc(key_memory_thd_transactions, &m_mem_root,
                 global_system_variables.trans_alloc_block_size,
                 global_system_variables.trans_prealloc_size);
}


void Transaction_ctx::push_unsafe_rollback_warnings(THD *thd)
{
  if (m_scope_info[SESSION].has_modified_non_trans_table())
    push_warning(thd, Sql_condition::SL_WARNING,
                 ER_WARNING_NOT_COMPLETE_ROLLBACK,
                 ER(ER_WARNING_NOT_COMPLETE_ROLLBACK));

  if (m_scope_info[SESSION].has_created_temp_table())
    push_warning(thd, Sql_condition::SL_WARNING,
                 ER_WARNING_NOT_COMPLETE_ROLLBACK_WITH_CREATED_TEMP_TABLE,
                 ER(ER_WARNING_NOT_COMPLETE_ROLLBACK_WITH_CREATED_TEMP_TABLE));

  if (m_scope_info[SESSION].has_dropped_temp_table())
    push_warning(thd, Sql_condition::SL_WARNING,
                 ER_WARNING_NOT_COMPLETE_ROLLBACK_WITH_DROPPED_TEMP_TABLE,
                 ER(ER_WARNING_NOT_COMPLETE_ROLLBACK_WITH_DROPPED_TEMP_TABLE));
}


bool Transaction_ctx::add_changed_table(const char *key, long key_length)
{
  DBUG_ENTER("Transaction_ctx::add_changed_table");
  CHANGED_TABLE_LIST **prev_changed = &m_changed_tables;
  CHANGED_TABLE_LIST *curr = m_changed_tables;

  for (; curr; prev_changed = &(curr->next), curr = curr->next)
  {
    int cmp =  (long)curr->key_length - key_length;
    if (cmp < 0)
    {
      if (list_include(prev_changed, curr,
                       changed_table_dup(key, key_length)))
        DBUG_RETURN(true);

      DBUG_PRINT("info",
                 ("key_length: %ld  %u", key_length,
                     (*prev_changed)->key_length));
      DBUG_RETURN(false);
    }
    else if (cmp == 0)
    {
      cmp = memcmp(curr->key, key, curr->key_length);
      if (cmp < 0)
      {
        if (list_include(prev_changed, curr, changed_table_dup(key, key_length)))
          DBUG_RETURN(true);

        DBUG_PRINT("info",
                   ("key_length:  %ld  %u", key_length,
                       (*prev_changed)->key_length));
        DBUG_RETURN(false);
      }
      else if (cmp == 0)
      {
        DBUG_PRINT("info", ("already in list"));
        DBUG_RETURN(false);
        DBUG_RETURN(false);
      }
    }
  }
  *prev_changed = changed_table_dup(key, key_length);
  if (!(*prev_changed))
    DBUG_RETURN(true);
  DBUG_PRINT("info", ("key_length: %ld  %u", key_length,
      (*prev_changed)->key_length));
  DBUG_RETURN(false);
}


CHANGED_TABLE_LIST* Transaction_ctx::changed_table_dup(const char *key,
                                                       long key_length)
{
  CHANGED_TABLE_LIST* new_table =
      (CHANGED_TABLE_LIST*) allocate_memory(
          ALIGN_SIZE(sizeof(CHANGED_TABLE_LIST)) + key_length + 1);
  if (!new_table)
  {
    my_error(EE_OUTOFMEMORY, MYF(ME_FATALERROR),
             ALIGN_SIZE(sizeof(TABLE_LIST)) + key_length + 1);
    return 0;
  }

  new_table->key= ((char*)new_table)+ ALIGN_SIZE(sizeof(CHANGED_TABLE_LIST));
  new_table->next = 0;
  new_table->key_length = key_length;
  ::memcpy(new_table->key, key, key_length);
  return new_table;
}
