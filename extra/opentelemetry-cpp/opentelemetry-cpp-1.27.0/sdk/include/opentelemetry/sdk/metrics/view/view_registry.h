// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <vector>

#include "opentelemetry/nostd/function_ref.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/view/instrument_selector.h"
#include "opentelemetry/sdk/metrics/view/meter_selector.h"
#include "opentelemetry/sdk/metrics/view/view.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace metrics
{
struct RegisteredView
{
  RegisteredView(
      std::unique_ptr<opentelemetry::sdk::metrics::InstrumentSelector> instrument_selector,
      std::unique_ptr<opentelemetry::sdk::metrics::MeterSelector> meter_selector,
      std::unique_ptr<opentelemetry::sdk::metrics::View> view);

  std::unique_ptr<opentelemetry::sdk::metrics::InstrumentSelector> instrument_selector_;
  std::unique_ptr<opentelemetry::sdk::metrics::MeterSelector> meter_selector_;
  std::unique_ptr<opentelemetry::sdk::metrics::View> view_;
};

class ViewRegistry
{
public:
  ViewRegistry() = default;

  ViewRegistry(const ViewRegistry &)            = delete;
  ViewRegistry(ViewRegistry &&)                 = delete;
  ViewRegistry &operator=(const ViewRegistry &) = delete;
  ViewRegistry &operator=(ViewRegistry &&)      = delete;

  ~ViewRegistry() = default;

  void AddView(std::unique_ptr<opentelemetry::sdk::metrics::InstrumentSelector> instrument_selector,
               std::unique_ptr<opentelemetry::sdk::metrics::MeterSelector> meter_selector,
               std::unique_ptr<opentelemetry::sdk::metrics::View> view);

  bool FindViews(
      const opentelemetry::sdk::metrics::InstrumentDescriptor &instrument_descriptor,
      const opentelemetry::sdk::instrumentationscope::InstrumentationScope &instrumentation_scope,
      nostd::function_ref<bool(const View &)> callback) const;

private:
  std::vector<std::unique_ptr<RegisteredView>> registered_views_;

  static bool MatchMeter(
      opentelemetry::sdk::metrics::MeterSelector *selector,
      const opentelemetry::sdk::instrumentationscope::InstrumentationScope &instrumentation_scope);

  static bool MatchInstrument(
      opentelemetry::sdk::metrics::InstrumentSelector *selector,
      const opentelemetry::sdk::metrics::InstrumentDescriptor &instrument_descriptor);
};
}  // namespace metrics
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
