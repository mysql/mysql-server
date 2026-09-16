// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace trace
{
class Tracer;
}  // namespace trace

namespace plugin
{
/**
 * Manage the ownership of a dynamically loaded tracer.
 */
class TracerHandle
{
public:
  TracerHandle()                                = default;
  TracerHandle(const TracerHandle &)            = delete;
  TracerHandle(TracerHandle &&)                 = delete;
  TracerHandle &operator=(const TracerHandle &) = delete;
  TracerHandle &operator=(TracerHandle &&)      = delete;
  virtual ~TracerHandle()                       = default;

  virtual trace::Tracer &tracer() const noexcept = 0;
};
}  // namespace plugin
OPENTELEMETRY_END_NAMESPACE
