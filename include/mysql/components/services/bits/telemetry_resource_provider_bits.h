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

#ifndef SERVER_TELEMETRY_RESOURCE_PROVIDER_BITS_H
#define SERVER_TELEMETRY_RESOURCE_PROVIDER_BITS_H

/** Opaque. */
struct telemetry_resource_t;

/** Opaque. */
struct telemetry_resource_iterator_t;

typedef telemetry_resource_t *(*tel_resource_create_v1_t)();

typedef void (*tel_resource_destroy_v1_t)(telemetry_resource_t *resource);

typedef telemetry_resource_iterator_t *(*tel_resource_iterator_create_v1_t)(
    telemetry_resource_t *resource);

typedef void (*tel_resource_iterator_destroy_v1_t)(
    telemetry_resource_iterator_t *iterator);

typedef bool (*tel_resource_iterator_next_v1_t)(
    telemetry_resource_iterator_t *iterator);

typedef bool (*tel_resource_iterator_get_key_name_v1_t)(
    telemetry_resource_iterator_t *iterator, const char **name);

typedef bool (*tel_resource_iterator_get_key_value_v1_t)(
    telemetry_resource_iterator_t *iterator, const char **value);

#endif /* SERVER_TELEMETRY_RESOURCE_PROVIDER_BITS_H */
