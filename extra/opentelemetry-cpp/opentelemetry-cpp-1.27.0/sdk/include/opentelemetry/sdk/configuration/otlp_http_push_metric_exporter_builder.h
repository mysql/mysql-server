// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/otlp_http_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class OtlpHttpPushMetricExporterBuilder
{
public:
  OtlpHttpPushMetricExporterBuilder()                                                = default;
  OtlpHttpPushMetricExporterBuilder(OtlpHttpPushMetricExporterBuilder &&)            = default;
  OtlpHttpPushMetricExporterBuilder(const OtlpHttpPushMetricExporterBuilder &)       = default;
  OtlpHttpPushMetricExporterBuilder &operator=(OtlpHttpPushMetricExporterBuilder &&) = default;
  OtlpHttpPushMetricExporterBuilder &operator=(const OtlpHttpPushMetricExporterBuilder &other) =
      default;
  virtual ~OtlpHttpPushMetricExporterBuilder() = default;

  virtual std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> Build(
      const opentelemetry::sdk::configuration::OtlpHttpPushMetricExporterConfiguration *model)
      const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
