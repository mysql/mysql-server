// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/sdk/configuration/log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/log_record_exporter_configuration_visitor.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/common.json
// YAML-NODE: Console
class ConsoleLogRecordExporterConfiguration : public LogRecordExporterConfiguration
{
public:
  void Accept(LogRecordExporterConfigurationVisitor *visitor) const override
  {
    visitor->VisitConsole(this);
  }
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
