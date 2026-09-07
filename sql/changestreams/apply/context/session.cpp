// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include "sql/changestreams/apply/context/session.h"
#include <mysql/group_replication_priv.h>
#include <mysql/thread_type.h>
#include <iostream>
#include <sstream>
#include "mutex_lock.h"  // MUTEX_LOCK
#include "sql/changestreams/apply/service/csa_service.h"
#include "sql/changestreams/apply/session/session_guard.h"
#include "sql/protocol_classic.h"
#include "sql/rpl_replica.h"

namespace mysql::csa {

Session::Session(THD *, std::size_t seq_num)
    : m_thd(nullptr), m_is_valid(false) {
  m_thd = new THD;
  if (!m_thd) {
    return;
  }
  m_thd->set_new_thread_id();
#ifdef HAVE_PSI_THREAD_INTERFACE
  struct PSI_thread *psi = PSI_THREAD_CALL(new_thread)(
      key_thread_session, seq_num, m_thd, m_thd->thread_id());
  PSI_THREAD_CALL(set_thread_THD)(psi, m_thd);
  m_thd->set_psi(psi);
#endif

  Session_guard replace_guard(m_thd);
  m_thd->thread_stack = (char *)&m_thd;
  m_thd->init_query_mem_roots();

  m_thd->slave_thread = true;
  m_thd->system_thread = SYSTEM_THREAD_SLAVE_WORKER;
  set_slave_thread_options(m_thd);

  m_thd->set_time();
  m_thd->variables.lock_wait_timeout = LONG_TIMEOUT;

  m_psi_id = m_thd->variables.pseudo_thread_id;
  global_thd_manager_add_thd(m_thd);
  m_is_valid = true;
  return;
}

ulonglong Session::get_thread_id() const { return m_psi_id; }

Session::~Session() {
  assert(m_thd);
  Session_guard replace_guard(m_thd);
  m_thd->release_resources();
#ifdef HAVE_PSI_THREAD_INTERFACE
  PSI_THREAD_CALL(delete_thread)(m_thd->get_psi());
  m_thd->set_psi(nullptr);
#endif
  global_thd_manager_remove_thd(m_thd);
  delete m_thd;
  m_thd = nullptr;
}

bool Session::is_valid() const { return m_thd != nullptr; }

unsigned int Session::get_thd_id() const { return m_thd->thread_id(); }

THD *Session::get_thd() {
  assert(m_thd);
  return m_thd;
}

void Session::attach(TABLE *temporary_tables) {
  std::lock_guard lock(m_session_lock);
  assert(m_thd);
  m_saved_thread_stack = m_thd->thread_stack;
  // this is the first stack entry after attaching
  // we are going to remove this from the stack, but
  // gives a rough calculation of how much stack we
  // are going to use.
  long stack_ptr = 0;
  m_thd->thread_stack = reinterpret_cast<char *>(&stack_ptr);
  m_thd->store_globals();
  m_thd->temporary_tables = temporary_tables;
  m_is_attached = true;
}

void Session::detach() {
  std::lock_guard lock(m_session_lock);
  assert(m_thd);
  if (!m_is_attached) {
    return;
  }
  m_thd->temporary_tables = nullptr;
  m_thd->restore_globals();
  // restore the thread stack
  m_thd->thread_stack = m_saved_thread_stack;
  m_saved_thread_stack = nullptr;
  m_is_attached = false;
}

void Session::awake(bool) {
  assert(m_thd);
  std::lock_guard lock(m_session_lock);
  // required by "awake"
  MUTEX_LOCK(guard, &m_thd->LOCK_thd_data);
  if (!m_is_killed) {
    m_is_killed = true;
    m_thd->awake(THD::KILL_CONNECTION);
  }
}

}  // namespace mysql::csa
