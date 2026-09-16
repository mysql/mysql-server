// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/otlp_grpc_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class OtlpGrpcPushMetricExporterBuilder
{
public:
  OtlpGrpcPushMetricExporterBuilder()                                                = default;
  OtlpGrpcPushMetricExporterBuilder(OtlpGrpcPushMetricExporterBuilder &&)            = default;
  OtlpGrpcPushMetricExporterBuilder(const OtlpGrpcPushMetricExporterBuilder &)       = default;
  OtlpGrpcPushMetricExporterBuilder &operator=(OtlpGrpcPushMetricExporterBuilder &&) = default;
  OtlpGrpcPushMetricExporterBuilder &operator=(const OtlpGrpcPushMetricExporterBuilder &other) =
      default;
  virtual ~OtlpGrpcPushMetricExporterBuilder() = default;

  virtual std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> Build(
      const opentelemetry::sdk::configuration::OtlpGrpcPushMetricExporterConfiguration *model)
      const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
