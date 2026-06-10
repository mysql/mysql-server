// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

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
// YAML-NODE: ExperimentalOtlpFileMetricExporter
class OtlpFilePushMetricExporterConfiguration : public PushMetricExporterConfiguration
{
public:
  void Accept(PushMetricExporterConfigurationVisitor *visitor) const override
  {
    visitor->VisitOtlpFile(this);
  }

  std::string output_stream;
  TemporalityPreference temporality_preference{TemporalityPreference::cumulative};
  DefaultHistogramAggregation default_histogram_aggregation{
      DefaultHistogramAggregation::explicit_bucket_histogram};
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
