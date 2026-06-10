// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

#include "opentelemetry/sdk/configuration/headers_configuration.h"
#include "opentelemetry/sdk/configuration/pull_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/pull_metric_exporter_configuration_visitor.h"
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
  bool without_units{false};
  bool without_type_suffix{false};
  bool without_scope_info{false};
  // FIXME: with_resource_constant_labels;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
