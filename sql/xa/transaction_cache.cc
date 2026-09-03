/* Copyright (c) 2013, 2026, Oracle and/or its affiliates.

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

#include "sql/xa.h"

#include <memory>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>

#include "include/mutex_lock.h"  // MUTEX_LOCK
#include "m_string.h"
#include "map_helpers.h"
#include "my_dbug.h"
#include "my_macros.h"
#include "my_psi_config.h"
#include "my_sys.h"
#include "mysql/my_loglevel.h"
#include "mysql/plugin.h"  // MYSQL_XIDDATASIZE
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/mysql_transaction.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/strings/m_ctype.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "scope_guard.h"  // Scope_guard
#include "sql/auth/sql_security_ctx.h"
#include "sql/log.h"
#include "sql/mdl.h"
#include "sql/mdl_context_backup.h"  // MDL_context_backup_manager
#include "sql/mysqld.h"              // server_id
#include "sql/protocol.h"
#include "sql/psi_memory_key.h"  // key_memory_xa_transaction_contexts
#include "sql/query_options.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_list.h"
#include "sql/system_variables.h"
#include "sql/transaction.h"  // trans_begin, trans_rollback
#include "sql/transaction_info.h"
#include "sql/xa/transaction_cache.h"
#include "sql_string.h"
#include "template_utils.h"
#include "thr_mutex.h"

#include <iostream>

// Used to create keys for the map
static std::string to_string(XID const &xid) {
  return std::string(pointer_cast<const char *>(xid.key()), xid.key_length());
}

struct Transaction_context_deleter {
  void operator()(Transaction_ctx *transaction) const {
    // Detached contexts are allocated by the cache during recovery.
    if (transaction->xid_state()->is_detached()) delete transaction;
  }
};

#ifdef HAVE_PSI_INTERFACE
xa::Transaction_cache::Transaction_cache()
    : m_key_LOCK_transaction_cache{},
      m_transaction_cache{m_key_LOCK_transaction_cache} {
  const char *category = "sql";
  mysql_mutex_register(category, this->m_transaction_cache_mutexes, 1);
  mysql_mutex_init(this->m_key_LOCK_transaction_cache,
                   &this->m_LOCK_transaction_cache, MY_MUTEX_INIT_FAST);
}
#else
xa::Transaction_cache::Transaction_cache()
    : m_transaction_cache{PSI_INSTRUMENT_ME} {
  mysql_mutex_init(PSI_INSTRUMENT_ME, &this->m_LOCK_transaction_cache,
                   MY_MUTEX_INIT_FAST);
}
#endif /* HAVE_PSI_INTERFACE */

bool xa::Transaction_cache::detach(Transaction_ctx *transaction) {
  bool res = false;
  XID_STATE *xs = transaction->xid_state();
  XID xid = *(xs->get_xid());
  bool was_logged = xs->is_binlogged();

  assert(xs->has_state(XID_STATE::XA_PREPARED));

  auto &instance = xa::Transaction_cache::instance();
  MUTEX_LOCK(mutex_guard, &instance.m_LOCK_transaction_cache);

  const auto key = to_string(xid);
  const auto it = instance.m_transaction_cache.find(key);
  assert(it != instance.m_transaction_cache.end());
  instance.m_transaction_cache.erase(it);
  res = xa::Transaction_cache::create_and_insert_new_transaction(
      &xid, was_logged, transaction);

  return res;
}

void xa::Transaction_cache::remove(Transaction_ctx *transaction) {
  auto &instance = xa::Transaction_cache::instance();
  MUTEX_LOCK(mutex_guard, &instance.m_LOCK_transaction_cache);
  const auto key = to_string(*transaction->xid_state()->get_xid());
  const auto it = instance.m_transaction_cache.find(key);
  if (it != instance.m_transaction_cache.end() &&
      it->second.transaction().get() == transaction) {
    instance.m_transaction_cache.erase(it);
  }
}

void xa::Transaction_cache::mark_prepared_visible_for_xa_recover(
    Transaction_ctx *transaction) {
  auto &instance = xa::Transaction_cache::instance();
  MUTEX_LOCK(mutex_guard, &instance.m_LOCK_transaction_cache);
  const auto key = to_string(*transaction->xid_state()->get_xid());
  const auto it = instance.m_transaction_cache.find(key);
  // Missing entry is a no-op: there is no cached XID whose XA RECOVER
  // visibility can be changed. The pointer check avoids changing visibility of
  // a different transaction if the same XID value was reused.
  if (it != instance.m_transaction_cache.end() &&
      it->second.transaction().get() == transaction) {
    it->second.mark_prepared_visible_for_xa_recover();
  }
}

