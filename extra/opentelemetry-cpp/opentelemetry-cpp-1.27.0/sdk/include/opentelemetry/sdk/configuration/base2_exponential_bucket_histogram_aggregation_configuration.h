// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/sdk/configuration/aggregation_configuration.h"
#include "opentelemetry/sdk/configuration/aggregation_configuration_visitor.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/meter_provider.json
// YAML-NODE: base2_exponential_bucket_histogram
class Base2ExponentialBucketHistogramAggregationConfiguration : public AggregationConfiguration
{
public:
  void Accept(AggregationConfigurationVisitor *visitor) const override
  {
    visitor->VisitBase2ExponentialBucketHistogram(this);
  }

  std::size_t max_scale{0};
  std::size_t max_size{0};
  bool record_min_max{false};
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
