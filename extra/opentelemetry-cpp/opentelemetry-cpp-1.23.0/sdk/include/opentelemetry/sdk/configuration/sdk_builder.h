// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/always_off_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/base2_exponential_bucket_histogram_aggregation_configuration.h"
#include "opentelemetry/sdk/configuration/batch_log_record_processor_configuration.h"
#include "opentelemetry/sdk/configuration/batch_span_processor_configuration.h"
#include "opentelemetry/sdk/configuration/boolean_array_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/boolean_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/configuration.h"
#include "opentelemetry/sdk/configuration/configured_sdk.h"
#include "opentelemetry/sdk/configuration/console_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/console_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/double_array_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/double_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/explicit_bucket_histogram_aggregation_configuration.h"
#include "opentelemetry/sdk/configuration/extension_pull_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/extension_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/extension_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/extension_span_processor_configuration.h"
#include "opentelemetry/sdk/configuration/integer_array_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/integer_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/metric_reader_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_file_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_file_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_file_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_http_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_http_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_http_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/periodic_metric_reader_configuration.h"
#include "opentelemetry/sdk/configuration/prometheus_pull_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/pull_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/pull_metric_reader_configuration.h"
#include "opentelemetry/sdk/configuration/push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/registry.h"
#include "opentelemetry/sdk/configuration/simple_span_processor_configuration.h"
#include "opentelemetry/sdk/configuration/span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/string_array_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/string_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/zipkin_span_exporter_configuration.h"
#include "opentelemetry/sdk/logs/exporter.h"
#include "opentelemetry/sdk/logs/logger_provider.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/sdk/trace/processor.h"
#include "opentelemetry/sdk/trace/sampler.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/trace/tracer_provider.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class SdkBuilder
{
public:
  SdkBuilder(std::shared_ptr<Registry> registry) : registry_(std::move(registry)) {}
  SdkBuilder(SdkBuilder &&)                      = default;
  SdkBuilder(const SdkBuilder &)                 = default;
  SdkBuilder &operator=(SdkBuilder &&)           = default;
  SdkBuilder &operator=(const SdkBuilder &other) = default;
  ~SdkBuilder()                                  = default;

  std::unique_ptr<opentelemetry::sdk::trace::Sampler> CreateAlwaysOffSampler(
      const opentelemetry::sdk::configuration::AlwaysOffSamplerConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::trace::Sampler> CreateAlwaysOnSampler(
      const opentelemetry::sdk::configuration::AlwaysOnSamplerConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::trace::Sampler> CreateJaegerRemoteSampler(
      const opentelemetry::sdk::configuration::JaegerRemoteSamplerConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::trace::Sampler> CreateParentBasedSampler(
      const opentelemetry::sdk::configuration::ParentBasedSamplerConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::trace::Sampler> CreateTraceIdRatioBasedSampler(
      const opentelemetry::sdk::configuration::TraceIdRatioBasedSamplerConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::trace::Sampler> CreateExtensionSampler(
      const opentelemetry::sdk::configuration::ExtensionSamplerConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::trace::Sampler> CreateSampler(
      const std::unique_ptr<opentelemetry::sdk::configuration::SamplerConfiguration> &model) const;

  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> CreateOtlpHttpSpanExporter(
      const opentelemetry::sdk::configuration::OtlpHttpSpanExporterConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> CreateOtlpGrpcSpanExporter(
      const opentelemetry::sdk::configuration::OtlpGrpcSpanExporterConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> CreateOtlpFileSpanExporter(
      const opentelemetry::sdk::configuration::OtlpFileSpanExporterConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> CreateConsoleSpanExporter(
      const opentelemetry::sdk::configuration::ConsoleSpanExporterConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> CreateZipkinSpanExporter(
      const opentelemetry::sdk::configuration::ZipkinSpanExporterConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> CreateExtensionSpanExporter(
      const opentelemetry::sdk::configuration::ExtensionSpanExporterConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> CreateSpanExporter(
      const std::unique_ptr<opentelemetry::sdk::configuration::SpanExporterConfiguration> &model)
      const;

  std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> CreateBatchSpanProcessor(
      const opentelemetry::sdk::configuration::BatchSpanProcessorConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> CreateSimpleSpanProcessor(
      const opentelemetry::sdk::configuration::SimpleSpanProcessorConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> CreateExtensionSpanProcessor(
      const opentelemetry::sdk::configuration::ExtensionSpanProcessorConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> CreateSpanProcessor(
      const std::unique_ptr<opentelemetry::sdk::configuration::SpanProcessorConfiguration> &model)
      const;

  std::unique_ptr<opentelemetry::sdk::trace::TracerProvider> CreateTracerProvider(
      const std::unique_ptr<opentelemetry::sdk::configuration::TracerProviderConfiguration> &model,
      const opentelemetry::sdk::resource::Resource &resource) const;

  std::unique_ptr<opentelemetry::context::propagation::TextMapPropagator> CreateTextMapPropagator(
      const std::string &name) const;

  std::unique_ptr<opentelemetry::context::propagation::TextMapPropagator> CreatePropagator(
      const std::unique_ptr<opentelemetry::sdk::configuration::PropagatorConfiguration> &model)
      const;

  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> CreateOtlpHttpPushMetricExporter(
      const opentelemetry::sdk::configuration::OtlpHttpPushMetricExporterConfiguration *model)
      const;

  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> CreateOtlpGrpcPushMetricExporter(
      const opentelemetry::sdk::configuration::OtlpGrpcPushMetricExporterConfiguration *model)
      const;

  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> CreateOtlpFilePushMetricExporter(
      const opentelemetry::sdk::configuration::OtlpFilePushMetricExporterConfiguration *model)
      const;

  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> CreateConsolePushMetricExporter(
      const opentelemetry::sdk::configuration::ConsolePushMetricExporterConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
  CreateExtensionPushMetricExporter(
      const opentelemetry::sdk::configuration::ExtensionPushMetricExporterConfiguration *model)
      const;

  std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> CreatePrometheusPullMetricExporter(
      const opentelemetry::sdk::configuration::PrometheusPullMetricExporterConfiguration *model)
      const;

  std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> CreateExtensionPullMetricExporter(
      const opentelemetry::sdk::configuration::ExtensionPullMetricExporterConfiguration *model)
      const;

  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> CreatePushMetricExporter(
      const std::unique_ptr<opentelemetry::sdk::configuration::PushMetricExporterConfiguration>
          &model) const;

  std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> CreatePullMetricExporter(
      const std::unique_ptr<opentelemetry::sdk::configuration::PullMetricExporterConfiguration>
          &model) const;

  std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> CreatePeriodicMetricReader(
      const opentelemetry::sdk::configuration::PeriodicMetricReaderConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> CreatePullMetricReader(
      const opentelemetry::sdk::configuration::PullMetricReaderConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> CreateMetricReader(
      const std::unique_ptr<opentelemetry::sdk::configuration::MetricReaderConfiguration> &model)
      const;

  std::unique_ptr<opentelemetry::sdk::metrics::Base2ExponentialHistogramAggregationConfig>
  CreateBase2ExponentialBucketHistogramAggregation(
      const opentelemetry::sdk::configuration::
          Base2ExponentialBucketHistogramAggregationConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::metrics::HistogramAggregationConfig>
  CreateExplicitBucketHistogramAggregation(
      const opentelemetry::sdk::configuration::ExplicitBucketHistogramAggregationConfiguration
          *model) const;

  std::unique_ptr<opentelemetry::sdk::metrics::AggregationConfig> CreateAggregationConfig(
      const std::unique_ptr<opentelemetry::sdk::configuration::AggregationConfiguration> &model,
      opentelemetry::sdk::metrics::AggregationType &aggregation_type) const;

  std::unique_ptr<opentelemetry::sdk::metrics::AttributesProcessor> CreateAttributesProcessor(
      const std::unique_ptr<opentelemetry::sdk::configuration::IncludeExcludeConfiguration> &model)
      const;

  void AddView(
      opentelemetry::sdk::metrics::ViewRegistry *view_registry,
      const std::unique_ptr<opentelemetry::sdk::configuration::ViewConfiguration> &model) const;

  std::unique_ptr<opentelemetry::sdk::metrics::MeterProvider> CreateMeterProvider(
      const std::unique_ptr<opentelemetry::sdk::configuration::MeterProviderConfiguration> &model,
      const opentelemetry::sdk::resource::Resource &resource) const;

  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> CreateOtlpHttpLogRecordExporter(
      const opentelemetry::sdk::configuration::OtlpHttpLogRecordExporterConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> CreateOtlpGrpcLogRecordExporter(
      const opentelemetry::sdk::configuration::OtlpGrpcLogRecordExporterConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> CreateOtlpFileLogRecordExporter(
      const opentelemetry::sdk::configuration::OtlpFileLogRecordExporterConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> CreateConsoleLogRecordExporter(
      const opentelemetry::sdk::configuration::ConsoleLogRecordExporterConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> CreateExtensionLogRecordExporter(
      const opentelemetry::sdk::configuration::ExtensionLogRecordExporterConfiguration *model)
      const;

  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> CreateLogRecordExporter(
      const std::unique_ptr<opentelemetry::sdk::configuration::LogRecordExporterConfiguration>
          &model) const;

  std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor> CreateBatchLogRecordProcessor(
      const opentelemetry::sdk::configuration::BatchLogRecordProcessorConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor> CreateSimpleLogRecordProcessor(
      const opentelemetry::sdk::configuration::SimpleLogRecordProcessorConfiguration *model) const;

  std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor> CreateExtensionLogRecordProcessor(
      const opentelemetry::sdk::configuration::ExtensionLogRecordProcessorConfiguration *model)
      const;

  std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor> CreateLogRecordProcessor(
      const std::unique_ptr<opentelemetry::sdk::configuration::LogRecordProcessorConfiguration>
          &model) const;

  std::unique_ptr<opentelemetry::sdk::logs::LoggerProvider> CreateLoggerProvider(
      const std::unique_ptr<opentelemetry::sdk::configuration::LoggerProviderConfiguration> &model,
      const opentelemetry::sdk::resource::Resource &resource) const;

  void SetResourceAttribute(
      opentelemetry::sdk::resource::ResourceAttributes &resource_attributes,
      const std::string &name,
      const opentelemetry::sdk::configuration::AttributeValueConfiguration *model) const;

  void SetResource(opentelemetry::sdk::resource::Resource &resource,
                   const std::unique_ptr<opentelemetry::sdk::configuration::ResourceConfiguration>
                       &opt_model) const;

  std::unique_ptr<ConfiguredSdk> CreateConfiguredSdk(
      const std::unique_ptr<opentelemetry::sdk::configuration::Configuration> &model) const;

private:
  std::shared_ptr<Registry> registry_;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
