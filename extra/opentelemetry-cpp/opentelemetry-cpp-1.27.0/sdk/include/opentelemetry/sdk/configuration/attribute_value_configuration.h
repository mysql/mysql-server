// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{
class AttributeValueConfigurationVisitor;

// YAML-SCHEMA: schema/resource.json
// YAML-NODE: AttributeNameValue
class AttributeValueConfiguration
{
public:
  AttributeValueConfiguration()                                                    = default;
  AttributeValueConfiguration(AttributeValueConfiguration &&)                      = default;
  AttributeValueConfiguration(const AttributeValueConfiguration &)                 = default;
  AttributeValueConfiguration &operator=(AttributeValueConfiguration &&)           = default;
  AttributeValueConfiguration &operator=(const AttributeValueConfiguration &other) = default;
  virtual ~AttributeValueConfiguration()                                           = default;

  virtual void Accept(AttributeValueConfigurationVisitor *visitor) const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
