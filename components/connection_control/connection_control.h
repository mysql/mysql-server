/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef CONNECTION_CONTROL_H
#define CONNECTION_CONTROL_H

#include <mysql/components/library_mysys/instrumented_mutex.h>
#include <mysql/components/library_mysys/my_memory.h>
#include <mysql/components/services/audit_api_connection_service.h>
#include <mysql/components/services/bits/mysql_rwlock_bits.h>
#include <mysql/components/services/bits/psi_cond_bits.h>
#include <mysql/components/services/bits/psi_rwlock_bits.h>
#include <mysql/components/services/bits/psi_stage_bits.h>
#include <mysql/components/services/component_status_var_service.h>
#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/components/services/dynamic_privilege.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/mysql_current_thread_reader.h>
#include <mysql/components/services/mysql_rwlock.h>
#include <mysql/components/services/pfs_plugin_table_service.h>
#include <mysql/components/services/psi_memory.h>
#include <mysql/components/services/psi_memory_service.h>
#include <mysql/components/services/security_context.h>
#include <mysql/components/util/event_tracking/event_tracking_connection_consumer_helper.h>
#include "connection_control_data.h"

extern REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_column_string_v2);
extern REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_table_v1);
extern REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_column_integer_v1);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_thd_security_context);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_security_context_options);
extern REQUIRES_SERVICE_PLACEHOLDER(log_builtins);
extern REQUIRES_SERVICE_PLACEHOLDER(log_builtins_string);
extern REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_register);
extern REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_unregister);
extern REQUIRES_SERVICE_PLACEHOLDER(status_variable_registration);
extern REQUIRES_SERVICE_PLACEHOLDER(registry);
extern REQUIRES_SERVICE_PLACEHOLDER(registry_registration);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_current_thread_reader);
extern PSI_memory_key key_connection_delay_memory;
extern PSI_mutex_key key_connection_delay_mutex;
extern PSI_rwlock_key key_connection_event_delay_lock;
extern PSI_cond_key key_connection_delay_wait;
extern PSI_stage_info stage_waiting_in_component_connection_control;

namespace connection_control {
/** Helper class : Wrapper on READ lock */

class RD_lock {
 public:
  explicit RD_lock(mysql_rwlock_t *lock) : m_lock(lock) {
    if (m_lock != nullptr) {
      mysql_rwlock_rdlock(m_lock);
    }
  }
  ~RD_lock() {
    if (m_lock != nullptr) {
      mysql_rwlock_unlock(m_lock);
    }
  }
  void lock() { mysql_rwlock_rdlock(m_lock); }
  void unlock() { mysql_rwlock_unlock(m_lock); }
  RD_lock(const RD_lock &) = delete;        /* Not copyable. */
  void operator=(const RD_lock &) = delete; /* Not assignable. */

 private:
  mysql_rwlock_t *m_lock;
};

/** Helper class : Wrapper on write lock */

class WR_lock {
 public:
  explicit WR_lock(mysql_rwlock_t *lock) : m_lock(lock) {
    if (m_lock != nullptr) {
      mysql_rwlock_wrlock(m_lock);
    }
  }
  ~WR_lock() {
    if (m_lock != nullptr) {
      mysql_rwlock_unlock(m_lock);
    }
  }
  void lock() { mysql_rwlock_wrlock(m_lock); }
  void unlock() { mysql_rwlock_unlock(m_lock); }
  WR_lock(const WR_lock &) = delete;        /* Not copyable. */
  void operator=(const WR_lock &) = delete; /* Not assignable. */

 private:
  mysql_rwlock_t *m_lock;
};
}  // namespace connection_control
#endif /* !CONNECTION_CONTROL_H */
