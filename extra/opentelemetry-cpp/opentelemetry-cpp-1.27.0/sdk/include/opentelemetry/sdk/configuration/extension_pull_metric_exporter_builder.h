// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/extension_pull_metric_exporter_configuration.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class ExtensionPullMetricExporterBuilder
{
public:
  ExtensionPullMetricExporterBuilder()                                                 = default;
  ExtensionPullMetricExporterBuilder(ExtensionPullMetricExporterBuilder &&)            = default;
  ExtensionPullMetricExporterBuilder(const ExtensionPullMetricExporterBuilder &)       = default;
  ExtensionPullMetricExporterBuilder &operator=(ExtensionPullMetricExporterBuilder &&) = default;
  ExtensionPullMetricExporterBuilder &operator=(const ExtensionPullMetricExporterBuilder &other) =
      default;
  virtual ~ExtensionPullMetricExporterBuilder() = default;

  virtual std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> Build(
      const opentelemetry::sdk::configuration::ExtensionPullMetricExporterConfiguration *model)
      const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
