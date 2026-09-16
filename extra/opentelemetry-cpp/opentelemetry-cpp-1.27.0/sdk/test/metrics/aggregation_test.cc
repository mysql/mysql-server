// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/sdk/metrics/aggregation/aggregation.h"
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <limits>
#include <string>
#include <vector>
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/metrics/aggregation/aggregation_config.h"
#include "opentelemetry/sdk/metrics/aggregation/base2_exponential_histogram_aggregation.h"
#include "opentelemetry/sdk/metrics/aggregation/default_aggregation.h"
#include "opentelemetry/sdk/metrics/aggregation/histogram_aggregation.h"
#include "opentelemetry/sdk/metrics/aggregation/lastvalue_aggregation.h"
#include "opentelemetry/sdk/metrics/aggregation/sum_aggregation.h"
#include "opentelemetry/sdk/metrics/data/circular_buffer.h"
#include "opentelemetry/sdk/metrics/data/point_data.h"
#include "opentelemetry/sdk/metrics/instruments.h"

using namespace opentelemetry::sdk::metrics;
namespace nostd = opentelemetry::nostd;

namespace
{

uint64_t SumAllBuckets(const Base2ExponentialHistogramPointData &point)
{
  uint64_t total = 0;
  if (point.positive_buckets_ && !point.positive_buckets_->Empty())
  {
    for (int32_t i = point.positive_buckets_->StartIndex();
         i <= point.positive_buckets_->EndIndex(); ++i)
    {
      total += point.positive_buckets_->Get(i);
    }
  }
  if (point.negative_buckets_ && !point.negative_buckets_->Empty())
  {
    for (int32_t i = point.negative_buckets_->StartIndex();
         i <= point.negative_buckets_->EndIndex(); ++i)
    {
      total += point.negative_buckets_->Get(i);
    }
  }
  return total;
}

Base2ExponentialHistogramAggregationConfig MakeAggregationConfig(int max_scale, size_t max_buckets)
{
  Base2ExponentialHistogramAggregationConfig config;
  config.max_scale_   = max_scale;
  config.max_buckets_ = max_buckets;
  return config;
}

Base2ExponentialHistogramPointData MakePointData(const Aggregation &a)
{
  return nostd::get<Base2ExponentialHistogramPointData>(a.ToPoint());
}

void ExpectCountInvariant(uint64_t expected_count,
                          const Base2ExponentialHistogramPointData &point,
                          nostd::string_view label = "")
{
  SCOPED_TRACE(label);
  EXPECT_EQ(point.count_, expected_count);
  EXPECT_EQ(point.count_, point.zero_count_ + SumAllBuckets(point));
}

void PopulateAggregation(Base2ExponentialHistogramAggregation &aggr,
                         double start,
                         double factor,
                         int count)
{
  // Populate an aggregation with values spread across several orders of magnitude to force a scale
  // reduction
  double v = start;
  for (int i = 0; i < count; ++i)
  {
    aggr.Aggregate(v, {});
    v *= factor;
  }
}

// Two sample ranges overlap in ~0.1 to ~1.0, exercising per-bucket addition (merge) and
// per-bucket subtraction (diff).
// Sample A: ~1e-4 to ~1.0
// Sample B: ~0.1 to ~2.4e4.
constexpr double kSampleAStart  = 1e-4;
constexpr double kSampleAFactor = 1.3;
constexpr int kSampleACount     = 36;
constexpr double kSampleBStart  = 0.1;
constexpr double kSampleBFactor = 1.5;
constexpr int kSampleBCount     = 30;

}  // namespace

TEST(Aggregation, LongSumAggregation)
{
  LongSumAggregation aggr(true);
  auto data = aggr.ToPoint();
  ASSERT_TRUE(nostd::holds_alternative<SumPointData>(data));
  auto sum_data = nostd::get<SumPointData>(data);
  ASSERT_TRUE(nostd::holds_alternative<int64_t>(sum_data.value_));
  EXPECT_EQ(nostd::get<int64_t>(sum_data.value_), 0);
  aggr.Aggregate(static_cast<int64_t>(12), {});
  aggr.Aggregate(static_cast<int64_t>(0), {});
  sum_data = nostd::get<SumPointData>(aggr.ToPoint());
  EXPECT_EQ(nostd::get<int64_t>(sum_data.value_), 12);
}

TEST(Aggregation, DoubleSumAggregation)
{
  DoubleSumAggregation aggr(true);
  auto data = aggr.ToPoint();
  ASSERT_TRUE(nostd::holds_alternative<SumPointData>(data));
  auto sum_data = nostd::get<SumPointData>(data);
  ASSERT_TRUE(nostd::holds_alternative<double>(sum_data.value_));
  EXPECT_EQ(nostd::get<double>(sum_data.value_), 0);
  aggr.Aggregate(12.0, {});
  aggr.Aggregate(1.0, {});
  sum_data = nostd::get<SumPointData>(aggr.ToPoint());
  EXPECT_EQ(nostd::get<double>(sum_data.value_), 13.0);
}

TEST(Aggregation, LongLastValueAggregation)
{
  LongLastValueAggregation aggr;
  auto data = aggr.ToPoint();
  ASSERT_TRUE(nostd::holds_alternative<LastValuePointData>(data));
  auto lastvalue_data = nostd::get<LastValuePointData>(data);
  ASSERT_TRUE(nostd::holds_alternative<int64_t>(lastvalue_data.value_));
  EXPECT_EQ(lastvalue_data.is_lastvalue_valid_, false);
  aggr.Aggregate(static_cast<int64_t>(12), {});
  aggr.Aggregate(static_cast<int64_t>(1), {});
  lastvalue_data = nostd::get<LastValuePointData>(aggr.ToPoint());
  EXPECT_EQ(nostd::get<int64_t>(lastvalue_data.value_), 1.0);
}

