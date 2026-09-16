// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

#include "opentelemetry/sdk/configuration/headers_configuration.h"
#include "opentelemetry/sdk/configuration/include_exclude_configuration.h"
#include "opentelemetry/sdk/configuration/pull_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/pull_metric_exporter_configuration_visitor.h"
#include "opentelemetry/sdk/configuration/translation_strategy.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/meter_provider.json
// YAML-NODE: Prometheus
class PrometheusPullMetricExporterConfiguration : public PullMetricExporterConfiguration
{
public:
  void Accept(PullMetricExporterConfigurationVisitor *visitor) const override
  {
    visitor->VisitPrometheus(this);
  }

  std::string host;
  std::size_t port{0};
  bool without_scope_info{false};
  bool without_target_info{false};
  std::unique_ptr<IncludeExcludeConfiguration> with_resource_constant_labels;
  TranslationStrategy translation_strategy;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
