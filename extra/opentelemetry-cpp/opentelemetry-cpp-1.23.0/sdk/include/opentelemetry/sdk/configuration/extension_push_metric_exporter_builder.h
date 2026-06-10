// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/extension_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class ExtensionPushMetricExporterBuilder
{
public:
  ExtensionPushMetricExporterBuilder()                                                 = default;
  ExtensionPushMetricExporterBuilder(ExtensionPushMetricExporterBuilder &&)            = default;
  ExtensionPushMetricExporterBuilder(const ExtensionPushMetricExporterBuilder &)       = default;
  ExtensionPushMetricExporterBuilder &operator=(ExtensionPushMetricExporterBuilder &&) = default;
  ExtensionPushMetricExporterBuilder &operator=(const ExtensionPushMetricExporterBuilder &other) =
      default;
  virtual ~ExtensionPushMetricExporterBuilder() = default;

  virtual std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> Build(
      const opentelemetry::sdk::configuration::ExtensionPushMetricExporterConfiguration *model)
      const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
