/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; version 2 of the
License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

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