TEST(Aggregation, DoubleLastValueAggregation)
{
  DoubleLastValueAggregation aggr;
  auto data = aggr.ToPoint();
  ASSERT_TRUE(nostd::holds_alternative<LastValuePointData>(data));
  auto lastvalue_data = nostd::get<LastValuePointData>(data);
  ASSERT_TRUE(nostd::holds_alternative<double>(lastvalue_data.value_));
  EXPECT_EQ(lastvalue_data.is_lastvalue_valid_, false);
  aggr.Aggregate(12.0, {});
  aggr.Aggregate(1.0, {});
  lastvalue_data = nostd::get<LastValuePointData>(aggr.ToPoint());
  EXPECT_EQ(nostd::get<double>(lastvalue_data.value_), 1.0);
}

TEST(Aggregation, LongHistogramAggregation)
{
  LongHistogramAggregation aggr;
  auto data = aggr.ToPoint();
  ASSERT_TRUE(nostd::holds_alternative<HistogramPointData>(data));
  auto histogram_data = nostd::get<HistogramPointData>(data);
  ASSERT_TRUE(nostd::holds_alternative<int64_t>(histogram_data.sum_));
  EXPECT_EQ(nostd::get<int64_t>(histogram_data.sum_), 0);
  EXPECT_EQ(histogram_data.count_, 0);
  aggr.Aggregate(static_cast<int64_t>(12), {});   // lies in third bucket
  aggr.Aggregate(static_cast<int64_t>(100), {});  // lies in sixth bucket
  histogram_data = nostd::get<HistogramPointData>(aggr.ToPoint());
  EXPECT_EQ(nostd::get<int64_t>(histogram_data.min_), 12);
  EXPECT_EQ(nostd::get<int64_t>(histogram_data.max_), 100);
  EXPECT_EQ(nostd::get<int64_t>(histogram_data.sum_), 112);
  EXPECT_EQ(histogram_data.count_, 2);
  EXPECT_EQ(histogram_data.counts_[3], 1);
  EXPECT_EQ(histogram_data.counts_[6], 1);
  aggr.Aggregate(static_cast<int64_t>(13), {});   // lies in third bucket
  aggr.Aggregate(static_cast<int64_t>(252), {});  // lies in eight bucket
  histogram_data = nostd::get<HistogramPointData>(aggr.ToPoint());
  EXPECT_EQ(histogram_data.count_, 4);
  EXPECT_EQ(histogram_data.counts_[3], 2);
  EXPECT_EQ(histogram_data.counts_[8], 1);
  EXPECT_EQ(nostd::get<int64_t>(histogram_data.min_), 12);
  EXPECT_EQ(nostd::get<int64_t>(histogram_data.max_), 252);

  // Merge
  LongHistogramAggregation aggr1;
  aggr1.Aggregate(static_cast<int64_t>(1), {});
  aggr1.Aggregate(static_cast<int64_t>(11), {});
  aggr1.Aggregate(static_cast<int64_t>(26), {});

  LongHistogramAggregation aggr2;
  aggr2.Aggregate(static_cast<int64_t>(2), {});
  aggr2.Aggregate(static_cast<int64_t>(3), {});
  aggr2.Aggregate(static_cast<int64_t>(13), {});
  aggr2.Aggregate(static_cast<int64_t>(28), {});
  aggr2.Aggregate(static_cast<int64_t>(105), {});

  auto aggr3     = aggr1.Merge(aggr2);
  histogram_data = nostd::get<HistogramPointData>(aggr3->ToPoint());

  EXPECT_EQ(histogram_data.count_, 8);      // 3 each from aggr1 and aggr2
  EXPECT_EQ(histogram_data.counts_[1], 3);  // 1, 2, 3
  EXPECT_EQ(histogram_data.counts_[3], 2);  // 11, 13
  EXPECT_EQ(histogram_data.counts_[4], 2);  // 25, 28
  EXPECT_EQ(histogram_data.counts_[7], 1);  // 105
  EXPECT_EQ(nostd::get<int64_t>(histogram_data.min_), 1);
  EXPECT_EQ(nostd::get<int64_t>(histogram_data.max_), 105);

  // Diff
  auto aggr4     = aggr1.Diff(aggr2);
  histogram_data = nostd::get<HistogramPointData>(aggr4->ToPoint());
  EXPECT_EQ(histogram_data.count_, 2);      // aggr2:5 - aggr1:3
  EXPECT_EQ(histogram_data.counts_[1], 1);  // aggr2(2, 3) - aggr1(1)
  EXPECT_EQ(histogram_data.counts_[3], 0);  // aggr2(13) - aggr1(11)
  EXPECT_EQ(histogram_data.counts_[4], 0);  // aggr2(28) - aggr1(25)
  EXPECT_EQ(histogram_data.counts_[7], 1);  // aggr2(105) - aggr1(0)
}

TEST(Aggregation, LongHistogramAggregationBoundaries)
{
  std::shared_ptr<opentelemetry::sdk::metrics::HistogramAggregationConfig> aggregation_config{
      new opentelemetry::sdk::metrics::HistogramAggregationConfig};
  std::vector<double> user_boundaries = {0.0,   50.0,   100.0,  250.0,  500.0,
                                         750.0, 1000.0, 2500.0, 5000.0, 10000.0};
  aggregation_config->boundaries_     = user_boundaries;
  LongHistogramAggregation aggr{aggregation_config.get()};
  auto data = aggr.ToPoint();
  ASSERT_TRUE(nostd::holds_alternative<HistogramPointData>(data));
  auto histogram_data = nostd::get<HistogramPointData>(data);
  EXPECT_EQ(histogram_data.boundaries_, user_boundaries);
}

TEST(Aggregation, DoubleHistogramAggregationBoundaries)
{
  std::shared_ptr<opentelemetry::sdk::metrics::HistogramAggregationConfig> aggregation_config{
      new opentelemetry::sdk::metrics::HistogramAggregationConfig};
  std::vector<double> user_boundaries = {0.0,   50.0,   100.0,  250.0,  500.0,
                                         750.0, 1000.0, 2500.0, 5000.0, 10000.0};
  aggregation_config->boundaries_     = user_boundaries;
  DoubleHistogramAggregation aggr{aggregation_config.get()};
  auto data = aggr.ToPoint();
  ASSERT_TRUE(nostd::holds_alternative<HistogramPointData>(data));
  auto histogram_data = nostd::get<HistogramPointData>(data);
  EXPECT_EQ(histogram_data.boundaries_, user_boundaries);
}

