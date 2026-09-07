// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <chrono>
#include <memory>
#include <string>
#include <utility>

#include "opentelemetry/exporters/otlp/otlp_builder_utils.h"
#include "opentelemetry/exporters/otlp/otlp_http.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_builder.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h"
#include "opentelemetry/sdk/configuration/http_tls_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_http_log_record_exporter_builder.h"
#include "opentelemetry/sdk/configuration/otlp_http_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/registry.h"
#include "opentelemetry/sdk/logs/exporter.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

void OtlpHttpLogRecordBuilder::Register(opentelemetry::sdk::configuration::Registry *registry)
{
  auto builder = std::make_unique<OtlpHttpLogRecordBuilder>();
  registry->SetOtlpHttpLogRecordBuilder(std::move(builder));
}

std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> OtlpHttpLogRecordBuilder::Build(
    const opentelemetry::sdk::configuration::OtlpHttpLogRecordExporterConfiguration *model) const
{
  OtlpHttpLogRecordExporterOptions options(nullptr);

  const auto *tls = model->tls.get();

  options.url                = model->endpoint;
  options.content_type       = OtlpBuilderUtils::ConvertOtlpHttpEncoding(model->encoding);
  options.json_bytes_mapping = JsonBytesMappingKind::kHexId;
  options.use_json_name      = false;
  options.console_debug      = false;
  options.timeout            = std::chrono::duration_cast<std::chrono::system_clock::duration>(
      std::chrono::seconds{model->timeout});
  options.http_headers =
      OtlpBuilderUtils::ConvertHeadersConfigurationModel(model->headers.get(), model->headers_list);
  options.ssl_insecure_skip_verify = false;

  if (tls != nullptr)
  {
    options.ssl_ca_cert_path     = tls->ca_file;
    options.ssl_client_key_path  = tls->key_file;
    options.ssl_client_cert_path = tls->cert_file;
  }

  options.compression = model->compression;

  return OtlpHttpLogRecordExporterFactory::Create(options);
}

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
