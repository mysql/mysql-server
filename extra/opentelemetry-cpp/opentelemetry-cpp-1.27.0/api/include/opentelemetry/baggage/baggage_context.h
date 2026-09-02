// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/baggage/baggage.h"
#include "opentelemetry/context/context.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE

namespace baggage
{

static const std::string kBaggageHeader = "baggage";

inline nostd::shared_ptr<Baggage> GetBaggage(const context::Context &context) noexcept
{
  context::ContextValue context_value = context.GetValue(kBaggageHeader);

  if (const nostd::shared_ptr<Baggage> *value =
          nostd::get_if<nostd::shared_ptr<Baggage>>(&context_value))
  {
    return *value;
  }
  static nostd::shared_ptr<Baggage> empty_baggage{new Baggage()};
  return empty_baggage;
}

inline context::Context SetBaggage(context::Context &context,
                                   const nostd::shared_ptr<Baggage> &baggage) noexcept
{
  return context.SetValue(kBaggageHeader, baggage);
}

}  // namespace baggage
OPENTELEMETRY_END_NAMESPACE
