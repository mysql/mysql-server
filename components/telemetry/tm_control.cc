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

#include "tm_control.h"
#include "tm_global.h"
#include "tm_notification.h"

#include "opentelemetry/logs/log_record.h"

namespace telemetry {

const char *k_reason_startup = "STARTUP";
const char *k_reason_configuration = "CONFIGURATION";
const char *k_reason_shutdown = "SHUTDOWN";

static const opentelemetry::nostd::string_view k_control("control");
static const opentelemetry::nostd::string_view k_traces_enabled(
    "mysql.traces_enabled");
static const opentelemetry::nostd::string_view k_metrics_enabled(
    "mysql.metrics_enabled");
static const opentelemetry::nostd::string_view k_logs_enabled(
    "mysql.logs_enabled");
static const opentelemetry::nostd::string_view k_details("mysql.details");

static const opentelemetry::nostd::string_view k_event_name("mysql.event_name");

static const opentelemetry::nostd::string_view k_log_message(
    "emit_control_log");
static const opentelemetry::nostd::string_view k_logger_name(
    "telemetry_component");

void emit_control_span(bool traces_enabled, bool metrics_enabled,
                       bool logs_enabled, const char *details) {
  auto tracer = g_tracer;

  if (tracer != nullptr) {
    opentelemetry::trace::StartSpanOptions options;
    options.kind = opentelemetry::trace::SpanKind::kInternal;

    const opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>
        otel_span = tracer->StartSpan(k_control, options);

    otel_span->SetAttribute(k_traces_enabled, traces_enabled);
    otel_span->SetAttribute(k_metrics_enabled, metrics_enabled);
    otel_span->SetAttribute(k_logs_enabled, logs_enabled);
    otel_span->SetAttribute(k_details, details);

    otel_span->End();
  }
}

void emit_control_log(bool traces_enabled, bool metrics_enabled,
                      bool logs_enabled, const char *details) {
  auto logger = g_logger;

  if (logger != nullptr) {
    opentelemetry::nostd::unique_ptr<opentelemetry::logs::LogRecord> record =
        logger->CreateLogRecord();

    const auto ts = std::chrono::system_clock::now();
    record->SetTimestamp(ts);

    record->SetSeverity(opentelemetry::logs::Severity::kInfo);

    opentelemetry::common::AttributeValue const otel_body(k_log_message);
    record->SetBody(otel_body);

    record->SetAttribute(k_event_name, k_logger_name);

    record->SetAttribute(k_traces_enabled, traces_enabled);
    record->SetAttribute(k_metrics_enabled, metrics_enabled);
    record->SetAttribute(k_logs_enabled, logs_enabled);
    record->SetAttribute(k_details, details);

    set_log_record_trace(record.get());

    logger->EmitLogRecord(std::move(record));
  }
}

}  // namespace telemetry
