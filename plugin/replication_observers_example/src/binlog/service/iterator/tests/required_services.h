/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef TEST_BINLOG_STORAGE_ITERATOR_REQUIRED_SERVICES_H
#define TEST_BINLOG_STORAGE_ITERATOR_REQUIRED_SERVICES_H

/* A place to specify component-wide declarations, including declarations of
 *   placeholders for Service dependencies. */

#include <mysql/components/component_implementation.h>
#include <mysql/components/my_service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/binlog_storage_iterator.h>
#include <mysql/components/services/component_status_var_service.h>
#include <mysql/components/services/mysql_current_thread_reader.h>
#include <mysql/components/services/pfs_plugin_table_service.h>

extern SERVICE_TYPE(pfs_plugin_table_v1) * table_srv;
extern SERVICE_TYPE(pfs_plugin_column_string_v2) * col_string_srv;
extern SERVICE_TYPE(pfs_plugin_column_bigint_v1) * pc_bigint_srv;
extern SERVICE_TYPE(pfs_plugin_column_blob_v1) * pc_blob_srv;
extern SERVICE_TYPE(binlog_storage_iterator) * binlog_iterator_svc;
extern SERVICE_TYPE(mysql_current_thread_reader) * current_thd_srv;
extern SERVICE_TYPE(status_variable_registration) * statvar_register_srv;

#endif /* TEST_BINLOG_STORAGE_ITERATOR_REQUIRED_SERVICES_H */
