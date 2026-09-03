// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>

#include <chrono>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/exporters/otlp/otlp_http.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h"

#include "opentelemetry/nostd/string_view.h"

#include "opentelemetry/sdk/common/attribute_utils.h"
#include "opentelemetry/sdk/common/global_log_handler.h"

#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"

#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/data/point_data.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/resource/resource.h"

using InstrumentationScope = opentelemetry::sdk::instrumentationscope::InstrumentationScope;
using namespace opentelemetry::sdk::metrics;
using namespace opentelemetry::exporter::otlp;

// Create ResourceMetrics with a single metric containing 'n' data points
static ResourceMetrics MakeMetrics(std::size_t n_points)
{
  ResourceMetrics rm;
  static const opentelemetry::sdk::resource::Resource resource =
      opentelemetry::sdk::resource::Resource::Create({{"service.name", "metric-benchmark"}});
  rm.resource_ = &resource;

  static auto scope = InstrumentationScope::Create("benchmark", "1.0.0");
  ScopeMetrics scope_metrics;
  scope_metrics.scope_ = scope.get();

  MetricData metric;
  metric.instrument_descriptor.name_ = "cpu.utilization";
  metric.instrument_descriptor.unit_ = "1";
  metric.instrument_descriptor.type_ = InstrumentType::kCounter;
  metric.aggregation_temporality     = AggregationTemporality::kCumulative;

  for (std::size_t i = 0; i < n_points; ++i)
  {
    SumPointData sum;
    sum.value_        = static_cast<double>(i);
    sum.is_monotonic_ = true;

    PointDataAttributes p;
    p.attributes = {{"core", static_cast<int>(i)}};
    p.point_data = sum;

    metric.point_data_attr_.push_back(p);
  }

  scope_metrics.metric_data_.push_back(metric);
  rm.scope_metric_data_.push_back(scope_metrics);
  return rm;
}

// Benchmark Export() using Binary encoding
static void BM_OtlpHttpMetricExporter_Export_Binary(benchmark::State &state)
{
  OtlpHttpMetricExporterOptions opts;
  opts.url                       = "http://localhost:4318/v1/metrics";
  opts.content_type              = HttpRequestContentType::kBinary;
  opts.timeout                   = std::chrono::milliseconds(1);
  opts.retry_policy_max_attempts = 0;

  OtlpHttpMetricExporter exporter(opts);
  auto metrics = MakeMetrics(state.range(0));

  for (auto _ : state)
  {
    benchmark::DoNotOptimize(exporter.Export(metrics));
  }

  state.SetItemsProcessed(state.iterations() * state.range(0));
}

// Benchmark Export() using JSON encoding
static void BM_OtlpHttpMetricExporter_Export_Json(benchmark::State &state)
{
  OtlpHttpMetricExporterOptions opts;
  opts.url                       = "http://localhost:4318/v1/metrics";
  opts.content_type              = HttpRequestContentType::kJson;
  opts.timeout                   = std::chrono::milliseconds(1);
  opts.retry_policy_max_attempts = 0;

  OtlpHttpMetricExporter exporter(opts);
  auto metrics = MakeMetrics(state.range(0));

  for (auto _ : state)
  {
    benchmark::DoNotOptimize(exporter.Export(metrics));
  }

  state.SetItemsProcessed(state.iterations() * state.range(0));
}

BENCHMARK(BM_OtlpHttpMetricExporter_Export_Binary)
    ->RangeMultiplier(10)
    ->Range(1, 10000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_OtlpHttpMetricExporter_Export_Json)
    ->RangeMultiplier(10)
    ->Range(1, 10000)
    ->Unit(benchmark::kMicrosecond);

int main(int argc, char **argv)
{
  opentelemetry::sdk::common::internal_log::GlobalLogHandler::SetLogLevel(
      opentelemetry::sdk::common::internal_log::LogLevel::None);

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
