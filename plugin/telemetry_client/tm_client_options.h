/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "my_compiler.h"

#include <mysql.h>
#include <mysql/client_plugin.h>
#include <mysql/plugin_client_telemetry.h>

/* Client options */

extern bool opt_help;

extern bool opt_trace_enabled;

extern const char *opt_otel_resource_attributes;

const unsigned long OTEL_LOG_LEVEL_SILENT = 0;
const unsigned long OTEL_LOG_LEVEL_ERROR = 1;
const unsigned long OTEL_LOG_LEVEL_WARNING = 2;
const unsigned long OTEL_LOG_LEVEL_INFO = 3;
const unsigned long OTEL_LOG_LEVEL_DEBUG = 4;
const unsigned long OTEL_DEFAULT_LOG_LEVEL = OTEL_LOG_LEVEL_SILENT;
const unsigned long OTEL_MIN_LOG_LEVEL = 0;
const unsigned long OTEL_MAX_LOG_LEVEL = 4;

extern unsigned long opt_otel_log_level;

const unsigned long OTLP_TLS_DEFAULT = 0;
const unsigned long OTLP_TLS_12 = 1;
const unsigned long OTLP_TLS_13 = 2;
const unsigned long OTLP_MIN_TLS = 0;
const unsigned long OTLP_MAX_TLS = 2;

const unsigned long OTLP_PROTOCOL_HTTP_PROTOBUF = 0;
const unsigned long OTLP_PROTOCOL_HTTP_JSON = 1;
const unsigned long OTLP_DEFAULT_PROTOCOL = OTLP_PROTOCOL_HTTP_PROTOBUF;
const unsigned long OTLP_MIN_PROTOCOL = 0;
const unsigned long OTLP_MAX_PROTOCOL = 1;

extern unsigned long opt_otel_exporter_otlp_traces_protocol;

extern const char *opt_otel_exporter_otlp_traces_endpoint;

extern const char *opt_otel_exporter_otlp_traces_certificates;

extern const char *opt_otel_exporter_otlp_traces_client_key;

extern const char *opt_otel_exporter_otlp_traces_client_certificates;

extern unsigned long opt_otel_exporter_otlp_traces_min_tls;
extern unsigned long opt_otel_exporter_otlp_traces_max_tls;
extern const char *opt_otel_exporter_otlp_traces_cipher;
extern const char *opt_otel_exporter_otlp_traces_cipher_suite;

extern const char *opt_otel_exporter_otlp_traces_headers;

const unsigned long OTLP_COMPRESSION_NONE = 0;
const unsigned long OTLP_COMPRESSION_GZIP = 1;
const unsigned long OTLP_DEFAULT_COMPRESSION = OTLP_COMPRESSION_NONE;
const unsigned long OTLP_MIN_COMPRESSION = 0;
const unsigned long OTLP_MAX_COMPRESSION = 1;

extern unsigned long opt_otel_exporter_otlp_traces_compression;

extern long opt_otel_exporter_otlp_traces_timeout;

extern long opt_otel_bsp_schedule_delay;
extern long opt_otel_bsp_max_queue_size;
extern long opt_otel_bsp_max_export_batch_size;

int register_client_options(const char *defaults_file,
                            const char *defaults_group_suffix,
                            const char *defaults_extra_file,
                            int *client_argc_ptr, char **client_argv);
