// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>

#include "opentelemetry/sdk/configuration/headers_configuration.h"
#include "opentelemetry/sdk/configuration/http_tls_configuration.h"
#include "opentelemetry/sdk/configuration/log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/log_record_exporter_configuration_visitor.h"
#include "opentelemetry/sdk/configuration/otlp_http_encoding.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/common.json
// YAML-NODE: OtlpHttpExporter
class OtlpHttpLogRecordExporterConfiguration : public LogRecordExporterConfiguration
{
public:
  void Accept(LogRecordExporterConfigurationVisitor *visitor) const override
  {
    visitor->VisitOtlpHttp(this);
  }

  std::string endpoint;
  std::unique_ptr<HttpTlsConfiguration> tls;
  std::unique_ptr<HeadersConfiguration> headers;
  std::string headers_list;
  std::string compression;
  std::size_t timeout{0};
  OtlpHttpEncoding encoding{OtlpHttpEncoding::protobuf};
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
