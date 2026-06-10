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

#ifdef _WIN32
#include <process.h>
#else
#include <sys/types.h>  // pid_t
#include <unistd.h>
#endif

#include <string>
#include <string_view>

#include <my_hostname.h>
#include <mysql.h>
#include <mysql/client_plugin.h>
#include <mysql/plugin_client_telemetry.h>

/* API */

#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/semconv/incubating/service_attributes.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/trace/span_metadata.h>
#include <opentelemetry/trace/tracer.h>
#include <opentelemetry/trace/tracer_provider.h>

/* SDK */

#include <opentelemetry/sdk/common/global_log_handler.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/trace/batch_span_processor_factory.h>
#include <opentelemetry/sdk/trace/batch_span_processor_options.h>
#include <opentelemetry/sdk/trace/exporter.h>
#include <opentelemetry/sdk/trace/processor.h>
#include <opentelemetry/sdk/trace/tracer.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>

/* Exporters */

#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>

#include "tm_client_options.h"

static const std::string mysql_schema_url =
    "http://mysql.com/telemetry/schema/1.0.0";

static std::unique_ptr<opentelemetry::sdk::trace::TracerProvider>
    g_tracer_provider;

static opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> g_tracer;

static opentelemetry::sdk::resource::ResourceAttributes
client_setup_resource_attributes() {
  opentelemetry::sdk::resource::ResourceAttributes attributes;

  std::string service_namespace = "mysql";
  attributes[opentelemetry::semconv::service::kServiceNamespace] =
      service_namespace;

  std::string service_name = "mysql";
  attributes[opentelemetry::semconv::service::kServiceName] = service_name;

  char host_name[HOSTNAME_LENGTH + 1];

  gethostname(host_name, sizeof(host_name));
  host_name[sizeof(host_name) - 1] = '\0';

  std::string service_instance_id;
  service_instance_id.append(host_name);
  service_instance_id.append(":");
  service_instance_id.append(std::to_string(getpid()));
  attributes[opentelemetry::semconv::service::kServiceInstanceId] =
      service_instance_id;

  std::string service_version = MYSQL_SERVER_VERSION;
  attributes[opentelemetry::semconv::service::kServiceVersion] =
      service_version;

  if (opt_otel_resource_attributes != nullptr) {
    opentelemetry::common::KeyValueStringTokenizer tokenizer(
        opt_otel_resource_attributes);

    bool valid_kv = false;
    opentelemetry::nostd::string_view key;
    opentelemetry::nostd::string_view value;

    while (tokenizer.next(valid_kv, key, value)) {
      if (valid_kv) {
        const std::string key2(key);
        const std::string value2(value);
        attributes[key2] = value2;
      } else {
        (void)fprintf(stderr,
                      "telemetry_client: Found invalid key/value pair in "
                      "resource attributes.\n");
      }
    }
  }

  return attributes;
}

static opentelemetry::sdk::resource::Resource client_setup_resource() {
  const opentelemetry::sdk::resource::ResourceAttributes attributes =
      client_setup_resource_attributes();

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
      (void)fprintf(
          stderr,
          "telemetry_client: Found invalid key/value pair in http headers.\n");
    }
  }
}