TEST(Aggregation, DoubleHistogramAggregation)
{
  DoubleHistogramAggregation aggr;
  auto data = aggr.ToPoint();
  ASSERT_TRUE(nostd::holds_alternative<HistogramPointData>(data));
  auto histogram_data = nostd::get<HistogramPointData>(data);
  ASSERT_TRUE(nostd::holds_alternative<double>(histogram_data.sum_));
  EXPECT_EQ(nostd::get<double>(histogram_data.sum_), 0);
  EXPECT_EQ(histogram_data.count_, 0);
  aggr.Aggregate(12.0, {});   // lies in third bucket
  aggr.Aggregate(100.0, {});  // lies in sixth bucket
  histogram_data = nostd::get<HistogramPointData>(aggr.ToPoint());
  EXPECT_EQ(nostd::get<double>(histogram_data.sum_), 112);
  EXPECT_EQ(histogram_data.count_, 2);
  EXPECT_EQ(histogram_data.counts_[3], 1);
  EXPECT_EQ(histogram_data.counts_[6], 1);
  EXPECT_EQ(nostd::get<double>(histogram_data.min_), 12);
  EXPECT_EQ(nostd::get<double>(histogram_data.max_), 100);
  aggr.Aggregate(13.0, {});   // lies in third bucket
  aggr.Aggregate(252.0, {});  // lies in eight bucket
  histogram_data = nostd::get<HistogramPointData>(aggr.ToPoint());
  EXPECT_EQ(histogram_data.count_, 4);
  EXPECT_EQ(histogram_data.counts_[3], 2);
  EXPECT_EQ(histogram_data.counts_[8], 1);
  EXPECT_EQ(nostd::get<double>(histogram_data.sum_), 377);
  EXPECT_EQ(nostd::get<double>(histogram_data.min_), 12);
  EXPECT_EQ(nostd::get<double>(histogram_data.max_), 252);

  // Merge
  DoubleHistogramAggregation aggr1;
  aggr1.Aggregate(1.0, {});
  aggr1.Aggregate(11.0, {});
  aggr1.Aggregate(25.1, {});

  DoubleHistogramAggregation aggr2;
  aggr2.Aggregate(2.0, {});
  aggr2.Aggregate(3.0, {});
  aggr2.Aggregate(13.0, {});
  aggr2.Aggregate(28.1, {});
  aggr2.Aggregate(105.0, {});

  auto aggr3     = aggr1.Merge(aggr2);
  histogram_data = nostd::get<HistogramPointData>(aggr3->ToPoint());

  EXPECT_EQ(histogram_data.count_, 8);      // 3 each from aggr1 and aggr2
  EXPECT_EQ(histogram_data.counts_[1], 3);  // 1.0, 2.0, 3.0
  EXPECT_EQ(histogram_data.counts_[3], 2);  // 11.0, 13.0
  EXPECT_EQ(histogram_data.counts_[4], 2);  // 25.1, 28.1
  EXPECT_EQ(histogram_data.counts_[7], 1);  // 105.0
  EXPECT_EQ(nostd::get<double>(histogram_data.min_), 1);
  EXPECT_EQ(nostd::get<double>(histogram_data.max_), 105);

  // Diff
  auto aggr4     = aggr1.Diff(aggr2);
  histogram_data = nostd::get<HistogramPointData>(aggr4->ToPoint());
  EXPECT_EQ(histogram_data.count_, 2);      // aggr2:5 - aggr1:3
  EXPECT_EQ(histogram_data.counts_[1], 1);  // aggr2(2.0, 3.0) - aggr1(1.0)
  EXPECT_EQ(histogram_data.counts_[3], 0);  // aggr2(13.0) - aggr1(11.0)
  EXPECT_EQ(histogram_data.counts_[4], 0);  // aggr2(28.1) - aggr1(25.1)
  EXPECT_EQ(histogram_data.counts_[7], 1);  // aggr2(105.0) - aggr1(0)
}

