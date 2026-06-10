// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/sdk/configuration/headers_configuration.h"
#include "opentelemetry/sdk/configuration/push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/push_metric_exporter_configuration_visitor.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/common.json
// YAML-NODE: Console
class ConsolePushMetricExporterConfiguration : public PushMetricExporterConfiguration
{
public:
  void Accept(PushMetricExporterConfigurationVisitor *visitor) const override
  {
    visitor->VisitConsole(this);
  }
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
