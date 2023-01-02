/* Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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

#include "storage/perfschema/pfs_visitor.h"

#include "my_config.h"

#include <assert.h>

#include "my_sys.h"
#include "sql/mysqld.h"
#include "sql/mysqld_thd_manager.h"
#include "sql/sql_class.h"
#include "storage/perfschema/pfs_account.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_host.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_user.h"

#ifdef WITH_LOCK_ORDER
#include "sql/debug_lock_order.h"
#endif /* WITH_LOCK_ORDER */

/**
  @file storage/perfschema/pfs_visitor.cc
  Visitors (implementation).
*/

/**
  @addtogroup performance_schema_buffers
  @{
*/

static PFS_thread *get_pfs_from_THD(THD *thd) {
  /*
    Get the instrumentation associated with a session.
    This can be 'any' instrumentation,
    not necessarily the performance schema,
    hence the opaque PSI_thread (not PFS_thread) type.
  */
  PSI_thread *psi = thd->get_psi();

  /*
    Now, we definitively break the encapsulation here,
    and assume we know exactly what psi actually points to.
  */
#ifdef WITH_LOCK_ORDER
  /*
    With LOCK_ORDER, this is a chain of responsibility,
    with:
    THD::m_psi -> LO_thread
    LO_thread::m_chain -> PFS_thread.
    Follow the first link on the chain,
    to find the underlying PFS_thread.

    Without LOCK_ORDER, psi is a direct pointer
    to the PFS_thread, so there is nothing to resolve.
  */
  psi = LO_get_chain_thread(psi);
#endif
  /*
    And now, finally dive into the performance schema itself.
  */
  auto *pfs = reinterpret_cast<PFS_thread *>(psi);
  return pfs;
}

class All_THD_visitor_adapter : public Do_THD_Impl {
 public:
  explicit All_THD_visitor_adapter(PFS_connection_visitor *visitor)
      : m_visitor(visitor) {}

  void operator()(THD *thd) override { m_visitor->visit_THD(thd); }

 private:
  PFS_connection_visitor *m_visitor;
};

/** Connection iterator */
void PFS_connection_iterator::visit_global(bool with_hosts, bool with_users,
                                           bool with_accounts,
                                           bool with_threads, bool with_THDs,
                                           PFS_connection_visitor *visitor) {
  assert(visitor != nullptr);
  assert(!with_threads || !with_THDs);

  visitor->visit_global();

  if (with_hosts) {
    PFS_host_iterator it = global_host_container.iterate();
    PFS_host *pfs = it.scan_next();

    while (pfs != nullptr) {
      visitor->visit_host(pfs);
      pfs = it.scan_next();
    }
  }

  if (with_users) {
    PFS_user_iterator it = global_user_container.iterate();
    PFS_user *pfs = it.scan_next();

    while (pfs != nullptr) {
      visitor->visit_user(pfs);
      pfs = it.scan_next();
    }
  }

  if (with_accounts) {
    PFS_account_iterator it = global_account_container.iterate();
    PFS_account *pfs = it.scan_next();

    while (pfs != nullptr) {
      visitor->visit_account(pfs);
      pfs = it.scan_next();
    }
  }

  if (with_threads) {
    PFS_thread_iterator it = global_thread_container.iterate();
    PFS_thread *pfs = it.scan_next();

    while (pfs != nullptr) {
      visitor->visit_thread(pfs);
      pfs = it.scan_next();
    }
  }

  if (with_THDs) {
    All_THD_visitor_adapter adapter(visitor);
    Global_THD_manager::get_instance()->do_for_all_thd(&adapter);
  }
}

class All_host_THD_visitor_adapter : public Do_THD_Impl {
 public:
  All_host_THD_visitor_adapter(PFS_connection_visitor *visitor, PFS_host *host)
      : m_visitor(visitor), m_host(host) {}

  void operator()(THD *thd) override {
    PFS_thread *pfs = get_pfs_from_THD(thd);
    pfs = sanitize_thread(pfs);
    if (pfs != nullptr) {
      PFS_account *account = sanitize_account(pfs->m_account);
      if (account != nullptr) {
        if (account->m_host == m_host) {
          m_visitor->visit_THD(thd);
        }
      } else if (pfs->m_host == m_host) {
        m_visitor->visit_THD(thd);
      }
    }
  }

 private:
  PFS_connection_visitor *m_visitor;
  PFS_host *m_host;
};

void PFS_connection_iterator::visit_host(PFS_host *host, bool with_accounts,
                                         bool with_threads, bool with_THDs,
                                         PFS_connection_visitor *visitor) {
  assert(visitor != nullptr);
  assert(!with_threads || !with_THDs);

  visitor->visit_host(host);

  if (with_accounts) {
    PFS_account_iterator it = global_account_container.iterate();
    PFS_account *pfs = it.scan_next();

    while (pfs != nullptr) {
      if (pfs->m_host == host) {
        visitor->visit_account(pfs);
      }
      pfs = it.scan_next();
    }
  }

  if (with_threads) {
    PFS_thread_iterator it = global_thread_container.iterate();
    PFS_thread *pfs = it.scan_next();

    while (pfs != nullptr) {
      PFS_account *safe_account = sanitize_account(pfs->m_account);
      if (((safe_account != nullptr) && (safe_account->m_host == host)) /* 1 */
          || (pfs->m_host == host))                                     /* 2 */
      {
        /*
          If the thread belongs to:
          - (1) a known user@host that belongs to this host,
          - (2) a 'lost' user@host that belongs to this host
          process it.
        */
        visitor->visit_thread(pfs);
      }
      pfs = it.scan_next();
    }
  }

  if (with_THDs) {
    All_host_THD_visitor_adapter adapter(visitor, host);
    Global_THD_manager::get_instance()->do_for_all_thd(&adapter);
  }
}

class All_user_THD_visitor_adapter : public Do_THD_Impl {
 public:
  All_user_THD_visitor_adapter(PFS_connection_visitor *visitor, PFS_user *user)
      : m_visitor(visitor), m_user(user) {}