/**
 * Convert a MySQL TLS option (as a client plugin option enum)
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

static std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>
client_setup_otel_otlp_http_exporter(bool json) {
  opentelemetry::exporter::otlp::OtlpHttpExporterOptions options;

  if (opt_otel_exporter_otlp_traces_endpoint != nullptr) {
    if (strlen(opt_otel_exporter_otlp_traces_endpoint) != 0) {
      options.url = opt_otel_exporter_otlp_traces_endpoint;
    }
  }

  if (opt_otel_exporter_otlp_traces_certificates != nullptr) {
    if (strlen(opt_otel_exporter_otlp_traces_certificates) != 0) {
      options.ssl_ca_cert_path = opt_otel_exporter_otlp_traces_certificates;
    }
  }

  if (opt_otel_exporter_otlp_traces_client_key != nullptr) {
    if (strlen(opt_otel_exporter_otlp_traces_client_key) != 0) {
      options.ssl_client_key_path = opt_otel_exporter_otlp_traces_client_key;
    }
  }

  if (opt_otel_exporter_otlp_traces_client_certificates != nullptr) {
    if (strlen(opt_otel_exporter_otlp_traces_client_certificates) != 0) {
      options.ssl_client_cert_path =
          opt_otel_exporter_otlp_traces_client_certificates;
    }
  }

  options.ssl_min_tls =
      tls_option_string(opt_otel_exporter_otlp_traces_min_tls, OTLP_TLS_12);

  options.ssl_max_tls = tls_option_string(opt_otel_exporter_otlp_traces_max_tls,
                                          OTLP_TLS_DEFAULT);

  if (opt_otel_exporter_otlp_traces_cipher != nullptr) {
    if (strlen(opt_otel_exporter_otlp_traces_cipher) != 0) {
      options.ssl_cipher = opt_otel_exporter_otlp_traces_cipher;
    }
  }

  if (opt_otel_exporter_otlp_traces_cipher_suite != nullptr) {
    if (strlen(opt_otel_exporter_otlp_traces_cipher_suite) != 0) {
      options.ssl_cipher_suite = opt_otel_exporter_otlp_traces_cipher_suite;
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
      std::chrono::milliseconds(opt_otel_exporter_otlp_traces_timeout);

  if (opt_otel_exporter_otlp_traces_headers != nullptr) {
    parse_headers(options.http_headers, opt_otel_exporter_otlp_traces_headers);
  }

  const char *endpoint = options.url.c_str();
  (void)fprintf(stderr,
                "telemetry_client: Using OTLP HTTP exporter to endpoint <%s>\n",
                endpoint);

  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter(
      opentelemetry::exporter::otlp::OtlpHttpExporterFactory::Create(options));

  return exporter;
}

static std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>
client_setup_otel_otlp_exporter() {
  if (opt_otel_exporter_otlp_traces_protocol == OTLP_PROTOCOL_HTTP_PROTOBUF) {
    return client_setup_otel_otlp_http_exporter(false);
  }

  if (opt_otel_exporter_otlp_traces_protocol == OTLP_PROTOCOL_HTTP_JSON) {
    return client_setup_otel_otlp_http_exporter(true);
  }

  return nullptr;
}

static std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>
client_setup_otel_batch_processor(
    std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter) {
  opentelemetry::sdk::trace::BatchSpanProcessorOptions options;

  options.max_queue_size = opt_otel_bsp_max_queue_size;

  options.schedule_delay_millis =
      std::chrono::milliseconds(opt_otel_bsp_schedule_delay);

  options.max_export_batch_size = opt_otel_bsp_max_export_batch_size;

  std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> processor(
      opentelemetry::sdk::trace::BatchSpanProcessorFactory::Create(
          std::move(exporter), options));

  return processor;
}

static std::unique_ptr<opentelemetry::sdk::trace::TracerProvider>
client_setup_otel_tracer_provider(
    const opentelemetry::sdk::resource::Resource &resource) {
  std::unique_ptr<opentelemetry::sdk::trace::TracerProvider> provider(nullptr);

  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter =
      client_setup_otel_otlp_exporter();

  if (exporter != nullptr) {
    std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> processor =
        client_setup_otel_batch_processor(std::move(exporter));

    provider = opentelemetry::sdk::trace::TracerProviderFactory::Create(
        std::move(processor), resource);
  }

  return provider;
}

static opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>
client_setup_otel_tracer(
    const std::unique_ptr<opentelemetry::sdk::trace::TracerProvider>
        &provider) {
  return provider->GetTracer("mysqltracer", "1.0.0");
}

static void client_setup_otel() {
  if (opt_trace_enabled) {
    auto resource = client_setup_resource();

    g_tracer_provider = client_setup_otel_tracer_provider(resource);

    if (g_tracer_provider != nullptr) {
      g_tracer = client_setup_otel_tracer(g_tracer_provider);
    }
  } else {
    g_tracer_provider = nullptr;
    g_tracer = nullptr;
  }
}

static void client_cleanup_otel() {
  /* Work around, nostd::shared_ptr::reset(nullptr) not available in otel-cpp.
   */
  const opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>
      no_tracer;

  g_tracer = no_tracer;

  if (g_tracer_provider != nullptr) {
    std::chrono::microseconds const flush_timeout(1000000);
    g_tracer_provider->ForceFlush(flush_timeout);
    g_tracer_provider->Shutdown();
  }
}

