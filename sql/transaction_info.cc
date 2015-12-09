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

#include "mysys_err.h"          // EE_OUTOFMEMORY
#include "derror.h"             // ER_THD
#include "mysqld.h"             // global_system_variables
#include "mysqld_error.h"       // ER_*
#include "psi_memory_key.h"     // key_memory_thd_transactions
#include "sql_cache.h"          // query_cache
#include "sql_class.h"          // THD_STAGE_INFO
#include "sql_error.h"          // Sql_condition
#include "system_variables.h"   // System_variables
#include "table.h"              // TABLE_LIST


typedef struct st_changed_table_list
{
  struct        st_changed_table_list *next;
  char	        *key;
  uint32        key_length;
} CHANGED_TABLE_LIST;


/* routings to adding tables to list of changed in transaction tables */
static void list_include(CHANGED_TABLE_LIST** prev,
                         CHANGED_TABLE_LIST* curr,
                         CHANGED_TABLE_LIST* new_table)
{
  *prev= new_table;
  (*prev)->next= curr;
}


static CHANGED_TABLE_LIST* changed_table_dup(MEM_ROOT *mem_root,
                                             const char *key,
                                             uint32 key_length)
{
  CHANGED_TABLE_LIST* new_table =
    reinterpret_cast<CHANGED_TABLE_LIST*>(alloc_root(mem_root,
      ALIGN_SIZE(sizeof(CHANGED_TABLE_LIST)) + key_length + 1));
  DBUG_ASSERT(new_table);

  new_table->key= reinterpret_cast<char*>(new_table) +
    ALIGN_SIZE(sizeof(CHANGED_TABLE_LIST));
  new_table->next= NULL;
  new_table->key_length = key_length;
  ::memcpy(new_table->key, key, key_length);
  return new_table;
}


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
                 ER_THD(thd, ER_WARNING_NOT_COMPLETE_ROLLBACK));

  if (m_scope_info[SESSION].has_created_temp_table())
    push_warning(thd, Sql_condition::SL_WARNING,
                 ER_WARNING_NOT_COMPLETE_ROLLBACK_WITH_CREATED_TEMP_TABLE,
                 ER_THD(thd, ER_WARNING_NOT_COMPLETE_ROLLBACK_WITH_CREATED_TEMP_TABLE));

  if (m_scope_info[SESSION].has_dropped_temp_table())
    push_warning(thd, Sql_condition::SL_WARNING,
                 ER_WARNING_NOT_COMPLETE_ROLLBACK_WITH_DROPPED_TEMP_TABLE,
                 ER_THD(thd, ER_WARNING_NOT_COMPLETE_ROLLBACK_WITH_DROPPED_TEMP_TABLE));
}


void Transaction_ctx::invalidate_changed_tables_in_cache(THD *thd)
{
  if (m_changed_tables)
  {
    CHANGED_TABLE_LIST *tables_used= m_changed_tables;
    THD_STAGE_INFO(thd, stage_invalidating_query_cache_entries_table_list);
    for (; tables_used; tables_used= tables_used->next)
    {
      query_cache.invalidate(thd, tables_used->key,
                             tables_used->key_length, false);
    }
  }
}


void Transaction_ctx::add_changed_table(const char *key, uint32 key_length)
{
  DBUG_ENTER("Transaction_ctx::add_changed_table");
  CHANGED_TABLE_LIST **prev_changed = &m_changed_tables;
  CHANGED_TABLE_LIST *curr = m_changed_tables;

  for (; curr; prev_changed = &(curr->next), curr = curr->next)
  {
    int cmp =  (long)curr->key_length - key_length;
    if (cmp < 0)
    {
      list_include(prev_changed, curr,
                   changed_table_dup(&m_mem_root, key, key_length));
      DBUG_VOID_RETURN;
    }
    else if (cmp == 0)
    {
      cmp = memcmp(curr->key, key, curr->key_length);
      if (cmp < 0)
      {
        list_include(prev_changed, curr,
                     changed_table_dup(&m_mem_root, key, key_length));
        DBUG_VOID_RETURN;
      }
      else if (cmp == 0)
      {
        DBUG_PRINT("info", ("already in list"));
        DBUG_VOID_RETURN;
      }
    }
  }
  *prev_changed= changed_table_dup(&m_mem_root, key, key_length);
  DBUG_VOID_RETURN;
}


void Transaction_ctx::register_ha(
  enum_trx_scope scope, Ha_trx_info *ha_info, handlerton *ht)
{
  ha_info->register_ha(&m_scope_info[scope], ht);
}
