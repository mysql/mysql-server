// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>
#include "common.h"

#include "opentelemetry/common/macros.h"
#include "opentelemetry/context/context.h"  // IWYU pragma: keep
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/sync_instruments.h"
#include "opentelemetry/nostd/function_ref.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/data/point_data.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/sdk/metrics/view/instrument_selector.h"
#include "opentelemetry/sdk/metrics/view/meter_selector.h"
#include "opentelemetry/sdk/metrics/view/view.h"

#if OPENTELEMETRY_HAVE_WORKING_REGEX

using namespace opentelemetry;
using namespace opentelemetry::sdk::instrumentationscope;
using namespace opentelemetry::sdk::metrics;

TEST(HistogramInstrumentToHistogramAggregation, Double)
{
  MeterProvider mp;
  auto m = mp.GetMeter("meter1", "version1", "schema1");

  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  auto h = m->CreateDoubleHistogram("histogram1", "histogram1_description", "histogram1_unit");

  h->Record(5, {});
  h->Record(10, {});
  h->Record(15, {});
  h->Record(20, {});
  h->Record(25, {});
  h->Record(30, {});
  h->Record(35, {});
  h->Record(40, {});
  h->Record(45, {});
  h->Record(50, {});
  h->Record(1e6, {});

  std::vector<HistogramPointData> actuals;
  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        for (const PointDataAttributes &dp : md.point_data_attr_)
        {
          actuals.push_back(opentelemetry::nostd::get<HistogramPointData>(dp.point_data));
        }
      }
    }
    return true;
  });

  ASSERT_EQ(1, actuals.size());
  const auto &actual = actuals.at(0);
  ASSERT_EQ(1000275.0, opentelemetry::nostd::get<double>(actual.sum_));
  ASSERT_EQ(11, actual.count_);
}

TEST(CounterToHistogram, Double)
{
  MeterProvider mp;
  auto m = mp.GetMeter("meter1", "version1", "schema1");

  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  std::unique_ptr<View> view{new View("view1", "view1_description", AggregationType::kHistogram)};
  std::unique_ptr<InstrumentSelector> instrument_selector{
      new InstrumentSelector(InstrumentType::kCounter, "counter1", "unit")};
  std::unique_ptr<MeterSelector> meter_selector{new MeterSelector("meter1", "version1", "schema1")};
  mp.AddView(std::move(instrument_selector), std::move(meter_selector), std::move(view));

  auto h = m->CreateDoubleCounter("counter1", "counter1_description", "unit");

  h->Add(5, {});
  h->Add(10, {});
  h->Add(15, {});
  h->Add(20, {});
  h->Add(25, {});
  h->Add(30, {});
  h->Add(35, {});
  h->Add(40, {});
  h->Add(45, {});
  h->Add(50, {});
  h->Add(1e6, {});

  std::vector<HistogramPointData> actuals;
  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        for (const PointDataAttributes &dp : md.point_data_attr_)
        {
          actuals.push_back(opentelemetry::nostd::get<HistogramPointData>(dp.point_data));
        }
      }
    }
    return true;
  });

  ASSERT_EQ(1, actuals.size());
  const auto &actual = actuals.at(0);
  ASSERT_EQ(1000275.0, opentelemetry::nostd::get<double>(actual.sum_));
  ASSERT_EQ(11, actual.count_);
}