TEST(Aggregation, Base2ExponentialHistogramAggregation)
{
  // Low res histo
  auto SCALE0       = 0;
  auto MAX_BUCKETS0 = 7;
  Base2ExponentialHistogramAggregationConfig scale0_config;
  scale0_config.max_scale_      = SCALE0;
  scale0_config.max_buckets_    = MAX_BUCKETS0;
  scale0_config.record_min_max_ = true;
  Base2ExponentialHistogramAggregation scale0_aggr(&scale0_config);
  auto point = scale0_aggr.ToPoint();
  ASSERT_TRUE(nostd::holds_alternative<Base2ExponentialHistogramPointData>(point));
  auto histo_point = nostd::get<Base2ExponentialHistogramPointData>(point);
  EXPECT_EQ(histo_point.count_, 0);
  EXPECT_EQ(histo_point.sum_, 0.0);
  EXPECT_EQ(histo_point.zero_count_, 0);
  EXPECT_EQ(histo_point.min_, (std::numeric_limits<double>::max)());
  EXPECT_EQ(histo_point.max_, (std::numeric_limits<double>::min)());
  EXPECT_EQ(histo_point.scale_, SCALE0);
  EXPECT_EQ(histo_point.max_buckets_, MAX_BUCKETS0);
  ASSERT_TRUE(histo_point.positive_buckets_ != nullptr);
  ASSERT_TRUE(histo_point.negative_buckets_ != nullptr);
  ASSERT_TRUE(histo_point.positive_buckets_->Empty());
  ASSERT_TRUE(histo_point.negative_buckets_->Empty());

  // Create a new aggreagte based in point data
  {
    const auto &point_data = histo_point;
    Base2ExponentialHistogramAggregation scale0_aggr2(point_data);
    scale0_aggr2.Aggregate(0.0, {});

    auto histo_point2 = nostd::get<Base2ExponentialHistogramPointData>(point);
    EXPECT_EQ(histo_point2.count_, 0);
    EXPECT_EQ(histo_point2.sum_, 0.0);
    EXPECT_EQ(histo_point2.zero_count_, 0);
    EXPECT_EQ(histo_point2.min_, (std::numeric_limits<double>::max)());
    EXPECT_EQ(histo_point2.max_, (std::numeric_limits<double>::min)());
    EXPECT_EQ(histo_point2.scale_, SCALE0);
    EXPECT_EQ(histo_point2.max_buckets_, MAX_BUCKETS0);
    ASSERT_TRUE(histo_point2.positive_buckets_->Empty());
    ASSERT_TRUE(histo_point2.negative_buckets_->Empty());
  }

  // zero point
  scale0_aggr.Aggregate(static_cast<int64_t>(0.0), {});
  histo_point = nostd::get<Base2ExponentialHistogramPointData>(scale0_aggr.ToPoint());
  EXPECT_EQ(histo_point.count_, 1);
  EXPECT_EQ(histo_point.zero_count_, 1);

  // Two recordings in the same bucket (bucket 1 at scale 0)
  scale0_aggr.Aggregate(3.0, {});
  scale0_aggr.Aggregate(3.5, {});
  histo_point = nostd::get<Base2ExponentialHistogramPointData>(scale0_aggr.ToPoint());
  EXPECT_EQ(histo_point.count_, 3);
  EXPECT_EQ(histo_point.sum_, 6.5);
  EXPECT_EQ(histo_point.min_, 0.0);
  EXPECT_EQ(histo_point.max_, 3.5);
  ASSERT_TRUE(histo_point.positive_buckets_ != nullptr);
  ASSERT_TRUE(histo_point.negative_buckets_ != nullptr);
  ASSERT_FALSE(histo_point.positive_buckets_->Empty());
  auto start_index = histo_point.positive_buckets_->StartIndex();
  auto end_index   = histo_point.positive_buckets_->EndIndex();
  EXPECT_EQ(start_index, 1);
  EXPECT_EQ(end_index, 1);
  EXPECT_EQ(histo_point.positive_buckets_->Get(start_index), 2);

  // Recording in a different bucket (bucket -2 at scale 0)
  scale0_aggr.Aggregate(-0.3, {});
  histo_point = nostd::get<Base2ExponentialHistogramPointData>(scale0_aggr.ToPoint());
  EXPECT_EQ(histo_point.count_, 4);
  EXPECT_EQ(histo_point.sum_, 6.2);
  EXPECT_EQ(histo_point.min_, -0.3);
  EXPECT_EQ(histo_point.max_, 3.5);
  ASSERT_TRUE(histo_point.positive_buckets_ != nullptr);
  ASSERT_TRUE(histo_point.negative_buckets_ != nullptr);
  EXPECT_EQ(histo_point.negative_buckets_->Get(-2), 1);
  EXPECT_EQ(histo_point.positive_buckets_->Get(1), 2);

  Base2ExponentialHistogramAggregationConfig scale1_config;
  scale1_config.max_scale_      = 1;
  scale1_config.max_buckets_    = 14;
  scale1_config.record_min_max_ = true;
  Base2ExponentialHistogramAggregation scale1_aggr(&scale1_config);

  scale1_aggr.Aggregate(0.0, {});
  scale1_aggr.Aggregate(3.0, {});
  scale1_aggr.Aggregate(3.5, {});
  scale1_aggr.Aggregate(0.3, {});
  auto scale1_point = nostd::get<Base2ExponentialHistogramPointData>(scale1_aggr.ToPoint());
  EXPECT_EQ(scale1_point.count_, 4);
  EXPECT_EQ(scale1_point.sum_, 6.8);
  EXPECT_EQ(scale1_point.zero_count_, 1);
  EXPECT_EQ(scale1_point.min_, 0.0);
  EXPECT_EQ(scale1_point.max_, 3.5);

  // Merge test
  auto merged       = scale0_aggr.Merge(scale1_aggr);
  auto merged_point = nostd::get<Base2ExponentialHistogramPointData>(merged->ToPoint());
  EXPECT_EQ(merged_point.count_, 8);
  EXPECT_EQ(merged_point.sum_, 13.0);
  EXPECT_EQ(merged_point.zero_count_, 2);
  EXPECT_EQ(merged_point.min_, -0.3);
  EXPECT_EQ(merged_point.max_, 3.5);
  EXPECT_EQ(merged_point.scale_, 0);
  ASSERT_TRUE(merged_point.positive_buckets_ != nullptr);
  ASSERT_TRUE(merged_point.negative_buckets_ != nullptr);
  EXPECT_EQ(merged_point.positive_buckets_->Get(1), 4);
  EXPECT_EQ(merged_point.negative_buckets_->Get(-2), 1);
  EXPECT_EQ(merged_point.positive_buckets_->Get(2), 0);

  // Diff test
  Base2ExponentialHistogramAggregation scale2_aggr(&scale1_config);
  Base2ExponentialHistogramAggregation scale3_aggr(&scale1_config);
  scale2_aggr.Aggregate(2.0, {});
  scale2_aggr.Aggregate(4.0, {});
  scale2_aggr.Aggregate(2.5, {});

  scale3_aggr.Aggregate(2.0, {});
  scale3_aggr.Aggregate(2.3, {});
  scale3_aggr.Aggregate(2.5, {});
  scale3_aggr.Aggregate(4.0, {});

  auto diffd       = scale2_aggr.Diff(scale3_aggr);
  auto diffd_point = nostd::get<Base2ExponentialHistogramPointData>(diffd->ToPoint());
  EXPECT_EQ(diffd_point.count_, 1);
  EXPECT_NEAR(diffd_point.sum_, 2.3, 1e-9);
  EXPECT_EQ(diffd_point.zero_count_, 0);
  EXPECT_EQ(diffd_point.scale_, 1);
  ASSERT_TRUE(diffd_point.positive_buckets_ != nullptr);
  ASSERT_TRUE(diffd_point.negative_buckets_ != nullptr);
  EXPECT_EQ(diffd_point.positive_buckets_->Get(2), 1);
}

