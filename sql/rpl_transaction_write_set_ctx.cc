/* Copyright (c) 2014, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "rpl_transaction_write_set_ctx.h"

#include "mysql/service_rpl_transaction_write_set.h" // Transaction_write_set
#include "mysqld_thd_manager.h"                      // Global_THD_manager
#include "sql_class.h"                               // THD
#include "sql_parse.h"                               // Find_thd_with_id
#include "debug_sync.h"                              // debug_sync_set_action
#include "binlog.h"                              // get_opt_max_history_size

int32 Rpl_transaction_write_set_ctx::m_global_component_requires_write_sets(0);
int64 Rpl_transaction_write_set_ctx::m_global_write_set_memory_size_limit(0);

Rpl_transaction_write_set_ctx::Rpl_transaction_write_set_ctx()
    : m_has_missing_keys(false),
      m_has_related_foreign_keys(false),
      m_ignore_write_set_memory_limit(false),
      m_local_allow_drop_write_set(false),
      m_local_has_reached_write_set_limit(false)
{
  DBUG_ENTER("Rpl_transaction_write_set_ctx::Rpl_transaction_write_set_ctx");
  /*
    In order to speed-up small transactions write-set extraction,
    we preallocate 24 elements.
    24 is a sufficient number to hold write-sets for single
    statement transactions, even on tables with foreign keys.
  */
  write_set.reserve(24);
  DBUG_VOID_RETURN;
}

bool Rpl_transaction_write_set_ctx::add_write_set(uint64 hash)
{
  DBUG_EXECUTE_IF("add_write_set_no_memory", throw std::bad_alloc(););

  if (!m_local_has_reached_write_set_limit) {
    ulong binlog_trx_dependency_history_size =
        mysql_bin_log.m_dependency_tracker.get_writeset()
            ->get_opt_max_history_size();
    bool is_full_writeset_required =
        m_global_component_requires_write_sets && !m_local_allow_drop_write_set;

    if (!is_full_writeset_required) {
      if (write_set.size() >= binlog_trx_dependency_history_size) {
        m_local_has_reached_write_set_limit = true;
        clear_write_set();
        return false;
      }
    }

    uint64 mem_limit = m_global_write_set_memory_size_limit;
    if (mem_limit && !m_ignore_write_set_memory_limit) {
      // Check if adding a new element goes over the limit
      if (sizeof(uint64) + write_set_memory_size() > mem_limit) {
        my_error(ER_WRITE_SET_EXCEEDS_LIMIT, MYF(0));
        return true;
      }
    }

    write_set.push_back(hash);
    write_set_unique.insert(hash);
  }

  return false;
}

std::set<uint64>* Rpl_transaction_write_set_ctx::get_write_set()
{
  DBUG_ENTER("Rpl_transaction_write_set_ctx::get_write_set");
  DBUG_RETURN(&write_set_unique);
}

void Rpl_transaction_write_set_ctx::reset_state() {
  clear_write_set();
  m_has_missing_keys = m_has_related_foreign_keys = false;
  m_local_has_reached_write_set_limit = false;
}

void Rpl_transaction_write_set_ctx::clear_write_set() {
  write_set.clear();
  write_set_unique.clear();
  savepoint.clear();
  savepoint_list.clear();
}

void Rpl_transaction_write_set_ctx::set_has_missing_keys()
{
  DBUG_ENTER("Transaction_context_log_event::set_has_missing_keys");
  m_has_missing_keys= true;
  DBUG_VOID_RETURN;
}

bool Rpl_transaction_write_set_ctx::get_has_missing_keys()
{
  DBUG_ENTER("Transaction_context_log_event::get_has_missing_keys");
  DBUG_RETURN(m_has_missing_keys);
}

void Rpl_transaction_write_set_ctx::set_has_related_foreign_keys()
{
  DBUG_ENTER("Transaction_context_log_event::set_has_related_foreign_keys");
  m_has_related_foreign_keys= true;
  DBUG_VOID_RETURN;
}

