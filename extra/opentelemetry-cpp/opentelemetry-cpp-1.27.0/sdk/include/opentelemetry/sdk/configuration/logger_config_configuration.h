// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/logger_provider.yaml
// YAML-NODE: LoggerConfig
class LoggerConfigConfiguration
{
public:
  bool enabled{true};
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