TEST(Aggregation, Base2ExponentialHistogramAggregationMerge)
{
  Base2ExponentialHistogramAggregationConfig config;
  config.max_scale_      = 10;
  config.max_buckets_    = 100;
  config.record_min_max_ = true;

  Base2ExponentialHistogramAggregation aggr(&config);

  int expected_count  = 0;
  double expected_sum = 0.0;

  // Aggregate some small values
  for (int i = 1; i < 10; ++i)
  {
    expected_count++;
    const double value = i * 1e-12;
    expected_sum += value;
    aggr.Aggregate(value);
  }

  const auto aggr_point = nostd::get<Base2ExponentialHistogramPointData>(aggr.ToPoint());

  ASSERT_EQ(aggr_point.count_, expected_count);
  ASSERT_DOUBLE_EQ(aggr_point.sum_, expected_sum);
  ASSERT_EQ(aggr_point.zero_count_, 0);
  ASSERT_GT(aggr_point.scale_, -10);
  ASSERT_EQ(aggr_point.max_buckets_, config.max_buckets_);

  auto test_merge = [](const std::unique_ptr<Aggregation> &merged_aggr, int expected_count,
                       double expected_sum, int expected_zero_count, int expected_scale,
                       size_t expected_max_buckets) {
    auto merged_point = nostd::get<Base2ExponentialHistogramPointData>(merged_aggr->ToPoint());
    EXPECT_EQ(merged_point.count_, expected_count);
    EXPECT_DOUBLE_EQ(merged_point.sum_, expected_sum);
    EXPECT_EQ(merged_point.zero_count_, expected_zero_count);
    EXPECT_EQ(merged_point.scale_, expected_scale);
    EXPECT_EQ(merged_point.max_buckets_, expected_max_buckets);
  };

  // default aggregation merge
  {
    InstrumentDescriptor descriptor;
    descriptor.type_        = InstrumentType::kHistogram;
    descriptor.unit_        = "unit";
    descriptor.name_        = "histogram";
    descriptor.description_ = "a histogram";
    descriptor.value_type_  = InstrumentValueType::kDouble;

    auto default_aggr = DefaultAggregation::CreateAggregation(
        AggregationType::kBase2ExponentialHistogram, descriptor);
    auto default_point = nostd::get<Base2ExponentialHistogramPointData>(default_aggr->ToPoint());

    const int expected_scale =
        aggr_point.scale_ < default_point.scale_ ? aggr_point.scale_ : default_point.scale_;
    const size_t expected_max_buckets = aggr_point.max_buckets_ < default_point.max_buckets_
                                            ? aggr_point.max_buckets_
                                            : default_point.max_buckets_;
    const int expected_zero_count     = 0;

    auto merged_from_default = aggr.Merge(*default_aggr);
    test_merge(merged_from_default, expected_count, expected_sum, expected_zero_count,
               expected_scale, expected_max_buckets);

    auto merged_to_default = default_aggr->Merge(aggr);
    test_merge(merged_to_default, expected_count, expected_sum, expected_zero_count, expected_scale,
               expected_max_buckets);
  }

  // zero count aggregation merge (Zero is a special case and does not increment the buckets)
  {
    Base2ExponentialHistogramAggregation zero_aggr(&config);
    zero_aggr.Aggregate(0.0);

    const auto zero_point = nostd::get<Base2ExponentialHistogramPointData>(zero_aggr.ToPoint());
    const int expected_scale =
        aggr_point.scale_ < zero_point.scale_ ? aggr_point.scale_ : zero_point.scale_;
    const size_t expected_max_buckets = aggr_point.max_buckets_ < zero_point.max_buckets_
                                            ? aggr_point.max_buckets_
                                            : zero_point.max_buckets_;
    const int expected_zero_count     = 1;

    auto merged_from_zero = aggr.Merge(zero_aggr);
    test_merge(merged_from_zero, expected_count + 1, expected_sum, expected_zero_count,
               expected_scale, expected_max_buckets);

    auto merged_to_zero = zero_aggr.Merge(aggr);
    test_merge(merged_to_zero, expected_count + 1, expected_sum, expected_zero_count,
               expected_scale, expected_max_buckets);
  }
}

