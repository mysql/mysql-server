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

#ifndef TELEMETRY_GLOBAL_H_INCLUDED
#define TELEMETRY_GLOBAL_H_INCLUDED

#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/sdk/logs/logger.h>
#include <opentelemetry/sdk/logs/logger_provider.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/trace/tracer.h>
#include <opentelemetry/trace/tracer_provider.h>

#include <mysql/components/services/mysql_cond_service.h>
#include <mysql/components/services/mysql_mutex_service.h>

#include <atomic>
#include <chrono>

#include "tm_mysql_metric.h"

namespace telemetry {

extern std::string sensitive_otel_exporter_otlp_traces_secret_headers;
extern std::string sensitive_otel_exporter_otlp_metrics_secret_headers;
extern std::string sensitive_otel_exporter_otlp_logs_secret_headers;

extern int g_traces_network_namespace;
extern int g_metrics_network_namespace;
extern int g_logs_network_namespace;

extern std::shared_ptr<opentelemetry::sdk::trace::TracerProvider>
    g_tracer_provider;

extern opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> g_tracer;

extern opentelemetry::nostd::shared_ptr<
    opentelemetry::sdk::logs::LoggerProvider>
    g_logger_provider;

extern opentelemetry::nostd::shared_ptr<opentelemetry::logs::Logger> g_logger;

extern std::atomic_int64_t g_session_count;

extern std::atomic_bool g_shutting_down;
extern std::atomic_int64_t g_sessions_closed;
extern std::atomic_int64_t g_run_level;

constexpr int RUN_LEVEL_BOOT = 0;
constexpr int RUN_LEVEL_INSTALL = 1;
constexpr int RUN_LEVEL_DETECT_RESOURCE = 2;
constexpr int RUN_LEVEL_DECODE_SECRET = 3;
constexpr int RUN_LEVEL_CONFIGURE = 4;
constexpr int RUN_LEVEL_READY = 5;
constexpr int RUN_LEVEL_FAILED = 6;
constexpr int RUN_LEVEL_UNINSTALL = 7;

extern mysql_mutex_t g_session_notify_mutex;
extern mysql_cond_t g_session_notify_cond;
extern mysql_mutex_t g_all_meters_mutex;
extern mysql_mutex_t g_option_usage_mutex;

extern mysql_mutex_t g_install_wait_lock;
extern mysql_mutex_t g_install_completed_lock;
extern mysql_cond_t g_install_wait_cond;
extern mysql_cond_t g_install_completed_cond;

// #define SINGLE_METER_PROVIDER

#ifdef SINGLE_METER_PROVIDER

/*
  Not fully supported in opentelemetry-cpp,
  when multiple export frequencies are defined.

  See below.
*/

extern opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>
    g_meter_provider;
#else

/*
  Work around while waiting for this fix:

  Metrics SDK: allow metric readers to filter Meters during Collect()

  https://github.com/open-telemetry/opentelemetry-specification/issues/3617
*/

extern MySQLMeterProviders g_all_meter_providers;
#endif

void to_timespec(const std::chrono::time_point<std::chrono::system_clock> &from,
                 struct timespec &to);

}  // namespace telemetry

#endif /* TELEMETRY_GLOBAL_H_INCLUDED */
