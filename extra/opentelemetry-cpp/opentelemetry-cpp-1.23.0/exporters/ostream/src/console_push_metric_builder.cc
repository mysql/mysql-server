// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <memory>
#include <utility>

#include "opentelemetry/exporters/ostream/console_push_metric_builder.h"
#include "opentelemetry/exporters/ostream/metric_exporter_factory.h"
#include "opentelemetry/sdk/configuration/console_push_metric_exporter_builder.h"
#include "opentelemetry/sdk/configuration/console_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/registry.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace metrics
{

void ConsolePushMetricBuilder::Register(opentelemetry::sdk::configuration::Registry *registry)
{
  auto builder = std::make_unique<ConsolePushMetricBuilder>();
  registry->SetConsolePushMetricExporterBuilder(std::move(builder));
}

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> ConsolePushMetricBuilder::Build(
    const opentelemetry::sdk::configuration::ConsolePushMetricExporterConfiguration * /* model */)
    const
{
  return OStreamMetricExporterFactory::Create();
}

}  // namespace metrics
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
