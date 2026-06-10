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

#ifndef SERVER_TELEMETRY_SECRET_PROVIDER_BITS_H
#define SERVER_TELEMETRY_SECRET_PROVIDER_BITS_H

/** Opaque. */
struct telemetry_secret_client_t;
struct telemetry_secret_t;

typedef telemetry_secret_client_t *(*tel_secret_init_v1_t)();

typedef void (*tel_secret_cleanup_v1_t)(telemetry_secret_client_t *client);

typedef telemetry_secret_t *(*tel_secret_open_v1_t)(
    telemetry_secret_client_t *client, const char *secret_name);

typedef bool (*tel_secret_read_v1_t)(telemetry_secret_t *secret,
                                     const char **secret_value);

typedef void (*tel_secret_close_v1_t)(telemetry_secret_t *secret);

#endif /* SERVER_TELEMETRY_RESOURCE_PROVIDER_BITS_H */