  void operator()(THD *thd) override {
    PFS_thread *pfs = get_pfs_from_THD(thd);
    pfs = sanitize_thread(pfs);
    if (pfs != nullptr) {
      PFS_account *account = sanitize_account(pfs->m_account);
      if (account != nullptr) {
        if (account->m_user == m_user) {
          m_visitor->visit_THD(thd);
        }
      } else if (pfs->m_user == m_user) {
        m_visitor->visit_THD(thd);
      }
    }
  }

 private:
  PFS_connection_visitor *m_visitor;
  PFS_user *m_user;
};

void PFS_connection_iterator::visit_user(PFS_user *user, bool with_accounts,
                                         bool with_threads, bool with_THDs,
                                         PFS_connection_visitor *visitor) {
  assert(visitor != nullptr);
  assert(!with_threads || !with_THDs);

  visitor->visit_user(user);

  if (with_accounts) {
    PFS_account_iterator it = global_account_container.iterate();
    PFS_account *pfs = it.scan_next();

    while (pfs != nullptr) {
      if (pfs->m_user == user) {
        visitor->visit_account(pfs);
      }
      pfs = it.scan_next();
    }
  }

  if (with_threads) {
    PFS_thread_iterator it = global_thread_container.iterate();
    PFS_thread *pfs = it.scan_next();

    while (pfs != nullptr) {
      PFS_account *safe_account = sanitize_account(pfs->m_account);
      if (((safe_account != nullptr) && (safe_account->m_user == user)) /* 1 */
          || (pfs->m_user == user))                                     /* 2 */
      {
        /*
          If the thread belongs to:
          - (1) a known user@host that belongs to this user,
          - (2) a 'lost' user@host that belongs to this user
          process it.
        */
        visitor->visit_thread(pfs);
      }
      pfs = it.scan_next();
    }
  }

  if (with_THDs) {
    All_user_THD_visitor_adapter adapter(visitor, user);
    Global_THD_manager::get_instance()->do_for_all_thd(&adapter);
  }
}

class All_account_THD_visitor_adapter : public Do_THD_Impl {
 public:
  All_account_THD_visitor_adapter(PFS_connection_visitor *visitor,
                                  PFS_account *account)
      : m_visitor(visitor), m_account(account) {}

  void operator()(THD *thd) override {
    PFS_thread *pfs = get_pfs_from_THD(thd);
    pfs = sanitize_thread(pfs);
    if (pfs != nullptr) {
      if (pfs->m_account == m_account) {
        m_visitor->visit_THD(thd);
      }
    }
  }

 private:
  PFS_connection_visitor *m_visitor;
  PFS_account *m_account;
};

void PFS_connection_iterator::visit_account(PFS_account *account,
                                            bool with_threads, bool with_THDs,
                                            PFS_connection_visitor *visitor) {
  assert(visitor != nullptr);
  assert(!with_threads || !with_THDs);

  visitor->visit_account(account);

  if (with_threads) {
    PFS_thread_iterator it = global_thread_container.iterate();
    PFS_thread *pfs = it.scan_next();

    while (pfs != nullptr) {
      if (pfs->m_account == account) {
        visitor->visit_thread(pfs);
      }
      pfs = it.scan_next();
    }
  }

  if (with_THDs) {
    All_account_THD_visitor_adapter adapter(visitor, account);
    Global_THD_manager::get_instance()->do_for_all_thd(&adapter);
  }
}

void PFS_instance_iterator::visit_all(PFS_instance_visitor *visitor) {
  visit_all_mutex(visitor);
  visit_all_rwlock(visitor);
  visit_all_cond(visitor);
  visit_all_file(visitor);
}

void PFS_instance_iterator::visit_all_mutex(PFS_instance_visitor *visitor) {
  visit_all_mutex_classes(visitor);
  visit_all_mutex_instances(visitor);
}

void PFS_instance_iterator::visit_all_mutex_classes(
    PFS_instance_visitor *visitor) {
  PFS_mutex_class *pfs = mutex_class_array;
  PFS_mutex_class *pfs_last = pfs + mutex_class_max;
  for (; pfs < pfs_last; pfs++) {
    if (pfs->m_name.length() != 0) {
      visitor->visit_mutex_class(pfs);
    }
  }
}

void PFS_instance_iterator::visit_all_mutex_instances(
    PFS_instance_visitor *visitor) {
  PFS_mutex_iterator it = global_mutex_container.iterate();
  PFS_mutex *pfs = it.scan_next();

  while (pfs != nullptr) {
    visitor->visit_mutex(pfs);
    pfs = it.scan_next();
  }
}

void PFS_instance_iterator::visit_all_rwlock(PFS_instance_visitor *visitor) {
  visit_all_rwlock_classes(visitor);
  visit_all_rwlock_instances(visitor);
}

void PFS_instance_iterator::visit_all_rwlock_classes(
    PFS_instance_visitor *visitor) {
  PFS_rwlock_class *pfs = rwlock_class_array;
  PFS_rwlock_class *pfs_last = pfs + rwlock_class_max;
  for (; pfs < pfs_last; pfs++) {
    if (pfs->m_name.length() != 0) {
      visitor->visit_rwlock_class(pfs);
    }
  }
}

void PFS_instance_iterator::visit_all_rwlock_instances(
    PFS_instance_visitor *visitor) {
  PFS_rwlock_iterator it = global_rwlock_container.iterate();
  PFS_rwlock *pfs = it.scan_next();

  while (pfs != nullptr) {
    visitor->visit_rwlock(pfs);
    pfs = it.scan_next();
  }
}

void PFS_instance_iterator::visit_all_cond(PFS_instance_visitor *visitor) {
  visit_all_cond_classes(visitor);
  visit_all_cond_instances(visitor);
}

void PFS_instance_iterator::visit_all_cond_classes(
    PFS_instance_visitor *visitor) {
  PFS_cond_class *pfs = cond_class_array;
  PFS_cond_class *pfs_last = pfs + cond_class_max;
  for (; pfs < pfs_last; pfs++) {
    if (pfs->m_name.length() != 0) {
      visitor->visit_cond_class(pfs);
    }
  }
}

