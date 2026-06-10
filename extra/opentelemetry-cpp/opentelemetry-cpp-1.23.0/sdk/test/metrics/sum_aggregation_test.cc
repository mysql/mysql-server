// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stddef.h>
#include <initializer_list>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "common.h"

#include "opentelemetry/common/macros.h"
#include "opentelemetry/context/context.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/sync_instruments.h"
#include "opentelemetry/nostd/function_ref.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/aggregation/aggregation_config.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/data/point_data.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/sdk/metrics/state/attributes_hashmap.h"
#include "opentelemetry/sdk/metrics/view/attributes_processor.h"
#include "opentelemetry/sdk/metrics/view/instrument_selector.h"
#include "opentelemetry/sdk/metrics/view/meter_selector.h"
#include "opentelemetry/sdk/metrics/view/view.h"

#if OPENTELEMETRY_HAVE_WORKING_REGEX

using namespace opentelemetry;
using namespace opentelemetry::sdk::instrumentationscope;
using namespace opentelemetry::sdk::metrics;

TEST(HistogramToSum, Double)
{
  MeterProvider mp;
  auto m                      = mp.GetMeter("meter1", "version1", "schema1");
  std::string instrument_unit = "ms";
  std::string instrument_name = "histogram1";
  std::string instrument_desc = "histogram metrics";

  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  std::unique_ptr<View> view{new View("view1", "view1_description", AggregationType::kSum)};
  std::unique_ptr<InstrumentSelector> instrument_selector{
      new InstrumentSelector(InstrumentType::kHistogram, instrument_name, instrument_unit)};
  std::unique_ptr<MeterSelector> meter_selector{new MeterSelector("meter1", "version1", "schema1")};
  mp.AddView(std::move(instrument_selector), std::move(meter_selector), std::move(view));

  auto h = m->CreateDoubleHistogram(instrument_name, instrument_desc, instrument_unit);

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

  std::vector<SumPointData> actuals;
  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        for (const PointDataAttributes &dp : md.point_data_attr_)
        {
          actuals.push_back(opentelemetry::nostd::get<SumPointData>(dp.point_data));
        }
      }
    }
    return true;
  });

  ASSERT_EQ(1, actuals.size());
  const auto &actual = actuals.at(0);
  ASSERT_EQ(1000275.0, opentelemetry::nostd::get<double>(actual.value_));
}

TEST(HistogramToSumFilterAttributes, Double)
{
  MeterProvider mp;
  auto m                      = mp.GetMeter("meter1", "version1", "schema1");
  std::string instrument_unit = "ms";
  std::string instrument_name = "histogram1";
  std::string instrument_desc = "histogram metrics";

  opentelemetry::sdk::metrics::FilterAttributeMap allowedattr;
  allowedattr["attr1"] = true;
  std::unique_ptr<opentelemetry::sdk::metrics::AttributesProcessor> attrproc{
      new opentelemetry::sdk::metrics::FilteringAttributesProcessor(allowedattr)};

  std::shared_ptr<opentelemetry::sdk::metrics::AggregationConfig> dummy_aggregation_config{
      new opentelemetry::sdk::metrics::AggregationConfig};
  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  std::unique_ptr<View> view{new View("view1", "view1_description", AggregationType::kSum,
                                      dummy_aggregation_config, std::move(attrproc))};
  std::unique_ptr<InstrumentSelector> instrument_selector{
      new InstrumentSelector(InstrumentType::kHistogram, instrument_name, instrument_unit)};
  std::unique_ptr<MeterSelector> meter_selector{new MeterSelector("meter1", "version1", "schema1")};
  mp.AddView(std::move(instrument_selector), std::move(meter_selector), std::move(view));

  auto h = m->CreateDoubleHistogram(instrument_name, instrument_desc, instrument_unit);
  std::unordered_map<std::string, std::string> attr1 = {{"attr1", "val1"}, {"attr2", "val2"}};
  std::unordered_map<std::string, std::string> attr2 = {{"attr1", "val1"}, {"attr2", "val2"}};
  h->Record(5, attr1, opentelemetry::context::Context{});
  h->Record(10, attr2, opentelemetry::context::Context{});

  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        EXPECT_EQ(1, md.point_data_attr_.size());
        if (md.point_data_attr_.size() == 1)
        {
          EXPECT_EQ(15.0, opentelemetry::nostd::get<double>(opentelemetry::nostd::get<SumPointData>(
                                                                md.point_data_attr_[0].point_data)
                                                                .value_));
          EXPECT_EQ(1, md.point_data_attr_[0].attributes.size());
          EXPECT_NE(md.point_data_attr_[0].attributes.end(),
                    md.point_data_attr_[0].attributes.find("attr1"));
        }
      }
    }
    return true;
  });
}

