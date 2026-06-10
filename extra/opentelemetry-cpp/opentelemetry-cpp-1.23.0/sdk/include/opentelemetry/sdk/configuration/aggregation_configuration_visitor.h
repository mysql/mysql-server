// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class Base2ExponentialBucketHistogramAggregationConfiguration;
class DefaultAggregationConfiguration;
class DropAggregationConfiguration;
class ExplicitBucketHistogramAggregationConfiguration;
class LastValueAggregationConfiguration;
class SumAggregationConfiguration;

class AggregationConfigurationVisitor
{
public:
  AggregationConfigurationVisitor()                                              = default;
  AggregationConfigurationVisitor(AggregationConfigurationVisitor &&)            = default;
  AggregationConfigurationVisitor(const AggregationConfigurationVisitor &)       = default;
  AggregationConfigurationVisitor &operator=(AggregationConfigurationVisitor &&) = default;
  AggregationConfigurationVisitor &operator=(const AggregationConfigurationVisitor &other) =
      default;
  virtual ~AggregationConfigurationVisitor() = default;

  virtual void VisitBase2ExponentialBucketHistogram(
      const Base2ExponentialBucketHistogramAggregationConfiguration *model) = 0;
  virtual void VisitDefault(const DefaultAggregationConfiguration *model)   = 0;
  virtual void VisitDrop(const DropAggregationConfiguration *model)         = 0;
  virtual void VisitExplicitBucketHistogram(
      const ExplicitBucketHistogramAggregationConfiguration *model)           = 0;
  virtual void VisitLastValue(const LastValueAggregationConfiguration *model) = 0;
  virtual void VisitSum(const SumAggregationConfiguration *model)             = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
