/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_observers/group_transaction_observation_manager.h"
#include "plugin/group_replication/include/observer_trans.h"
#include "plugin/group_replication/include/plugin_psi.h"

Group_transaction_listener::~Group_transaction_listener() = default;

Group_transaction_observation_manager::Group_transaction_observation_manager() {
  transaction_observer_list_lock = new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
      key_GR_RWLOCK_transaction_observation_list
#endif
  );
  registered_observers.store(false);
}

Group_transaction_observation_manager::
    ~Group_transaction_observation_manager() {
  if (!group_transaction_listeners.empty()) {
    /* purecov: begin inspected */
    for (Group_transaction_listener *trans_observer :
         group_transaction_listeners) {
      delete trans_observer;
    }
    group_transaction_listeners.clear();
    /* purecov: end */
  }
  delete transaction_observer_list_lock;
}

void Group_transaction_observation_manager::register_transaction_observer(
    Group_transaction_listener *observer) {
  DBUG_TRACE;
  write_lock_observer_list();
  group_transaction_listeners.push_back(observer);
  registered_observers.store(true);
  unlock_observer_list();
}

void Group_transaction_observation_manager::unregister_transaction_observer(
    Group_transaction_listener *observer) {
  DBUG_TRACE;
  write_lock_observer_list();
  group_transaction_listeners.remove(observer);
  if (group_transaction_listeners.empty()) registered_observers.store(false);
  unlock_observer_list();
}

std::list<Group_transaction_listener *>
    *Group_transaction_observation_manager::get_all_observers() {
  DBUG_TRACE;
#ifndef NDEBUG
  transaction_observer_list_lock->assert_some_lock();
#endif
  return &group_transaction_listeners;
}

void Group_transaction_observation_manager::read_lock_observer_list() {
  transaction_observer_list_lock->rdlock();
}

void Group_transaction_observation_manager::write_lock_observer_list() {
  transaction_observer_list_lock->wrlock();
}

void Group_transaction_observation_manager::unlock_observer_list() {
  transaction_observer_list_lock->unlock();
}

bool Group_transaction_observation_manager::is_any_observer_present() {
  return registered_observers.load();
}
