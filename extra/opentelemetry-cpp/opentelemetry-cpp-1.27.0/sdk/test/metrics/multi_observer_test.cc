// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

// Multi-observer functionality requires ABI version 2+
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
#  include <gtest/gtest.h>
#  include <cstddef>
#  include <cstdint>
#  include <string>
#  include <utility>
#  include <vector>

#  include "common.h"
#  include "opentelemetry/metrics/async_instruments.h"
#  include "opentelemetry/metrics/meter.h"
#  include "opentelemetry/metrics/meter_provider.h"
#  include "opentelemetry/metrics/multi_observer_result.h"
#  include "opentelemetry/metrics/observer_result.h"
#  include "opentelemetry/nostd/function_ref.h"
#  include "opentelemetry/nostd/shared_ptr.h"
#  include "opentelemetry/nostd/span.h"
#  include "opentelemetry/nostd/string_view.h"
#  include "opentelemetry/sdk/metrics/data/metric_data.h"
#  include "opentelemetry/sdk/metrics/export/metric_producer.h"
#  include "opentelemetry/sdk/metrics/instruments.h"
#  include "opentelemetry/sdk/metrics/meter_provider.h"
#  include "opentelemetry/sdk/metrics/metric_reader.h"

using namespace opentelemetry;
using namespace opentelemetry::sdk::metrics;

namespace
{
nostd::shared_ptr<metrics::Meter> InitMeter(MetricReader **metricReaderPtr,
                                            const std::string &meter_name = "meter_name")
{
  static std::shared_ptr<metrics::MeterProvider> provider(new MeterProvider());
  std::unique_ptr<MetricReader> metric_reader(new MockMetricReader());
  *metricReaderPtr = metric_reader.get();
  auto p           = std::static_pointer_cast<MeterProvider>(provider);
  p->AddMetricReader(std::move(metric_reader));
  auto meter = provider->GetMeter(meter_name);
  return meter;
}

void basic_multi_observable_callback(metrics::MultiObserverResult &result, void *state)
{
  auto *instruments = reinterpret_cast<std::vector<metrics::ObservableInstrument *> *>(state);

  auto &counter_record = result.ForInstrument<int64_t>(instruments->at(0));
  counter_record.Observe(100, {{"service", "frontend"}});
  counter_record.Observe(200, {{"service", "backend"}});

  auto &gauge_record = result.ForInstrument<double>(instruments->at(1));
  gauge_record.Observe(95.5, {{"cpu", "core1"}});
  gauge_record.Observe(87.2, {{"cpu", "core2"}});
}
}  // namespace

TEST(MultiObserverTest, BasicMultiObservableCallback)
{
  MetricReader *metric_reader_ptr = nullptr;
  auto meter                      = InitMeter(&metric_reader_ptr, "multi_observer_test");

  auto observable_counter =
      meter->CreateInt64ObservableCounter("test_counter", "Test counter", "1");
  auto observable_gauge = meter->CreateDoubleObservableGauge("test_gauge", "Test gauge", "%");

  // Create a vector of raw pointers for the span
  std::vector<metrics::ObservableInstrument *> instrument_ptrs;
  instrument_ptrs.push_back(observable_counter.get());
  instrument_ptrs.push_back(observable_gauge.get());

  // Register a single callback that observes both instruments
  auto callback_id = meter->RegisterCallback(
      basic_multi_observable_callback, &instrument_ptrs,
      nostd::span<metrics::ObservableInstrument *>{instrument_ptrs.data(), instrument_ptrs.size()});

  // Collect metrics and verify both instruments have data
  size_t counter_metrics = 0;
  size_t gauge_metrics   = 0;

  metric_reader_ptr->Collect([&](ResourceMetrics &metric_data) {
    EXPECT_EQ(metric_data.scope_metric_data_.size(), 1);

    for (const auto &metric : metric_data.scope_metric_data_[0].metric_data_)
    {
      if (metric.instrument_descriptor.name_ == "test_counter")
      {
        counter_metrics++;
        EXPECT_EQ(metric.point_data_attr_.size(), 2);  // frontend and backend services
      }
      else if (metric.instrument_descriptor.name_ == "test_gauge")
      {
        gauge_metrics++;
        EXPECT_EQ(metric.point_data_attr_.size(), 2);  // core1 and core2 CPUs
      }
    }
    return true;
  });

  EXPECT_EQ(counter_metrics, 1);
  EXPECT_EQ(gauge_metrics, 1);

  // Clean up
  meter->DeregisterCallback(callback_id);
}