void PFS_instance_iterator::visit_all_cond_instances(
    PFS_instance_visitor *visitor) {
  PFS_cond_iterator it = global_cond_container.iterate();
  PFS_cond *pfs = it.scan_next();

  while (pfs != nullptr) {
    visitor->visit_cond(pfs);
    pfs = it.scan_next();
  }
}

void PFS_instance_iterator::visit_all_file(PFS_instance_visitor *visitor) {
  visit_all_file_classes(visitor);
  visit_all_file_instances(visitor);
}

void PFS_instance_iterator::visit_all_file_classes(
    PFS_instance_visitor *visitor) {
  PFS_file_class *pfs = file_class_array;
  PFS_file_class *pfs_last = pfs + file_class_max;
  for (; pfs < pfs_last; pfs++) {
    if (pfs->m_name.length() != 0) {
      visitor->visit_file_class(pfs);
    }
  }
}

void PFS_instance_iterator::visit_all_file_instances(
    PFS_instance_visitor *visitor) {
  PFS_file_iterator it = global_file_container.iterate();
  PFS_file *pfs = it.scan_next();

  while (pfs != nullptr) {
    visitor->visit_file(pfs);
    pfs = it.scan_next();
  }
}

/** Instance iterator */

void PFS_instance_iterator::visit_mutex_instances(
    PFS_mutex_class *klass, PFS_instance_visitor *visitor) {
  assert(visitor != nullptr);

  visitor->visit_mutex_class(klass);

  if (klass->is_singleton()) {
    PFS_mutex *pfs = sanitize_mutex(klass->m_singleton);
    if (likely(pfs != nullptr)) {
      if (likely(pfs->m_lock.is_populated())) {
        visitor->visit_mutex(pfs);
      }
    }
  } else {
    PFS_mutex_iterator it = global_mutex_container.iterate();
    PFS_mutex *pfs = it.scan_next();

    while (pfs != nullptr) {
      if (pfs->m_class == klass) {
        visitor->visit_mutex(pfs);
      }
      pfs = it.scan_next();
    }
  }
}

void PFS_instance_iterator::visit_rwlock_instances(
    PFS_rwlock_class *klass, PFS_instance_visitor *visitor) {
  assert(visitor != nullptr);

  visitor->visit_rwlock_class(klass);

  if (klass->is_singleton()) {
    PFS_rwlock *pfs = sanitize_rwlock(klass->m_singleton);
    if (likely(pfs != nullptr)) {
      if (likely(pfs->m_lock.is_populated())) {
        visitor->visit_rwlock(pfs);
      }
    }
  } else {
    PFS_rwlock_iterator it = global_rwlock_container.iterate();
    PFS_rwlock *pfs = it.scan_next();

    while (pfs != nullptr) {
      if (pfs->m_class == klass) {
        visitor->visit_rwlock(pfs);
      }
      pfs = it.scan_next();
    }
  }
}

void PFS_instance_iterator::visit_cond_instances(
    PFS_cond_class *klass, PFS_instance_visitor *visitor) {
  assert(visitor != nullptr);

  visitor->visit_cond_class(klass);

  if (klass->is_singleton()) {
    PFS_cond *pfs = sanitize_cond(klass->m_singleton);
    if (likely(pfs != nullptr)) {
      if (likely(pfs->m_lock.is_populated())) {
        visitor->visit_cond(pfs);
      }
    }
  } else {
    PFS_cond_iterator it = global_cond_container.iterate();
    PFS_cond *pfs = it.scan_next();

    while (pfs != nullptr) {
      if (pfs->m_class == klass) {
        visitor->visit_cond(pfs);
      }
      pfs = it.scan_next();
    }
  }
}

void PFS_instance_iterator::visit_file_instances(
    PFS_file_class *klass, PFS_instance_visitor *visitor) {
  assert(visitor != nullptr);

  visitor->visit_file_class(klass);

  if (klass->is_singleton()) {
    PFS_file *pfs = sanitize_file(klass->m_singleton);
    if (likely(pfs != nullptr)) {
      if (likely(pfs->m_lock.is_populated())) {
        visitor->visit_file(pfs);
      }
    }
  } else {
    PFS_file_iterator it = global_file_container.iterate();
    PFS_file *pfs = it.scan_next();

    while (pfs != nullptr) {
      if (pfs->m_class == klass) {
        visitor->visit_file(pfs);
      }
      pfs = it.scan_next();
    }
  }
}

/** Socket instance iterator visiting a socket class and all instances */

void PFS_instance_iterator::visit_socket_instances(
    PFS_socket_class *klass, PFS_instance_visitor *visitor) {
  assert(visitor != nullptr);

  visitor->visit_socket_class(klass);

  if (klass->is_singleton()) {
    PFS_socket *pfs = sanitize_socket(klass->m_singleton);
    if (likely(pfs != nullptr)) {
      if (likely(pfs->m_lock.is_populated())) {
        visitor->visit_socket(pfs);
      }
    }
  } else {
    PFS_socket_iterator it = global_socket_container.iterate();
    PFS_socket *pfs = it.scan_next();

    while (pfs != nullptr) {
      if (pfs->m_class == klass) {
        visitor->visit_socket(pfs);
      }
      pfs = it.scan_next();
    }
  }
}

/** Socket instance iterator visiting sockets owned by a PFS_thread. */

void PFS_instance_iterator::visit_socket_instances(
    PFS_socket_class *klass, PFS_instance_visitor *visitor, PFS_thread *thread,
    bool visit_class) {
  assert(visitor != nullptr);
  assert(thread != nullptr);

  if (visit_class) {
    visitor->visit_socket_class(klass);
  }

  if (klass->is_singleton()) {
    PFS_socket *pfs = sanitize_socket(klass->m_singleton);
    if (likely(pfs != nullptr)) {
      if (unlikely(pfs->m_thread_owner == thread)) {
        visitor->visit_socket(pfs);
      }
    }
  } else {
    /* Get current socket stats from each socket instance owned by this thread
     */
    PFS_socket_iterator it = global_socket_container.iterate();
    PFS_socket *pfs = it.scan_next();

    while (pfs != nullptr) {
      if (unlikely((pfs->m_class == klass) &&
                   (pfs->m_thread_owner == thread))) {
        visitor->visit_socket(pfs);
      }
      pfs = it.scan_next();
    }
  }
}

