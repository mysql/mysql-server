// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{
class SpanProcessorConfigurationVisitor;

// YAML-SCHEMA: schema/tracer_provider.json
// YAML-NODE: SpanProcessor
class SpanProcessorConfiguration
{
public:
  SpanProcessorConfiguration()                                                   = default;
  SpanProcessorConfiguration(SpanProcessorConfiguration &&)                      = default;
  SpanProcessorConfiguration(const SpanProcessorConfiguration &)                 = default;
  SpanProcessorConfiguration &operator=(SpanProcessorConfiguration &&)           = default;
  SpanProcessorConfiguration &operator=(const SpanProcessorConfiguration &other) = default;
  virtual ~SpanProcessorConfiguration()                                          = default;

  virtual void Accept(SpanProcessorConfigurationVisitor *visitor) const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