TEST(HistogramToSumFilterAttributesWithCardinalityLimit, Double)
{
  MeterProvider mp;
  auto m                      = mp.GetMeter("meter1", "version1", "schema1");
  std::string instrument_unit = "ms";
  std::string instrument_name = "histogram1";
  std::string instrument_desc = "histogram metrics";
  size_t cardinality_limit    = 10000;

  opentelemetry::sdk::metrics::FilterAttributeMap allowedattr;
  allowedattr["attr1"] = true;
  std::unique_ptr<opentelemetry::sdk::metrics::AttributesProcessor> attrproc{
      new opentelemetry::sdk::metrics::FilteringAttributesProcessor(allowedattr)};

  std::shared_ptr<opentelemetry::sdk::metrics::AggregationConfig> dummy_aggregation_config{
      new opentelemetry::sdk::metrics::AggregationConfig(cardinality_limit)};
  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  std::unique_ptr<View> view{new View("view1", "view1_description", AggregationType::kSum,
                                      dummy_aggregation_config, std::move(attrproc))};
  std::unique_ptr<InstrumentSelector> instrument_selector{
      new InstrumentSelector(InstrumentType::kHistogram, instrument_name, instrument_unit)};
  std::unique_ptr<MeterSelector> meter_selector{new MeterSelector("meter1", "version1", "schema1")};
  mp.AddView(std::move(instrument_selector), std::move(meter_selector), std::move(view));

  auto h = m->CreateDoubleHistogram(instrument_name, instrument_desc, instrument_unit);
  size_t total_metrics_times = 5;

  size_t agg_repeat_count = 5;
  for (size_t repeat = 0; repeat < agg_repeat_count; repeat++)
  {

    for (size_t times = 0; times < total_metrics_times; times++)
    {
      for (size_t i = 0; i < 2 * cardinality_limit; i++)
      {
        std::unordered_map<std::string, std::string> attr = {{"attr1", std::to_string(i)},
                                                             {"attr2", "val2"}};
        h->Record(1, attr, opentelemetry::context::Context{});
      }
    }

    reader->Collect([&](ResourceMetrics &rm) {
      for (const ScopeMetrics &smd : rm.scope_metric_data_)
      {
        for (const MetricData &md : smd.metric_data_)
        {
          // Something weird about attributes hashmap. If cardinality is setup to n, it emits n-1
          // including overflow. Just making the logic generic here to succeed for n or n-1 total
          // cardinality.
          EXPECT_GE(cardinality_limit, md.point_data_attr_.size());
          EXPECT_LT(cardinality_limit / 2, md.point_data_attr_.size());
          for (size_t i = 0; i < md.point_data_attr_.size(); i++)
          {
            EXPECT_EQ(1, md.point_data_attr_[i].attributes.size());
            if (md.point_data_attr_[i].attributes.end() !=
                md.point_data_attr_[i].attributes.find("attr1"))
            {
              EXPECT_EQ(total_metrics_times * (repeat + 1),
                        opentelemetry::nostd::get<double>(opentelemetry::nostd::get<SumPointData>(
                                                              md.point_data_attr_[i].point_data)
                                                              .value_));
            }
            else
            {
              EXPECT_NE(md.point_data_attr_[i].attributes.end(),
                        md.point_data_attr_[i].attributes.find(
                            sdk::metrics::kAttributesLimitOverflowKey));
            }
          }
        }
      }
      return true;
    });
  }
}

