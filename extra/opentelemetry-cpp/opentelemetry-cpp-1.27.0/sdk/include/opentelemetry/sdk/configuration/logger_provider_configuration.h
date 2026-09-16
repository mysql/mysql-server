// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <vector>

#include "opentelemetry/sdk/configuration/log_record_limits_configuration.h"
#include "opentelemetry/sdk/configuration/log_record_processor_configuration.h"
#include "opentelemetry/sdk/configuration/logger_configurator_configuration.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/logger_provider.yaml
// YAML-NODE: LoggerProvider
class LoggerProviderConfiguration
{
public:
  std::vector<std::unique_ptr<LogRecordProcessorConfiguration>> processors;
  std::unique_ptr<LogRecordLimitsConfiguration> limits;
  std::unique_ptr<LoggerConfiguratorConfiguration> logger_configurator;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