// Test that verifies the count invariant is maintained during merge operations that require
// downscaling. The invariant is: count == zero_count + sum(positive_bucket_counts) +
// sum(negative_bucket_counts)
//
TEST(Aggregation, Base2ExponentialHistogramAggregationMergeCountInvariant)
{
  // Use scale 0 for easy bucket index reasoning: value 2^N -> index N-1
  // Use max_buckets=5 so we can trigger the bug with small powers of 2
  const auto config = MakeAggregationConfig(0, 5);

  std::unique_ptr<Aggregation> cumulative =
      std::make_unique<Base2ExponentialHistogramAggregation>(&config);

  uint64_t expected_count = 0;

  // === Cycle 1: values 2,4,8,16,32 -> indices 0,1,2,3,4 (5 buckets, fits in max_buckets=5) ===
  {
    Base2ExponentialHistogramAggregation delta(&config);
    delta.Aggregate(2.0, {});   // 2^1 -> index 0
    delta.Aggregate(4.0, {});   // 2^2 -> index 1
    delta.Aggregate(8.0, {});   // 2^3 -> index 2
    delta.Aggregate(16.0, {});  // 2^4 -> index 3
    delta.Aggregate(32.0, {});  // 2^5 -> index 4
    expected_count += 5;

    cumulative = cumulative->Merge(delta);

    auto point = nostd::get<Base2ExponentialHistogramPointData>(cumulative->ToPoint());
    ExpectCountInvariant(expected_count, point, "Cycle 1: indices 0-4");

    // Verify bucket positions
    EXPECT_EQ(point.positive_buckets_->StartIndex(), 0);
    EXPECT_EQ(point.positive_buckets_->EndIndex(), 4);
  }

  // === Cycle 2: values 4,8,16,32,64 -> indices 1,2,3,4,5 (5 buckets, fits individually) ===
  // But combined with Cycle 1: indices 0-5 = 6 buckets = max_buckets + 1
  // This triggers the off-by-one bug as of commit
  // https://github.com/open-telemetry/opentelemetry-cpp/commit/5fc4707a8b7820f6bdbc782ccdffac7ccafbe80d.
  {
    Base2ExponentialHistogramAggregation delta(&config);
    delta.Aggregate(4.0, {});   // 2^2 -> index 1
    delta.Aggregate(8.0, {});   // 2^3 -> index 2
    delta.Aggregate(16.0, {});  // 2^4 -> index 3
    delta.Aggregate(32.0, {});  // 2^5 -> index 4
    delta.Aggregate(64.0, {});  // 2^6 -> index 5
    expected_count += 5;

    cumulative = cumulative->Merge(delta);

    auto point = nostd::get<Base2ExponentialHistogramPointData>(cumulative->ToPoint());
    ExpectCountInvariant(expected_count, point, "Cycle 2: indices 1-5");
  }

  // === Cycle 3: values 8,16,32,64,128 -> indices 2,3,4,5,6 ===
  {
    Base2ExponentialHistogramAggregation delta(&config);
    delta.Aggregate(8.0, {});    // 2^3 -> index 2
    delta.Aggregate(16.0, {});   // 2^4 -> index 3
    delta.Aggregate(32.0, {});   // 2^5 -> index 4
    delta.Aggregate(64.0, {});   // 2^6 -> index 5
    delta.Aggregate(128.0, {});  // 2^7 -> index 6
    expected_count += 5;

    cumulative = cumulative->Merge(delta);

    auto point = nostd::get<Base2ExponentialHistogramPointData>(cumulative->ToPoint());
    ExpectCountInvariant(expected_count, point, "Cycle 3: indices 2-6");
  }

  // === Cycle 4: values 16,32,64,128,256 -> indices 3,4,5,6,7 ===
  {
    Base2ExponentialHistogramAggregation delta(&config);
    delta.Aggregate(16.0, {});   // 2^4 -> index 3
    delta.Aggregate(32.0, {});   // 2^5 -> index 4
    delta.Aggregate(64.0, {});   // 2^6 -> index 5
    delta.Aggregate(128.0, {});  // 2^7 -> index 6
    delta.Aggregate(256.0, {});  // 2^8 -> index 7
    expected_count += 5;

    cumulative = cumulative->Merge(delta);

    auto point = nostd::get<Base2ExponentialHistogramPointData>(cumulative->ToPoint());
    ExpectCountInvariant(expected_count, point, "Cycle 4: indices 3-7");
  }

  // === Cycle 5: values 32,64,128,256,512 -> indices 4,5,6,7,8 ===
  {
    Base2ExponentialHistogramAggregation delta(&config);
    delta.Aggregate(32.0, {});   // 2^5 -> index 4
    delta.Aggregate(64.0, {});   // 2^6 -> index 5
    delta.Aggregate(128.0, {});  // 2^7 -> index 6
    delta.Aggregate(256.0, {});  // 2^8 -> index 7
    delta.Aggregate(512.0, {});  // 2^9 -> index 8
    expected_count += 5;

    cumulative = cumulative->Merge(delta);

    auto point = nostd::get<Base2ExponentialHistogramPointData>(cumulative->ToPoint());
    ExpectCountInvariant(expected_count, point, "Cycle 5: indices 4-8");
  }

  // === Cycle 6: values 64,128,256,512,1024 -> indices 5,6,7,8,9 ===
  {
    Base2ExponentialHistogramAggregation delta(&config);
    delta.Aggregate(64.0, {});    // 2^6 -> index 5
    delta.Aggregate(128.0, {});   // 2^7 -> index 6
    delta.Aggregate(256.0, {});   // 2^8 -> index 7
    delta.Aggregate(512.0, {});   // 2^9 -> index 8
    delta.Aggregate(1024.0, {});  // 2^10 -> index 9
    expected_count += 5;

    cumulative = cumulative->Merge(delta);

    auto point = nostd::get<Base2ExponentialHistogramPointData>(cumulative->ToPoint());
    ExpectCountInvariant(expected_count, point, "Cycle 6: indices 5-9");
  }

  // === Cycle 7: zeros only -> zero_count_ grows, buckets unchanged ===
  {
    Base2ExponentialHistogramAggregation delta(&config);
    delta.Aggregate(0.0, {});
    delta.Aggregate(0.0, {});
    delta.Aggregate(0.0, {});
    expected_count += 3;

    cumulative = cumulative->Merge(delta);

    auto point = nostd::get<Base2ExponentialHistogramPointData>(cumulative->ToPoint());
    ExpectCountInvariant(expected_count, point, "Cycle 7: zeros");
    EXPECT_GE(point.zero_count_, 3u);
  }

  // === Cycle 8: small positives -> positive bucket range spans both negative and positive indices,
  // requires scale reduction ===
  {
    Base2ExponentialHistogramAggregation delta(&config);
    delta.Aggregate(1.0 / 64.0, {});  // 2^-6 -> index -7 at scale 0
    delta.Aggregate(1.0 / 32.0, {});  // 2^-5 -> index -6 at scale 0
    delta.Aggregate(1.0 / 16.0, {});  // 2^-4 -> index -5 at scale 0
    expected_count += 3;

    cumulative = cumulative->Merge(delta);

    auto point = nostd::get<Base2ExponentialHistogramPointData>(cumulative->ToPoint());
    ExpectCountInvariant(expected_count, point, "Cycle 8: small positives");
  }

  // === Cycle 9: large negatives -> populate negative_buckets_ [0..4] for the first time ===
  {
    Base2ExponentialHistogramAggregation delta(&config);
    delta.Aggregate(-2.0, {});   // |2^1|  -> negative index 0
    delta.Aggregate(-4.0, {});   // |2^2|  -> negative index 1
    delta.Aggregate(-8.0, {});   // |2^3|  -> negative index 2
    delta.Aggregate(-16.0, {});  // |2^4|  -> negative index 3
    delta.Aggregate(-32.0, {});  // |2^5|  -> negative index 4
    expected_count += 5;

    cumulative = cumulative->Merge(delta);

    auto point = nostd::get<Base2ExponentialHistogramPointData>(cumulative->ToPoint());
    ExpectCountInvariant(expected_count, point, "Cycle 9: large negatives");
    ASSERT_TRUE(point.negative_buckets_ && !point.negative_buckets_->Empty());
  }

  // === Cycle 10: small negatives -> negative bucket range spans both negative and positive
  // indices, requires scale reduction ===
  {
    Base2ExponentialHistogramAggregation delta(&config);
    delta.Aggregate(-1.0 / 64.0, {});  // -> negative index -7
    delta.Aggregate(-1.0 / 32.0, {});  // -> negative index -6
    delta.Aggregate(-1.0 / 16.0, {});  // -> negative index -5
    expected_count += 3;

    cumulative = cumulative->Merge(delta);

    auto point = nostd::get<Base2ExponentialHistogramPointData>(cumulative->ToPoint());
    ExpectCountInvariant(expected_count, point, "Cycle 10: small negatives");
  }

  // === Cycle 11: mixed-sign + zero -> all three accumulators populated on both sides ===
  {
    Base2ExponentialHistogramAggregation delta(&config);
    delta.Aggregate(0.0, {});
    delta.Aggregate(2048.0, {});        // large positive
    delta.Aggregate(-2048.0, {});       // large negative
    delta.Aggregate(1.0 / 128.0, {});   // small positive
    delta.Aggregate(-1.0 / 128.0, {});  // small negative
    expected_count += 5;

    cumulative = cumulative->Merge(delta);

    auto point = nostd::get<Base2ExponentialHistogramPointData>(cumulative->ToPoint());
    ExpectCountInvariant(expected_count, point, "Cycle 11: mixed-sign + zero");
  }

  // === Cycle 12: merge into empty aggregation ===
  {
    Base2ExponentialHistogramAggregation empty_aggregation(&config);
    auto remerged = empty_aggregation.Merge(*cumulative);

    auto point = nostd::get<Base2ExponentialHistogramPointData>(remerged->ToPoint());
    ExpectCountInvariant(expected_count, point, "Cycle 12: merge into empty");
  }
}