TEST(CounterToSum, Double)
{
  MeterProvider mp;
  auto m = mp.GetMeter("meter1", "version1", "schema1");

  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  std::unique_ptr<View> view{new View("view1", "view1_description", AggregationType::kSum)};
  std::unique_ptr<InstrumentSelector> instrument_selector{
      new InstrumentSelector(InstrumentType::kCounter, "counter1", "ms")};
  std::unique_ptr<MeterSelector> meter_selector{new MeterSelector("meter1", "version1", "schema1")};
  mp.AddView(std::move(instrument_selector), std::move(meter_selector), std::move(view));

  auto h = m->CreateDoubleCounter("counter1", "counter1_description", "counter1_unit");

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

  std::vector<SumPointData> actuals;
  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        for (const PointDataAttributes &dp : md.point_data_attr_)
        {
          actuals.push_back(opentelemetry::nostd::get<SumPointData>(dp.point_data));
        }
      }
    }
    return true;
  });

  ASSERT_EQ(1, actuals.size());
  const auto &actual = actuals.at(0);
  ASSERT_EQ(1000275.0, opentelemetry::nostd::get<double>(actual.value_));
}

TEST(CounterToSumFilterAttributes, Double)
{
  MeterProvider mp;
  auto m                      = mp.GetMeter("meter1", "version1", "schema1");
  std::string instrument_unit = "ms";
  std::string instrument_name = "counter1";
  std::string instrument_desc = "counter metrics";

  opentelemetry::sdk::metrics::FilterAttributeMap allowedattr;
  allowedattr["attr1"] = true;
  std::unique_ptr<opentelemetry::sdk::metrics::AttributesProcessor> attrproc{
      new opentelemetry::sdk::metrics::FilteringAttributesProcessor(allowedattr)};

  std::shared_ptr<opentelemetry::sdk::metrics::AggregationConfig> dummy_aggregation_config{
      new opentelemetry::sdk::metrics::AggregationConfig};
  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  std::unique_ptr<View> view{new View("view1", "view1_description", AggregationType::kSum,
                                      dummy_aggregation_config, std::move(attrproc))};
  std::unique_ptr<InstrumentSelector> instrument_selector{
      new InstrumentSelector(InstrumentType::kCounter, instrument_name, instrument_unit)};
  std::unique_ptr<MeterSelector> meter_selector{new MeterSelector("meter1", "version1", "schema1")};
  mp.AddView(std::move(instrument_selector), std::move(meter_selector), std::move(view));

  auto c = m->CreateDoubleCounter(instrument_name, instrument_desc, instrument_unit);
  std::unordered_map<std::string, std::string> attr1 = {{"attr1", "val1"}, {"attr2", "val2"}};
  std::unordered_map<std::string, std::string> attr2 = {{"attr1", "val1"}, {"attr2", "val2"}};
  c->Add(5, attr1, opentelemetry::context::Context{});
  c->Add(10, attr2, opentelemetry::context::Context{});

  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        EXPECT_EQ(1, md.point_data_attr_.size());
        if (md.point_data_attr_.size() == 1)
        {
          EXPECT_EQ(15.0, opentelemetry::nostd::get<double>(opentelemetry::nostd::get<SumPointData>(
                                                                md.point_data_attr_[0].point_data)
                                                                .value_));
          EXPECT_EQ(1, md.point_data_attr_[0].attributes.size());
          EXPECT_NE(md.point_data_attr_[0].attributes.end(),
                    md.point_data_attr_[0].attributes.find("attr1"));
        }
      }
    }
    return true;
  });
}

