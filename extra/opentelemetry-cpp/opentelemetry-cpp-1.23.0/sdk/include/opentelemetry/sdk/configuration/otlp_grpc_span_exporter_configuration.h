// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>

#include "opentelemetry/sdk/configuration/headers_configuration.h"
#include "opentelemetry/sdk/configuration/span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/span_exporter_configuration_visitor.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/common.json
// YAML-NODE: OtlpGrpcExporter
class OtlpGrpcSpanExporterConfiguration : public SpanExporterConfiguration
{
public:
  void Accept(SpanExporterConfigurationVisitor *visitor) const override
  {
    visitor->VisitOtlpGrpc(this);
  }

  std::string endpoint;
  std::string certificate_file;
  std::string client_key_file;
  std::string client_certificate_file;
  std::unique_ptr<HeadersConfiguration> headers;
  std::string headers_list;
  std::string compression;
  std::size_t timeout{0};
  bool insecure{false};
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
