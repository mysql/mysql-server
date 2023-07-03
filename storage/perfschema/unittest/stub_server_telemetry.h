/* Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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

#include <mysql/components/services/mysql_server_telemetry_traces_service.h>
#include <mysql/plugin.h>
#include "storage/perfschema/mysql_server_telemetry_traces_service_imp.h"

void initialize_mysql_server_telemetry_traces_service() {}
void cleanup_mysql_server_telemetry_traces_service() {}
void server_telemetry_tracing_lock() {}
void server_telemetry_tracing_unlock() {}

SERVICE_TYPE(mysql_server_telemetry_traces_v1)
SERVICE_IMPLEMENTATION(performance_schema, mysql_server_telemetry_traces_v1){
    nullptr, nullptr, nullptr};

std::atomic<telemetry_t *> g_telemetry = nullptr;
