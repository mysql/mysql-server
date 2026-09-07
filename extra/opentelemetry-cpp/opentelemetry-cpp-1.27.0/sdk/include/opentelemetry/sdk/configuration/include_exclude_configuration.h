// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/string_array_configuration.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/common.json
// YAML-NODE: IncludeExclude
class IncludeExcludeConfiguration
{
public:
  std::unique_ptr<StringArrayConfiguration> included;
  std::unique_ptr<StringArrayConfiguration> excluded;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