bool Rpl_transaction_write_set_ctx::get_has_related_foreign_keys()
{
  DBUG_ENTER("Transaction_context_log_event::get_has_related_foreign_keys");
  DBUG_RETURN(m_has_related_foreign_keys);
}

bool Rpl_transaction_write_set_ctx::was_write_set_limit_reached() {
  return m_local_has_reached_write_set_limit;
}

size_t Rpl_transaction_write_set_ctx::write_set_memory_size() {
  return sizeof(uint64) * write_set.size();
}

void Rpl_transaction_write_set_ctx::set_global_require_full_write_set(
    bool requires_ws) {
  assert(!requires_ws || !m_global_component_requires_write_sets);
  if (requires_ws)
    my_atomic_store32(&m_global_component_requires_write_sets, 1);
  else
    my_atomic_store32(&m_global_component_requires_write_sets, 0);
}

void require_full_write_set(int requires_ws) {
  Rpl_transaction_write_set_ctx::set_global_require_full_write_set(requires_ws);
}

void Rpl_transaction_write_set_ctx::set_global_write_set_memory_size_limit(int64 limit) {
  assert(m_global_write_set_memory_size_limit == 0);
  m_global_write_set_memory_size_limit = limit;
}

void Rpl_transaction_write_set_ctx::update_global_write_set_memory_size_limit(
    int64 limit) {
  m_global_write_set_memory_size_limit = limit;
}

void set_write_set_memory_size_limit(long long size_limit) {
  Rpl_transaction_write_set_ctx::set_global_write_set_memory_size_limit(
      size_limit);
}

void update_write_set_memory_size_limit(long long size_limit) {
  Rpl_transaction_write_set_ctx::update_global_write_set_memory_size_limit(
      size_limit);
}

void Rpl_transaction_write_set_ctx::set_local_ignore_write_set_memory_limit(
    bool ignore_limit) {
  m_ignore_write_set_memory_limit = ignore_limit;
}

void Rpl_transaction_write_set_ctx::set_local_allow_drop_write_set(
    bool allow_drop_write_set) {
  m_local_allow_drop_write_set = allow_drop_write_set;
}

/**
  Implementation of service_rpl_transaction_write_set, see
  @file include/mysql/service_rpl_transaction_write_set.h
*/

Transaction_write_set* get_transaction_write_set(unsigned long m_thread_id)
{
  DBUG_ENTER("get_transaction_write_set");
  THD *thd= NULL;
  Transaction_write_set *result_set= NULL;
  Find_thd_with_id find_thd_with_id(m_thread_id);

  thd= Global_THD_manager::get_instance()->find_thd(&find_thd_with_id);
  if (thd)
  {
    std::set<uint64> *write_set= thd->get_transaction()
        ->get_transaction_write_set_ctx()->get_write_set();
    unsigned long write_set_size= write_set->size();
    if (write_set_size == 0)
    {
      mysql_mutex_unlock(&thd->LOCK_thd_data);
      DBUG_RETURN(NULL);
    }

    result_set= (Transaction_write_set*)my_malloc(key_memory_write_set_extraction,
                                                  sizeof(Transaction_write_set),
                                                  MYF(0));
    result_set->write_set_size= write_set_size;
    result_set->write_set=
        (unsigned long long*)my_malloc(key_memory_write_set_extraction,
                                       write_set_size *
                                       sizeof(unsigned long long),
                                       MYF(0));
    int result_set_index= 0;
    for (std::set<uint64>::iterator it= write_set->begin();
         it != write_set->end();
         ++it)
    {
      uint64 temp= *it;
      result_set->write_set[result_set_index++]=temp;
    }
    mysql_mutex_unlock(&thd->LOCK_thd_data);
  }
  DBUG_RETURN(result_set);
}