namespace
{
void empty_multi_observable_callback(metrics::MultiObserverResult &, void *state)
{
  bool *was_called = static_cast<bool *>(state);
  *was_called      = true;
}
}  // namespace

TEST(MultiObserverTest, EmptyInstrumentsList)
{
  MetricReader *metric_reader_ptr = nullptr;
  auto meter                      = InitMeter(&metric_reader_ptr, "multi_observer_empty_test");
  std::vector<metrics::ObservableInstrument *> empty_instruments;

  // Register callback with empty instruments list
  bool was_called = false;
  auto callback_id =
      meter->RegisterCallback(empty_multi_observable_callback, &was_called,
                              nostd::span<metrics::ObservableInstrument *>{empty_instruments});

  metric_reader_ptr->Collect([&](ResourceMetrics &) { return true; });

  EXPECT_TRUE(was_called);

  // Clean up
  meter->DeregisterCallback(callback_id);

  // It won't be called again if we collect
  was_called = false;
  metric_reader_ptr->Collect([&](ResourceMetrics &) { return true; });
  EXPECT_FALSE(was_called);
}

namespace
{
void non_registered_instrument_callback(metrics::MultiObserverResult &result, void *state)
{
  auto *gauge = reinterpret_cast<metrics::ObservableInstrument *>(state);
  result.ForInstrument<double>(gauge).Observe(23.5);
}
}  // namespace

TEST(MultiObserverTest, NonRegisteredInstrument)
{
  MetricReader *metric_reader_ptr = nullptr;
  auto meter = InitMeter(&metric_reader_ptr, "multi_observer_non_registered_test");

  auto observable_counter =
      meter->CreateInt64ObservableCounter("test_counter", "Test counter", "1");
  auto observable_counter_ptr = observable_counter.get();
  auto observable_gauge       = meter->CreateDoubleObservableGauge("test_gauge", "Test gauge", "%");

  // Register with one instrument (the counter), but the callback will try and record a metric
  // for the other instrument (the gauge)
  auto callback_id = meter->RegisterCallback(
      non_registered_instrument_callback, observable_gauge.get(),
      nostd::span<metrics::ObservableInstrument *>{&observable_counter_ptr, 1});

  metric_reader_ptr->Collect([&](ResourceMetrics &metric_data) {
    // It should not have recorded any metrics; the observation for the gauge should have been
    // discarded.
    EXPECT_EQ(0, metric_data.scope_metric_data_.size());
    return true;
  });
  meter->DeregisterCallback(callback_id);
}

namespace
{

struct CallbackDeregisteredWhenDestroyedState
{
  metrics::ObservableInstrument *counter = nullptr;
  int times_called                       = 0;
};

void callback_deregistered_when_destroyed_callback(metrics::MultiObserverResult &result,
                                                   void *vstate)
{
  auto state = reinterpret_cast<CallbackDeregisteredWhenDestroyedState *>(vstate);
  state->times_called++;
  result.ForInstrument<int64_t>(state->counter).Observe(state->times_called);
}
}  // namespace

TEST(MultiObserverTest, CallbackDeregisteredWhenInstrumentDestroyed)
{
  MetricReader *metric_reader_ptr = nullptr;
  auto meter = InitMeter(&metric_reader_ptr, "callback_deregistered_when_destroyed_test");

  CallbackDeregisteredWhenDestroyedState state{};
  auto counter_owning = meter->CreateInt64ObservableCounter("counter", "Counter", "1");
  state.counter       = counter_owning.get();
  meter->RegisterCallback(callback_deregistered_when_destroyed_callback, &state,
                          nostd::span<metrics::ObservableInstrument *>{&state.counter, 1});

  // If we run a collection, the callback gets called
  metric_reader_ptr->Collect([&](ResourceMetrics &) { return true; });
  EXPECT_EQ(state.times_called, 1);

  // If we allow the instrument to be destroyed, the callback should now _NOT_ get called anymore
  counter_owning = nullptr;
  metric_reader_ptr->Collect([&](ResourceMetrics &) { return true; });
  EXPECT_EQ(state.times_called, 1);
}

#endif
