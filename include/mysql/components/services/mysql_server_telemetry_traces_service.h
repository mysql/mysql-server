/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_SERVER_TELEMETRY_TRACES_SERVICE_INCLUDED
#define MYSQL_SERVER_TELEMETRY_TRACES_SERVICE_INCLUDED

#include <mysql/components/service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/bits/server_telemetry_traces_bits.h>

/*
  Version 1.
  Introduced in MySQL 8.0.33
  Status: Active.
*/
BEGIN_SERVICE_DEFINITION(mysql_server_telemetry_traces_v1)

/**
  Register set of telemetry notification callbacks.
*/
register_telemetry_v1_t register_telemetry;

/**
  Abort the current statement and session.
*/
abort_telemetry_v1_t abort_telemetry;

/**
  Unregister set of telemetry notification callbacks.
*/
unregister_telemetry_v1_t unregister_telemetry;

END_SERVICE_DEFINITION(mysql_server_telemetry_traces_v1)

#endif /* MYSQL_SERVER_TELEMETRY_TRACES_SERVICE_INCLUDED */
