// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/console_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class ConsolePushMetricExporterBuilder
{
public:
  ConsolePushMetricExporterBuilder()                                               = default;
  ConsolePushMetricExporterBuilder(ConsolePushMetricExporterBuilder &&)            = default;
  ConsolePushMetricExporterBuilder(const ConsolePushMetricExporterBuilder &)       = default;
  ConsolePushMetricExporterBuilder &operator=(ConsolePushMetricExporterBuilder &&) = default;
  ConsolePushMetricExporterBuilder &operator=(const ConsolePushMetricExporterBuilder &other) =
      default;
  virtual ~ConsolePushMetricExporterBuilder() = default;

  virtual std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> Build(
      const opentelemetry::sdk::configuration::ConsolePushMetricExporterConfiguration *model)
      const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
