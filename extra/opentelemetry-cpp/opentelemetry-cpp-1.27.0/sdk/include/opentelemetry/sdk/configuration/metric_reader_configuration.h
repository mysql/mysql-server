// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{
class MetricReaderConfigurationVisitor;

// YAML-SCHEMA: schema/meter_provider.json
// YAML-NODE: MetricReader
class MetricReaderConfiguration
{
public:
  MetricReaderConfiguration()                                                  = default;
  MetricReaderConfiguration(MetricReaderConfiguration &&)                      = default;
  MetricReaderConfiguration(const MetricReaderConfiguration &)                 = default;
  MetricReaderConfiguration &operator=(MetricReaderConfiguration &&)           = default;
  MetricReaderConfiguration &operator=(const MetricReaderConfiguration &other) = default;
  virtual ~MetricReaderConfiguration()                                         = default;

  virtual void Accept(MetricReaderConfigurationVisitor *visitor) const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
