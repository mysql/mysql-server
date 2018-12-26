/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

AUDIT_NULL_VAR(general_log)
AUDIT_NULL_VAR(general_error)
AUDIT_NULL_VAR(general_result)
AUDIT_NULL_VAR(general_status)

AUDIT_NULL_VAR(connection_connect)
AUDIT_NULL_VAR(connection_disconnect)
AUDIT_NULL_VAR(connection_change_user)
AUDIT_NULL_VAR(connection_pre_authenticate)

AUDIT_NULL_VAR(parse_preparse)
AUDIT_NULL_VAR(parse_postparse)

AUDIT_NULL_VAR(command_start)
AUDIT_NULL_VAR(command_end)

AUDIT_NULL_VAR(authorization_user)
AUDIT_NULL_VAR(authorization_db)
AUDIT_NULL_VAR(authorization_table)
AUDIT_NULL_VAR(authorization_column)
AUDIT_NULL_VAR(authorization_procedure)
AUDIT_NULL_VAR(authorization_proxy)

AUDIT_NULL_VAR(query_start)
AUDIT_NULL_VAR(query_nested_start)
AUDIT_NULL_VAR(query_status_end)
AUDIT_NULL_VAR(query_nested_status_end)

AUDIT_NULL_VAR(server_startup)
AUDIT_NULL_VAR(server_shutdown)

AUDIT_NULL_VAR(table_access_insert)
AUDIT_NULL_VAR(table_access_delete)
AUDIT_NULL_VAR(table_access_update)
AUDIT_NULL_VAR(table_access_read)

AUDIT_NULL_VAR(global_variable_get)
AUDIT_NULL_VAR(global_variable_set)
