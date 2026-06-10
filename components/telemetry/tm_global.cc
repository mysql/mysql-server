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

#include "tm_global.h"
#include "tm_ns.h"

namespace telemetry {

std::string sensitive_otel_exporter_otlp_traces_secret_headers;
std::string sensitive_otel_exporter_otlp_metrics_secret_headers;
std::string sensitive_otel_exporter_otlp_logs_secret_headers;

int g_traces_network_namespace = NO_FD;
int g_metrics_network_namespace = NO_FD;
int g_logs_network_namespace = NO_FD;

std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> g_tracer_provider(
    nullptr);

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> g_tracer(
    nullptr);

opentelemetry::nostd::shared_ptr<opentelemetry::sdk::logs::LoggerProvider>
    g_logger_provider;

opentelemetry::nostd::shared_ptr<opentelemetry::logs::Logger> g_logger;

std::atomic_int64_t g_session_count(0LL);

std::atomic_bool g_shutting_down(false);
std::atomic_int64_t g_sessions_closed(0LL);

std::atomic_int64_t g_run_level(RUN_LEVEL_BOOT);

mysql_mutex_t g_session_notify_mutex;
mysql_cond_t g_session_notify_cond;
mysql_mutex_t g_all_meters_mutex;
mysql_mutex_t g_option_usage_mutex;

mysql_mutex_t g_install_wait_lock;
mysql_mutex_t g_install_completed_lock;
mysql_cond_t g_install_wait_cond;
mysql_cond_t g_install_completed_cond;

#ifdef SINGLE_METER_PROVIDER
opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>
    g_meter_provider(nullptr);
#else
MySQLMeterProviders g_all_meter_providers;
#endif

void to_timespec(const std::chrono::time_point<std::chrono::system_clock> &from,
                 struct timespec &to) {
  auto secs = std::chrono::time_point_cast<std::chrono::seconds>(from);
  auto ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(from) -
            std::chrono::time_point_cast<std::chrono::nanoseconds>(secs);

  to.tv_sec = secs.time_since_epoch().count();
  to.tv_nsec = ns.count();
}

}  // namespace telemetry
