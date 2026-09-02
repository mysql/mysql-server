// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class StringAttributeValueConfiguration;
class IntegerAttributeValueConfiguration;
class DoubleAttributeValueConfiguration;
class BooleanAttributeValueConfiguration;
class StringArrayAttributeValueConfiguration;
class IntegerArrayAttributeValueConfiguration;
class DoubleArrayAttributeValueConfiguration;
class BooleanArrayAttributeValueConfiguration;

class AttributeValueConfigurationVisitor
{
public:
  AttributeValueConfigurationVisitor()                                                 = default;
  AttributeValueConfigurationVisitor(AttributeValueConfigurationVisitor &&)            = default;
  AttributeValueConfigurationVisitor(const AttributeValueConfigurationVisitor &)       = default;
  AttributeValueConfigurationVisitor &operator=(AttributeValueConfigurationVisitor &&) = default;
  AttributeValueConfigurationVisitor &operator=(const AttributeValueConfigurationVisitor &other) =
      default;
  virtual ~AttributeValueConfigurationVisitor() = default;

  virtual void VisitString(const StringAttributeValueConfiguration *model)             = 0;
  virtual void VisitInteger(const IntegerAttributeValueConfiguration *model)           = 0;
  virtual void VisitDouble(const DoubleAttributeValueConfiguration *model)             = 0;
  virtual void VisitBoolean(const BooleanAttributeValueConfiguration *model)           = 0;
  virtual void VisitStringArray(const StringArrayAttributeValueConfiguration *model)   = 0;
  virtual void VisitIntegerArray(const IntegerArrayAttributeValueConfiguration *model) = 0;
  virtual void VisitDoubleArray(const DoubleArrayAttributeValueConfiguration *model)   = 0;
  virtual void VisitBooleanArray(const BooleanArrayAttributeValueConfiguration *model) = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