/** Generic instance iterator with PFS_thread as matching criteria */

void PFS_instance_iterator::visit_instances(PFS_instr_class *klass,
                                            PFS_instance_visitor *visitor,
                                            PFS_thread *thread,
                                            bool visit_class) {
  assert(visitor != nullptr);
  assert(klass != nullptr);

  switch (klass->m_type) {
    case PFS_CLASS_SOCKET: {
      auto *socket_class = reinterpret_cast<PFS_socket_class *>(klass);
      PFS_instance_iterator::visit_socket_instances(socket_class, visitor,
                                                    thread, visit_class);
    } break;
    default:
      break;
  }
}

/** Object iterator */
void PFS_object_iterator::visit_all(PFS_object_visitor *visitor) {
  visit_all_tables(visitor);
}

class Proc_all_table_shares : public PFS_buffer_processor<PFS_table_share> {
 public:
  explicit Proc_all_table_shares(PFS_object_visitor *visitor)
      : m_visitor(visitor) {}

  void operator()(PFS_table_share *pfs) override {
    m_visitor->visit_table_share(pfs);
  }

 private:
  PFS_object_visitor *m_visitor;
};

class Proc_all_table_handles : public PFS_buffer_processor<PFS_table> {
 public:
  explicit Proc_all_table_handles(PFS_object_visitor *visitor)
      : m_visitor(visitor) {}

  void operator()(PFS_table *pfs) override {
    PFS_table_share *safe_share = sanitize_table_share(pfs->m_share);
    if (safe_share != nullptr) {
      m_visitor->visit_table(pfs);
    }
  }

 private:
  PFS_object_visitor *m_visitor;
};

void PFS_object_iterator::visit_all_tables(PFS_object_visitor *visitor) {
  assert(visitor != nullptr);

  visitor->visit_global();

  /* For all the table shares ... */
  Proc_all_table_shares proc_shares(visitor);
  global_table_share_container.apply(proc_shares);

  /* For all the table handles ... */
  Proc_all_table_handles proc_handles(visitor);
  global_table_container.apply(proc_handles);
}

class Proc_one_table_share_handles : public PFS_buffer_processor<PFS_table> {
 public:
  Proc_one_table_share_handles(PFS_object_visitor *visitor,
                               PFS_table_share *share)
      : m_visitor(visitor), m_share(share) {}

  void operator()(PFS_table *pfs) override {
    if (pfs->m_share == m_share) {
      m_visitor->visit_table(pfs);
    }
  }

 private:
  PFS_object_visitor *m_visitor;
  PFS_table_share *m_share;
};

void PFS_object_iterator::visit_tables(PFS_table_share *share,
                                       PFS_object_visitor *visitor) {
  assert(visitor != nullptr);

  visitor->visit_table_share(share);

#ifdef LATER
  if (share->get_refcount() == 0) {
    return;
  }
#endif

  /* For all the table handles ... */
  Proc_one_table_share_handles proc(visitor, share);
  global_table_container.apply(proc);
}

class Proc_one_table_share_indexes : public PFS_buffer_processor<PFS_table> {
 public:
  Proc_one_table_share_indexes(PFS_object_visitor *visitor,
                               PFS_table_share *share, uint index)
      : m_visitor(visitor), m_share(share), m_index(index) {}

  void operator()(PFS_table *pfs) override {
    if (pfs->m_share == m_share) {
      m_visitor->visit_table_index(pfs, m_index);
    }
  }

 private:
  PFS_object_visitor *m_visitor;
  PFS_table_share *m_share;
  uint m_index;
};

void PFS_object_iterator::visit_table_indexes(PFS_table_share *share,
                                              uint index,
                                              PFS_object_visitor *visitor) {
  assert(visitor != nullptr);

  visitor->visit_table_share_index(share, index);

#ifdef LATER
  if (share->get_refcount() == 0) {
    return;
  }
#endif

  /* For all the table handles ... */
  Proc_one_table_share_indexes proc(visitor, share, index);
  global_table_container.apply(proc);
}

/** Connection wait visitor */

PFS_connection_wait_visitor::PFS_connection_wait_visitor(
    PFS_instr_class *klass) {
  m_index = klass->m_event_name_index;
}

PFS_connection_wait_visitor::~PFS_connection_wait_visitor() = default;

void PFS_connection_wait_visitor::visit_global() {
  /*
    This visitor is used only for global instruments
    that do not have instances.
    For waits, do not sum by connection but by instances,
    it is more efficient.
  */
  assert((m_index == global_idle_class.m_event_name_index) ||
         (m_index == global_metadata_class.m_event_name_index));

  if (m_index == global_idle_class.m_event_name_index) {
    m_stat.aggregate(&global_idle_stat);
  } else {
    m_stat.aggregate(&global_metadata_stat);
  }
}

void PFS_connection_wait_visitor::visit_host(PFS_host *pfs) {
  const PFS_single_stat *event_name_array;
  event_name_array = pfs->read_instr_class_waits_stats();
  if (event_name_array != nullptr) {
    m_stat.aggregate(&event_name_array[m_index]);
  }
}

void PFS_connection_wait_visitor::visit_user(PFS_user *pfs) {
  const PFS_single_stat *event_name_array;
  event_name_array = pfs->read_instr_class_waits_stats();
  if (event_name_array != nullptr) {
    m_stat.aggregate(&event_name_array[m_index]);
  }
}

void PFS_connection_wait_visitor::visit_account(PFS_account *pfs) {
  const PFS_single_stat *event_name_array;
  event_name_array = pfs->read_instr_class_waits_stats();
  if (event_name_array != nullptr) {
    m_stat.aggregate(&event_name_array[m_index]);
  }
}