class MySQLClientOtelLogHandler
    : public opentelemetry::sdk::common::internal_log::LogHandler {
 public:
  void Handle(opentelemetry::sdk::common::internal_log::LogLevel level,
              const char *file, int line, const char *msg,
              const opentelemetry::sdk::common::AttributeMap
                  & /* attributes */) noexcept override {
    if (opt_otel_log_level == OTEL_LOG_LEVEL_SILENT) {
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
        (void)fprintf(stderr, "telemetry_client: [OTEL] %s:%d %s\n", file, line,
                      msg);
        break;
      case opentelemetry::sdk::common::internal_log::LogLevel::Warning:
        (void)fprintf(stderr, "telemetry_client: [OTEL] %s:%d %s\n", file, line,
                      msg);
        break;
      case opentelemetry::sdk::common::internal_log::LogLevel::Info:
        (void)fprintf(stderr, "telemetry_client: [OTEL] %s:%d %s\n", file, line,
                      msg);
        break;
      case opentelemetry::sdk::common::internal_log::LogLevel::Debug:
        (void)fprintf(stderr, "telemetry_client: [OTEL DEBUG] %s:%d %s\n", file,
                      line, msg);
        break;
    }
  }
};

static opentelemetry::nostd::shared_ptr<
    opentelemetry::sdk::common::internal_log::LogHandler>
    g_log_handler(new MySQLClientOtelLogHandler());

static void setup_internal_logger() {
  opentelemetry::sdk::common::internal_log::GlobalLogHandler::SetLogHandler(
      g_log_handler);
  opentelemetry::sdk::common::internal_log::GlobalLogHandler::SetLogLevel(
      opentelemetry::sdk::common::internal_log::LogLevel::Error);
}

int telemetry_client_plugin_init(char *errmsg, size_t errmsg_size, int argc,
                                 va_list args) {
  int rc = 0;
  const char *defaults_file = nullptr;
  const char *defaults_group_suffix = nullptr;
  const char *defaults_extra_file = nullptr;
  int client_argc = 0;
  int *client_argc_ptr = &client_argc;
  char **client_argv = nullptr;

  if (argc >= 1) {
    defaults_file = va_arg(args, const char *);
  }

  if (argc >= 2) {
    defaults_group_suffix = va_arg(args, const char *);
  }

  if (argc >= 3) {
    defaults_extra_file = va_arg(args, const char *);
  }

  if (argc >= 4) {
    client_argc_ptr = va_arg(args, int *);
  }

  if (argc >= 5) {
    client_argv = va_arg(args, char **);
  }

  rc = register_client_options(defaults_file, defaults_group_suffix,
                               defaults_extra_file, client_argc_ptr,
                               client_argv);
  if (rc != 0) {
    snprintf(errmsg, errmsg_size, "Failed to register client options");
    return 1;
  }

  setup_internal_logger();
  client_setup_otel();
  return 0;
}

int telemetry_client_plugin_deinit() {
  client_cleanup_otel();
  return 0;
}

int telemetry_client_plugin_option(const char * /* option */, const void *) {
  return 0;
}

int telemetry_client_get_plugin_option(const char * /* option */, void *) {
  return 0;
}

class ClientSpan {
 public:
  static telemetry_span_t *to_api(ClientSpan *span) {
    auto *api = reinterpret_cast<telemetry_span_t *>(span);
    return api;
  }

