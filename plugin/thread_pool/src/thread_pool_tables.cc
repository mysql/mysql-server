/* Copyright (c) 2011, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation. The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

#include "plugin/thread_pool/src/thread_pool_tables.h"
#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/pfs_plugin_table_service.h>
#include "plugin/thread_pool/src/table_thread_group_state.h"
#include "plugin/thread_pool/src/table_thread_group_stats.h"
#include "plugin/thread_pool/src/table_thread_state.h"

#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/plugin.h>
#include <mysql/psi/mysql_thread.h>
#include <mysql/service_thread_scheduler.h>
#include <mysql/thread_pool_priv.h>
#include <sys/types.h>
#include <cstddef>

#include "my_atomic.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
/* Thread pool includes */
#include "plugin/thread_pool/src/thread_pool.h"

SERVICE_TYPE(pfs_plugin_table_v1) *pt_srv = nullptr;
SERVICE_TYPE(pfs_plugin_column_integer_v1) *pc_integer_srv = nullptr;
SERVICE_TYPE(pfs_plugin_column_bigint_v1) *pc_bigint_srv = nullptr;
SERVICE_TYPE(pfs_plugin_column_string_v2) *pc_string_srv = nullptr;

int init_plugin_table_service(SERVICE_TYPE(registry) * reg_srv) {
  if (reg_srv == nullptr) {
    return 1;
  }

  my_h_service opaque_pfs = nullptr;

  if (reg_srv->acquire("pfs_plugin_table_v1.performance_schema", &opaque_pfs) ==
      0) {
    pt_srv = reinterpret_cast<SERVICE_TYPE(pfs_plugin_table_v1) *>(opaque_pfs);
  }

  if (reg_srv->acquire("pfs_plugin_column_integer_v1.performance_schema",
                       &opaque_pfs) == 0) {
    pc_integer_srv =
        reinterpret_cast<SERVICE_TYPE(pfs_plugin_column_integer_v1) *>(
            opaque_pfs);
  }

  if (reg_srv->acquire("pfs_plugin_column_bigint_v1.performance_schema",
                       &opaque_pfs) == 0) {
    pc_bigint_srv =
        reinterpret_cast<SERVICE_TYPE(pfs_plugin_column_bigint_v1) *>(
            opaque_pfs);
  }

  if (reg_srv->acquire("pfs_plugin_column_string_v2.performance_schema",
                       &opaque_pfs) == 0) {
    pc_string_srv =
        reinterpret_cast<SERVICE_TYPE(pfs_plugin_column_string_v2) *>(
            opaque_pfs);
  }

  if ((pt_srv == nullptr) || (pc_integer_srv == nullptr) ||
      (pc_bigint_srv == nullptr) || (pc_string_srv == nullptr)) {
    /*
      Caller should invoke deinit_plugin_table_service()
      to release all references.
    */
    return 1;
  }

  return 0;
}

void deinit_plugin_table_service(SERVICE_TYPE(registry) * reg_srv) {
  if (reg_srv != nullptr) {
    using pt_srv_t = SERVICE_TYPE_NO_CONST(pfs_plugin_table_v1);
    reg_srv->release(
        reinterpret_cast<my_h_service>(const_cast<pt_srv_t *>(pt_srv)));
    pt_srv = nullptr;
    using pc_integer_srv_t =
        SERVICE_TYPE_NO_CONST(pfs_plugin_column_integer_v1);
    reg_srv->release(reinterpret_cast<my_h_service>(
        const_cast<pc_integer_srv_t *>(pc_integer_srv)));
    pc_integer_srv = nullptr;
    using pc_bigint_srv_t = SERVICE_TYPE_NO_CONST(pfs_plugin_column_bigint_v1);
    reg_srv->release(reinterpret_cast<my_h_service>(
        const_cast<pc_bigint_srv_t *>(pc_bigint_srv)));
    pc_bigint_srv = nullptr;
    using pc_string_srv_t = SERVICE_TYPE_NO_CONST(pfs_plugin_column_string_v2);
    reg_srv->release(reinterpret_cast<my_h_service>(
        const_cast<pc_string_srv_t *>(pc_string_srv)));
    pc_string_srv = nullptr;
  }
}

/* Performance schema thread pool tables. */

// Since the share is made up of values known at compile-time, we do not
// actually need to call a function during plugin init to initialize it. So we
// do not create an init function for tp_connections_proxy_share, and instead
// place a pointer to it directly into the tables array. The other members of
// the array are set to nullptr here, since they will be assigned by
// tp_tables_init() below.
extern constinit PFS_engine_table_share_proxy *tp_connections_proxy_share;
static PFS_engine_table_share_proxy *tables[] = {nullptr, nullptr, nullptr,
                                                 tp_connections_proxy_share};
constexpr auto table_count = std::end(tables) - std::begin(tables);

mysql_service_status_t tp_tables_init() {
  mysql_service_status_t rc = 1;

  if (pt_srv != nullptr) {
    tables[0] = table_share_tp_thread_state_init();
    tables[1] = table_share_tp_thread_group_state_init();
    tables[2] = table_share_tp_thread_group_stats_init();
    // We deliberately omit tp_connections_proxy_share here since that element
    // of the array has already been set (and we did not create an init
    // function for the share):

    rc = pt_srv->add_tables(tables, table_count);
  }

  return rc;
}

mysql_service_status_t tp_tables_deinit() {
  mysql_service_status_t rc = 1;

  if (pt_srv != nullptr) {
    rc = pt_srv->delete_tables(tables, table_count);
  }

  return rc;
}
