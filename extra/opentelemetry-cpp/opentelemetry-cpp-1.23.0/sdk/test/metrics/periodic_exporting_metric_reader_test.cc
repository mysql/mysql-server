// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stddef.h>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <ratio>
#include <thread>
#include <utility>
#include <vector>

#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"

#if defined(_MSC_VER)
#  include "opentelemetry/sdk/common/env_variables.h"
using opentelemetry::sdk::common::setenv;
using opentelemetry::sdk::common::unsetenv;
#endif

using namespace opentelemetry;
using namespace opentelemetry::sdk::instrumentationscope;
using namespace opentelemetry::sdk::metrics;

class MockPushMetricExporter : public PushMetricExporter
{
public:
  MockPushMetricExporter(std::chrono::milliseconds wait) : wait_(wait) {}

  opentelemetry::sdk::common::ExportResult Export(const ResourceMetrics &record) noexcept override
  {
    if (wait_ > std::chrono::milliseconds::zero())
    {
      std::this_thread::sleep_for(wait_);
    }
    records_.push_back(record);
    return opentelemetry::sdk::common::ExportResult::kSuccess;
  }

  bool ForceFlush(std::chrono::microseconds /* timeout */) noexcept override { return false; }

  sdk::metrics::AggregationTemporality GetAggregationTemporality(
      sdk::metrics::InstrumentType /* instrument_type */) const noexcept override
  {
    return sdk::metrics::AggregationTemporality::kCumulative;
  }

  bool Shutdown(std::chrono::microseconds /* timeout */) noexcept override { return true; }

  size_t GetDataCount() { return records_.size(); }

private:
  std::vector<ResourceMetrics> records_;
  std::chrono::milliseconds wait_;
};

class MockMetricProducer : public MetricProducer
{
public:
  MockMetricProducer(std::chrono::microseconds sleep_ms = std::chrono::microseconds::zero())
      : sleep_ms_{sleep_ms}
  {}

  MetricProducer::Result Produce() noexcept override
  {
    std::this_thread::sleep_for(sleep_ms_);
    data_sent_size_++;
    ResourceMetrics data;
    return {data, MetricProducer::Status::kSuccess};
  }

  size_t GetDataCount() { return data_sent_size_; }

private:
  std::chrono::microseconds sleep_ms_;
  size_t data_sent_size_{0};
};

TEST(PeriodicExportingMetricReader, BasicTests)
{
  std::unique_ptr<PushMetricExporter> exporter(
      new MockPushMetricExporter(std::chrono::milliseconds{0}));
  PeriodicExportingMetricReaderOptions options;
  options.export_timeout_millis  = std::chrono::milliseconds(200);
  options.export_interval_millis = std::chrono::milliseconds(500);
  auto exporter_ptr              = exporter.get();
  std::shared_ptr<PeriodicExportingMetricReader> reader =
      std::make_shared<PeriodicExportingMetricReader>(std::move(exporter), options);
  MockMetricProducer producer;
  reader->SetMetricProducer(&producer);
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  reader->ForceFlush();
  reader->Shutdown();
  EXPECT_EQ(static_cast<MockPushMetricExporter *>(exporter_ptr)->GetDataCount(),
            static_cast<MockMetricProducer *>(&producer)->GetDataCount());
}

TEST(PeriodicExportingMetricReader, Timeout)
{
  std::unique_ptr<PushMetricExporter> exporter(
      new MockPushMetricExporter(std::chrono::milliseconds{2000}));
  PeriodicExportingMetricReaderOptions options;
  options.export_timeout_millis  = std::chrono::milliseconds(200);
  options.export_interval_millis = std::chrono::milliseconds(500);
  std::shared_ptr<PeriodicExportingMetricReader> reader =
      std::make_shared<PeriodicExportingMetricReader>(std::move(exporter), options);
  MockMetricProducer producer;
  reader->SetMetricProducer(&producer);
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  reader->Shutdown();
}

TEST(PeriodicExportingMetricReaderOptions, UsesEnvVars)
{
  const char *env_interval = "OTEL_METRIC_EXPORT_INTERVAL";
  const char *env_timeout  = "OTEL_METRIC_EXPORT_TIMEOUT";

  setenv(env_interval, "1500ms", 1);
  setenv(env_timeout, "1000ms", 1);

  PeriodicExportingMetricReaderOptions options;
  EXPECT_EQ(options.export_interval_millis, std::chrono::milliseconds(1500));
  EXPECT_EQ(options.export_timeout_millis, std::chrono::milliseconds(1000));

  unsetenv(env_interval);
  unsetenv(env_timeout);
}

TEST(PeriodicExportingMetricReaderOptions, UsesDefault)
{
  const char *env_interval = "OTEL_METRIC_EXPORT_INTERVAL";
  const char *env_timeout  = "OTEL_METRIC_EXPORT_TIMEOUT";

  unsetenv(env_interval);
  unsetenv(env_timeout);

  PeriodicExportingMetricReaderOptions options;
  EXPECT_EQ(options.export_interval_millis, std::chrono::milliseconds(60000));
  EXPECT_EQ(options.export_timeout_millis, std::chrono::milliseconds(30000));
}