  static ClientSpan *from_api(telemetry_span_t *api) {
    auto *span = reinterpret_cast<ClientSpan *>(api);
    return span;
  }

  ClientSpan() = default;
  ClientSpan(const ClientSpan &) = delete;
  ClientSpan(ClientSpan &&) = delete;
  ClientSpan &operator=(const ClientSpan &) = delete;
  ClientSpan &operator=(ClientSpan &&) = delete;

  ~ClientSpan() = default;

  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> m_otel_span;
};

class ClientTextMapCarrier
    : public opentelemetry::context::propagation::TextMapCarrier {
 public:
  ClientTextMapCarrier(void *carrier_data,
                       telemetry_text_map_carrier_set_t carrier)
      : m_carrier_data(carrier_data), m_carrier(carrier) {}

  ClientTextMapCarrier(const ClientTextMapCarrier &) = delete;
  ClientTextMapCarrier(ClientTextMapCarrier &&) = delete;
  ClientTextMapCarrier &operator=(const ClientTextMapCarrier &) = delete;
  ClientTextMapCarrier &operator=(ClientTextMapCarrier &&) = delete;

  ~ClientTextMapCarrier() override = default;

  opentelemetry::nostd::string_view Get(
      opentelemetry::nostd::string_view /* key */) const noexcept override {
    assert(false);
    return "";
  }

  void Set(opentelemetry::nostd::string_view key,
           opentelemetry::nostd::string_view value) noexcept override {
    m_carrier(m_carrier_data, key.data(), key.length(), value.data(),
              value.length());
  }

  bool Keys(opentelemetry::nostd::function_ref<
            bool(opentelemetry::nostd::string_view)>
            /* callback */) const noexcept override {
    assert(false);
    return false;
  }

 private:
  void *m_carrier_data;
  telemetry_text_map_carrier_set_t m_carrier;
};

class ClientTextMapPropagator
    : public opentelemetry::trace::propagation::HttpTraceContext {};

telemetry_span_t *telemetry_client_start_span(const char *name) {
  telemetry_span_t *api = nullptr;

  if (g_tracer != nullptr) {
    auto *span = new ClientSpan();
    opentelemetry::trace::StartSpanOptions options;
    options.kind = opentelemetry::trace::SpanKind::kClient;

    span->m_otel_span = g_tracer->StartSpan(name, options);
    api = ClientSpan::to_api(span);
  }

  return api;
}

void telemetry_client_injector(telemetry_span_t *api, void *carrier_data,
                               telemetry_text_map_carrier_set_t carrier) {
  ClientSpan *span = ClientSpan::from_api(api);
  if (span != nullptr) {
    if (span->m_otel_span != nullptr) {
      ClientTextMapCarrier client_carrier(carrier_data, carrier);
      ClientTextMapPropagator propagator;

      // See https://github.com/open-telemetry/opentelemetry-cpp/issues/1467
      opentelemetry::context::Context empty;
      auto wrapped_span_context = SetSpan(empty, span->m_otel_span);

      propagator.Inject(client_carrier, wrapped_span_context);
    }
  }
}

void telemetry_client_end_span(telemetry_span_t *api) {
  ClientSpan *span = ClientSpan::from_api(api);
  if (span != nullptr) {
    if (span->m_otel_span != nullptr) {
      span->m_otel_span->End();
    }
    delete span;
  }
}

// clang-format off
mysql_declare_client_plugin(TELEMETRY)
  "telemetry_client",
  MYSQL_CLIENT_PLUGIN_AUTHOR_ORACLE,
  "Telemetry Client Plugin",
  {1, 0, 0}, // Version
  "GPL", // License
  nullptr,
  telemetry_client_plugin_init,
  telemetry_client_plugin_deinit,
  telemetry_client_plugin_option,
  telemetry_client_get_plugin_option,
  telemetry_client_start_span,
  telemetry_client_injector,
  telemetry_client_end_span
mysql_end_client_plugin;
// clang-format on
