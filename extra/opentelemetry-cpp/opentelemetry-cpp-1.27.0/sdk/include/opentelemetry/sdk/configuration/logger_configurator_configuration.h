// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vector>

#include "opentelemetry/sdk/configuration/logger_config_configuration.h"
#include "opentelemetry/sdk/configuration/logger_matcher_and_config_configuration.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/logger_provider.yaml
// YAML-NODE: LoggerConfigurator
class LoggerConfiguratorConfiguration
{
public:
  LoggerConfigConfiguration default_config;
  std::vector<LoggerMatcherAndConfigConfiguration> loggers;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
