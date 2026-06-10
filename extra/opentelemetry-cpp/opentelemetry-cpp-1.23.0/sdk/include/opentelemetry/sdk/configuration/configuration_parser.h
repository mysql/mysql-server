// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/configuration.h"
#include "opentelemetry/sdk/configuration/document_node.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class ConfigurationParser
{
public:
  static std::unique_ptr<Configuration> Parse(std::unique_ptr<Document> doc);
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
