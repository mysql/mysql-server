/* Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#ifndef PFS_EXAMPLE_COMPONENT_H
#define PFS_EXAMPLE_COMPONENT_H

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/pfs_plugin_table_service.h>

/* A place to specify component-wide declarations, including declarations of
  placeholders for Service dependencies. */

extern REQUIRES_SERVICE_PLACEHOLDER_AS(pfs_plugin_table_v1, pt_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(pfs_plugin_column_string_v2,
                                       pc_string_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(pfs_plugin_column_year_v1, pc_year_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(pfs_plugin_column_bigint_v1,
                                       pc_bigint_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(pfs_plugin_column_double_v1,
                                       pc_double_srv);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(pfs_plugin_column_text_v1, pc_text_srv);

/* Number of characters * max multibyte length */
#define COUNTRY_NAME_LEN 20 * 4
#define CONTINENT_NAME_LEN 20 * 4
#define COUNTRY_CODE_LEN 4

#endif /* PFS_EXAMPLE_COMPONENT_H */
