/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/sql_service/sql_service_context_base.h"

const st_command_service_cbs Sql_service_context_base::sql_service_callbacks = {
    &Sql_service_context_base::sql_start_result_metadata,
    &Sql_service_context_base::sql_field_metadata,
    &Sql_service_context_base::sql_end_result_metadata,
    &Sql_service_context_base::sql_start_row,
    &Sql_service_context_base::sql_end_row,
    &Sql_service_context_base::sql_abort_row,
    &Sql_service_context_base::sql_get_client_capabilities,
    &Sql_service_context_base::sql_get_null,
    &Sql_service_context_base::sql_get_integer,
    &Sql_service_context_base::sql_get_longlong,
    &Sql_service_context_base::sql_get_decimal,
    &Sql_service_context_base::sql_get_double,
    &Sql_service_context_base::sql_get_date,
    &Sql_service_context_base::sql_get_time,
    &Sql_service_context_base::sql_get_datetime,
    &Sql_service_context_base::sql_get_string,
    &Sql_service_context_base::sql_handle_ok,
    &Sql_service_context_base::sql_handle_error,
    &Sql_service_context_base::sql_shutdown,
    &Sql_service_context_base::sql_connection_alive,
};
