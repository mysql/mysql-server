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

#ifndef CONNECTION_DELAY_H
#define CONNECTION_DELAY_H

#include <my_hostname.h> /* HOSTNAME_LENGTH */
#include <my_inttypes.h>
#include <mysql/components/services/bits/thd.h>
#include <mysql/service_parser.h>
#include <algorithm>
#include "connection_control_data.h"       /* variables and status */
#include "connection_control_interfaces.h" /* Observer interface */
#include "connection_control_pfs_table.h"
#include "connection_delay_api.h" /* Constants */

namespace connection_control {
/**
  Connection event action to enforce max failed login constraint
*/

class Connection_delay_action : public Connection_event_observer,
                                public Connection_control_alloc {
 public:
  Connection_delay_action(int64 threshold, int64 min_delay, int64 max_delay,
                          bool exempt_unknown_users,
                          opt_connection_control *sys_vars,
                          size_t sys_vars_size,
                          stats_connection_control *status_vars,
                          size_t status_vars_size, mysql_rwlock_t *lock);

  /** Destructor */
  ~Connection_delay_action() override {
    deinit();
    m_lock = nullptr;
  }

  void init(Connection_event_coordinator *coordinator);

  /**
    Set threshold value.

    @param threshold [in]        New threshold value
  */

  void set_threshold(int64 threshold) { m_threshold = threshold; }

  /** Get threshold value */
  int64 get_threshold() const { return m_threshold; }

  /**
    Set min/max delay

    @param new_value [in]        New m_min_delay/m_max_delay value
    @param min [in]              true for m_min_delay. false otherwise.

    @returns whether m_min_delay/m_max_delay value was changed successfully or
    not
      @retval false  Success
      @retval true Failure. Invalid value specified.
  */

  bool set_delay(int64 new_value, bool min) {
    const int64 current_max = get_max_delay();
    const int64 current_min = get_min_delay();

    if (new_value < MIN_DELAY) {
      return true;
    }
    if (new_value > MAX_DELAY) {
      return true;
    }
    if ((min && new_value > current_max) || (!min && new_value < current_min)) {
      return true;
    }

    if (min) {
      m_min_delay = new_value;
    } else {
      m_max_delay = new_value;
    }
    return false;
  }

  /** Get max value */
  int64 get_max_delay() const { return m_max_delay; }

  /** Get min value */
  int64 get_min_delay() const { return m_min_delay; }

  /**
    Set if component should exempt un-authenticated connections
    from connection control.

    @param new_value [in]        New value
  */
  void set_exempt_unknown_users(bool new_value) {
    m_exempt_unknown_users = new_value;
  }

  /** Get exempt_unknown_users flag */
  [[nodiscard]] bool get_exempt_unknown_users() const {
    return m_exempt_unknown_users;
  }

  /** Overridden functions */
  bool notify_event(
      MYSQL_THD thd, Connection_event_coordinator *coordinator,
      const mysql_event_tracking_connection_data *connection_event) override;
  bool notify_sys_var(Connection_event_coordinator *coordinator,
                      opt_connection_control variable,
                      void *new_value) override;

 private:
  void deinit();
  void make_hash_key(MYSQL_THD thd, Sql_string &s);
  void get_priv_account(MYSQL_THD thd, Sql_string &s);
  /**
    Generates wait time

    @param count [in] Proposed delay in msec

    @returns wait time
  */

  ulonglong get_wait_time(int64 count) const {
    const int64 max_delay = get_max_delay();
    const int64 min_delay = get_min_delay();

    /*
      if count < 0 (can happen in edge cases
      we return max_delay.
      Otherwise, following equation will be used:
      wait_time = MIN(MAX(count, min_delay),
                      max_delay)
    */
    if (count < 0) {
      return max_delay;
    }
    return std::min((std::max(count, min_delay)), max_delay);
  }
  void conditional_wait(ulonglong wait_time);

  /** Threshold value which triggers wait */
  int64 m_threshold;
  /** Lower cap on delay in msec to be generated */
  int64 m_min_delay;
  /** Upper cap on delay in msec to be generated */
  int64 m_max_delay;
  /** Do not apply delays for failing unauthenticated TCP connections */
  bool m_exempt_unknown_users;
  /** System variables */
  std::vector<opt_connection_control, CustomAllocator<opt_connection_control>>
      m_sys_vars;
  /** Status variables */
  std::vector<stats_connection_control,
              CustomAllocator<stats_connection_control>>
      m_stats_vars;
  /** RW lock */
  mysql_rwlock_t *m_lock;
};
}  // namespace connection_control
#endif /* !CONNECTION_DELAY_H */