void PFS_connection_wait_visitor::visit_thread(PFS_thread *pfs) {
  const PFS_single_stat *event_name_array;
  event_name_array = pfs->read_instr_class_waits_stats();
  if (event_name_array != nullptr) {
    m_stat.aggregate(&event_name_array[m_index]);
  }
}

PFS_connection_all_wait_visitor::PFS_connection_all_wait_visitor() = default;

PFS_connection_all_wait_visitor::~PFS_connection_all_wait_visitor() = default;

void PFS_connection_all_wait_visitor::visit_global() {
  /* Sum by instances, not by connection */
  assert(false);
}

void PFS_connection_all_wait_visitor::visit_connection_slice(
    PFS_connection_slice *pfs) {
  const PFS_single_stat *stat = pfs->read_instr_class_waits_stats();
  if (stat != nullptr) {
    const PFS_single_stat *stat_last = stat + wait_class_max;
    for (; stat < stat_last; stat++) {
      m_stat.aggregate(stat);
    }
  }
}

void PFS_connection_all_wait_visitor::visit_host(PFS_host *pfs) {
  visit_connection_slice(pfs);
}

void PFS_connection_all_wait_visitor::visit_user(PFS_user *pfs) {
  visit_connection_slice(pfs);
}

void PFS_connection_all_wait_visitor::visit_account(PFS_account *pfs) {
  visit_connection_slice(pfs);
}

void PFS_connection_all_wait_visitor::visit_thread(PFS_thread *pfs) {
  visit_connection_slice(pfs);
}

PFS_connection_stage_visitor::PFS_connection_stage_visitor(
    PFS_stage_class *klass) {
  m_index = klass->m_event_name_index;
}

PFS_connection_stage_visitor::~PFS_connection_stage_visitor() = default;

void PFS_connection_stage_visitor::visit_global() {
  m_stat.aggregate(&global_instr_class_stages_array[m_index]);
}

void PFS_connection_stage_visitor::visit_host(PFS_host *pfs) {
  const PFS_stage_stat *event_name_array;
  event_name_array = pfs->read_instr_class_stages_stats();
  if (event_name_array != nullptr) {
    m_stat.aggregate(&event_name_array[m_index]);
  }
}

void PFS_connection_stage_visitor::visit_user(PFS_user *pfs) {
  const PFS_stage_stat *event_name_array;
  event_name_array = pfs->read_instr_class_stages_stats();
  if (event_name_array != nullptr) {
    m_stat.aggregate(&event_name_array[m_index]);
  }
}

void PFS_connection_stage_visitor::visit_account(PFS_account *pfs) {
  const PFS_stage_stat *event_name_array;
  event_name_array = pfs->read_instr_class_stages_stats();
  if (event_name_array != nullptr) {
    m_stat.aggregate(&event_name_array[m_index]);
  }
}

void PFS_connection_stage_visitor::visit_thread(PFS_thread *pfs) {
  const PFS_stage_stat *event_name_array;
  event_name_array = pfs->read_instr_class_stages_stats();
  if (event_name_array != nullptr) {
    m_stat.aggregate(&event_name_array[m_index]);
  }
}

PFS_connection_statement_visitor::PFS_connection_statement_visitor(
    PFS_statement_class *klass) {
  m_index = klass->m_event_name_index;
}

PFS_connection_statement_visitor::~PFS_connection_statement_visitor() = default;

void PFS_connection_statement_visitor::visit_global() {
  m_stat.aggregate(&global_instr_class_statements_array[m_index]);
}

void PFS_connection_statement_visitor::visit_host(PFS_host *pfs) {
  const PFS_statement_stat *event_name_array;
  event_name_array = pfs->read_instr_class_statements_stats();
  if (event_name_array != nullptr) {
    m_stat.aggregate(&event_name_array[m_index]);
  }
}

void PFS_connection_statement_visitor::visit_user(PFS_user *pfs) {
  const PFS_statement_stat *event_name_array;
  event_name_array = pfs->read_instr_class_statements_stats();
  if (event_name_array != nullptr) {
    m_stat.aggregate(&event_name_array[m_index]);
  }
}

void PFS_connection_statement_visitor::visit_account(PFS_account *pfs) {
  const PFS_statement_stat *event_name_array;
  event_name_array = pfs->read_instr_class_statements_stats();
  if (event_name_array != nullptr) {
    m_stat.aggregate(&event_name_array[m_index]);
  }
}

void PFS_connection_statement_visitor::visit_thread(PFS_thread *pfs) {
  const PFS_statement_stat *event_name_array;
  event_name_array = pfs->read_instr_class_statements_stats();
  if (event_name_array != nullptr) {
    m_stat.aggregate(&event_name_array[m_index]);
  }
}

/** Instance wait visitor */
PFS_connection_all_statement_visitor::PFS_connection_all_statement_visitor() =
    default;

PFS_connection_all_statement_visitor::~PFS_connection_all_statement_visitor() =
    default;

void PFS_connection_all_statement_visitor::visit_global() {
  PFS_statement_stat *stat = global_instr_class_statements_array;
  PFS_statement_stat *stat_last = stat + statement_class_max;
  for (; stat < stat_last; stat++) {
    m_stat.aggregate(stat);
  }
}

void PFS_connection_all_statement_visitor::visit_connection_slice(
    PFS_connection_slice *pfs) {
  const PFS_statement_stat *stat = pfs->read_instr_class_statements_stats();
  if (stat != nullptr) {
    const PFS_statement_stat *stat_last = stat + statement_class_max;
    for (; stat < stat_last; stat++) {
      m_stat.aggregate(stat);
    }
  }
}

void PFS_connection_all_statement_visitor::visit_host(PFS_host *pfs) {
  visit_connection_slice(pfs);
}

void PFS_connection_all_statement_visitor::visit_user(PFS_user *pfs) {
  visit_connection_slice(pfs);
}

