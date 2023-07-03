/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

// Implements
#include "storage/ndb/plugin/ndb_pfs_init.h"

// Uses
#include "mysql/components/services/pfs_plugin_table_service.h"
#include "storage/ndb/plugin/ndb_mysql_services.h"

static SERVICE_TYPE_NO_CONST(pfs_plugin_table_v1) *pfs_table = nullptr;
SERVICE_TYPE_NO_CONST(pfs_plugin_column_string_v2) *pfscol_string = nullptr;
SERVICE_TYPE_NO_CONST(pfs_plugin_column_enum_v1) *pfscol_enum = nullptr;

extern PFS_engine_table_share_proxy *ndb_sync_pending_objects_share;
extern PFS_engine_table_share_proxy *ndb_sync_excluded_objects_share;
static PFS_engine_table_share_proxy *pfs_proxy_shares[2] = {
    ndb_sync_pending_objects_share, ndb_sync_excluded_objects_share};

PSI_memory_key key_memory_thd_ndb_batch_mem_root;

bool ndb_pfs_init() {
  {
    // List of memory keys to register
    PSI_memory_info mem_keys[] = {{&key_memory_thd_ndb_batch_mem_root,
                                   "Thd_ndb::batch_mem_root",
                                   (PSI_FLAG_THREAD | PSI_FLAG_MEM_COLLECT), 0,
                                   "Memory used for transaction batching"}};
    mysql_memory_register("ndbcluster", mem_keys,
                          sizeof(mem_keys) / sizeof(mem_keys[0]));
  }

  Ndb_mysql_services services;

  // Get table service
  if (services.acquire_service(pfs_table, "pfs_plugin_table_v1")) return true;
  // Get column services
  if (services.acquire_service(pfscol_string, "pfs_plugin_column_string_v2"))
    return true;
  if (services.acquire_service(pfscol_enum, "pfs_plugin_column_enum_v1"))
    return true;

  return pfs_table->add_tables(pfs_proxy_shares, 2);
}

void ndb_pfs_deinit() {
  if (pfs_table) {
    static_cast<void>(pfs_table->delete_tables(pfs_proxy_shares, 2));
  }

  Ndb_mysql_services services;
  services.release_service(pfs_table);
  services.release_service(pfscol_string);
  services.release_service(pfscol_enum);
}
