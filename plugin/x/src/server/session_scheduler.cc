/*
 * Copyright (c) 2019, 2023, Oracle and/or its affiliates.
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

#include "plugin/x/src/server/session_scheduler.h"

#include <memory>
#include <utility>

#include "my_config.h"  // NOLINT(build/include_subdir)
#include "mysql/psi/mysql_thread.h"
#include "mysql/service_srv_session.h"
#include "mysql/service_ssl_wrapper.h"

#include "plugin/x/src/xpl_log.h"
#include "plugin/x/src/xpl_performance_schema.h"

namespace xpl {

Session_scheduler::Session_scheduler(
    const char *name, void *plugin,
    std::unique_ptr<iface::Scheduler_dynamic::Monitor> monitor)
    : ngs::Scheduler_dynamic(name, KEY_thread_x_worker, std::move(monitor)),
      m_plugin_ptr(plugin) {}

bool Session_scheduler::thread_init() {
  if (srv_session_init_thread(m_plugin_ptr) != 0) {
    log_error(ER_XPLUGIN_SRV_SESSION_INIT_THREAD_FAILED);
    return false;
  }

#ifdef HAVE_PSI_THREAD_INTERFACE
  // Reset user name and hostname stored in PFS_thread
  // which were copied from parent thread
  PSI_THREAD_CALL(set_thread_account)("", 0, "", 0);
#endif  // HAVE_PSI_THREAD_INTERFACE

  ngs::Scheduler_dynamic::thread_init();

  return true;
}

void Session_scheduler::thread_end() {
  ngs::Scheduler_dynamic::thread_end();
  srv_session_deinit_thread();

  ssl_wrapper_thread_cleanup();
}

}  // namespace xpl
