// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vector>

#include "opentelemetry/sdk/configuration/attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/attribute_value_configuration_visitor.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/resource.json
// YAML-NODE: AttributeNameValue
class DoubleArrayAttributeValueConfiguration : public AttributeValueConfiguration
{
public:
  void Accept(AttributeValueConfigurationVisitor *visitor) const override
  {
    visitor->VisitDoubleArray(this);
  }

  std::vector<double> value;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
