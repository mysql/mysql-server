// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <unordered_map>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/key_value_iterable.h"
#include "opentelemetry/common/macros.h"
#include "opentelemetry/metrics/observer_result.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/sdk/metrics/state/attributes_hashmap.h"
#include "opentelemetry/sdk/metrics/view/attributes_processor.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace metrics
{
template <class T>
class ObserverResultT final : public opentelemetry::metrics::ObserverResultT<T>
{
public:
  explicit ObserverResultT(const AttributesProcessor *attributes_processor = nullptr)
      : attributes_processor_(attributes_processor)
  {}

  ObserverResultT(const ObserverResultT &)            = default;
  ObserverResultT(ObserverResultT &&)                 = default;
  ObserverResultT &operator=(const ObserverResultT &) = default;
  ObserverResultT &operator=(ObserverResultT &&)      = default;

  ~ObserverResultT() override = default;

  void Observe(T value) noexcept override
#if OPENTELEMETRY_HAVE_EXCEPTIONS
  try
#endif
  {
    data_[MetricAttributes{{}, attributes_processor_}] = value;
  }
#if OPENTELEMETRY_HAVE_EXCEPTIONS
  catch (...)
  {
    // Silently drop the measurement; per opentelemetry-cpp guidance (PR #3964),
    // exceptions in noexcept API/SDK code must not log or abort.
    return;
  }
#endif

  void Observe(T value, const opentelemetry::common::KeyValueIterable &attributes) noexcept override
#if OPENTELEMETRY_HAVE_EXCEPTIONS
  try
#endif
  {
    data_[MetricAttributes{attributes, attributes_processor_}] =
        value;  // overwrites the previous value if present
  }
#if OPENTELEMETRY_HAVE_EXCEPTIONS
  catch (...)
  {
    // Silently drop the measurement; per opentelemetry-cpp guidance (PR #3964),
    // exceptions in noexcept API/SDK code must not log or abort.
    return;
  }
#endif

  const std::unordered_map<MetricAttributes, T, AttributeHashGenerator> &GetMeasurements()
  {
    return data_;
  }

private:
  std::unordered_map<MetricAttributes, T, AttributeHashGenerator> data_;
  const AttributesProcessor *attributes_processor_;
};
}  // namespace metrics
}  // namespace sdk

OPENTELEMETRY_END_NAMESPACE
