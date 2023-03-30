/*
 * Copyright (c) 2020, 2023, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/helper/multithread/xsync_point.h"

#include <mutex>  // NOLINT(build/c++11)
#include <set>
#include <string>

#include "my_systime.h"  // my_sleep() NOLINT(build/include_subdir)

namespace xpl {

#ifndef NDEBUG

class Dbug_context {
 public:
  static Dbug_context *singleton() {
    static Dbug_context obj;

    return &obj;
  }

  bool is_sync_points_blocked(const std::string &sync_point) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return 0 != m_waiting_sync_points.count(sync_point);
  }

  void block(const std::string &sync_point) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_waiting_sync_points.insert(sync_point);
  }

  void wakeup(const std::string &sync_point) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_waiting_sync_points.erase(sync_point);
  }

 private:
  Dbug_context() = default;
  std::set<std::string> m_waiting_sync_points;
  std::mutex m_mutex;
};

void xdbug_sync_points_enable(const std::vector<const char *> &sync_points) {
  auto ctxt = Dbug_context::singleton();

  for (auto sync : sync_points) {
    ctxt->block(sync);
  }
}

void xdbug_sync_point_check(const char *const sync_name,
                            const char *const wakeup_sync_name) {
  auto ctxt = Dbug_context::singleton();

  bool is_blocked = sync_name && ctxt->is_sync_points_blocked(sync_name);

  if ((!sync_name || is_blocked) && wakeup_sync_name) {
    ctxt->wakeup(wakeup_sync_name);
  }

  while (is_blocked) {
    my_sleep(100000);
    is_blocked = ctxt->is_sync_points_blocked(sync_name);
  }
}

void dbug_sync_point_check(const char *const sync_name) {
  while (DBUG_EVALUATE_IF(sync_name, true, false)) {
    my_sleep(100000);
  }
}

#endif  // NDEBUG

}  // namespace xpl
