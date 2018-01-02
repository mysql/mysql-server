/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef PFS_PLUGIN_TABLE_H
#define PFS_PLUGIN_TABLE_H

#include <mysql/plugin.h>
#include <mysql/components/services/pfs_plugin_table_service.h>

/**
  @file storage/perfschema/pfs_plugin_table.h
  The performance schema implementation of plugin table.
*/
extern SERVICE_TYPE(pfs_plugin_table)
  SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_table);

void init_pfs_plugin_table();
void cleanup_pfs_plugin_table();
#endif /* PFS_PLUGIN_TABLE_H */
