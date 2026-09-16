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

#include "sql/changestreams/apply/util/module.h"

namespace mysql::csa {

Module::Module(PSI_mutex_key, PSI_rwlock_key rwlock_key)
    : m_rwlock_key(rwlock_key) {
  init_locking();
}

Module::~Module() { mysql_rwlock_destroy(&m_rwlock); }

void Module::init_locking() {
  mysql::csa::init_csa_psi();
  mysql_rwlock_init(m_rwlock_key, &m_rwlock);
}

bool Module::init() {
  auto scoped_guard = wrlock();
  if (m_inited.load() != State::off) {
    return true;
  }
  m_inited.store(State::start);
  bool ret = do_init();
  if (!ret) {
    m_inited.store(State::on);
  }
  return ret;
}

Module::Lock_guard Module::rdlock() {
  mysql_rwlock_rdlock(&m_rwlock);
  return Lock_guard(this);
}

Module::Lock_guard Module::wrlock() {
  mysql_rwlock_wrlock(&m_rwlock);
  return Lock_guard(this);
}

bool Module::deinit() {
  auto scoped_guard = wrlock();
  if (m_inited.load() == State::off) {
    return true;
  }
  bool res = false;
  m_inited.store(State::stop);
  res = do_deinit();
  m_inited.store(State::off);
  return res;
}

bool Module::is_inited() {
  auto scoped_guard = rdlock();
  return (m_inited.load() == State::on);
}

void Module::unlock() { mysql_rwlock_unlock(&m_rwlock); }

bool Module::do_init() { return false; }

bool Module::do_deinit() { return false; }

}  // namespace mysql::csa