TEST(Aggregation, Base2ExponentialHistogramAggregationMergeIndexCrossesZero)
{
  const auto config = MakeAggregationConfig(0, 5);
  Base2ExponentialHistogramAggregation small_aggr(&config);
  small_aggr.Aggregate(1.0 / 64.0, {});  // 2^-6 -> index -7
  small_aggr.Aggregate(1.0 / 32.0, {});  // 2^-5 -> index -6

  Base2ExponentialHistogramAggregation large_aggr(&config);
  large_aggr.Aggregate(4.0, {});   // index 1
  large_aggr.Aggregate(8.0, {});   // index 2
  large_aggr.Aggregate(16.0, {});  // index 3

  // Verify that each side individually fits within max_buckets at scale 0.
  EXPECT_EQ(MakePointData(small_aggr).scale_, 0);
  EXPECT_EQ(MakePointData(large_aggr).scale_, 0);

  const auto merged_point = MakePointData(*small_aggr.Merge(large_aggr));

  EXPECT_EQ(merged_point.zero_count_, 0u);
  ExpectCountInvariant(5u, merged_point, "MergeIndexCrossesZero");
  EXPECT_LT(merged_point.scale_, 0);
}

TEST(Aggregation, Base2ExponentialHistogramAggregationMergeNegativeIndexCrossesZero)
{
  const auto config = MakeAggregationConfig(0, 5);

  // At scale 0: |v| = 2^k -> negative_buckets_[k-1].
  Base2ExponentialHistogramAggregation small_aggr(&config);
  small_aggr.Aggregate(-1.0 / 64.0, {});  // index -7
  small_aggr.Aggregate(-1.0 / 32.0, {});  // index -6

  Base2ExponentialHistogramAggregation large_aggr(&config);
  large_aggr.Aggregate(-4.0, {});   // index 1
  large_aggr.Aggregate(-8.0, {});   // index 2
  large_aggr.Aggregate(-16.0, {});  // index 3

  // Combined range [-7..3] = 11 > max_buckets_=5.
  const auto merged_point = MakePointData(*small_aggr.Merge(large_aggr));

  EXPECT_EQ(merged_point.zero_count_, 0u);
  ExpectCountInvariant(5u, merged_point, "MergeNegativeIndexCrossesZero");
  EXPECT_LT(merged_point.scale_, 0);
}

TEST(Aggregation, Base2ExponentialHistogramAggregationMergeMixedSignAsymmetricSpan)
{
  const auto config = MakeAggregationConfig(0, 5);

  Base2ExponentialHistogramAggregation aggr_a(&config);
  aggr_a.Aggregate(0.5, {});
  aggr_a.Aggregate(0.25, {});
  aggr_a.Aggregate(-16.0, {});
  aggr_a.Aggregate(-8.0, {});

  Base2ExponentialHistogramAggregation aggr_b(&config);
  aggr_b.Aggregate(8.0, {});
  aggr_b.Aggregate(4.0, {});
  aggr_b.Aggregate(-1.0 / 32.0, {});
  aggr_b.Aggregate(-1.0 / 64.0, {});

  const auto merged_point = MakePointData(*aggr_a.Merge(aggr_b));

  EXPECT_EQ(merged_point.zero_count_, 0u);
  ExpectCountInvariant(8u, merged_point, "MergeMixedSignAsymmetricSpan");
  EXPECT_EQ(merged_point.scale_, -2);
}

TEST(Aggregation, Base2ExponentialHistogramAggregationDiffIndexCrossesZero)
{
  // Diff's union [-7..3] (11 > max_buckets_=5) forces a cross-zero downscale,
  // mirroring MergeIndexCrossesZero. right is not a superset of left, so we
  // check downscale + bucket sum instead of the count invariant.
  const auto config = MakeAggregationConfig(0, 5);

  Base2ExponentialHistogramAggregation left(&config);
  left.Aggregate(1.0 / 64.0, {});  // index -7
  left.Aggregate(1.0 / 32.0, {});  // index -6
  EXPECT_EQ(MakePointData(left).scale_, 0);

  Base2ExponentialHistogramAggregation right(&config);
  right.Aggregate(4.0, {});   // index 1
  right.Aggregate(8.0, {});   // index 2
  right.Aggregate(16.0, {});  // index 3
  EXPECT_EQ(MakePointData(right).scale_, 0);

  const auto diffed_point = MakePointData(*left.Diff(right));

  EXPECT_EQ(diffed_point.zero_count_, 0u);
  EXPECT_LT(diffed_point.scale_, 0);
  ASSERT_TRUE(diffed_point.positive_buckets_ != nullptr);
  EXPECT_EQ(SumAllBuckets(diffed_point), 3u);
}