void xa::Transaction_cache::mark_finalized_for_recover(
    Transaction_ctx *transaction) {
  auto &instance = xa::Transaction_cache::instance();
  MUTEX_LOCK(mutex_guard, &instance.m_LOCK_transaction_cache);
  const auto key = to_string(*transaction->xid_state()->get_xid());
  const auto it = instance.m_transaction_cache.find(key);
  // Missing entry is a no-op: there is nothing left to hide from XA RECOVER.
  // The pointer check avoids changing visibility of a different transaction if
  // the same XID value was reused.
  if (it != instance.m_transaction_cache.end() &&
      it->second.transaction().get() == transaction) {
    it->second.mark_finalized_for_recover();
  }
}

bool xa::Transaction_cache::insert(XID *xid, Transaction_ctx *transaction) {
  auto &instance = xa::Transaction_cache::instance();
  bool res{false};
  {
    MUTEX_LOCK(mutex_guard, &instance.m_LOCK_transaction_cache);
    std::shared_ptr<Transaction_ctx> ptr{transaction,
                                         Transaction_context_deleter{}};
    const auto key = to_string(*xid);
    // XA START reserves the XID immediately, but XA RECOVER must not list it
    // until XA PREPARE reaches the recover-visible point.
    res = !instance.m_transaction_cache
               .emplace(key, Cache_entry{std::move(ptr),
                                         Xa_recover_visibility::Hidden})
               .second;
  }
  if (res) {
    my_error(ER_XAER_DUPID, MYF(0));
  }
  return res;
}

bool xa::Transaction_cache::insert(XID *xid) {
  auto &instance = xa::Transaction_cache::instance();
  MUTEX_LOCK(mutex_guard, &instance.m_LOCK_transaction_cache);
  if (instance.m_transaction_cache.count(to_string(*xid))) return false;

  /*
    It's assumed that XA transaction was binlogged before the server
    shutdown. If --log-bin has changed since that from OFF to ON, XA
    COMMIT or XA ROLLBACK of this transaction may be logged alone into
    the binary log.
  */
  bool res = xa::Transaction_cache::create_and_insert_new_transaction(xid, true,
                                                                      nullptr);

  return res;
}

std::shared_ptr<Transaction_ctx> xa::Transaction_cache::find(
    XID *xid, filter_predicate_t filter) {
  auto &instance = xa::Transaction_cache::instance();
  MUTEX_LOCK(mutex_guard, &instance.m_LOCK_transaction_cache);
  auto found = instance.m_transaction_cache.find(to_string(*xid));
  if (found == instance.m_transaction_cache.end()) return nullptr;
  const auto &transaction = found->second.transaction();
  if (!transaction->xid_state()->get_xid()->eq(xid)) return nullptr;
  if (filter != nullptr && !filter(transaction)) return nullptr;
  return transaction;
}

std::vector<XID> xa::Transaction_cache::get_xids_visible_to_xa_recover() {
  auto &instance = xa::Transaction_cache::instance();
  std::vector<XID> to_return;
  MUTEX_LOCK(mutex_guard, &instance.m_LOCK_transaction_cache);
  to_return.reserve(instance.m_transaction_cache.size());
  // Attached contexts may be reset after detach. Snapshot their XIDs while
  // detach is serialized by the cache mutex.
  for (const auto &entry : instance.m_transaction_cache) {
    if (entry.second.is_visible_to_xa_recover())
      to_return.push_back(*entry.second.transaction()->xid_state()->get_xid());
  }
  return to_return;
}

void xa::Transaction_cache::initialize() { xa::Transaction_cache::instance(); }

void xa::Transaction_cache::dispose() {
  auto &instance = xa::Transaction_cache::instance();
  mysql_mutex_destroy(&instance.m_LOCK_transaction_cache);
}

xa::Transaction_cache &xa::Transaction_cache::instance() {
  static xa::Transaction_cache new_instance;
  return new_instance;
}

bool xa::Transaction_cache::create_and_insert_new_transaction(
    XID *xid, bool is_binlogged_arg, const Transaction_ctx *src) {
  auto transaction =
      std::unique_ptr<Transaction_ctx>{new (std::nothrow) Transaction_ctx()};
  XID_STATE *xs;

  if (!transaction) {
    my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR), sizeof(Transaction_ctx));
    return true;
  }
  if (src) {
    // Copy over the session unsafe rollback flags from the original
    // Transaction_ctx object, so that we can emit warnings also when
    // rolling back with the detached Transaction_ctx object.
    transaction->set_unsafe_rollback_flags(
        Transaction_ctx::SESSION,
        src->get_unsafe_rollback_flags(Transaction_ctx::SESSION));
  }

  xs = transaction->xid_state();
  xs->start_detached_xa(xid, is_binlogged_arg);

  auto &instance = xa::Transaction_cache::instance();
  const auto key = to_string(*xs->get_xid());
  std::shared_ptr<Transaction_ctx> transaction_ptr{
      transaction.get(), Transaction_context_deleter{}};
  transaction.release();
  // Recovered or detached XA transactions are already prepared from
  // XA RECOVER's perspective, so insert them as visible.
  return !instance.m_transaction_cache
              .emplace(key, Cache_entry{std::move(transaction_ptr),
                                        Xa_recover_visibility::Visible})
              .second;
}
