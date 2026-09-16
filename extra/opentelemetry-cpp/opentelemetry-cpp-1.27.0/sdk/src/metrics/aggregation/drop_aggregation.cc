// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <memory>

#include "opentelemetry/sdk/metrics/aggregation/aggregation.h"
#include "opentelemetry/sdk/metrics/aggregation/drop_aggregation.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/data/point_data.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace metrics
{

DropAggregation::DropAggregation(const DropPointData &) {}

std::unique_ptr<Aggregation> DropAggregation::Merge(const Aggregation &) const noexcept
{
  return std::unique_ptr<Aggregation>(new DropAggregation());
}

std::unique_ptr<Aggregation> DropAggregation::Diff(const Aggregation &) const noexcept
{
  return std::unique_ptr<Aggregation>(new DropAggregation());
}

PointType DropAggregation::ToPoint() const noexcept
{
  static DropPointData point_data;
  return point_data;
}

}  // namespace metrics
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
