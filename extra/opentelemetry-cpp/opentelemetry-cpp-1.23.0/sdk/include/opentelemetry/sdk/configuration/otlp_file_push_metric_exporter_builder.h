// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/otlp_file_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class OtlpFilePushMetricExporterBuilder
{
public:
  OtlpFilePushMetricExporterBuilder()                                                = default;
  OtlpFilePushMetricExporterBuilder(OtlpFilePushMetricExporterBuilder &&)            = default;
  OtlpFilePushMetricExporterBuilder(const OtlpFilePushMetricExporterBuilder &)       = default;
  OtlpFilePushMetricExporterBuilder &operator=(OtlpFilePushMetricExporterBuilder &&) = default;
  OtlpFilePushMetricExporterBuilder &operator=(const OtlpFilePushMetricExporterBuilder &other) =
      default;
  virtual ~OtlpFilePushMetricExporterBuilder() = default;

  virtual std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> Build(
      const opentelemetry::sdk::configuration::OtlpFilePushMetricExporterConfiguration *model)
      const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
