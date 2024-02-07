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

#ifndef PLUGIN_CLIENT_TELEMETRY_INCLUDED
#define PLUGIN_CLIENT_TELEMETRY_INCLUDED

/**
  @file include/mysql/plugin_client_telemetry.h

  Declarations for client-side plugins of type MYSQL_CLIENT_TELEMETRY_PLUGIN.
*/

#include <mysql/client_plugin.h>

struct st_mysql_client_plugin_TELEMETRY;
struct MYSQL;

struct telemetry_span_t;

/**
  Start an OpenTelemetry trace span.
  @param [in] name span name
*/
typedef telemetry_span_t *(*telemetry_start_span_t)(const char *name);

/**
  Abstract text map carrier set interface.
  This is used for propagation, to set a key / value pair inside
  an arbitrary carrier.
  @param carrier_data Opaque carrier data to set
  @param key Key to set
  @param key_length Length of key
  @param value Value to set
  @param value_length Length of value
*/
typedef void (*telemetry_text_map_carrier_set_t)(void *carrier_data,
                                                 const char *key,
                                                 size_t key_length,
                                                 const char *value,
                                                 size_t value_length);

/**
  Inject an OpenTelemetry trace context into an arbitrary text map carrier.
  @param [in] span The trace span to inject
  @param [in] carrier_data text map carrier to set
  @param [in] carrier Function to use to set the data in the carrier
*/
typedef void (*telemetry_injector_t)(telemetry_span_t *span, void *carrier_data,
                                     telemetry_text_map_carrier_set_t carrier);

/**
  End an OpenTelemetry trace span.
  @param [in] span Span to end
*/
typedef void (*telemetry_end_span_t)(telemetry_span_t *span);

struct st_mysql_client_plugin_TELEMETRY {
  MYSQL_CLIENT_PLUGIN_HEADER
  telemetry_start_span_t start_span;
  telemetry_injector_t injector;
  telemetry_end_span_t end_span;
};

/**
  The global telemetry_plugin pointer.
*/
extern struct st_mysql_client_plugin_TELEMETRY *client_telemetry_plugin;

#endif /* PLUGIN_CLIENT_TELEMETRY_INCLUDED */
