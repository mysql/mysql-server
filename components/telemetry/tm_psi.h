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

#ifndef TELEMETRY_PSI_H_INCLUDED
#define TELEMETRY_PSI_H_INCLUDED

#include <mysql/components/services/mysql_cond_service.h>
#include <mysql/components/services/mysql_mutex_service.h>
#include <mysql/components/services/psi_stage_service.h>
#include <mysql/components/services/psi_thread_service.h>

namespace telemetry {

extern PSI_mutex_key g_notify_mutex_key;
extern PSI_mutex_key g_metric_mutex_key;
extern PSI_mutex_key g_meter_mutex_key;
extern PSI_mutex_key g_option_usage_mutex_key;
extern PSI_cond_key g_notify_cond_key;
extern PSI_stage_info g_session_stage;

extern PSI_thread_key g_otel_bsp_thread_key;
extern PSI_thread_key g_otel_metric_periodic_reader_thread_key;
extern PSI_thread_key g_otel_blrp_thread_key;
extern PSI_thread_key g_otel_otlp_traces_exporter_thread_key;
extern PSI_thread_key g_otel_otlp_metrics_exporter_thread_key;
extern PSI_thread_key g_otel_otlp_logs_exporter_thread_key;

void register_performance_schema();
void init_mutexes();
void cleanup_mutexes();

}  // namespace telemetry

#endif /* TELEMETRY_PSI_H_INCLUDED */
