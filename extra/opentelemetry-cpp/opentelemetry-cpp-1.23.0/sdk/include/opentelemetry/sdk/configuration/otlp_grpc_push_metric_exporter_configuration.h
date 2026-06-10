// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>

#include "opentelemetry/sdk/configuration/default_histogram_aggregation.h"
#include "opentelemetry/sdk/configuration/headers_configuration.h"
#include "opentelemetry/sdk/configuration/push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/push_metric_exporter_configuration_visitor.h"
#include "opentelemetry/sdk/configuration/temporality_preference.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/meter_provider.json
// YAML-NODE: OtlpGrpcMetricExporter
class OtlpGrpcPushMetricExporterConfiguration : public PushMetricExporterConfiguration
{
public:
  void Accept(PushMetricExporterConfigurationVisitor *visitor) const override
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
  TemporalityPreference temporality_preference{TemporalityPreference::cumulative};
  DefaultHistogramAggregation default_histogram_aggregation{
      DefaultHistogramAggregation::explicit_bucket_histogram};
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