void PFS_connection_all_statement_visitor::visit_account(PFS_account *pfs) {
  visit_connection_slice(pfs);
}

void PFS_connection_all_statement_visitor::visit_thread(PFS_thread *pfs) {
  visit_connection_slice(pfs);
}

PFS_connection_transaction_visitor::PFS_connection_transaction_visitor(
    PFS_transaction_class *klass) {
  m_index = klass->m_event_name_index;
}

PFS_connection_transaction_visitor::~PFS_connection_transaction_visitor() =
    default;

void PFS_connection_transaction_visitor::visit_global() {
  m_stat.aggregate(&global_transaction_stat);
}

void PFS_connection_transaction_visitor::visit_host(PFS_host *pfs) {
  const PFS_transaction_stat *event_name_array;
  event_name_array = pfs->read_instr_class_transactions_stats();
  if (event_name_array != nullptr) {
    m_stat.aggregate(&event_name_array[m_index]);
  }
}

void PFS_connection_transaction_visitor::visit_user(PFS_user *pfs) {
  const PFS_transaction_stat *event_name_array;
  event_name_array = pfs->read_instr_class_transactions_stats();
  if (event_name_array != nullptr) {
    m_stat.aggregate(&event_name_array[m_index]);
  }
}

void PFS_connection_transaction_visitor::visit_account(PFS_account *pfs) {
  const PFS_transaction_stat *event_name_array;
  event_name_array = pfs->read_instr_class_transactions_stats();
  if (event_name_array != nullptr) {
    m_stat.aggregate(&event_name_array[m_index]);
  }
}

void PFS_connection_transaction_visitor::visit_thread(PFS_thread *pfs) {
  const PFS_transaction_stat *event_name_array;
  event_name_array = pfs->read_instr_class_transactions_stats();
  if (event_name_array != nullptr) {
    m_stat.aggregate(&event_name_array[m_index]);
  }
}

PFS_connection_error_visitor::PFS_connection_error_visitor(
    PFS_error_class *klass, uint error_index)
    : m_error_index(error_index) {
  m_index = klass->m_event_name_index;
  m_stat.reset();
}

PFS_connection_error_visitor::~PFS_connection_error_visitor() = default;

void PFS_connection_error_visitor::visit_global() {
  m_stat.aggregate(global_error_stat.get_stat(m_error_index));
}

void PFS_connection_error_visitor::visit_host(PFS_host *pfs) {
  const PFS_error_stat *event_name_array;
  event_name_array = pfs->read_instr_class_errors_stats();

  if (event_name_array == nullptr) {
    return;
  }

  m_stat.aggregate(event_name_array->get_stat(m_error_index));
}

void PFS_connection_error_visitor::visit_user(PFS_user *pfs) {
  const PFS_error_stat *event_name_array;
  event_name_array = pfs->read_instr_class_errors_stats();

  if (event_name_array == nullptr) {
    return;
  }

  m_stat.aggregate(event_name_array->get_stat(m_error_index));
}

void PFS_connection_error_visitor::visit_account(PFS_account *pfs) {
  const PFS_error_stat *event_name_array;
  event_name_array = pfs->read_instr_class_errors_stats();

  if (event_name_array == nullptr) {
    return;
  }

  m_stat.aggregate(event_name_array->get_stat(m_error_index));
}

void PFS_connection_error_visitor::visit_thread(PFS_thread *pfs) {
  const PFS_error_stat *event_name_array;
  event_name_array = pfs->read_instr_class_errors_stats();

  if (event_name_array == nullptr) {
    return;
  }

  m_stat.aggregate(event_name_array->get_stat(m_error_index));
}

PFS_connection_stat_visitor::PFS_connection_stat_visitor() = default;

PFS_connection_stat_visitor::~PFS_connection_stat_visitor() = default;

void PFS_connection_stat_visitor::visit_global() {}

void PFS_connection_stat_visitor::visit_host(PFS_host *pfs) {
  m_stat.aggregate_disconnected(pfs->m_disconnected_count,
                                pfs->m_max_controlled_memory,
                                pfs->m_max_total_memory);
}

void PFS_connection_stat_visitor::visit_user(PFS_user *pfs) {
  m_stat.aggregate_disconnected(pfs->m_disconnected_count,
                                pfs->m_max_controlled_memory,
                                pfs->m_max_total_memory);
}

void PFS_connection_stat_visitor::visit_account(PFS_account *pfs) {
  m_stat.aggregate_disconnected(pfs->m_disconnected_count,
                                pfs->m_max_controlled_memory,
                                pfs->m_max_total_memory);
}

void PFS_connection_stat_visitor::visit_thread(PFS_thread *pfs) {
  m_stat.aggregate_active(
      1, pfs->m_session_all_memory_stat.m_controlled.get_session_max(),
      pfs->m_session_all_memory_stat.m_total.get_session_max());
}

PFS_connection_memory_visitor::PFS_connection_memory_visitor(
    PFS_memory_class *klass) {
  m_index = klass->m_event_name_index;
  m_stat.reset();
}

PFS_connection_memory_visitor::~PFS_connection_memory_visitor() = default;

void PFS_connection_memory_visitor::visit_global() {
  PFS_memory_shared_stat *stat;
  stat = &global_instr_class_memory_array[m_index];
  memory_monitoring_aggregate(stat, &m_stat);
}

void PFS_connection_memory_visitor::visit_host(PFS_host *pfs) {
  const PFS_memory_shared_stat *event_name_array;
  event_name_array = pfs->read_instr_class_memory_stats();
  if (event_name_array != nullptr) {
    const PFS_memory_shared_stat *stat;
    stat = &event_name_array[m_index];
    memory_monitoring_aggregate(stat, &m_stat);
  }
}

void PFS_connection_memory_visitor::visit_user(PFS_user *pfs) {
  const PFS_memory_shared_stat *event_name_array;
  event_name_array = pfs->read_instr_class_memory_stats();
  if (event_name_array != nullptr) {
    const PFS_memory_shared_stat *stat;
    stat = &event_name_array[m_index];
    memory_monitoring_aggregate(stat, &m_stat);
  }
}

