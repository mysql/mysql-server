/* Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/components/services/pfs_plugin_table_service.h"

template <typename T>
static bool acquire_service(SERVICE_TYPE(registry) * mysql_service_registry,
                            T &service, const char *name) {
  my_h_service mysql_service;
  if (mysql_service_registry->acquire(name, &mysql_service)) {
    return true;
  }
  service = reinterpret_cast<T>(mysql_service);
  return false;
}

template <typename T>
static void release_service(SERVICE_TYPE(registry) * mysql_service_registry,
                            T &service) {
  if (service != nullptr) {
    mysql_service_registry->release(reinterpret_cast<my_h_service>(service));
    service = nullptr;
  }
}

SERVICE_TYPE_NO_CONST(pfs_plugin_table_v1) *pfs_table = nullptr;
SERVICE_TYPE_NO_CONST(pfs_plugin_column_string_v1) *pfscol_string = nullptr;
SERVICE_TYPE_NO_CONST(pfs_plugin_column_enum_v1) *pfscol_enum = nullptr;

extern PFS_engine_table_share_proxy *ndb_sync_pending_objects_share;
extern PFS_engine_table_share_proxy *ndb_sync_excluded_objects_share;
static PFS_engine_table_share_proxy *pfs_proxy_shares[2] = {
    ndb_sync_pending_objects_share, ndb_sync_excluded_objects_share};

bool ndb_pfs_init(SERVICE_TYPE(registry) * mysql_service_registry) {
  if (mysql_service_registry == nullptr) {
    return false;
  }

  // Get table service
  if (acquire_service(mysql_service_registry, pfs_table, "pfs_plugin_table_v1"))
    return true;
  // Get column services
  if (acquire_service(mysql_service_registry, pfscol_string,
                      "pfs_plugin_column_string_v1"))
    return true;
  if (acquire_service(mysql_service_registry, pfscol_enum,
                      "pfs_plugin_column_enum_v1"))
    return true;

  return pfs_table->add_tables(pfs_proxy_shares, 2);
  ;
}

void ndb_pfs_deinit(SERVICE_TYPE(registry) * mysql_service_registry) {
  if (mysql_service_registry == nullptr) {
    return;
  }

  static_cast<void>(pfs_table->delete_tables(pfs_proxy_shares, 2));
  release_service(mysql_service_registry, pfs_table);
  release_service(mysql_service_registry, pfscol_string);
  release_service(mysql_service_registry, pfscol_enum);
}