namespace
{

void CollectPoints(MetricReader &reader, std::vector<Base2ExponentialHistogramPointData> &out)
{
  reader.Collect([&](ResourceMetrics &resource_metrics) {
    for (const ScopeMetrics &scope_metrics : resource_metrics.scope_metric_data_)
    {
      for (const MetricData &metric_data : scope_metrics.metric_data_)
      {
        for (const PointDataAttributes &point_data_attributes : metric_data.point_data_attr_)
        {
          out.push_back(opentelemetry::nostd::get<Base2ExponentialHistogramPointData>(
              point_data_attributes.point_data));
        }
      }
    }
    return true;
  });
}

uint64_t SumAllBuckets(const Base2ExponentialHistogramPointData &point)
{
  uint64_t total = point.zero_count_;
  if (point.positive_buckets_ && !point.positive_buckets_->Empty())
  {
    for (int32_t index = point.positive_buckets_->StartIndex();
         index <= point.positive_buckets_->EndIndex(); ++index)
    {
      total += point.positive_buckets_->Get(index);
    }
  }
  if (point.negative_buckets_ && !point.negative_buckets_->Empty())
  {
    for (int32_t index = point.negative_buckets_->StartIndex();
         index <= point.negative_buckets_->EndIndex(); ++index)
    {
      total += point.negative_buckets_->Get(index);
    }
  }
  return total;
}

void RecordValues(opentelemetry::metrics::Histogram<double> &histogram)
{
  histogram.Record(0.5, {});
  histogram.Record(1.0, {});
  histogram.Record(2.0, {});
  histogram.Record(4.0, {});
  histogram.Record(8.0, {});
  histogram.Record(16.0, {});
  histogram.Record(32.0, {});
  histogram.Record(64.0, {});
  histogram.Record(128.0, {});
  histogram.Record(256.0, {});
}

std::shared_ptr<MeterProvider> MakeBase2ExponentialHistogramViewProvider(
    const std::shared_ptr<MetricReader> &reader)
{
  auto meter_provider = std::make_shared<MeterProvider>();
  meter_provider->AddMetricReader(reader);

  Base2ExponentialHistogramAggregationConfig config;
  config.max_scale_   = 5;
  config.max_buckets_ = 160;
  auto view =
      std::make_unique<View>("exponential_histogram", "exponential_histogram_description",
                             AggregationType::kBase2ExponentialHistogram,
                             std::make_shared<Base2ExponentialHistogramAggregationConfig>(config));
  auto instrument_selector =
      std::make_unique<InstrumentSelector>(InstrumentType::kHistogram, "histogram", "unit");
  auto meter_selector = std::make_unique<MeterSelector>("meter1", "version1", "schema1");
  meter_provider->AddView(std::move(instrument_selector), std::move(meter_selector),
                          std::move(view));
  return meter_provider;
}

}  // namespace

TEST(Base2ExponentialHistogramView, CumulativeTemporality)
{
  auto exporter       = std::make_unique<MockMetricExporter>(AggregationTemporality::kCumulative);
  auto reader         = std::make_shared<MockMetricReader>(std::move(exporter));
  auto meter_provider = MakeBase2ExponentialHistogramViewProvider(reader);

  auto meter     = meter_provider->GetMeter("meter1", "version1", "schema1");
  auto histogram = meter->CreateDoubleHistogram("histogram", "histogram_description", "unit");

  RecordValues(*histogram);

  std::vector<Base2ExponentialHistogramPointData> first_collection;
  CollectPoints(*reader, first_collection);
  ASSERT_EQ(first_collection.size(), 1u);
  EXPECT_EQ(first_collection[0].count_, 10u);
  EXPECT_EQ(SumAllBuckets(first_collection[0]), 10u);
  EXPECT_LT(first_collection[0].scale_, 5);

  RecordValues(*histogram);

  std::vector<Base2ExponentialHistogramPointData> second_collection;
  CollectPoints(*reader, second_collection);
  ASSERT_EQ(second_collection.size(), 1u);
  EXPECT_EQ(second_collection[0].count_, 20u);
  EXPECT_EQ(SumAllBuckets(second_collection[0]), 20u);
}

TEST(Base2ExponentialHistogramView, DeltaTemporality)
{
  auto exporter       = std::make_unique<MockMetricExporter>(AggregationTemporality::kDelta);
  auto reader         = std::make_shared<MockMetricReader>(std::move(exporter));
  auto meter_provider = MakeBase2ExponentialHistogramViewProvider(reader);

  auto meter     = meter_provider->GetMeter("meter1", "version1", "schema1");
  auto histogram = meter->CreateDoubleHistogram("histogram", "histogram_description", "unit");

  RecordValues(*histogram);

  std::vector<Base2ExponentialHistogramPointData> first_collection;
  CollectPoints(*reader, first_collection);
  ASSERT_EQ(first_collection.size(), 1u);
  EXPECT_EQ(first_collection[0].count_, 10u);
  EXPECT_EQ(SumAllBuckets(first_collection[0]), 10u);
  EXPECT_LT(first_collection[0].scale_, 5);

  RecordValues(*histogram);

  std::vector<Base2ExponentialHistogramPointData> second_collection;
  CollectPoints(*reader, second_collection);
  ASSERT_EQ(second_collection.size(), 1u);
  EXPECT_EQ(second_collection[0].count_, 10u);
  EXPECT_EQ(SumAllBuckets(second_collection[0]), 10u);
}
#endif
