/*
   Copyright (c) 2013, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/transaction_info.h"

#include <string.h>

#include "mysqld_error.h"          // ER_*
#include "sql/derror.h"            // ER_THD
#include "sql/mysqld.h"            // global_system_variables
#include "sql/psi_memory_key.h"    // key_memory_thd_transactions
#include "sql/sql_error.h"         // Sql_condition
#include "sql/system_variables.h"  // System_variables
#include "sql/thr_malloc.h"

struct CHANGED_TABLE_LIST {
  struct CHANGED_TABLE_LIST *next;
  char *key;
  uint32 key_length;
};

Transaction_ctx::Transaction_ctx()
    : m_savepoints(nullptr),
      m_xid_state(),
      m_mem_root(key_memory_thd_transactions,
                 global_system_variables.trans_alloc_block_size),
      last_committed(0),
      sequence_number(0),
      m_rpl_transaction_ctx(),
      m_transaction_write_set_ctx(),
      trans_begin_hook_invoked(false) {
  memset(&m_scope_info, 0, sizeof(m_scope_info));
}

void Transaction_ctx::push_unsafe_rollback_warnings(THD *thd) {
  if (m_scope_info[SESSION].has_modified_non_trans_table())
    push_warning(thd, Sql_condition::SL_WARNING,
                 ER_WARNING_NOT_COMPLETE_ROLLBACK,
                 ER_THD(thd, ER_WARNING_NOT_COMPLETE_ROLLBACK));

  if (m_scope_info[SESSION].has_created_temp_table())
    push_warning(
        thd, Sql_condition::SL_WARNING,
        ER_WARNING_NOT_COMPLETE_ROLLBACK_WITH_CREATED_TEMP_TABLE,
        ER_THD(thd, ER_WARNING_NOT_COMPLETE_ROLLBACK_WITH_CREATED_TEMP_TABLE));

  if (m_scope_info[SESSION].has_dropped_temp_table())
    push_warning(
        thd, Sql_condition::SL_WARNING,
        ER_WARNING_NOT_COMPLETE_ROLLBACK_WITH_DROPPED_TEMP_TABLE,
        ER_THD(thd, ER_WARNING_NOT_COMPLETE_ROLLBACK_WITH_DROPPED_TEMP_TABLE));
}

void Transaction_ctx::register_ha(enum_trx_scope scope, Ha_trx_info *ha_info,
                                  handlerton *ht) {
  ha_info->register_ha(&m_scope_info[scope], ht);
}

Ha_trx_info_list Transaction_ctx::ha_trx_info(enum_trx_scope scope) {
  return {m_scope_info[scope].m_ha_list};
}

Ha_trx_info_list::Iterator::Iterator(Ha_trx_info *parent) : m_current{parent} {
  this->set_next();
}
Ha_trx_info_list::Iterator::Iterator(std::nullptr_t) : m_current{nullptr} {}

Ha_trx_info_list::Iterator::Iterator(Iterator const &rhs)
    : m_current{rhs.m_current}, m_next{rhs.m_next} {}

Ha_trx_info_list::Iterator &Ha_trx_info_list::Iterator::operator=(
    const Iterator &rhs) {
  this->m_current = rhs.m_current;
  this->m_next = rhs.m_next;
  return (*this);
}

Ha_trx_info_list::Iterator &Ha_trx_info_list::Iterator::operator++() {
  assert(this->m_current != nullptr);
  if (this->m_current != nullptr) this->m_current = this->m_next;
  this->set_next();
  return (*this);
}

Ha_trx_info_list::Iterator::reference Ha_trx_info_list::Iterator::operator*()
    const {
  return *this->m_current;
}

Ha_trx_info_list::Iterator Ha_trx_info_list::Iterator::operator++(int) {
  const Iterator to_return{*this};
  ++(*this);
  return to_return;
}

Ha_trx_info_list::Iterator::pointer Ha_trx_info_list::Iterator::operator->()
    const {
  return this->m_current;
}

bool Ha_trx_info_list::Iterator::operator==(Iterator const &rhs) const {
  return this->m_current == rhs.m_current;
}

bool Ha_trx_info_list::Iterator::operator==(Ha_trx_info const *rhs) const {
  return this->m_current == rhs;
}

bool Ha_trx_info_list::Iterator::operator==(Ha_trx_info const &rhs) const {
  return this->m_current == &rhs;
}

bool Ha_trx_info_list::Iterator::operator!=(Iterator const &rhs) const {
  return !((*this) == rhs);
}

bool Ha_trx_info_list::Iterator::operator!=(Ha_trx_info const *rhs) const {
  return !((*this) == rhs);
}

bool Ha_trx_info_list::Iterator::operator!=(Ha_trx_info const &rhs) const {
  return !((*this) == rhs);
}

Ha_trx_info_list::Iterator &Ha_trx_info_list::Iterator::set_next() {
  if (this->m_current != nullptr) this->m_next = this->m_current->m_next;
  return (*this);
}

Ha_trx_info_list::Ha_trx_info_list(Ha_trx_info *rhs) : m_underlying{rhs} {}

Ha_trx_info_list::Ha_trx_info_list(Ha_trx_info_list const &rhs)
    : m_underlying{rhs.m_underlying} {}

Ha_trx_info_list::Ha_trx_info_list(Ha_trx_info_list &&rhs)
    : m_underlying{rhs.m_underlying} {
  rhs.m_underlying = nullptr;
}

Ha_trx_info_list &Ha_trx_info_list::operator=(Ha_trx_info_list const &rhs) {
  this->m_underlying = rhs.m_underlying;
  return (*this);
}

Ha_trx_info_list &Ha_trx_info_list::operator=(Ha_trx_info_list &&rhs) {
  this->m_underlying = rhs.m_underlying;
  rhs.m_underlying = nullptr;
  return (*this);
}

Ha_trx_info &Ha_trx_info_list::operator*() { return (*this->m_underlying); }

Ha_trx_info const &Ha_trx_info_list::operator*() const {
  return (*this->m_underlying);
}

Ha_trx_info *Ha_trx_info_list::operator->() { return this->m_underlying; }

Ha_trx_info const *Ha_trx_info_list::operator->() const {
  return this->m_underlying;
}

bool Ha_trx_info_list::operator==(Ha_trx_info_list const &rhs) const {
  return this->m_underlying == rhs.m_underlying;
}

bool Ha_trx_info_list::operator==(Ha_trx_info const *rhs) const {
  return this->m_underlying == rhs;
}

bool Ha_trx_info_list::operator==(std::nullptr_t) const {
  return this->m_underlying == nullptr;
}

bool Ha_trx_info_list::operator!=(Ha_trx_info_list const &rhs) const {
  return !((*this) == rhs);
}

bool Ha_trx_info_list::operator!=(Ha_trx_info const *rhs) const {
  return !((*this) == rhs);
}

bool Ha_trx_info_list::operator!=(std::nullptr_t) const {
  return !((*this) == nullptr);
}

Ha_trx_info_list::operator bool() const {
  return this->m_underlying != nullptr;
}

Ha_trx_info *Ha_trx_info_list::head() { return this->m_underlying; }

Ha_trx_info_list::Iterator Ha_trx_info_list::begin() {
  return {this->m_underlying};
}

const Ha_trx_info_list::Iterator Ha_trx_info_list::begin() const {
  return {this->m_underlying};
}

Ha_trx_info_list::Iterator Ha_trx_info_list::end() { return {nullptr}; }

const Ha_trx_info_list::Iterator Ha_trx_info_list::end() const {
  return {nullptr};
}