TEST(Aggregation, Base2ExponentialHistogramAggregationDiffAsymmetricEmptyBuckets)
{
  const auto config = MakeAggregationConfig(0, 5);

  Base2ExponentialHistogramAggregation left(&config);
  left.Aggregate(2.0, {});

  Base2ExponentialHistogramAggregation right(&config);
  right.Aggregate(-2.0, {});

  const auto diffed_point = MakePointData(*left.Diff(right));
  EXPECT_EQ(diffed_point.scale_, 0);
}

TEST(Aggregation, Base2ExponentialHistogramAggregationDiffEmptyEmpty)
{
  const auto config = MakeAggregationConfig(0, 5);

  Base2ExponentialHistogramAggregation left(&config);
  Base2ExponentialHistogramAggregation right(&config);

  const auto diffed_point = MakePointData(*left.Diff(right));

  EXPECT_EQ(diffed_point.sum_, 0.0);
  EXPECT_EQ(diffed_point.zero_count_, 0u);
  EXPECT_EQ(SumAllBuckets(diffed_point), 0u);
  ExpectCountInvariant(0u, diffed_point, "DiffEmptyEmpty");
}

TEST(Aggregation, Base2ExponentialHistogramAggregationDiffNoOp)
{
  const auto config = MakeAggregationConfig(0, 5);

  Base2ExponentialHistogramAggregation left(&config);
  left.Aggregate(2.0, {});
  left.Aggregate(-2.0, {});
  left.Aggregate(0.0, {});

  auto right = MakePointData(left);
  Base2ExponentialHistogramAggregation right_aggr(right);

  const auto diffed_point = MakePointData(*left.Diff(right_aggr));

  EXPECT_EQ(diffed_point.sum_, 0.0);
  EXPECT_EQ(diffed_point.zero_count_, 0u);
  EXPECT_EQ(SumAllBuckets(diffed_point), 0u);
  ExpectCountInvariant(0u, diffed_point, "DiffNoOp");
}

TEST(Aggregation, Base2ExponentialHistogramAggregationDiffUnionDownscale)
{
  const auto config = MakeAggregationConfig(0, 5);

  Base2ExponentialHistogramAggregation left(&config);
  left.Aggregate(2.0, {});  // index 0

  Base2ExponentialHistogramAggregation extra(&config);
  extra.Aggregate(128.0, {});  // index 6

  // right = left merged with extra. Together they span indices 0..6 (7 buckets),
  // more than max_buckets_=5, so Diff() must reduce the scale.
  auto right          = left.Merge(extra);
  const auto left_pt  = MakePointData(left);
  const auto right_pt = MakePointData(*right);
  auto diffed_point   = MakePointData(*left.Diff(*right));

  EXPECT_LT(diffed_point.scale_, 0);
  ExpectCountInvariant(1u, diffed_point, "DiffUnionDownscale");
}

TEST(Aggregation, Base2ExponentialHistogramAggregationDiffScaleMismatch)
{
  const auto config = MakeAggregationConfig(0, 5);

  Base2ExponentialHistogramAggregation left(&config);
  left.Aggregate(2.0, {});

  Base2ExponentialHistogramAggregation large(&config);
  large.Aggregate(1.0 / 64.0, {});
  large.Aggregate(1.0 / 32.0, {});
  large.Aggregate(4.0, {});
  large.Aggregate(8.0, {});
  large.Aggregate(16.0, {});
  large.Aggregate(128.0, {});

  const auto right    = left.Merge(large);
  const auto right_pt = MakePointData(*right);
  const auto left_pt  = MakePointData(left);
  ASSERT_LT(right_pt.scale_, 0);

  const auto diffed_point = MakePointData(*left.Diff(*right));

  ExpectCountInvariant(right_pt.count_ - 1u, diffed_point, "DiffScaleMismatch");
}

TEST(Aggregation, Base2ExponentialHistogramAggregationDefaultConfigMerge)
{
  Base2ExponentialHistogramAggregationConfig config;
  Base2ExponentialHistogramAggregation a(&config);
  Base2ExponentialHistogramAggregation b(&config);

  PopulateAggregation(a, kSampleAStart, kSampleAFactor, kSampleACount);
  PopulateAggregation(b, kSampleBStart, kSampleBFactor, kSampleBCount);

  auto pa = MakePointData(a);
  auto pb = MakePointData(b);
  ExpectCountInvariant(pa.count_, pa, "a");
  ExpectCountInvariant(pb.count_, pb, "b");
  EXPECT_LE(pa.scale_, 20);
  EXPECT_LE(pb.scale_, 20);
  EXPECT_EQ(pa.max_buckets_, config.max_buckets_);
  EXPECT_GT(pa.max_, pa.min_);
  EXPECT_GT(pb.max_, pb.min_);

  const auto merged = MakePointData(*a.Merge(b));
  ExpectCountInvariant(pa.count_ + pb.count_, merged, "DefaultConfigMerge");
  EXPECT_DOUBLE_EQ(merged.sum_, pa.sum_ + pb.sum_);
  EXPECT_EQ(merged.min_, (std::min)(pa.min_, pb.min_));
  EXPECT_EQ(merged.max_, (std::max)(pa.max_, pb.max_));
  EXPECT_EQ(SumAllBuckets(merged), SumAllBuckets(pa) + SumAllBuckets(pb));
}

TEST(Aggregation, Base2ExponentialHistogramAggregationDefaultConfigDiff)
{
  Base2ExponentialHistogramAggregationConfig config;
  Base2ExponentialHistogramAggregation a(&config);
  Base2ExponentialHistogramAggregation b(&config);

  PopulateAggregation(a, kSampleAStart, kSampleAFactor, kSampleACount);
  PopulateAggregation(b, kSampleBStart, kSampleBFactor, kSampleBCount);

  const auto pa = MakePointData(a);
  const auto pb = MakePointData(b);

  const auto ab     = a.Merge(b);
  const auto merged = MakePointData(*ab);
  const auto diffed = MakePointData(*a.Diff(*ab));
  ExpectCountInvariant(pb.count_, diffed, "DefaultConfigDiff");
  EXPECT_EQ(SumAllBuckets(diffed), SumAllBuckets(merged) - SumAllBuckets(pa));
}
