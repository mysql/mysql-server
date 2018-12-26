/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_service_context_base.h"

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
};
