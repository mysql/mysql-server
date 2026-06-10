// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{
class AggregationConfigurationVisitor;

// YAML-SCHEMA: schema/meter_provider.json
// YAML-NODE: aggregation
class AggregationConfiguration
{
public:
  AggregationConfiguration()                                                 = default;
  AggregationConfiguration(AggregationConfiguration &&)                      = default;
  AggregationConfiguration(const AggregationConfiguration &)                 = default;
  AggregationConfiguration &operator=(AggregationConfiguration &&)           = default;
  AggregationConfiguration &operator=(const AggregationConfiguration &other) = default;
  virtual ~AggregationConfiguration()                                        = default;

  virtual void Accept(AggregationConfigurationVisitor *visitor) const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