void PFS_connection_memory_visitor::visit_account(PFS_account *pfs) {
  const PFS_memory_shared_stat *event_name_array;
  event_name_array = pfs->read_instr_class_memory_stats();
  if (event_name_array != nullptr) {
    const PFS_memory_shared_stat *stat;
    stat = &event_name_array[m_index];
    memory_monitoring_aggregate(stat, &m_stat);
  }
}

void PFS_connection_memory_visitor::visit_thread(PFS_thread *pfs) {
  const PFS_memory_safe_stat *event_name_array;
  event_name_array = pfs->read_instr_class_memory_stats();
  if (event_name_array != nullptr) {
    const PFS_memory_safe_stat *stat;
    stat = &event_name_array[m_index];
    memory_monitoring_aggregate(stat, &m_stat);
  }
}

PFS_connection_status_visitor::PFS_connection_status_visitor(
    System_status_var *status_vars)
    : m_status_vars(status_vars) {
  memset(m_status_vars, 0, sizeof(System_status_var));
}

PFS_connection_status_visitor::~PFS_connection_status_visitor() = default;

/** Aggregate from global status. */
void PFS_connection_status_visitor::visit_global() {
  /* NOTE: Requires lock on LOCK_status. */
  mysql_mutex_assert_owner(&LOCK_status);
  add_to_status(m_status_vars, &global_status_var);
}

void PFS_connection_status_visitor::visit_host(PFS_host *pfs) {
  pfs->m_status_stats.aggregate_to(m_status_vars);
}

void PFS_connection_status_visitor::visit_user(PFS_user *pfs) {
  pfs->m_status_stats.aggregate_to(m_status_vars);
}

void PFS_connection_status_visitor::visit_account(PFS_account *pfs) {
  pfs->m_status_stats.aggregate_to(m_status_vars);
}

void PFS_connection_status_visitor::visit_thread(PFS_thread *) {}

void PFS_connection_status_visitor::visit_THD(THD *thd) {
  add_to_status(m_status_vars, &thd->status_var);
}

PFS_instance_wait_visitor::PFS_instance_wait_visitor() = default;

PFS_instance_wait_visitor::~PFS_instance_wait_visitor() = default;

void PFS_instance_wait_visitor::visit_mutex_class(PFS_mutex_class *pfs) {
  m_stat.aggregate(&pfs->m_mutex_stat.m_wait_stat);
}

void PFS_instance_wait_visitor::visit_rwlock_class(PFS_rwlock_class *pfs) {
  m_stat.aggregate(&pfs->m_rwlock_stat.m_wait_stat);
}

void PFS_instance_wait_visitor::visit_cond_class(PFS_cond_class *pfs) {
  m_stat.aggregate(&pfs->m_cond_stat.m_wait_stat);
}

void PFS_instance_wait_visitor::visit_file_class(PFS_file_class *pfs) {
  pfs->m_file_stat.m_io_stat.sum_waits(&m_stat);
}

void PFS_instance_wait_visitor::visit_socket_class(PFS_socket_class *pfs) {
  pfs->m_socket_stat.m_io_stat.sum_waits(&m_stat);
}

void PFS_instance_wait_visitor::visit_mutex(PFS_mutex *pfs) {
  m_stat.aggregate(&pfs->m_mutex_stat.m_wait_stat);
}

void PFS_instance_wait_visitor::visit_rwlock(PFS_rwlock *pfs) {
  m_stat.aggregate(&pfs->m_rwlock_stat.m_wait_stat);
}

void PFS_instance_wait_visitor::visit_cond(PFS_cond *pfs) {
  m_stat.aggregate(&pfs->m_cond_stat.m_wait_stat);
}

void PFS_instance_wait_visitor::visit_file(PFS_file *pfs) {
  /* Combine per-operation file wait stats before aggregating */
  PFS_single_stat stat;
  pfs->m_file_stat.m_io_stat.sum_waits(&stat);
  m_stat.aggregate(&stat);
}

void PFS_instance_wait_visitor::visit_socket(PFS_socket *pfs) {
  /* Combine per-operation socket wait stats before aggregating */
  PFS_single_stat stat;
  pfs->m_socket_stat.m_io_stat.sum_waits(&stat);
  m_stat.aggregate(&stat);
}

/** Table I/O wait visitor */

PFS_object_wait_visitor::PFS_object_wait_visitor() = default;

PFS_object_wait_visitor::~PFS_object_wait_visitor() = default;

void PFS_object_wait_visitor::visit_global() {
  global_table_io_stat.sum(&m_stat);
  global_table_lock_stat.sum(&m_stat);
}

void PFS_object_wait_visitor::visit_table_share(PFS_table_share *pfs) {
  const uint safe_key_count = sanitize_index_count(pfs->m_key_count);
  pfs->sum(&m_stat, safe_key_count);
}

void PFS_object_wait_visitor::visit_table(PFS_table *pfs) {
  PFS_table_share *table_share = sanitize_table_share(pfs->m_share);
  if (table_share != nullptr) {
    const uint safe_key_count = sanitize_index_count(table_share->m_key_count);
    pfs->m_table_stat.sum(&m_stat, safe_key_count);
  }
}

PFS_table_io_wait_visitor::PFS_table_io_wait_visitor() = default;

PFS_table_io_wait_visitor::~PFS_table_io_wait_visitor() = default;

void PFS_table_io_wait_visitor::visit_global() {
  global_table_io_stat.sum(&m_stat);
}

void PFS_table_io_wait_visitor::visit_table_share(PFS_table_share *pfs) {
  PFS_table_io_stat io_stat;
  const uint safe_key_count = sanitize_index_count(pfs->m_key_count);
  uint index;
  PFS_table_share_index *index_stat;

  /* Aggregate index stats */
  for (index = 0; index < safe_key_count; index++) {
    index_stat = pfs->find_index_stat(index);
    if (index_stat != nullptr) {
      io_stat.aggregate(&index_stat->m_stat);
    }
  }

  /* Aggregate global stats */
  index_stat = pfs->find_index_stat(MAX_INDEXES);
  if (index_stat != nullptr) {
    io_stat.aggregate(&index_stat->m_stat);
  }

  io_stat.sum(&m_stat);
}

