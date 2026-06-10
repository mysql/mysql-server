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

#include <opentelemetry/exporters/otlp/otlp_http_client.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/sdk/common/global_log_handler.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/semconv/incubating/service_attributes.h>

#include <opentelemetry/exporters/otlp/otlp_http_exporter.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_runtime_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_runtime_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_runtime_options.h>

#include "mysql_version.h"

#include "tm_global.h"
#include "tm_log.h"
#include "tm_otel_log.h"
#include "tm_otel_metric.h"
#include "tm_otel_trace.h"
#include "tm_psi.h"
#include "tm_setup_otel.h"
#include "tm_system_variables.h"
#include "tm_thread_instrumentation.h"

namespace telemetry {

// URI, not URL.
static const std::string mysql_schema_url =
    "http://mysql.com/telemetry/schema/1.0.0";

class MySQLOtelLogHandler
    : public opentelemetry::sdk::common::internal_log::LogHandler {
 public:
  void Handle(opentelemetry::sdk::common::internal_log::LogLevel level,
              const char *file, int line, const char *msg,
              const opentelemetry::sdk::common::AttributeMap
                  & /* attributes */) noexcept override {
    if (sv_otel_log_level == OTEL_LOG_LEVEL_SILENT) {
      return;
    }

    if (file == nullptr) {
      file = "<unknown>";
    }

    if (msg == nullptr) {
      msg = "<no msg>";
    }
    switch (level) {
      case opentelemetry::sdk::common::internal_log::LogLevel::None:
        break;
      case opentelemetry::sdk::common::internal_log::LogLevel::Error:
        log_error("%s: [OTEL] %s:%d %s", component_name, file, line, msg);
        break;
      case opentelemetry::sdk::common::internal_log::LogLevel::Warning:
        log_warning("%s: [OTEL] %s:%d %s", component_name, file, line, msg);
        break;
      case opentelemetry::sdk::common::internal_log::LogLevel::Info:
        log_info("%s: [OTEL] %s:%d %s", component_name, file, line, msg);
        break;
      case opentelemetry::sdk::common::internal_log::LogLevel::Debug:
        log_info("%s: [OTEL DEBUG] %s:%d %s", component_name, file, line, msg);
        break;
    }
  }
};

static opentelemetry::nostd::shared_ptr<
    opentelemetry::sdk::common::internal_log::LogHandler>
    g_log_handler(new MySQLOtelLogHandler());

void setup_internal_logger() {
  opentelemetry::sdk::common::internal_log::GlobalLogHandler::SetLogHandler(
      g_log_handler);
  setup_internal_logger_level(sv_otel_log_level);
}

void setup_internal_logger_level(unsigned long log_level) {
  opentelemetry::sdk::common::internal_log::LogLevel otel_log_level =
      opentelemetry::sdk::common::internal_log::LogLevel::Error;

  switch (log_level) {
    case OTEL_LOG_LEVEL_SILENT:
      otel_log_level = opentelemetry::sdk::common::internal_log::LogLevel::None;
      break;
    case OTEL_LOG_LEVEL_ERROR:
      otel_log_level =
          opentelemetry::sdk::common::internal_log::LogLevel::Error;
      break;
    case OTEL_LOG_LEVEL_WARNING:
      otel_log_level =
          opentelemetry::sdk::common::internal_log::LogLevel::Warning;
      break;
    case OTEL_LOG_LEVEL_INFO:
      otel_log_level = opentelemetry::sdk::common::internal_log::LogLevel::Info;
      break;
    case OTEL_LOG_LEVEL_DEBUG:
    default:
      otel_log_level =
          opentelemetry::sdk::common::internal_log::LogLevel::Debug;
      break;
  }

  opentelemetry::sdk::common::internal_log::GlobalLogHandler::SetLogLevel(
      otel_log_level);
}

/**
 * Convert a MySQL TLS option (as a system variable enum)
 * to an OpenTelemetry TLS option.
 * @param value Input TLS option
 * @param value_for_default Default value to enforce
 */
static std::string tls_option_string(unsigned long value,
                                     unsigned long value_for_default) {
  static std::string const option_default;
  static std::string const option_tls12("1.2");
  static std::string const option_tls13("1.3");

  if (value == OTLP_TLS_DEFAULT) {
    value = value_for_default;
  }

  switch (value) {
    case OTLP_TLS_DEFAULT:
      return option_default;

    case OTLP_TLS_12:
      return option_tls12;

    case OTLP_TLS_13:
      return option_tls13;
    default:
      assert(false);
      return option_default;
  }
}

static void merge_resource_attributes(
    opentelemetry::sdk::resource::ResourceAttributes &attributes,
    const char *attributes_string) {
  const std::string service_namespace = "mysql";
  attributes[opentelemetry::semconv::service::kServiceNamespace] =
      service_namespace;

  const std::string service_name = "mysqld";
  attributes[opentelemetry::semconv::service::kServiceName] = service_name;

  const std::string service_version = MYSQL_SERVER_VERSION;
  attributes[opentelemetry::semconv::service::kServiceVersion] =
      service_version;

  if (attributes_string != nullptr) {
    opentelemetry::common::KeyValueStringTokenizer tokenizer(attributes_string);

    bool valid_kv = false;
    opentelemetry::nostd::string_view key;
    opentelemetry::nostd::string_view value;

    while (tokenizer.next(valid_kv, key, value)) {
      if (valid_kv) {
        const std::string key2(key);
        const std::string value2(value);
        attributes[key2] = value2;
      } else {
        /* Do not print resource attributes, might contain sensitive data. */
        log_warning("%s: Found invalid key/value pair in resource attributes",
                    component_name);
      }
    }
  }
}

opentelemetry::sdk::resource::Resource setup_resource(
    opentelemetry::sdk::resource::ResourceAttributes &attributes,
    const char *resource_atrtributes_string) {
  merge_resource_attributes(attributes, resource_atrtributes_string);

  const opentelemetry::sdk::resource::Resource resource =
      opentelemetry::sdk::resource::Resource::Create(attributes,
                                                     mysql_schema_url);

  return resource;
}

static void parse_headers(opentelemetry::exporter::otlp::OtlpHeaders &result,
                          const char *input) {
  opentelemetry::common::KeyValueStringTokenizer tokenizer{input};
  opentelemetry::nostd::string_view header_key;
  opentelemetry::nostd::string_view header_value;
  bool header_valid = true;

  while (tokenizer.next(header_valid, header_key, header_value)) {
    if (header_valid) {
      std::string key(header_key);
      std::string value(header_value);
      result.emplace(std::make_pair(std::move(key), std::move(value)));
    } else {
      /* Do not print headers, might contain sensitive data (auth credentials)
       */
      log_warning("%s: Found invalid key/value pair in http headers",
                  component_name);
    }
  }
}

// ====================================================================

static std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>
setup_otel_otlp_http_exporter(bool json) {
  opentelemetry::exporter::otlp::OtlpHttpExporterOptions options;
  opentelemetry::exporter::otlp::OtlpHttpExporterRuntimeOptions runtime_options;

  if (sv_otel_exporter_otlp_traces_endpoint == nullptr) {
    return nullptr;
  }

  if (strlen(sv_otel_exporter_otlp_traces_endpoint) == 0) {
    return nullptr;
  }

  options.url = sv_otel_exporter_otlp_traces_endpoint;

  if (sv_otel_exporter_otlp_traces_certificates != nullptr) {
    if (strlen(sv_otel_exporter_otlp_traces_certificates) != 0) {
      options.ssl_ca_cert_path = sv_otel_exporter_otlp_traces_certificates;
    }
  }

  if (sv_otel_exporter_otlp_traces_client_key != nullptr) {
    if (strlen(sv_otel_exporter_otlp_traces_client_key) != 0) {
      options.ssl_client_key_path = sv_otel_exporter_otlp_traces_client_key;
    }
  }

  if (sv_otel_exporter_otlp_traces_client_certificates != nullptr) {
    if (strlen(sv_otel_exporter_otlp_traces_client_certificates) != 0) {
      options.ssl_client_cert_path =
          sv_otel_exporter_otlp_traces_client_certificates;
    }
  }

  options.ssl_min_tls =
      tls_option_string(sv_otel_exporter_otlp_traces_min_tls, OTLP_TLS_12);

  options.ssl_max_tls =
      tls_option_string(sv_otel_exporter_otlp_traces_max_tls, OTLP_TLS_DEFAULT);

  if (sv_otel_exporter_otlp_traces_cipher != nullptr) {
    if (strlen(sv_otel_exporter_otlp_traces_cipher) != 0) {
      options.ssl_cipher = sv_otel_exporter_otlp_traces_cipher;
    }
  }

  if (sv_otel_exporter_otlp_traces_cipher_suite != nullptr) {
    if (strlen(sv_otel_exporter_otlp_traces_cipher_suite) != 0) {
      options.ssl_cipher_suite = sv_otel_exporter_otlp_traces_cipher_suite;
    }
  }

  if (json) {
    options.content_type =
        opentelemetry::exporter::otlp::HttpRequestContentType::kJson;

    options.json_bytes_mapping =
        opentelemetry::exporter::otlp::JsonBytesMappingKind::kHexId;
  } else {
    options.content_type =
        opentelemetry::exporter::otlp::HttpRequestContentType::kBinary;
  }

  options.timeout =
      std::chrono::milliseconds(sv_otel_exporter_otlp_traces_timeout);

  if (sv_otel_exporter_otlp_traces_headers != nullptr) {
    parse_headers(options.http_headers, sv_otel_exporter_otlp_traces_headers);
  }

  if (!sensitive_otel_exporter_otlp_traces_secret_headers.empty()) {
    parse_headers(options.http_headers,
                  sensitive_otel_exporter_otlp_traces_secret_headers.c_str());
  }

  auto instrumentation =
      std::shared_ptr<opentelemetry::sdk::common::ThreadInstrumentation>(
          new MySQLThreadInstrumentation(g_otel_otlp_traces_exporter_thread_key,
                                         g_traces_network_namespace));
  runtime_options.thread_instrumentation = instrumentation;

  const char *endpoint = options.url.c_str();
  log_info("%s: Using OTLP HTTP exporter to endpoint <%s>", component_name,
           endpoint);
  return otel_create_otlp_http_exporter(options, runtime_options);
}

static std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>
setup_otel_otlp_exporter() {
  if (sv_otel_exporter_otlp_traces_protocol == OTLP_PROTOCOL_HTTP_PROTOBUF) {
    return setup_otel_otlp_http_exporter(false);
  }

  if (sv_otel_exporter_otlp_traces_protocol == OTLP_PROTOCOL_HTTP_JSON) {
    return setup_otel_otlp_http_exporter(true);
  }

  return nullptr;
}

static std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>
setup_otel_batch_processor(
    std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter) {
  opentelemetry::sdk::trace::BatchSpanProcessorOptions options;
  opentelemetry::sdk::trace::BatchSpanProcessorRuntimeOptions runtime_options;

  options.max_queue_size = sv_otel_bsp_max_queue_size;

  options.schedule_delay_millis =
      std::chrono::milliseconds(sv_otel_bsp_schedule_delay);

  options.max_export_batch_size = sv_otel_bsp_max_export_batch_size;

  auto instrumentation =
      std::shared_ptr<opentelemetry::sdk::common::ThreadInstrumentation>(
          new MySQLThreadInstrumentation(g_otel_bsp_thread_key,
                                         g_traces_network_namespace));
  runtime_options.thread_instrumentation = instrumentation;

  return otel_create_batch_processor(options, runtime_options,
                                     std::move(exporter));
}

std::shared_ptr<opentelemetry::sdk::trace::TracerProvider>
setup_otel_tracer_provider(
    const opentelemetry::sdk::resource::Resource &resource) {
  std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> provider;
  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter =
      setup_otel_otlp_exporter();

  if (exporter != nullptr) {
    std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> processor =
        setup_otel_batch_processor(std::move(exporter));

    provider = otel_create_tracer_provider(resource, std::move(processor));
  }

  return provider;
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>
setup_otel_tracer(
    const std::shared_ptr<opentelemetry::trace::TracerProvider> &provider) {
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> tracer;

  if (provider != nullptr) {
    tracer = provider->GetTracer("mysqltracer", "1.0.0");
    OTEL_INTERNAL_LOG_INFO("MySQL tracer created.");
  } else {
    OTEL_INTERNAL_LOG_INFO("MySQL tracer skipped (no provider).");
  }

  return tracer;
}

// ====================================================================

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
setup_otel_otlp_http_metric_exporter(bool json) {
  opentelemetry::exporter::otlp::OtlpHttpMetricExporterOptions options;
  opentelemetry::exporter::otlp::OtlpHttpMetricExporterRuntimeOptions
      runtime_options;

  if (sv_otel_exporter_otlp_metrics_endpoint == nullptr) {
    return nullptr;
  }

  if (strlen(sv_otel_exporter_otlp_metrics_endpoint) == 0) {
    return nullptr;
  }

  options.url = sv_otel_exporter_otlp_metrics_endpoint;

  if (sv_otel_exporter_otlp_metrics_certificates != nullptr) {
    if (strlen(sv_otel_exporter_otlp_metrics_certificates) != 0) {
      options.ssl_ca_cert_path = sv_otel_exporter_otlp_metrics_certificates;
    }
  }

  if (sv_otel_exporter_otlp_metrics_client_key != nullptr) {
    if (strlen(sv_otel_exporter_otlp_metrics_client_key) != 0) {
      options.ssl_client_key_path = sv_otel_exporter_otlp_metrics_client_key;
    }
  }

  if (sv_otel_exporter_otlp_metrics_client_certificates != nullptr) {
    if (strlen(sv_otel_exporter_otlp_metrics_client_certificates) != 0) {
      options.ssl_client_cert_path =
          sv_otel_exporter_otlp_metrics_client_certificates;
    }
  }

  options.ssl_min_tls =
      tls_option_string(sv_otel_exporter_otlp_metrics_min_tls, OTLP_TLS_12);

  options.ssl_max_tls = tls_option_string(sv_otel_exporter_otlp_metrics_max_tls,
                                          OTLP_TLS_DEFAULT);

  if (sv_otel_exporter_otlp_metrics_cipher != nullptr) {
    if (strlen(sv_otel_exporter_otlp_metrics_cipher) != 0) {
      options.ssl_cipher = sv_otel_exporter_otlp_metrics_cipher;
    }
  }

  if (sv_otel_exporter_otlp_metrics_cipher_suite != nullptr) {
    if (strlen(sv_otel_exporter_otlp_metrics_cipher_suite) != 0) {
      options.ssl_cipher_suite = sv_otel_exporter_otlp_metrics_cipher_suite;
    }
  }

  if (json) {
    options.content_type =
        opentelemetry::exporter::otlp::HttpRequestContentType::kJson;

    options.json_bytes_mapping =
        opentelemetry::exporter::otlp::JsonBytesMappingKind::kHexId;
  } else {
    options.content_type =
        opentelemetry::exporter::otlp::HttpRequestContentType::kBinary;
  }

  options.timeout =
      std::chrono::milliseconds(sv_otel_exporter_otlp_metrics_timeout);

  if (sv_otel_exporter_otlp_metrics_headers != nullptr) {
    parse_headers(options.http_headers, sv_otel_exporter_otlp_metrics_headers);
  }

  if (!sensitive_otel_exporter_otlp_metrics_secret_headers.empty()) {
    parse_headers(options.http_headers,
                  sensitive_otel_exporter_otlp_metrics_secret_headers.c_str());
  }

  auto instrumentation =
      std::shared_ptr<opentelemetry::sdk::common::ThreadInstrumentation>(
          new MySQLThreadInstrumentation(
              g_otel_otlp_metrics_exporter_thread_key,
              g_metrics_network_namespace));
  runtime_options.thread_instrumentation = instrumentation;

  const char *endpoint = options.url.c_str();
  log_info("Using OTLP HTTP metric exporter to endpoint <%s>", endpoint);
  return otel_create_otlp_http_metric_exporter(options, runtime_options);
}

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
setup_otel_otlp_metrics_exporter() {
  if (sv_otel_exporter_otlp_metrics_protocol == OTLP_PROTOCOL_HTTP_PROTOBUF) {
    return setup_otel_otlp_http_metric_exporter(false);
  }

  if (sv_otel_exporter_otlp_metrics_protocol == OTLP_PROTOCOL_HTTP_JSON) {
    return setup_otel_otlp_http_metric_exporter(true);
  }

  return nullptr;
}

std::unique_ptr<opentelemetry::sdk::metrics::MetricReader>
setup_otel_metric_reader(
    std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter,
    const opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
        &options,
    const opentelemetry::sdk::metrics::
        PeriodicExportingMetricReaderRuntimeOptions &runtime_options) {
  return otel_create_metric_reader(std::move(exporter), options,
                                   runtime_options);
}

std::unique_ptr<opentelemetry::metrics::MeterProvider>
setup_otel_meter_provider(
    const opentelemetry::sdk::resource::Resource &resource,
    const opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
        &options,
    const opentelemetry::sdk::metrics::
        PeriodicExportingMetricReaderRuntimeOptions &runtime_options) {
  std::unique_ptr<opentelemetry::metrics::MeterProvider> provider;

  std::unique_ptr<opentelemetry::sdk::metrics::ViewRegistry> views =
      otel_create_metric_view_registry();

  std::unique_ptr<opentelemetry::sdk::metrics::MeterContext> context =
      otel_create_metric_meter_context(std::move(views), resource);

  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter =
      setup_otel_otlp_metrics_exporter();

  if (exporter != nullptr) {
    std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> reader =
        setup_otel_metric_reader(std::move(exporter), options, runtime_options);

    context->AddMetricReader(std::move(reader));

    provider = otel_create_meter_provider(std::move(context));
  }

  return provider;
}

void setup_otel_meter_providers(
    const opentelemetry::sdk::resource::Resource &resource) {
  std::unique_ptr<opentelemetry::metrics::MeterProvider> provider;
  opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions options;
  opentelemetry::sdk::metrics::PeriodicExportingMetricReaderRuntimeOptions
      runtime_options;

  std::vector<ulong> frequencies;
  ulong timeout;
  /* Cap timeout at 5 minutes. */
  static const ulong max_timeout = 300000;

  frequencies.push_back(sv_metrics_reader_frequency_1);

  if (sv_metrics_reader_frequency_2 != 0) {
    frequencies.push_back(sv_metrics_reader_frequency_2);
  }

  if (sv_metrics_reader_frequency_3 != 0) {
    frequencies.push_back(sv_metrics_reader_frequency_3);
  }

  /*
    Accept frequency_1, frequency_2 and frequency_3
    in any order, with duplicates.
  */
  std::sort(frequencies.begin(), frequencies.end());
  auto last = std::unique(frequencies.begin(), frequencies.end());
  frequencies.erase(last, frequencies.end());

  auto periodic_instrumentation =
      std::shared_ptr<opentelemetry::sdk::common::ThreadInstrumentation>(
          new MySQLThreadInstrumentation(
              g_otel_metric_periodic_reader_thread_key,
              g_metrics_network_namespace));
  runtime_options.periodic_thread_instrumentation = periodic_instrumentation;

  for (ulong const f : frequencies) {
    /* Export frequency in seconds, converted to milliseconds. */
    options.export_interval_millis = std::chrono::milliseconds(f * 1000);
    /* Timeout = 80 % of export, capped to 5 minutes. */
    timeout = std::min(f * 800, max_timeout);
    options.export_timeout_millis = std::chrono::milliseconds(timeout);

    provider = setup_otel_meter_provider(resource, options, runtime_options);
    g_all_meter_providers.add(f, std::move(provider));
  }
}

void cleanup_otel_meter_providers() { g_all_meter_providers.reset(); }

// ====================================================================

static std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>
setup_otel_otlp_http_log_exporter(bool json) {
  opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterOptions options;
  opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterRuntimeOptions
      runtime_options;

  if (sv_otel_exporter_otlp_logs_endpoint == nullptr) {
    return nullptr;
  }

  if (strlen(sv_otel_exporter_otlp_logs_endpoint) == 0) {
    return nullptr;
  }

  options.url = sv_otel_exporter_otlp_logs_endpoint;

  if (sv_otel_exporter_otlp_logs_certificates != nullptr) {
    if (strlen(sv_otel_exporter_otlp_logs_certificates) != 0) {
      options.ssl_ca_cert_path = sv_otel_exporter_otlp_logs_certificates;
    }
  }

  if (sv_otel_exporter_otlp_logs_client_key != nullptr) {
    if (strlen(sv_otel_exporter_otlp_logs_client_key) != 0) {
      options.ssl_client_key_path = sv_otel_exporter_otlp_logs_client_key;
    }
  }

  if (sv_otel_exporter_otlp_logs_client_certificates != nullptr) {
    if (strlen(sv_otel_exporter_otlp_logs_client_certificates) != 0) {
      options.ssl_client_cert_path =
          sv_otel_exporter_otlp_logs_client_certificates;
    }
  }

  options.ssl_min_tls =
      tls_option_string(sv_otel_exporter_otlp_logs_min_tls, OTLP_TLS_12);

  options.ssl_max_tls =
      tls_option_string(sv_otel_exporter_otlp_logs_max_tls, OTLP_TLS_DEFAULT);

  if (sv_otel_exporter_otlp_logs_cipher != nullptr) {
    if (strlen(sv_otel_exporter_otlp_logs_cipher) != 0) {
      options.ssl_cipher = sv_otel_exporter_otlp_logs_cipher;
    }
  }

  if (sv_otel_exporter_otlp_logs_cipher_suite != nullptr) {
    if (strlen(sv_otel_exporter_otlp_logs_cipher_suite) != 0) {
      options.ssl_cipher_suite = sv_otel_exporter_otlp_logs_cipher_suite;
    }
  }

  if (json) {
    options.content_type =
        opentelemetry::exporter::otlp::HttpRequestContentType::kJson;

    options.json_bytes_mapping =
        opentelemetry::exporter::otlp::JsonBytesMappingKind::kHexId;
  } else {
    options.content_type =
        opentelemetry::exporter::otlp::HttpRequestContentType::kBinary;
  }

  options.timeout =
      std::chrono::milliseconds(sv_otel_exporter_otlp_logs_timeout);

  if (sv_otel_exporter_otlp_logs_headers != nullptr) {
    parse_headers(options.http_headers, sv_otel_exporter_otlp_logs_headers);
  }

  if (!sensitive_otel_exporter_otlp_logs_secret_headers.empty()) {
    parse_headers(options.http_headers,
                  sensitive_otel_exporter_otlp_logs_secret_headers.c_str());
  }

  auto instrumentation =
      std::shared_ptr<opentelemetry::sdk::common::ThreadInstrumentation>(
          new MySQLThreadInstrumentation(g_otel_otlp_logs_exporter_thread_key,
                                         g_logs_network_namespace));
  runtime_options.thread_instrumentation = instrumentation;

  const char *endpoint = options.url.c_str();
  log_info("%s: Using OTLP HTTP log exporter to endpoint <%s>", component_name,
           endpoint);
  return otel_create_otlp_http_log_exporter(options, runtime_options);
}

static std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>
setup_otel_otlp_log_exporter() {
  if (sv_otel_exporter_otlp_logs_protocol == OTLP_PROTOCOL_HTTP_PROTOBUF) {
    return setup_otel_otlp_http_log_exporter(false);
  }

  if (sv_otel_exporter_otlp_logs_protocol == OTLP_PROTOCOL_HTTP_JSON) {
    return setup_otel_otlp_http_log_exporter(true);
  }

  return nullptr;
}

static std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor>
setup_otel_batch_log_processor(
    std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> exporter) {
  opentelemetry::sdk::logs::BatchLogRecordProcessorOptions options;
  opentelemetry::sdk::logs::BatchLogRecordProcessorRuntimeOptions
      runtime_options;

  options.max_queue_size = sv_otel_blrp_max_queue_size;

  options.schedule_delay_millis =
      std::chrono::milliseconds(sv_otel_blrp_schedule_delay);

  options.max_export_batch_size = sv_otel_blrp_max_export_batch_size;

  auto instrumentation =
      std::shared_ptr<opentelemetry::sdk::common::ThreadInstrumentation>(
          new MySQLThreadInstrumentation(g_otel_blrp_thread_key,
                                         g_logs_network_namespace));
  runtime_options.thread_instrumentation = instrumentation;

  return otel_create_batch_log_processor(options, runtime_options,
                                         std::move(exporter));
}

opentelemetry::nostd::shared_ptr<opentelemetry::sdk::logs::LoggerProvider>
setup_otel_logger_provider(
    const opentelemetry::sdk::resource::Resource &resource) {
  opentelemetry::nostd::shared_ptr<opentelemetry::sdk::logs::LoggerProvider>
      provider;

  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> exporter =
      setup_otel_otlp_log_exporter();

  if (exporter != nullptr) {
    std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor> processor =
        setup_otel_batch_log_processor(std::move(exporter));

    provider = otel_create_logger_provider(resource, std::move(processor));
  }

  return provider;
}

opentelemetry::nostd::shared_ptr<opentelemetry::logs::Logger> setup_otel_logger(
    const opentelemetry::nostd::shared_ptr<
        opentelemetry::sdk::logs::LoggerProvider> &provider) {
  opentelemetry::nostd::shared_ptr<opentelemetry::logs::Logger> logger;

  if (provider != nullptr) {
    logger = provider->GetLogger("mysqllogger", "1.0.0");
    OTEL_INTERNAL_LOG_INFO("MySQL logger created.");
  } else {
    OTEL_INTERNAL_LOG_INFO("MySQL logger skipped (no provider).");
  }

  return logger;
}

}  // namespace telemetry