TEST(CounterToSumFilterAttributesWithCardinalityLimit, Double)
{
  MeterProvider mp;
  auto m                      = mp.GetMeter("meter1", "version1", "schema1");
  std::string instrument_unit = "ms";
  std::string instrument_name = "counter1";
  std::string instrument_desc = "counter metrics";
  size_t cardinality_limit    = 10000;

  opentelemetry::sdk::metrics::FilterAttributeMap allowedattr;
  allowedattr["attr1"] = true;
  std::unique_ptr<opentelemetry::sdk::metrics::AttributesProcessor> attrproc{
      new opentelemetry::sdk::metrics::FilteringAttributesProcessor(allowedattr)};

  std::shared_ptr<opentelemetry::sdk::metrics::AggregationConfig> dummy_aggregation_config{
      new opentelemetry::sdk::metrics::AggregationConfig(cardinality_limit)};
  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  std::unique_ptr<View> view{new View("view1", "view1_description", AggregationType::kSum,
                                      dummy_aggregation_config, std::move(attrproc))};
  std::unique_ptr<InstrumentSelector> instrument_selector{
      new InstrumentSelector(InstrumentType::kCounter, instrument_name, instrument_unit)};
  std::unique_ptr<MeterSelector> meter_selector{new MeterSelector("meter1", "version1", "schema1")};
  mp.AddView(std::move(instrument_selector), std::move(meter_selector), std::move(view));

  auto c = m->CreateDoubleCounter(instrument_name, instrument_desc, instrument_unit);

  size_t agg_repeat_count = 5;
  for (size_t repeat = 0; repeat < agg_repeat_count; repeat++)
  {
    size_t total_metrics_times = 5;

    for (size_t times = 0; times < total_metrics_times; times++)
    {
      for (size_t i = 0; i < 2 * cardinality_limit; i++)
      {
        std::unordered_map<std::string, std::string> attr = {{"attr1", std::to_string(i)},
                                                             {"attr2", "val2"}};
        c->Add(1, attr, opentelemetry::context::Context{});
      }
    }

    reader->Collect([&](ResourceMetrics &rm) {
      for (const ScopeMetrics &smd : rm.scope_metric_data_)
      {
        for (const MetricData &md : smd.metric_data_)
        {
          // When the number of unique attribute sets exceeds the cardinality limit, the
          // implementation emits up to (cardinality_limit - 1) unique sets and one overflow set,
          // resulting in a total of cardinality_limit sets. This test checks that the number of
          // emitted attribute sets is within the expected range, accounting for the overflow
          // behavior.
          EXPECT_GE(cardinality_limit, md.point_data_attr_.size());
          EXPECT_LT(cardinality_limit / 2, md.point_data_attr_.size());
          for (size_t i = 0; i < md.point_data_attr_.size(); i++)
          {
            EXPECT_EQ(1, md.point_data_attr_[i].attributes.size());
            if (md.point_data_attr_[i].attributes.find("attr1") !=
                md.point_data_attr_[i].attributes.end())
            {
              EXPECT_EQ(total_metrics_times * (repeat + 1),
                        opentelemetry::nostd::get<double>(opentelemetry::nostd::get<SumPointData>(
                                                              md.point_data_attr_[i].point_data)
                                                              .value_));
            }
            else
            {
              EXPECT_NE(md.point_data_attr_[i].attributes.end(),
                        md.point_data_attr_[i].attributes.find(
                            sdk::metrics::kAttributesLimitOverflowKey));
            }
          }
        }
      }
      return true;
    });
  }
}

class UpDownCounterToSumFixture : public ::testing::TestWithParam<bool>
{};

TEST_P(UpDownCounterToSumFixture, Double)
{
  bool is_matching_view = GetParam();
  MeterProvider mp;
  auto m                      = mp.GetMeter("meter1", "version1", "schema1");
  std::string instrument_name = "updowncounter1";
  std::string instrument_desc = "updowncounter desc";
  std::string instrument_unit = "ms";

  std::unique_ptr<MockMetricExporter> exporter(new MockMetricExporter());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  if (is_matching_view)
  {
    std::unique_ptr<View> view{new View("view1", "view1_description", AggregationType::kSum)};
    std::unique_ptr<InstrumentSelector> instrument_selector{
        new InstrumentSelector(InstrumentType::kUpDownCounter, instrument_name, instrument_unit)};
    std::unique_ptr<MeterSelector> meter_selector{
        new MeterSelector("meter1", "version1", "schema1")};
    mp.AddView(std::move(instrument_selector), std::move(meter_selector), std::move(view));
  }
  auto h = m->CreateDoubleUpDownCounter(instrument_name, instrument_desc, instrument_unit);

  h->Add(5, {});
  h->Add(10, {});
  h->Add(-15, {});
  h->Add(20, {});
  h->Add(25, {});
  h->Add(-30, {});

  std::vector<SumPointData> actuals;
  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        for (const PointDataAttributes &dp : md.point_data_attr_)
        {
          actuals.push_back(opentelemetry::nostd::get<SumPointData>(dp.point_data));
        }
      }
    }
    return true;
  });

  ASSERT_EQ(1, actuals.size());
  const auto &actual = actuals.at(0);
  ASSERT_EQ(15.0, opentelemetry::nostd::get<double>(actual.value_));
}
INSTANTIATE_TEST_SUITE_P(UpDownCounterToSum,
                         UpDownCounterToSumFixture,
                         ::testing::Values(true, false));
#endif
