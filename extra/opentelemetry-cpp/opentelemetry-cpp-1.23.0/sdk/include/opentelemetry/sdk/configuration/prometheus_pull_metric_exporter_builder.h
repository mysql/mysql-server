// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/prometheus_pull_metric_exporter_configuration.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class PrometheusPullMetricExporterBuilder
{
public:
  PrometheusPullMetricExporterBuilder()                                                  = default;
  PrometheusPullMetricExporterBuilder(PrometheusPullMetricExporterBuilder &&)            = default;
  PrometheusPullMetricExporterBuilder(const PrometheusPullMetricExporterBuilder &)       = default;
  PrometheusPullMetricExporterBuilder &operator=(PrometheusPullMetricExporterBuilder &&) = default;
  PrometheusPullMetricExporterBuilder &operator=(const PrometheusPullMetricExporterBuilder &other) =
      default;
  virtual ~PrometheusPullMetricExporterBuilder() = default;

  virtual std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> Build(
      const opentelemetry::sdk::configuration::PrometheusPullMetricExporterConfiguration *model)
      const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
