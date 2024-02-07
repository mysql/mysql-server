/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_SRC_MODULE_MYSQLX_H_
#define PLUGIN_X_SRC_MODULE_MYSQLX_H_

#include "plugin/x/src/helper/multithread/lock_container.h"
#include "plugin/x/src/helper/multithread/rw_lock.h"
#include "plugin/x/src/interface/server.h"
#include "plugin/x/src/interface/sha256_password_cache.h"
#include "plugin/x/src/mq/notice_input_queue.h"
#include "plugin/x/src/services/services.h"
#include "plugin/x/src/udf/registry.h"

namespace modules {

class Module_mysqlx {
  using Server_interface = xpl::iface::Server;
  using Notice_input_queue = xpl::Notice_input_queue;
  using SHA256_password_cache_interface = xpl::iface::SHA256_password_cache;

  using Server_with_lock =
      xpl::Locked_container<Server_interface, xpl::RWLock_readlock,
                            xpl::RWLock>;
  using Notice_queue_with_lock =
      xpl::Locked_container<Notice_input_queue, xpl::RWLock_readlock,
                            xpl::RWLock>;
  using Sha245_cache_with_lock =
      xpl::Locked_container<SHA256_password_cache_interface,
                            xpl::RWLock_readlock, xpl::RWLock>;
  using Services_with_lock =
      xpl::Locked_container<xpl::Services, xpl::RWLock_readlock, xpl::RWLock>;

 public:
  static int initialize(MYSQL_PLUGIN p);
  static int deinitialize(MYSQL_PLUGIN p);

  static bool reset();

  static struct SYS_VAR **get_plugin_variables();
  static struct SHOW_VAR *get_status_variables();

  static Server_with_lock get_instance_server();
  static Notice_queue_with_lock get_instance_notice_queue();
  static Sha245_cache_with_lock get_instance_sha256_password_cache();
  static Services_with_lock get_instance_services();

 private:
  static void require_services();
  static void unrequire_services();

  static void provide_services();
  static void unprovide_services();

  static void provide_udfs();
  static void unregister_udfs();

  static Server_interface *m_server;
  static Notice_input_queue *m_input_queue;
  static SHA256_password_cache_interface *m_sha256_password_cache;
  static xpl::RWLock m_instance_rwl;
  static MYSQL_PLUGIN plugin_ref;
  static xpl::Services *m_services;
  static xpl::udf::Registry *m_udf_register;
};

}  // namespace modules

#endif  // PLUGIN_X_SRC_MODULE_MYSQLX_H_
