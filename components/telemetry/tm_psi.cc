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

#include "tm_psi.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "tm_global.h"
#include "tm_log.h"
#include "tm_required_services.h"

namespace telemetry {

PSI_mutex_key g_notify_mutex_key = PSI_NOT_INSTRUMENTED;
PSI_mutex_key g_metric_mutex_key = PSI_NOT_INSTRUMENTED;
PSI_mutex_key g_meter_mutex_key = PSI_NOT_INSTRUMENTED;
PSI_mutex_key g_option_usage_mutex_key = PSI_NOT_INSTRUMENTED;

static PSI_mutex_info all_mutexes[] = {
    {&g_notify_mutex_key, "session_notify", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
    {&g_metric_mutex_key, "MySQLMetric::m_lock", 0, 0, PSI_DOCUMENT_ME},
    {&g_meter_mutex_key, "MySQLMeter", PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME},
    {&g_option_usage_mutex_key, "option_usage", PSI_FLAG_SINGLETON, 0,
     "Serialize access to option_usage"},
    {&g_metric_mutex_key, "MySQLMetric::m_lock", 0, 0, PSI_DOCUMENT_ME},
};

PSI_cond_key g_notify_cond_key = PSI_NOT_INSTRUMENTED;

static PSI_cond_info all_conds[] = {
    {&g_notify_cond_key, "session_notify", PSI_FLAG_SINGLETON, 0,
     PSI_DOCUMENT_ME},
};

PSI_stage_info g_session_stage = {0, "session", PSI_FLAG_STAGE_PROGRESS,
                                  PSI_DOCUMENT_ME};

static PSI_stage_info *all_stages[] = {&g_session_stage};

PSI_thread_key g_otel_bsp_thread_key = PSI_NOT_INSTRUMENTED;
PSI_thread_key g_otel_metric_periodic_reader_thread_key = PSI_NOT_INSTRUMENTED;
PSI_thread_key g_otel_blrp_thread_key = PSI_NOT_INSTRUMENTED;
PSI_thread_key g_otel_otlp_traces_exporter_thread_key = PSI_NOT_INSTRUMENTED;
PSI_thread_key g_otel_otlp_metrics_exporter_thread_key = PSI_NOT_INSTRUMENTED;
PSI_thread_key g_otel_otlp_logs_exporter_thread_key = PSI_NOT_INSTRUMENTED;

static PSI_thread_info all_threads[] = {
    {&g_otel_bsp_thread_key, "otel_bsp", "otel_bsp",
     PSI_FLAG_THREAD_SYSTEM | PSI_FLAG_AUTO_SEQNUM, 0,
     "OpenTelemetry batch record processor thread"},
    {&g_otel_metric_periodic_reader_thread_key, "otel_pmr", "otel_pmr",
     PSI_FLAG_THREAD_SYSTEM | PSI_FLAG_AUTO_SEQNUM, 0,
     "OpenTelemetry metric periodic reader thread"},
    {&g_otel_blrp_thread_key, "otel_blrp", "otel_blrp",
     PSI_FLAG_THREAD_SYSTEM | PSI_FLAG_AUTO_SEQNUM, 0,
     "OpenTelemetry batch log record processor thread"},
    {&g_otel_otlp_traces_exporter_thread_key, "otlp_traces", "otlp_traces",
     PSI_FLAG_THREAD_SYSTEM | PSI_FLAG_NO_SEQNUM, 0,
     "OpenTelemetry OTLP HTTP span exporter thread"},
    {&g_otel_otlp_metrics_exporter_thread_key, "otlp_metrics", "otlp_metrics",
     PSI_FLAG_THREAD_SYSTEM | PSI_FLAG_NO_SEQNUM, 0,
     "OpenTelemetry OTLP HTTP metrics exporter thread"},
    {&g_otel_otlp_logs_exporter_thread_key, "otlp_logs", "otlp_logs",
     PSI_FLAG_THREAD_SYSTEM | PSI_FLAG_NO_SEQNUM, 0,
     "OpenTelemetry OTLP HTTP log record exporter thread"}};

void register_performance_schema() {
  const char *component = "telemetry";

  mutex_srv->register_info(component, all_mutexes, std::size(all_mutexes));

  cond_srv->register_info(component, all_conds, std::size(all_conds));

  stage_srv->register_stage(component, all_stages, std::size(all_stages));

  thread_srv->register_thread(component, all_threads, std::size(all_threads));
}

void init_mutexes() {
  mutex_srv->init(g_notify_mutex_key, &g_session_notify_mutex, nullptr,
                  __FILE__, __LINE__);
  cond_srv->init(g_notify_cond_key, &g_session_notify_cond, __FILE__, __LINE__);
  mutex_srv->init(g_meter_mutex_key, &g_all_meters_mutex, nullptr, __FILE__,
                  __LINE__);
  mutex_srv->init(g_option_usage_mutex_key, &g_option_usage_mutex, nullptr,
                  __FILE__, __LINE__);
}

void cleanup_mutexes() {
  mutex_srv->destroy(&g_session_notify_mutex, __FILE__, __LINE__);
  cond_srv->destroy(&g_session_notify_cond, __FILE__, __LINE__);
  mutex_srv->destroy(&g_all_meters_mutex, __FILE__, __LINE__);
  mutex_srv->destroy(&g_option_usage_mutex, __FILE__, __LINE__);
}

}  // namespace telemetry
