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

#include "applier_metrics_pfs_table.h"  // get_applier_metrics_table_share
#include "mysql/components/component_implementation.h"  // REQUIRES_SERVICE_PLACEHOLDER
#include "mysql/components/services/pfs_plugin_table_service.h"  // pfs_plugin_table_v1
#include "worker_metrics_pfs_table.h"  // get_worker_metrics_table_share

extern REQUIRES_SERVICE_PLACEHOLDER(pfs_plugin_table_v1);

namespace applier_metrics {

static PFS_engine_table_share_proxy *share_list[2];

bool register_pfs_tables() {
  share_list[0] = get_applier_metrics_table_share();
  share_list[1] = get_worker_metrics_table_share();
  return SERVICE_PLACEHOLDER(pfs_plugin_table_v1)
      ->add_tables(&share_list[0], 2);
}

bool unregister_pfs_tables() {
  return SERVICE_PLACEHOLDER(pfs_plugin_table_v1)
      ->delete_tables(&share_list[0], 2);
}

}  // namespace applier_metrics