void PFS_table_io_wait_visitor::visit_table(PFS_table *pfs) {
  PFS_table_share *safe_share = sanitize_table_share(pfs->m_share);

  if (likely(safe_share != nullptr)) {
    PFS_table_io_stat io_stat;
    const uint safe_key_count = sanitize_index_count(safe_share->m_key_count);
    uint index;

    /* Aggregate index stats */
    for (index = 0; index < safe_key_count; index++) {
      io_stat.aggregate(&pfs->m_table_stat.m_index_stat[index]);
    }

    /* Aggregate global stats */
    io_stat.aggregate(&pfs->m_table_stat.m_index_stat[MAX_INDEXES]);

    io_stat.sum(&m_stat);
  }
}

/** Table I/O stat visitor */

PFS_table_io_stat_visitor::PFS_table_io_stat_visitor() = default;

PFS_table_io_stat_visitor::~PFS_table_io_stat_visitor() = default;

void PFS_table_io_stat_visitor::visit_table_share(PFS_table_share *pfs) {
  const uint safe_key_count = sanitize_index_count(pfs->m_key_count);
  uint index;
  PFS_table_share_index *index_stat;

  /* Aggregate index stats */
  for (index = 0; index < safe_key_count; index++) {
    index_stat = pfs->find_index_stat(index);
    if (index_stat != nullptr) {
      m_stat.aggregate(&index_stat->m_stat);
    }
  }

  /* Aggregate global stats */
  index_stat = pfs->find_index_stat(MAX_INDEXES);
  if (index_stat != nullptr) {
    m_stat.aggregate(&index_stat->m_stat);
  }
}

void PFS_table_io_stat_visitor::visit_table(PFS_table *pfs) {
  PFS_table_share *safe_share = sanitize_table_share(pfs->m_share);

  if (likely(safe_share != nullptr)) {
    const uint safe_key_count = sanitize_index_count(safe_share->m_key_count);
    uint index;

    /* Aggregate index stats */
    for (index = 0; index < safe_key_count; index++) {
      m_stat.aggregate(&pfs->m_table_stat.m_index_stat[index]);
    }

    /* Aggregate global stats */
    m_stat.aggregate(&pfs->m_table_stat.m_index_stat[MAX_INDEXES]);
  }
}

/** Index I/O stat visitor */

PFS_index_io_stat_visitor::PFS_index_io_stat_visitor() = default;

PFS_index_io_stat_visitor::~PFS_index_io_stat_visitor() = default;

void PFS_index_io_stat_visitor::visit_table_share_index(PFS_table_share *pfs,
                                                        uint index) {
  PFS_table_share_index *index_stat;

  index_stat = pfs->find_index_stat(index);
  if (index_stat != nullptr) {
    m_stat.aggregate(&index_stat->m_stat);
  }
}

void PFS_index_io_stat_visitor::visit_table_index(PFS_table *pfs, uint index) {
  m_stat.aggregate(&pfs->m_table_stat.m_index_stat[index]);
}

/** Table lock wait visitor */

PFS_table_lock_wait_visitor::PFS_table_lock_wait_visitor() = default;

PFS_table_lock_wait_visitor::~PFS_table_lock_wait_visitor() = default;

void PFS_table_lock_wait_visitor::visit_global() {
  global_table_lock_stat.sum(&m_stat);
}

void PFS_table_lock_wait_visitor::visit_table_share(PFS_table_share *pfs) {
  pfs->sum_lock(&m_stat);
}

void PFS_table_lock_wait_visitor::visit_table(PFS_table *pfs) {
  pfs->m_table_stat.sum_lock(&m_stat);
}

/** Table lock stat visitor */

PFS_table_lock_stat_visitor::PFS_table_lock_stat_visitor() = default;

PFS_table_lock_stat_visitor::~PFS_table_lock_stat_visitor() = default;

void PFS_table_lock_stat_visitor::visit_table_share(PFS_table_share *pfs) {
  PFS_table_share_lock *lock_stat;

  lock_stat = pfs->find_lock_stat();
  if (lock_stat != nullptr) {
    m_stat.aggregate(&lock_stat->m_stat);
  }
}

void PFS_table_lock_stat_visitor::visit_table(PFS_table *pfs) {
  m_stat.aggregate(&pfs->m_table_stat.m_lock_stat);
}

PFS_instance_socket_io_stat_visitor::PFS_instance_socket_io_stat_visitor() =
    default;

PFS_instance_socket_io_stat_visitor::~PFS_instance_socket_io_stat_visitor() =
    default;

void PFS_instance_socket_io_stat_visitor::visit_socket_class(
    PFS_socket_class *pfs) {
  /* Aggregate wait times, event counts and byte counts */
  m_socket_io_stat.aggregate(&pfs->m_socket_stat.m_io_stat);
}

void PFS_instance_socket_io_stat_visitor::visit_socket(PFS_socket *pfs) {
  /* Aggregate wait times, event counts and byte counts */
  m_socket_io_stat.aggregate(&pfs->m_socket_stat.m_io_stat);
}

PFS_instance_file_io_stat_visitor::PFS_instance_file_io_stat_visitor() =
    default;

PFS_instance_file_io_stat_visitor::~PFS_instance_file_io_stat_visitor() =
    default;

void PFS_instance_file_io_stat_visitor::visit_file_class(PFS_file_class *pfs) {
  /* Aggregate wait times, event counts and byte counts */
  m_file_io_stat.aggregate(&pfs->m_file_stat.m_io_stat);
}

void PFS_instance_file_io_stat_visitor::visit_file(PFS_file *pfs) {
  /* Aggregate wait times, event counts and byte counts */
  m_file_io_stat.aggregate(&pfs->m_file_stat.m_io_stat);
}
/** @} */