void Rpl_transaction_write_set_ctx::add_savepoint(char* name)
{
  DBUG_ENTER("Rpl_transaction_write_set_ctx::add_savepoint");
  std::string identifier(name);

  DBUG_EXECUTE_IF("transaction_write_set_savepoint_clear_on_commit_rollback",
                  {
                    assert(savepoint.size() == 0);
                    assert(write_set.size() == 0);
                    assert(write_set_unique.size() == 0);
                    assert(savepoint_list.size() == 0);
                  });

  DBUG_EXECUTE_IF("transaction_write_set_savepoint_level",
                  assert(savepoint.size() == 0););

  std::map<std::string, size_t>::iterator it;

  /*
    Savepoint with the same name, the old savepoint is deleted and a new one
    is set
  */
  if ((it= savepoint.find(name)) != savepoint.end())
      savepoint.erase(it);

  savepoint.insert(std::pair<std::string, size_t>(identifier,
                                                  write_set.size()));

  DBUG_EXECUTE_IF("transaction_write_set_savepoint_add_savepoint",
                  assert(savepoint.find(identifier)->second == write_set.size()););

  DBUG_VOID_RETURN;
}

void Rpl_transaction_write_set_ctx::del_savepoint(char* name)
{
  DBUG_ENTER("Rpl_transaction_write_set_ctx::del_savepoint");
  std::string identifier(name);

  DBUG_EXECUTE_IF("transaction_write_set_savepoint_block_before_release",
                    {
                      const char act[]= "now wait_for signal.unblock_release";
                      assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
                    });

  savepoint.erase(identifier);

  DBUG_VOID_RETURN;
}

void Rpl_transaction_write_set_ctx::rollback_to_savepoint(char* name)
{
  DBUG_ENTER("Rpl_transaction_write_set_ctx::rollback_to_savepoint");
  size_t position= 0;
  std::string identifier(name);
  std::map<std::string, size_t>::iterator elem;

  if ((elem = savepoint.find(identifier)) != savepoint.end())
  {
    assert(elem->second <= write_set.size());

    DBUG_EXECUTE_IF("transaction_write_set_savepoint_block_before_rollback",
                    {
                      const char act[]= "now wait_for signal.unblock_rollback";
                      assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
                    });

    position= elem->second;

    // Remove all savepoints created after the savepoint identifier given as
    // parameter
    std::map<std::string, size_t>::iterator it= savepoint.begin();
    while (it != savepoint.end())
    {
      if (it->second > position)
        savepoint.erase(it++);
      else
        ++it;
    }

    /*
      We need to check that:
       - starting index of the range we want to erase does exist.
       - write_set size have elements to be removed
    */
    if (write_set.size() > 0 && position < write_set.size())
    {
      // Clear all elements after savepoint
      write_set.erase(write_set.begin() + position, write_set.end());
      // Since the write_set_unique set does not have insert order, the
      // elements are ordered according its value, we need to rebuild it.
      write_set_unique.clear();
      write_set_unique.insert(write_set.begin(), write_set.end());
    }

    DBUG_EXECUTE_IF("transaction_write_set_savepoint_add_savepoint", {
        assert(write_set.size() == 2);
        assert(write_set_unique.size() == 2);});

    DBUG_EXECUTE_IF("transaction_write_set_size_2", {
        assert(write_set.size() == 4);
        assert(write_set_unique.size() == 4);});
  }

  DBUG_VOID_RETURN;
}


void Rpl_transaction_write_set_ctx::reset_savepoint_list()
{
  DBUG_ENTER("Rpl_transaction_write_set_ctx::reset_savepoint_list");

  savepoint_list.push_back(savepoint);
  savepoint.clear();

  DBUG_VOID_RETURN;
}

void Rpl_transaction_write_set_ctx::restore_savepoint_list()
{
  DBUG_ENTER("Rpl_transaction_write_set_ctx::restore_savepoint_list");

  if (!savepoint_list.empty())
  {
    savepoint = savepoint_list.back();
    savepoint_list.pop_back();
  }

  DBUG_VOID_RETURN;
}
