/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_SERVER_TELEMETRY_SECRET_PROVIDER_SERVICE_INCLUDED
#define MYSQL_SERVER_TELEMETRY_SECRET_PROVIDER_SERVICE_INCLUDED

#include <mysql/components/service.h>
#include <mysql/components/service_implementation.h>

#include <mysql/components/services/bits/telemetry_secret_provider_bits.h>

/*
  Version 1.
  Introduced in MySQL 9.3
  Status: Active.
*/
BEGIN_SERVICE_DEFINITION(telemetry_secret_provider)

tel_secret_init_v1_t secret_init;
tel_secret_cleanup_v1_t secret_cleanup;
tel_secret_open_v1_t secret_open;
tel_secret_read_v1_t secret_read;
tel_secret_close_v1_t secret_close;

END_SERVICE_DEFINITION(telemetry_secret_provider)

#endif /* MYSQL_SERVER_TELEMETRY_SECRET_PROVIDER_SERVICE_INCLUDED */
