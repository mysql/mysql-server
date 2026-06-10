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

#ifndef TELEMETRY_SYSTEM_VARIABLES_H_INCLUDED
#define TELEMETRY_SYSTEM_VARIABLES_H_INCLUDED

namespace telemetry {

extern bool sv_trace_enabled;
extern bool sv_metrics_enabled;
extern bool sv_log_enabled;

extern bool sv_query_text_enabled;

const unsigned long OTEL_LOG_LEVEL_SILENT = 0;
const unsigned long OTEL_LOG_LEVEL_ERROR = 1;
const unsigned long OTEL_LOG_LEVEL_WARNING = 2;
const unsigned long OTEL_LOG_LEVEL_INFO = 3;
const unsigned long OTEL_LOG_LEVEL_DEBUG = 4;
extern unsigned long sv_otel_log_level;

const unsigned long OTLP_TLS_DEFAULT = 0;
const unsigned long OTLP_TLS_12 = 1;
const unsigned long OTLP_TLS_13 = 2;

extern char *sv_otel_resource_attributes;

const unsigned long OTLP_PROTOCOL_HTTP_PROTOBUF = 0;
const unsigned long OTLP_PROTOCOL_HTTP_JSON = 1;
extern unsigned long sv_otel_exporter_otlp_traces_protocol;

extern char *sv_otel_exporter_otlp_traces_endpoint;

extern char *sv_otel_exporter_otlp_traces_network_namespace;

extern char *sv_otel_exporter_otlp_traces_certificates;

extern char *sv_otel_exporter_otlp_traces_client_key;

extern char *sv_otel_exporter_otlp_traces_client_certificates;

extern unsigned long sv_otel_exporter_otlp_traces_min_tls;
extern unsigned long sv_otel_exporter_otlp_traces_max_tls;
extern char *sv_otel_exporter_otlp_traces_cipher;
extern char *sv_otel_exporter_otlp_traces_cipher_suite;

extern char *sv_otel_exporter_otlp_traces_headers;
extern char *sv_otel_exporter_otlp_traces_secret_headers;

const unsigned long OTLP_COMPRESSION_NONE = 0;
const unsigned long OTLP_COMPRESSION_GZIP = 1;
extern unsigned long sv_otel_exporter_otlp_traces_compression;

extern long sv_otel_exporter_otlp_traces_timeout;

extern long sv_otel_bsp_schedule_delay;
extern long sv_otel_bsp_max_queue_size;
extern long sv_otel_bsp_max_export_batch_size;

/* METRICS */

extern unsigned long sv_otel_exporter_otlp_metrics_protocol;

extern char *sv_otel_exporter_otlp_metrics_endpoint;

extern char *sv_otel_exporter_otlp_metrics_network_namespace;

extern char *sv_otel_exporter_otlp_metrics_certificates;

extern char *sv_otel_exporter_otlp_metrics_client_key;

extern char *sv_otel_exporter_otlp_metrics_client_certificates;

extern unsigned long sv_otel_exporter_otlp_metrics_min_tls;
extern unsigned long sv_otel_exporter_otlp_metrics_max_tls;
extern char *sv_otel_exporter_otlp_metrics_cipher;
extern char *sv_otel_exporter_otlp_metrics_cipher_suite;

extern char *sv_otel_exporter_otlp_metrics_headers;
extern char *sv_otel_exporter_otlp_metrics_secret_headers;

extern unsigned long sv_otel_exporter_otlp_metrics_compression;

extern long sv_otel_exporter_otlp_metrics_timeout;

extern unsigned long sv_metrics_reader_frequency_1;
extern unsigned long sv_metrics_reader_frequency_2;
extern unsigned long sv_metrics_reader_frequency_3;

/* LOGS */

extern unsigned long sv_otel_exporter_otlp_logs_protocol;

extern char *sv_otel_exporter_otlp_logs_endpoint;

extern char *sv_otel_exporter_otlp_logs_network_namespace;

extern char *sv_otel_exporter_otlp_logs_certificates;

extern char *sv_otel_exporter_otlp_logs_client_key;

extern char *sv_otel_exporter_otlp_logs_client_certificates;

extern unsigned long sv_otel_exporter_otlp_logs_min_tls;
extern unsigned long sv_otel_exporter_otlp_logs_max_tls;
extern char *sv_otel_exporter_otlp_logs_cipher;
extern char *sv_otel_exporter_otlp_logs_cipher_suite;

extern char *sv_otel_exporter_otlp_logs_headers;
extern char *sv_otel_exporter_otlp_logs_secret_headers;

extern unsigned long sv_otel_exporter_otlp_logs_compression;

extern long sv_otel_exporter_otlp_logs_timeout;

extern long sv_otel_blrp_schedule_delay;
extern long sv_otel_blrp_max_queue_size;
extern long sv_otel_blrp_max_export_batch_size;

extern char *sv_resource_provider;
extern char *sv_secret_provider;

int register_system_variables();
void unregister_system_variables();

}  // namespace telemetry

#endif /* TELEMETRY_SYSTEM_VARIABLES_H_INCLUDED */
