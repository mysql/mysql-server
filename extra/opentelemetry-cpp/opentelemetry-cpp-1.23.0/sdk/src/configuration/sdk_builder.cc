// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <stddef.h>
#include <stdint.h>
#include <chrono>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/kv_properties.h"
#include "opentelemetry/context/propagation/composite_propagator.h"
#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/configuration/aggregation_configuration.h"
#include "opentelemetry/sdk/configuration/aggregation_configuration_visitor.h"
#include "opentelemetry/sdk/configuration/always_off_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/always_on_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/attribute_value_configuration_visitor.h"
#include "opentelemetry/sdk/configuration/attributes_configuration.h"
#include "opentelemetry/sdk/configuration/base2_exponential_bucket_histogram_aggregation_configuration.h"
#include "opentelemetry/sdk/configuration/batch_log_record_processor_configuration.h"
#include "opentelemetry/sdk/configuration/batch_span_processor_configuration.h"
#include "opentelemetry/sdk/configuration/boolean_array_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/boolean_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/configuration.h"
#include "opentelemetry/sdk/configuration/configured_sdk.h"
#include "opentelemetry/sdk/configuration/console_log_record_exporter_builder.h"
#include "opentelemetry/sdk/configuration/console_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/console_push_metric_exporter_builder.h"
#include "opentelemetry/sdk/configuration/console_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/console_span_exporter_builder.h"
#include "opentelemetry/sdk/configuration/console_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/double_array_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/double_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/explicit_bucket_histogram_aggregation_configuration.h"
#include "opentelemetry/sdk/configuration/extension_log_record_exporter_builder.h"
#include "opentelemetry/sdk/configuration/extension_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/extension_log_record_processor_builder.h"
#include "opentelemetry/sdk/configuration/extension_log_record_processor_configuration.h"
#include "opentelemetry/sdk/configuration/extension_pull_metric_exporter_builder.h"
#include "opentelemetry/sdk/configuration/extension_pull_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/extension_push_metric_exporter_builder.h"
#include "opentelemetry/sdk/configuration/extension_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/extension_sampler_builder.h"
#include "opentelemetry/sdk/configuration/extension_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/extension_span_exporter_builder.h"
#include "opentelemetry/sdk/configuration/extension_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/extension_span_processor_builder.h"
#include "opentelemetry/sdk/configuration/extension_span_processor_configuration.h"
#include "opentelemetry/sdk/configuration/include_exclude_configuration.h"
#include "opentelemetry/sdk/configuration/instrument_type.h"
#include "opentelemetry/sdk/configuration/integer_array_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/integer_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/jaeger_remote_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/log_record_exporter_configuration_visitor.h"
#include "opentelemetry/sdk/configuration/log_record_processor_configuration.h"
#include "opentelemetry/sdk/configuration/log_record_processor_configuration_visitor.h"
#include "opentelemetry/sdk/configuration/logger_provider_configuration.h"
#include "opentelemetry/sdk/configuration/meter_provider_configuration.h"
#include "opentelemetry/sdk/configuration/metric_reader_configuration.h"
#include "opentelemetry/sdk/configuration/metric_reader_configuration_visitor.h"
#include "opentelemetry/sdk/configuration/otlp_file_log_record_exporter_builder.h"
#include "opentelemetry/sdk/configuration/otlp_file_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_file_push_metric_exporter_builder.h"
#include "opentelemetry/sdk/configuration/otlp_file_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_file_span_exporter_builder.h"
#include "opentelemetry/sdk/configuration/otlp_file_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_log_record_exporter_builder.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_push_metric_exporter_builder.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_span_exporter_builder.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_http_log_record_exporter_builder.h"
#include "opentelemetry/sdk/configuration/otlp_http_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_http_push_metric_exporter_builder.h"
#include "opentelemetry/sdk/configuration/otlp_http_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_http_span_exporter_builder.h"
#include "opentelemetry/sdk/configuration/otlp_http_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/parent_based_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/periodic_metric_reader_configuration.h"
#include "opentelemetry/sdk/configuration/prometheus_pull_metric_exporter_builder.h"
#include "opentelemetry/sdk/configuration/prometheus_pull_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/propagator_configuration.h"
#include "opentelemetry/sdk/configuration/pull_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/pull_metric_exporter_configuration_visitor.h"
#include "opentelemetry/sdk/configuration/pull_metric_reader_configuration.h"
#include "opentelemetry/sdk/configuration/push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/push_metric_exporter_configuration_visitor.h"
#include "opentelemetry/sdk/configuration/registry.h"
#include "opentelemetry/sdk/configuration/resource_configuration.h"
#include "opentelemetry/sdk/configuration/sampler_configuration.h"
#include "opentelemetry/sdk/configuration/sampler_configuration_visitor.h"
#include "opentelemetry/sdk/configuration/sdk_builder.h"
#include "opentelemetry/sdk/configuration/simple_log_record_processor_configuration.h"
#include "opentelemetry/sdk/configuration/simple_span_processor_configuration.h"
#include "opentelemetry/sdk/configuration/span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/span_exporter_configuration_visitor.h"
#include "opentelemetry/sdk/configuration/span_processor_configuration.h"
#include "opentelemetry/sdk/configuration/span_processor_configuration_visitor.h"
#include "opentelemetry/sdk/configuration/string_array_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/string_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/text_map_propagator_builder.h"
#include "opentelemetry/sdk/configuration/trace_id_ratio_based_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/tracer_provider_configuration.h"
#include "opentelemetry/sdk/configuration/unsupported_exception.h"
#include "opentelemetry/sdk/configuration/view_configuration.h"
#include "opentelemetry/sdk/configuration/view_selector_configuration.h"
#include "opentelemetry/sdk/configuration/view_stream_configuration.h"
#include "opentelemetry/sdk/configuration/zipkin_span_exporter_builder.h"
#include "opentelemetry/sdk/configuration/zipkin_span_exporter_configuration.h"
#include "opentelemetry/sdk/logs/batch_log_record_processor_factory.h"
#include "opentelemetry/sdk/logs/batch_log_record_processor_options.h"
#include "opentelemetry/sdk/logs/exporter.h"
#include "opentelemetry/sdk/logs/logger_provider.h"
#include "opentelemetry/sdk/logs/logger_provider_factory.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor_factory.h"
#include "opentelemetry/sdk/metrics/aggregation/aggregation_config.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/meter_context.h"
#include "opentelemetry/sdk/metrics/meter_context_factory.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/sdk/metrics/view/attributes_processor.h"
#include "opentelemetry/sdk/metrics/view/instrument_selector.h"
#include "opentelemetry/sdk/metrics/view/meter_selector.h"
#include "opentelemetry/sdk/metrics/view/view.h"
#include "opentelemetry/sdk/metrics/view/view_registry.h"
#include "opentelemetry/sdk/metrics/view/view_registry_factory.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/trace/batch_span_processor_factory.h"
#include "opentelemetry/sdk/trace/batch_span_processor_options.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/sdk/trace/processor.h"
#include "opentelemetry/sdk/trace/sampler.h"
#include "opentelemetry/sdk/trace/samplers/always_off_factory.h"
#include "opentelemetry/sdk/trace/samplers/always_on_factory.h"
#include "opentelemetry/sdk/trace/samplers/parent_factory.h"
#include "opentelemetry/sdk/trace/samplers/trace_id_ratio_factory.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class ResourceAttributeValueSetter
    : public opentelemetry::sdk::configuration::AttributeValueConfigurationVisitor
{
public:
  ResourceAttributeValueSetter(
      opentelemetry::sdk::resource::ResourceAttributes &resource_attributes,
      const std::string &name)
      : resource_attributes_(resource_attributes), name_(name)
  {}
  ResourceAttributeValueSetter(ResourceAttributeValueSetter &&)                      = delete;
  ResourceAttributeValueSetter(const ResourceAttributeValueSetter &)                 = delete;
  ResourceAttributeValueSetter &operator=(ResourceAttributeValueSetter &&)           = delete;
  ResourceAttributeValueSetter &operator=(const ResourceAttributeValueSetter &other) = delete;
  ~ResourceAttributeValueSetter() override                                           = default;

  void VisitString(
      const opentelemetry::sdk::configuration::StringAttributeValueConfiguration *model) override
  {
    opentelemetry::common::AttributeValue attribute_value(model->value);
    resource_attributes_.SetAttribute(name_, attribute_value);
  }

  void VisitInteger(
      const opentelemetry::sdk::configuration::IntegerAttributeValueConfiguration *model) override
  {
    opentelemetry::common::AttributeValue attribute_value(model->value);
    resource_attributes_.SetAttribute(name_, attribute_value);
  }

  void VisitDouble(
      const opentelemetry::sdk::configuration::DoubleAttributeValueConfiguration *model) override
  {
    opentelemetry::common::AttributeValue attribute_value(model->value);
    resource_attributes_.SetAttribute(name_, attribute_value);
  }

  void VisitBoolean(
      const opentelemetry::sdk::configuration::BooleanAttributeValueConfiguration *model) override
  {
    opentelemetry::common::AttributeValue attribute_value(model->value);
    resource_attributes_.SetAttribute(name_, attribute_value);
  }

  void VisitStringArray(
      const opentelemetry::sdk::configuration::StringArrayAttributeValueConfiguration *model)
      override
  {
    size_t length = model->value.size();
    std::vector<nostd::string_view> string_view_array(length);

    // We have: std::vector<std::string>
    // We need: nostd::span<const nostd::string_view>

    for (size_t i = 0; i < length; i++)
    {
      string_view_array[i] = model->value[i];
    }

    nostd::span<const nostd::string_view> span(string_view_array.data(), string_view_array.size());

    opentelemetry::common::AttributeValue attribute_value(span);
    resource_attributes_.SetAttribute(name_, attribute_value);
  }

  void VisitIntegerArray(
      const opentelemetry::sdk::configuration::IntegerArrayAttributeValueConfiguration *model)
      override
  {
    size_t length = model->value.size();
    std::vector<int64_t> int_array(length);

    // We have: std::vector<size_t>
    // We need: nostd::span<const int64_t>

    for (size_t i = 0; i < length; i++)
    {
      int_array[i] = model->value[i];
    }

    nostd::span<const int64_t> span(int_array.data(), int_array.size());

    opentelemetry::common::AttributeValue attribute_value(span);
    resource_attributes_.SetAttribute(name_, attribute_value);
  }

  void VisitDoubleArray(
      const opentelemetry::sdk::configuration::DoubleArrayAttributeValueConfiguration *model)
      override
  {
    // We have: std::vector<double>
    // We need: nostd::span<const double>
    // so no data conversion needed

    nostd::span<const double> span(model->value.data(), model->value.size());

    opentelemetry::common::AttributeValue attribute_value(span);
    resource_attributes_.SetAttribute(name_, attribute_value);
  }

  void VisitBooleanArray(
      const opentelemetry::sdk::configuration::BooleanArrayAttributeValueConfiguration *model)
      override
  {
    size_t length = model->value.size();

    // Can not use std::vector<bool>,
    // it has no data() to convert it to a span
    std::unique_ptr<bool[]> bool_array(new bool[length]);

    // We have: std::vector<bool>
    // We need: nostd::span<const bool>

    for (size_t i = 0; i < length; i++)
    {
      bool_array[i] = model->value[i];
    }

    nostd::span<const bool> span(&bool_array[0], length);

    opentelemetry::common::AttributeValue attribute_value(span);
    resource_attributes_.SetAttribute(name_, attribute_value);
  }

  opentelemetry::common::AttributeValue attribute_value;

private:
  opentelemetry::sdk::resource::ResourceAttributes &resource_attributes_;
  std::string name_;
};

class SamplerBuilder : public opentelemetry::sdk::configuration::SamplerConfigurationVisitor
{
public:
  SamplerBuilder(const SdkBuilder *b) : sdk_builder_(b) {}
  SamplerBuilder(SamplerBuilder &&)                      = delete;
  SamplerBuilder(const SamplerBuilder &)                 = delete;
  SamplerBuilder &operator=(SamplerBuilder &&)           = delete;
  SamplerBuilder &operator=(const SamplerBuilder &other) = delete;
  ~SamplerBuilder() override                             = default;

  void VisitAlwaysOff(
      const opentelemetry::sdk::configuration::AlwaysOffSamplerConfiguration *model) override
  {
    sampler = sdk_builder_->CreateAlwaysOffSampler(model);
  }

  void VisitAlwaysOn(
      const opentelemetry::sdk::configuration::AlwaysOnSamplerConfiguration *model) override
  {
    sampler = sdk_builder_->CreateAlwaysOnSampler(model);
  }

  void VisitJaegerRemote(
      const opentelemetry::sdk::configuration::JaegerRemoteSamplerConfiguration *model) override
  {
    sampler = sdk_builder_->CreateJaegerRemoteSampler(model);
  }

  void VisitParentBased(
      const opentelemetry::sdk::configuration::ParentBasedSamplerConfiguration *model) override
  {
    sampler = sdk_builder_->CreateParentBasedSampler(model);
  }

  void VisitTraceIdRatioBased(
      const opentelemetry::sdk::configuration::TraceIdRatioBasedSamplerConfiguration *model)
      override
  {
    sampler = sdk_builder_->CreateTraceIdRatioBasedSampler(model);
  }

  void VisitExtension(
      const opentelemetry::sdk::configuration::ExtensionSamplerConfiguration *model) override
  {
    sampler = sdk_builder_->CreateExtensionSampler(model);
  }

  std::unique_ptr<opentelemetry::sdk::trace::Sampler> sampler;

private:
  const SdkBuilder *sdk_builder_;
};

class SpanProcessorBuilder
    : public opentelemetry::sdk::configuration::SpanProcessorConfigurationVisitor
{
public:
  SpanProcessorBuilder(const SdkBuilder *b) : sdk_builder_(b) {}
  SpanProcessorBuilder(SpanProcessorBuilder &&)                      = delete;
  SpanProcessorBuilder(const SpanProcessorBuilder &)                 = delete;
  SpanProcessorBuilder &operator=(SpanProcessorBuilder &&)           = delete;
  SpanProcessorBuilder &operator=(const SpanProcessorBuilder &other) = delete;
  ~SpanProcessorBuilder() override                                   = default;

  void VisitBatch(
      const opentelemetry::sdk::configuration::BatchSpanProcessorConfiguration *model) override
  {
    processor = sdk_builder_->CreateBatchSpanProcessor(model);
  }

  void VisitSimple(
      const opentelemetry::sdk::configuration::SimpleSpanProcessorConfiguration *model) override
  {
    processor = sdk_builder_->CreateSimpleSpanProcessor(model);
  }

  void VisitExtension(
      const opentelemetry::sdk::configuration::ExtensionSpanProcessorConfiguration *model) override
  {
    processor = sdk_builder_->CreateExtensionSpanProcessor(model);
  }

  std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> processor;

private:
  const SdkBuilder *sdk_builder_;
};

class SpanExporterBuilder
    : public opentelemetry::sdk::configuration::SpanExporterConfigurationVisitor
{
public:
  SpanExporterBuilder(const SdkBuilder *b) : sdk_builder_(b) {}
  SpanExporterBuilder(SpanExporterBuilder &&)                      = delete;
  SpanExporterBuilder(const SpanExporterBuilder &)                 = delete;
  SpanExporterBuilder &operator=(SpanExporterBuilder &&)           = delete;
  SpanExporterBuilder &operator=(const SpanExporterBuilder &other) = delete;
  ~SpanExporterBuilder() override                                  = default;

  void VisitOtlpHttp(
      const opentelemetry::sdk::configuration::OtlpHttpSpanExporterConfiguration *model) override
  {
    exporter = sdk_builder_->CreateOtlpHttpSpanExporter(model);
  }

  void VisitOtlpGrpc(
      const opentelemetry::sdk::configuration::OtlpGrpcSpanExporterConfiguration *model) override
  {
    exporter = sdk_builder_->CreateOtlpGrpcSpanExporter(model);
  }

  void VisitOtlpFile(
      const opentelemetry::sdk::configuration::OtlpFileSpanExporterConfiguration *model) override
  {
    exporter = sdk_builder_->CreateOtlpFileSpanExporter(model);
  }

  void VisitConsole(
      const opentelemetry::sdk::configuration::ConsoleSpanExporterConfiguration *model) override
  {
    exporter = sdk_builder_->CreateConsoleSpanExporter(model);
  }

  void VisitZipkin(
      const opentelemetry::sdk::configuration::ZipkinSpanExporterConfiguration *model) override
  {
    exporter = sdk_builder_->CreateZipkinSpanExporter(model);
  }

  void VisitExtension(
      const opentelemetry::sdk::configuration::ExtensionSpanExporterConfiguration *model) override
  {
    exporter = sdk_builder_->CreateExtensionSpanExporter(model);
  }

  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter;

private:
  const SdkBuilder *sdk_builder_;
};

class MetricReaderBuilder
    : public opentelemetry::sdk::configuration::MetricReaderConfigurationVisitor
{
public:
  MetricReaderBuilder(const SdkBuilder *b) : sdk_builder_(b) {}
  MetricReaderBuilder(MetricReaderBuilder &&)                      = delete;
  MetricReaderBuilder(const MetricReaderBuilder &)                 = delete;
  MetricReaderBuilder &operator=(MetricReaderBuilder &&)           = delete;
  MetricReaderBuilder &operator=(const MetricReaderBuilder &other) = delete;
  ~MetricReaderBuilder() override                                  = default;

  void VisitPeriodic(
      const opentelemetry::sdk::configuration::PeriodicMetricReaderConfiguration *model) override
  {
    metric_reader = sdk_builder_->CreatePeriodicMetricReader(model);
  }

  void VisitPull(
      const opentelemetry::sdk::configuration::PullMetricReaderConfiguration *model) override
  {
    metric_reader = sdk_builder_->CreatePullMetricReader(model);
  }

  std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> metric_reader;

private:
  const SdkBuilder *sdk_builder_;
};

class PushMetricExporterBuilder
    : public opentelemetry::sdk::configuration::PushMetricExporterConfigurationVisitor
{
public:
  PushMetricExporterBuilder(const SdkBuilder *b) : sdk_builder_(b) {}
  PushMetricExporterBuilder(PushMetricExporterBuilder &&)                      = delete;
  PushMetricExporterBuilder(const PushMetricExporterBuilder &)                 = delete;
  PushMetricExporterBuilder &operator=(PushMetricExporterBuilder &&)           = delete;
  PushMetricExporterBuilder &operator=(const PushMetricExporterBuilder &other) = delete;
  ~PushMetricExporterBuilder() override                                        = default;

  void VisitOtlpHttp(
      const opentelemetry::sdk::configuration::OtlpHttpPushMetricExporterConfiguration *model)
      override
  {
    exporter = sdk_builder_->CreateOtlpHttpPushMetricExporter(model);
  }

  void VisitOtlpGrpc(
      const opentelemetry::sdk::configuration::OtlpGrpcPushMetricExporterConfiguration *model)
      override
  {
    exporter = sdk_builder_->CreateOtlpGrpcPushMetricExporter(model);
  }

  void VisitOtlpFile(
      const opentelemetry::sdk::configuration::OtlpFilePushMetricExporterConfiguration *model)
      override
  {
    exporter = sdk_builder_->CreateOtlpFilePushMetricExporter(model);
  }

  void VisitConsole(const opentelemetry::sdk::configuration::ConsolePushMetricExporterConfiguration
                        *model) override
  {
    exporter = sdk_builder_->CreateConsolePushMetricExporter(model);
  }

  void VisitExtension(
      const opentelemetry::sdk::configuration::ExtensionPushMetricExporterConfiguration *model)
      override
  {
    exporter = sdk_builder_->CreateExtensionPushMetricExporter(model);
  }

  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter;

private:
  const SdkBuilder *sdk_builder_;
};

class PullMetricExporterBuilder
    : public opentelemetry::sdk::configuration::PullMetricExporterConfigurationVisitor
{
public:
  PullMetricExporterBuilder(const SdkBuilder *b) : sdk_builder_(b) {}
  PullMetricExporterBuilder(PullMetricExporterBuilder &&)                      = delete;
  PullMetricExporterBuilder(const PullMetricExporterBuilder &)                 = delete;
  PullMetricExporterBuilder &operator=(PullMetricExporterBuilder &&)           = delete;
  PullMetricExporterBuilder &operator=(const PullMetricExporterBuilder &other) = delete;
  ~PullMetricExporterBuilder() override                                        = default;

  void VisitPrometheus(
      const opentelemetry::sdk::configuration::PrometheusPullMetricExporterConfiguration *model)
      override
  {
    exporter = sdk_builder_->CreatePrometheusPullMetricExporter(model);
  }

  void VisitExtension(
      const opentelemetry::sdk::configuration::ExtensionPullMetricExporterConfiguration *model)
      override
  {
    exporter = sdk_builder_->CreateExtensionPullMetricExporter(model);
  }

  std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> exporter;

private:
  const SdkBuilder *sdk_builder_;
};

class AggregationConfigBuilder
    : public opentelemetry::sdk::configuration::AggregationConfigurationVisitor
{
public:
  AggregationConfigBuilder(const SdkBuilder *b) : sdk_builder_(b) {}
  AggregationConfigBuilder(AggregationConfigBuilder &&)                      = delete;
  AggregationConfigBuilder(const AggregationConfigBuilder &)                 = delete;
  AggregationConfigBuilder &operator=(AggregationConfigBuilder &&)           = delete;
  AggregationConfigBuilder &operator=(const AggregationConfigBuilder &other) = delete;
  ~AggregationConfigBuilder() override                                       = default;

  void VisitBase2ExponentialBucketHistogram(
      const opentelemetry::sdk::configuration::
          Base2ExponentialBucketHistogramAggregationConfiguration *model) override
  {
    aggregation_type   = opentelemetry::sdk::metrics::AggregationType::kBase2ExponentialHistogram;
    aggregation_config = sdk_builder_->CreateBase2ExponentialBucketHistogramAggregation(model);
  }

  void VisitDefault(const opentelemetry::sdk::configuration::DefaultAggregationConfiguration
                        * /* model */) override
  {
    aggregation_type = opentelemetry::sdk::metrics::AggregationType::kDefault;
  }

  void VisitDrop(
      const opentelemetry::sdk::configuration::DropAggregationConfiguration * /* model */) override
  {
    aggregation_type = opentelemetry::sdk::metrics::AggregationType::kDrop;
  }

  void VisitExplicitBucketHistogram(
      const opentelemetry::sdk::configuration::ExplicitBucketHistogramAggregationConfiguration
          *model) override
  {
    aggregation_type   = opentelemetry::sdk::metrics::AggregationType::kHistogram;
    aggregation_config = sdk_builder_->CreateExplicitBucketHistogramAggregation(model);
  }

  void VisitLastValue(const opentelemetry::sdk::configuration::LastValueAggregationConfiguration
                          * /* model */) override
  {
    aggregation_type = opentelemetry::sdk::metrics::AggregationType::kLastValue;
  }

  void VisitSum(
      const opentelemetry::sdk::configuration::SumAggregationConfiguration * /* model */) override
  {
    aggregation_type = opentelemetry::sdk::metrics::AggregationType::kSum;
  }

  opentelemetry::sdk::metrics::AggregationType aggregation_type;
  std::unique_ptr<opentelemetry::sdk::metrics::AggregationConfig> aggregation_config;

private:
  const SdkBuilder *sdk_builder_;
};

class LogRecordProcessorBuilder
    : public opentelemetry::sdk::configuration::LogRecordProcessorConfigurationVisitor
{
public:
  LogRecordProcessorBuilder(const SdkBuilder *b) : sdk_builder_(b) {}
  LogRecordProcessorBuilder(LogRecordProcessorBuilder &&)                      = delete;
  LogRecordProcessorBuilder(const LogRecordProcessorBuilder &)                 = delete;
  LogRecordProcessorBuilder &operator=(LogRecordProcessorBuilder &&)           = delete;
  LogRecordProcessorBuilder &operator=(const LogRecordProcessorBuilder &other) = delete;
  ~LogRecordProcessorBuilder() override                                        = default;

  void VisitBatch(
      const opentelemetry::sdk::configuration::BatchLogRecordProcessorConfiguration *model) override
  {
    processor = sdk_builder_->CreateBatchLogRecordProcessor(model);
  }

  void VisitSimple(const opentelemetry::sdk::configuration::SimpleLogRecordProcessorConfiguration
                       *model) override
  {
    processor = sdk_builder_->CreateSimpleLogRecordProcessor(model);
  }

  void VisitExtension(
      const opentelemetry::sdk::configuration::ExtensionLogRecordProcessorConfiguration *model)
      override
  {
    processor = sdk_builder_->CreateExtensionLogRecordProcessor(model);
  }

  std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor> processor;

private:
  const SdkBuilder *sdk_builder_;
};

class LogRecordExporterBuilder
    : public opentelemetry::sdk::configuration::LogRecordExporterConfigurationVisitor
{
public:
  LogRecordExporterBuilder(const SdkBuilder *b) : sdk_builder_(b) {}
  LogRecordExporterBuilder(LogRecordExporterBuilder &&)                      = delete;
  LogRecordExporterBuilder(const LogRecordExporterBuilder &)                 = delete;
  LogRecordExporterBuilder &operator=(LogRecordExporterBuilder &&)           = delete;
  LogRecordExporterBuilder &operator=(const LogRecordExporterBuilder &other) = delete;
  ~LogRecordExporterBuilder() override                                       = default;

  void VisitOtlpHttp(const opentelemetry::sdk::configuration::OtlpHttpLogRecordExporterConfiguration
                         *model) override
  {
    exporter = sdk_builder_->CreateOtlpHttpLogRecordExporter(model);
  }

  void VisitOtlpGrpc(const opentelemetry::sdk::configuration::OtlpGrpcLogRecordExporterConfiguration
                         *model) override
  {
    exporter = sdk_builder_->CreateOtlpGrpcLogRecordExporter(model);
  }

  void VisitOtlpFile(const opentelemetry::sdk::configuration::OtlpFileLogRecordExporterConfiguration
                         *model) override
  {
    exporter = sdk_builder_->CreateOtlpFileLogRecordExporter(model);
  }

  void VisitConsole(const opentelemetry::sdk::configuration::ConsoleLogRecordExporterConfiguration
                        *model) override
  {
    exporter = sdk_builder_->CreateConsoleLogRecordExporter(model);
  }

  void VisitExtension(
      const opentelemetry::sdk::configuration::ExtensionLogRecordExporterConfiguration *model)
      override
  {
    exporter = sdk_builder_->CreateExtensionLogRecordExporter(model);
  }

  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> exporter;

private:
  const SdkBuilder *sdk_builder_;
};

std::unique_ptr<opentelemetry::sdk::trace::Sampler> SdkBuilder::CreateAlwaysOffSampler(
    const opentelemetry::sdk::configuration::AlwaysOffSamplerConfiguration * /* model */) const
{
  std::unique_ptr<opentelemetry::sdk::trace::Sampler> sdk;

  sdk = opentelemetry::sdk::trace::AlwaysOffSamplerFactory::Create();

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::trace::Sampler> SdkBuilder::CreateAlwaysOnSampler(
    const opentelemetry::sdk::configuration::AlwaysOnSamplerConfiguration * /* model */) const
{
  std::unique_ptr<opentelemetry::sdk::trace::Sampler> sdk;

  sdk = opentelemetry::sdk::trace::AlwaysOnSamplerFactory::Create();

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::trace::Sampler> SdkBuilder::CreateJaegerRemoteSampler(
    const opentelemetry::sdk::configuration::JaegerRemoteSamplerConfiguration * /* model */) const
{
  std::unique_ptr<opentelemetry::sdk::trace::Sampler> sdk;

  static const std::string die("JaegerRemoteSampler not supported");
  throw UnsupportedException(die);

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::trace::Sampler> SdkBuilder::CreateParentBasedSampler(
    const opentelemetry::sdk::configuration::ParentBasedSamplerConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::trace::Sampler> sdk;
  std::unique_ptr<opentelemetry::sdk::trace::Sampler> remote_parent_sampled_sdk;
  std::unique_ptr<opentelemetry::sdk::trace::Sampler> remote_parent_not_sampled_sdk;
  std::unique_ptr<opentelemetry::sdk::trace::Sampler> local_parent_sampled_sdk;
  std::unique_ptr<opentelemetry::sdk::trace::Sampler> local_parent_not_sampled_sdk;

  auto root_sdk = SdkBuilder::CreateSampler(model->root);

  if (model->remote_parent_sampled != nullptr)
  {
    remote_parent_sampled_sdk = SdkBuilder::CreateSampler(model->remote_parent_sampled);
  }
  else
  {
    remote_parent_sampled_sdk = opentelemetry::sdk::trace::AlwaysOnSamplerFactory::Create();
  }

  if (model->remote_parent_not_sampled != nullptr)
  {
    remote_parent_not_sampled_sdk = SdkBuilder::CreateSampler(model->remote_parent_not_sampled);
  }
  else
  {
    remote_parent_not_sampled_sdk = opentelemetry::sdk::trace::AlwaysOffSamplerFactory::Create();
  }

  if (model->local_parent_sampled != nullptr)
  {
    local_parent_sampled_sdk = SdkBuilder::CreateSampler(model->local_parent_sampled);
  }
  else
  {
    local_parent_sampled_sdk = opentelemetry::sdk::trace::AlwaysOnSamplerFactory::Create();
  }

  if (model->local_parent_not_sampled != nullptr)
  {
    local_parent_not_sampled_sdk = SdkBuilder::CreateSampler(model->local_parent_not_sampled);
  }
  else
  {
    local_parent_not_sampled_sdk = opentelemetry::sdk::trace::AlwaysOffSamplerFactory::Create();
  }

  std::shared_ptr<opentelemetry::sdk::trace::Sampler> shared_root = std::move(root_sdk);
  std::shared_ptr<opentelemetry::sdk::trace::Sampler> shared_remote_parent_sampled =
      std::move(remote_parent_sampled_sdk);
  std::shared_ptr<opentelemetry::sdk::trace::Sampler> shared_remote_parent_not_sampled =
      std::move(remote_parent_not_sampled_sdk);
  std::shared_ptr<opentelemetry::sdk::trace::Sampler> shared_local_parent_sampled =
      std::move(local_parent_sampled_sdk);
  std::shared_ptr<opentelemetry::sdk::trace::Sampler> shared_local_parent_not_sampled =
      std::move(local_parent_not_sampled_sdk);

  sdk = opentelemetry::sdk::trace::ParentBasedSamplerFactory::Create(
      shared_root, shared_remote_parent_sampled, shared_remote_parent_not_sampled,
      shared_local_parent_sampled, shared_local_parent_not_sampled);

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::trace::Sampler> SdkBuilder::CreateTraceIdRatioBasedSampler(
    const opentelemetry::sdk::configuration::TraceIdRatioBasedSamplerConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::trace::Sampler> sdk;

  sdk = opentelemetry::sdk::trace::TraceIdRatioBasedSamplerFactory::Create(model->ratio);

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::trace::Sampler> SdkBuilder::CreateExtensionSampler(
    const opentelemetry::sdk::configuration::ExtensionSamplerConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::trace::Sampler> sdk;
  std::string name = model->name;

  const ExtensionSamplerBuilder *builder = registry_->GetExtensionSamplerBuilder(name);

  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateExtensionSampler() using registered builder " << name);
    sdk = builder->Build(model);
    return sdk;
  }

  std::string die("CreateExtensionSampler() no builder for ");
  die.append(name);
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::trace::Sampler> SdkBuilder::CreateSampler(
    const std::unique_ptr<opentelemetry::sdk::configuration::SamplerConfiguration> &model) const
{
  std::unique_ptr<opentelemetry::sdk::trace::Sampler> sdk;

  SamplerBuilder builder(this);
  model->Accept(&builder);
  sdk = std::move(builder.sampler);

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> SdkBuilder::CreateOtlpHttpSpanExporter(
    const opentelemetry::sdk::configuration::OtlpHttpSpanExporterConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> sdk;
  const OtlpHttpSpanExporterBuilder *builder;

  builder = registry_->GetOtlpHttpSpanBuilder();
  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateOtlpHttpSpanExporter() using registered http builder");
    sdk = builder->Build(model);
    return sdk;
  }

  static const std::string die("No http builder for OtlpHttpSpanExporter");
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> SdkBuilder::CreateOtlpGrpcSpanExporter(
    const opentelemetry::sdk::configuration::OtlpGrpcSpanExporterConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> sdk;
  const OtlpGrpcSpanExporterBuilder *builder;

  builder = registry_->GetOtlpGrpcSpanBuilder();
  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateOtlpGrpcSpanExporter() using registered grpc builder");
    sdk = builder->Build(model);
    return sdk;
  }

  static const std::string die("No builder for OtlpGrpcSpanExporter");
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> SdkBuilder::CreateOtlpFileSpanExporter(
    const opentelemetry::sdk::configuration::OtlpFileSpanExporterConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> sdk;
  const OtlpFileSpanExporterBuilder *builder;

  builder = registry_->GetOtlpFileSpanBuilder();
  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateOtlpFileSpanExporter() using registered file builder");
    sdk = builder->Build(model);
    return sdk;
  }

  static const std::string die("No builder for OtlpFileSpanExporter");
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> SdkBuilder::CreateConsoleSpanExporter(
    const opentelemetry::sdk::configuration::ConsoleSpanExporterConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> sdk;
  const ConsoleSpanExporterBuilder *builder = registry_->GetConsoleSpanBuilder();

  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateConsoleSpanExporter() using registered builder");
    sdk = builder->Build(model);
    return sdk;
  }

  static const std::string die("No builder for ConsoleSpanExporter");
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> SdkBuilder::CreateZipkinSpanExporter(
    const opentelemetry::sdk::configuration::ZipkinSpanExporterConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> sdk;
  const ZipkinSpanExporterBuilder *builder = registry_->GetZipkinSpanBuilder();

  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateZipkinSpanExporter() using registered builder");
    sdk = builder->Build(model);
    return sdk;
  }

  static const std::string die("No builder for ZipkinSpanExporter");
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> SdkBuilder::CreateExtensionSpanExporter(
    const opentelemetry::sdk::configuration::ExtensionSpanExporterConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> sdk;
  std::string name = model->name;

  const ExtensionSpanExporterBuilder *builder = registry_->GetExtensionSpanExporterBuilder(name);

  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateExtensionSpanExporter() using registered builder " << name);
    sdk = builder->Build(model);
    return sdk;
  }

  std::string die("CreateExtensionSpanExporter() no builder for ");
  die.append(name);
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> SdkBuilder::CreateSpanExporter(
    const std::unique_ptr<opentelemetry::sdk::configuration::SpanExporterConfiguration> &model)
    const
{
  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> sdk;

  SpanExporterBuilder builder(this);
  model->Accept(&builder);
  sdk = std::move(builder.exporter);

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> SdkBuilder::CreateBatchSpanProcessor(
    const opentelemetry::sdk::configuration::BatchSpanProcessorConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> sdk;
  opentelemetry::sdk::trace::BatchSpanProcessorOptions options;

  options.schedule_delay_millis = std::chrono::milliseconds(model->schedule_delay);

#ifdef LATER
  options.xxx = model->export_timeout;
#endif

  options.max_queue_size        = model->max_queue_size;
  options.max_export_batch_size = model->max_export_batch_size;

  auto exporter_sdk = CreateSpanExporter(model->exporter);

  sdk = opentelemetry::sdk::trace::BatchSpanProcessorFactory::Create(std::move(exporter_sdk),
                                                                     options);
  return sdk;
}

std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> SdkBuilder::CreateSimpleSpanProcessor(
    const opentelemetry::sdk::configuration::SimpleSpanProcessorConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> sdk;

  auto exporter_sdk = CreateSpanExporter(model->exporter);

  sdk = opentelemetry::sdk::trace::SimpleSpanProcessorFactory::Create(std::move(exporter_sdk));

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> SdkBuilder::CreateExtensionSpanProcessor(
    const opentelemetry::sdk::configuration::ExtensionSpanProcessorConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> sdk;
  std::string name = model->name;

  const ExtensionSpanProcessorBuilder *builder = registry_->GetExtensionSpanProcessorBuilder(name);

  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateExtensionSpanProcessor() using registered builder " << name);
    sdk = builder->Build(model);
    return sdk;
  }

  std::string die("CreateExtensionSpanProcessor() no builder for ");
  die.append(name);
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> SdkBuilder::CreateSpanProcessor(
    const std::unique_ptr<opentelemetry::sdk::configuration::SpanProcessorConfiguration> &model)
    const
{
  std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> sdk;

  SpanProcessorBuilder builder(this);
  model->Accept(&builder);
  sdk = std::move(builder.processor);

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::trace::TracerProvider> SdkBuilder::CreateTracerProvider(
    const std::unique_ptr<opentelemetry::sdk::configuration::TracerProviderConfiguration> &model,
    const opentelemetry::sdk::resource::Resource &resource) const
{
  std::unique_ptr<opentelemetry::sdk::trace::TracerProvider> sdk;

  // FIXME-CONFIG: https://github.com/open-telemetry/opentelemetry-configuration/issues/70
  // FIXME-CONFIG: Add support for IdGenerator

  std::unique_ptr<opentelemetry::sdk::trace::Sampler> sampler;

  if (model->sampler)
  {
    sampler = CreateSampler(model->sampler);
  }

  std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>> sdk_processors;

  for (const auto &processor_model : model->processors)
  {
    sdk_processors.push_back(CreateSpanProcessor(processor_model));
  }

  // FIXME-SDK: https://github.com/open-telemetry/opentelemetry-cpp/issues/3303
  // FIXME-SDK: use limits, id_generator, ...
  sdk = opentelemetry::sdk::trace::TracerProviderFactory::Create(std::move(sdk_processors),
                                                                 resource, std::move(sampler));

  return sdk;
}

std::unique_ptr<opentelemetry::context::propagation::TextMapPropagator>
SdkBuilder::CreateTextMapPropagator(const std::string &name) const
{
  std::unique_ptr<opentelemetry::context::propagation::TextMapPropagator> sdk;

  const TextMapPropagatorBuilder *builder = registry_->GetTextMapPropagatorBuilder(name);

  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateTextMapPropagator() using registered builder " << name);
    sdk = builder->Build();
    return sdk;
  }

  std::string die("CreateTextMapPropagator() no builder for ");
  die.append(name);
  throw UnsupportedException(die);
}

static bool IsDuplicate(const std::vector<std::string> &propagator_seen, const std::string &name)
{
  bool duplicate = false;
  for (const auto &seen : propagator_seen)
  {
    if (name == seen)
    {
      duplicate = true;
    }
  }

  return duplicate;
}

std::unique_ptr<opentelemetry::context::propagation::TextMapPropagator>
SdkBuilder::CreatePropagator(
    const std::unique_ptr<opentelemetry::sdk::configuration::PropagatorConfiguration> &model) const
{
  std::unique_ptr<opentelemetry::context::propagation::CompositePropagator> sdk;
  std::vector<std::unique_ptr<opentelemetry::context::propagation::TextMapPropagator>> propagators;
  std::unique_ptr<opentelemetry::context::propagation::TextMapPropagator> propagator;
  std::vector<std::string> propagator_seen;
  bool duplicate = false;

  /*
   * Note that the spec only requires to check duplicates between
   * composite and composite_list.
   * Here we check for duplicates globally, for ease of use.
   */

  for (const auto &name : model->composite)
  {
    duplicate = IsDuplicate(propagator_seen, name);

    if (!duplicate)
    {
      propagator = CreateTextMapPropagator(name);
      propagators.push_back(std::move(propagator));
      propagator_seen.push_back(name);
    }
  }

  if (model->composite_list.size() > 0)
  {
    std::string str_list = model->composite_list;
    size_t start_pos     = 0;
    size_t end_pos       = 0;
    char separator       = ',';
    std::string name;

    while ((end_pos = str_list.find(separator, start_pos)) != std::string::npos)
    {
      name = str_list.substr(start_pos, end_pos - start_pos);

      duplicate = IsDuplicate(propagator_seen, name);

      if (!duplicate)
      {
        propagator = CreateTextMapPropagator(name);
        propagators.push_back(std::move(propagator));
        propagator_seen.push_back(name);
      }
      start_pos = end_pos + 1;
    }

    name = str_list.substr(start_pos);

    duplicate = IsDuplicate(propagator_seen, name);

    if (!duplicate)
    {
      propagator = CreateTextMapPropagator(name);
      propagators.push_back(std::move(propagator));
    }
  }

  if (propagators.size() > 0)
  {
    sdk = std::make_unique<opentelemetry::context::propagation::CompositePropagator>(
        std::move(propagators));
  }

  return sdk;
}

static opentelemetry::sdk::metrics::InstrumentType ConvertInstrumentType(
    enum opentelemetry::sdk::configuration::InstrumentType config)
{
  opentelemetry::sdk::metrics::InstrumentType sdk;

  switch (config)
  {
    case opentelemetry::sdk::configuration::InstrumentType::none:
    case opentelemetry::sdk::configuration::InstrumentType::counter:
      sdk = opentelemetry::sdk::metrics::InstrumentType::kCounter;
      break;
    case opentelemetry::sdk::configuration::InstrumentType::histogram:
      sdk = opentelemetry::sdk::metrics::InstrumentType::kHistogram;
      break;
    case opentelemetry::sdk::configuration::InstrumentType::observable_counter:
      sdk = opentelemetry::sdk::metrics::InstrumentType::kObservableCounter;
      break;
    case opentelemetry::sdk::configuration::InstrumentType::observable_gauge:
      sdk = opentelemetry::sdk::metrics::InstrumentType::kObservableGauge;
      break;
    case opentelemetry::sdk::configuration::InstrumentType::observable_up_down_counter:
      sdk = opentelemetry::sdk::metrics::InstrumentType::kObservableUpDownCounter;
      break;
    case opentelemetry::sdk::configuration::InstrumentType::up_down_counter:
      sdk = opentelemetry::sdk::metrics::InstrumentType::kUpDownCounter;
      break;
  }

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
SdkBuilder::CreateOtlpHttpPushMetricExporter(
    const opentelemetry::sdk::configuration::OtlpHttpPushMetricExporterConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> sdk;
  const OtlpHttpPushMetricExporterBuilder *builder;

  builder = registry_->GetOtlpHttpPushMetricExporterBuilder();
  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateOtlpHttpPushMetricExporter() using registered http builder");
    sdk = builder->Build(model);
    return sdk;
  }

  static const std::string die("No http builder for OtlpPushMetricExporter");
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
SdkBuilder::CreateOtlpGrpcPushMetricExporter(
    const opentelemetry::sdk::configuration::OtlpGrpcPushMetricExporterConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> sdk;
  const OtlpGrpcPushMetricExporterBuilder *builder;

  builder = registry_->GetOtlpGrpcPushMetricExporterBuilder();
  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateOtlpGrpcPushMetricExporter() using registered grpc builder");
    sdk = builder->Build(model);
    return sdk;
  }

  static const std::string die("No grpc builder for OtlpPushMetricExporter");
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
SdkBuilder::CreateOtlpFilePushMetricExporter(
    const opentelemetry::sdk::configuration::OtlpFilePushMetricExporterConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> sdk;
  const OtlpFilePushMetricExporterBuilder *builder;

  builder = registry_->GetOtlpFilePushMetricExporterBuilder();
  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateOtlpFilePushMetricExporter() using registered file builder");
    sdk = builder->Build(model);
    return sdk;
  }

  static const std::string die("No file builder for OtlpPushMetricExporter");
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
SdkBuilder::CreateConsolePushMetricExporter(
    const opentelemetry::sdk::configuration::ConsolePushMetricExporterConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> sdk;

  const ConsolePushMetricExporterBuilder *builder =
      registry_->GetConsolePushMetricExporterBuilder();

  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateConsolePushMetricExporter() using registered builder");
    sdk = builder->Build(model);
    return sdk;
  }

  static const std::string die("No builder for ConsolePushMetricExporter");
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
SdkBuilder::CreateExtensionPushMetricExporter(
    const opentelemetry::sdk::configuration::ExtensionPushMetricExporterConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> sdk;
  std::string name = model->name;

  const ExtensionPushMetricExporterBuilder *builder =
      registry_->GetExtensionPushMetricExporterBuilder(name);

  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateExtensionPushMetricExporter() using registered builder "
                            << name);
    sdk = builder->Build(model);
    return sdk;
  }

  std::string die("No builder for ExtensionPushMetricExporter ");
  die.append(name);
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::metrics::MetricReader>
SdkBuilder::CreatePrometheusPullMetricExporter(
    const opentelemetry::sdk::configuration::PrometheusPullMetricExporterConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> sdk;

  const PrometheusPullMetricExporterBuilder *builder =
      registry_->GetPrometheusPullMetricExporterBuilder();

  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreatePrometheusPullMetricExporter() using registered builder");
    sdk = builder->Build(model);
    return sdk;
  }

  static const std::string die("No builder for PrometheusMetricExporter");
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::metrics::MetricReader>
SdkBuilder::CreateExtensionPullMetricExporter(
    const opentelemetry::sdk::configuration::ExtensionPullMetricExporterConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> sdk;
  std::string name = model->name;

  const ExtensionPullMetricExporterBuilder *builder =
      registry_->GetExtensionPullMetricExporterBuilder(name);

  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateExtensionPullMetricExporter() using registered builder "
                            << name);
    sdk = builder->Build(model);
    return sdk;
  }

  std::string die("No builder for ExtensionPullMetricExporter ");
  die.append(name);
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
SdkBuilder::CreatePushMetricExporter(
    const std::unique_ptr<opentelemetry::sdk::configuration::PushMetricExporterConfiguration>
        &model) const
{
  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> sdk;

  PushMetricExporterBuilder builder(this);
  model->Accept(&builder);
  sdk = std::move(builder.exporter);

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> SdkBuilder::CreatePullMetricExporter(
    const std::unique_ptr<opentelemetry::sdk::configuration::PullMetricExporterConfiguration>
        &model) const
{
  std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> sdk;

  PullMetricExporterBuilder builder(this);
  model->Accept(&builder);
  sdk = std::move(builder.exporter);

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> SdkBuilder::CreatePeriodicMetricReader(
    const opentelemetry::sdk::configuration::PeriodicMetricReaderConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> sdk;

  opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions options;

  options.export_interval_millis = std::chrono::milliseconds(model->interval);
  options.export_timeout_millis  = std::chrono::milliseconds(model->timeout);

  auto exporter_sdk = CreatePushMetricExporter(model->exporter);

  if (model->producers.size() > 0)
  {
    OTEL_INTERNAL_LOG_WARN("metric producer not supported, ignoring");
  }

  sdk = opentelemetry::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
      std::move(exporter_sdk), options);

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> SdkBuilder::CreatePullMetricReader(
    const opentelemetry::sdk::configuration::PullMetricReaderConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> sdk;

  sdk = CreatePullMetricExporter(model->exporter);

  if (model->producers.size() > 0)
  {
    OTEL_INTERNAL_LOG_WARN("metric producer not supported, ignoring");
  }

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> SdkBuilder::CreateMetricReader(
    const std::unique_ptr<opentelemetry::sdk::configuration::MetricReaderConfiguration> &model)
    const
{
  std::unique_ptr<opentelemetry::sdk::metrics::MetricReader> sdk;

  MetricReaderBuilder builder(this);
  model->Accept(&builder);
  sdk = std::move(builder.metric_reader);

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::metrics::Base2ExponentialHistogramAggregationConfig>
SdkBuilder::CreateBase2ExponentialBucketHistogramAggregation(
    const opentelemetry::sdk::configuration::Base2ExponentialBucketHistogramAggregationConfiguration
        *model) const
{
  auto sdk =
      std::make_unique<opentelemetry::sdk::metrics::Base2ExponentialHistogramAggregationConfig>();

  sdk->max_buckets_    = model->max_size;
  sdk->max_scale_      = static_cast<int32_t>(model->max_scale);
  sdk->record_min_max_ = model->record_min_max;

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::metrics::HistogramAggregationConfig>
SdkBuilder::CreateExplicitBucketHistogramAggregation(
    const opentelemetry::sdk::configuration::ExplicitBucketHistogramAggregationConfiguration *model)
    const
{
  auto sdk = std::make_unique<opentelemetry::sdk::metrics::HistogramAggregationConfig>();

  sdk->boundaries_     = model->boundaries;
  sdk->record_min_max_ = model->record_min_max;

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::metrics::AggregationConfig> SdkBuilder::CreateAggregationConfig(
    const std::unique_ptr<opentelemetry::sdk::configuration::AggregationConfiguration> &model,
    opentelemetry::sdk::metrics::AggregationType &aggregation_type) const
{
  std::unique_ptr<opentelemetry::sdk::metrics::AggregationConfig> sdk;

  AggregationConfigBuilder builder(this);
  model->Accept(&builder);
  aggregation_type = builder.aggregation_type;
  sdk              = std::move(builder.aggregation_config);

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::metrics::AttributesProcessor>
SdkBuilder::CreateAttributesProcessor(
    const std::unique_ptr<opentelemetry::sdk::configuration::IncludeExcludeConfiguration>
        & /* model */) const
{
  std::unique_ptr<opentelemetry::sdk::metrics::AttributesProcessor> sdk;

  // FIXME-SDK: https://github.com/open-telemetry/opentelemetry-cpp/issues/3546
  // FIXME-SDK: Need a subclass of AttributesProcessor for IncludeExclude
  OTEL_INTERNAL_LOG_WARN("IncludeExclude attribute processor not supported, ignoring");

  return sdk;
}

void SdkBuilder::AddView(
    opentelemetry::sdk::metrics::ViewRegistry *view_registry,
    const std::unique_ptr<opentelemetry::sdk::configuration::ViewConfiguration> &model) const
{
  auto *selector = model->selector.get();

  if (selector->instrument_type == opentelemetry::sdk::configuration::InstrumentType::none)
  {
    std::string die("Runtime does not support instrument_type: null");
    throw UnsupportedException(die);
  }

  auto sdk_instrument_type = ConvertInstrumentType(selector->instrument_type);

  auto sdk_instrument_selector = std::make_unique<opentelemetry::sdk::metrics::InstrumentSelector>(
      sdk_instrument_type, selector->instrument_name, selector->unit);

  auto sdk_meter_selector = std::make_unique<opentelemetry::sdk::metrics::MeterSelector>(
      selector->meter_name, selector->meter_version, selector->meter_schema_url);

  auto *stream = model->stream.get();

  opentelemetry::sdk::metrics::AggregationType sdk_aggregation_type =
      opentelemetry::sdk::metrics::AggregationType::kDefault;

  std::shared_ptr<opentelemetry::sdk::metrics::AggregationConfig> sdk_aggregation_config;

  sdk_aggregation_config = CreateAggregationConfig(stream->aggregation, sdk_aggregation_type);

  std::unique_ptr<opentelemetry::sdk::metrics::AttributesProcessor> sdk_attribute_processor;

  if (stream->attribute_keys != nullptr)
  {
    sdk_attribute_processor = CreateAttributesProcessor(stream->attribute_keys);
  }

  auto sdk_view = std::make_unique<opentelemetry::sdk::metrics::View>(
      stream->name, stream->description, sdk_aggregation_type, sdk_aggregation_config,
      std::move(sdk_attribute_processor));

  view_registry->AddView(std::move(sdk_instrument_selector), std::move(sdk_meter_selector),
                         std::move(sdk_view));
}

std::unique_ptr<opentelemetry::sdk::metrics::MeterProvider> SdkBuilder::CreateMeterProvider(
    const std::unique_ptr<opentelemetry::sdk::configuration::MeterProviderConfiguration> &model,
    const opentelemetry::sdk::resource::Resource &resource) const
{
  std::unique_ptr<opentelemetry::sdk::metrics::MeterProvider> sdk;

  auto view_registry = opentelemetry::sdk::metrics::ViewRegistryFactory::Create();

  for (const auto &view_configuration : model->views)
  {
    AddView(view_registry.get(), view_configuration);
  }

  auto meter_context =
      opentelemetry::sdk::metrics::MeterContextFactory::Create(std::move(view_registry), resource);

  for (const auto &reader_configuration : model->readers)
  {
    std::shared_ptr<opentelemetry::sdk::metrics::MetricReader> metric_reader;
    metric_reader = CreateMetricReader(reader_configuration);
    meter_context->AddMetricReader(metric_reader);
  }

  sdk = opentelemetry::sdk::metrics::MeterProviderFactory::Create(std::move(meter_context));

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>
SdkBuilder::CreateOtlpHttpLogRecordExporter(
    const opentelemetry::sdk::configuration::OtlpHttpLogRecordExporterConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> sdk;
  const OtlpHttpLogRecordExporterBuilder *builder;

  builder = registry_->GetOtlpHttpLogRecordBuilder();
  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateOtlpHttpLogRecordExporter() using registered http builder");
    sdk = builder->Build(model);
    return sdk;
  }

  static const std::string die("No http builder for OtlpLogRecordExporter");
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>
SdkBuilder::CreateOtlpGrpcLogRecordExporter(
    const opentelemetry::sdk::configuration::OtlpGrpcLogRecordExporterConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> sdk;
  const OtlpGrpcLogRecordExporterBuilder *builder;

  builder = registry_->GetOtlpGrpcLogRecordBuilder();
  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateOtlpGrpcLogRecordExporter() using registered grpc builder");
    sdk = builder->Build(model);
    return sdk;
  }

  static const std::string die("No grpc builder for OtlpLogRecordExporter");
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>
SdkBuilder::CreateOtlpFileLogRecordExporter(
    const opentelemetry::sdk::configuration::OtlpFileLogRecordExporterConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> sdk;
  const OtlpFileLogRecordExporterBuilder *builder;

  builder = registry_->GetOtlpFileLogRecordBuilder();
  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateOtlpFileLogRecordExporter() using registered file builder");
    sdk = builder->Build(model);
    return sdk;
  }

  static const std::string die("No file builder for OtlpLogRecordExporter");
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>
SdkBuilder::CreateConsoleLogRecordExporter(
    const opentelemetry::sdk::configuration::ConsoleLogRecordExporterConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> sdk;
  const ConsoleLogRecordExporterBuilder *builder = registry_->GetConsoleLogRecordBuilder();

  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateConsoleLogRecordExporter() using registered builder");
    sdk = builder->Build(model);
    return sdk;
  }

  static const std::string die("No builder for ConsoleLogRecordExporter");
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>
SdkBuilder::CreateExtensionLogRecordExporter(
    const opentelemetry::sdk::configuration::ExtensionLogRecordExporterConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> sdk;
  std::string name = model->name;

  const ExtensionLogRecordExporterBuilder *builder =
      registry_->GetExtensionLogRecordExporterBuilder(name);

  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateExtensionLogRecordExporter() using registered builder " << name);
    sdk = builder->Build(model);
    return sdk;
  }

  std::string die("CreateExtensionLogRecordExporter() no builder for ");
  die.append(name);
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> SdkBuilder::CreateLogRecordExporter(
    const std::unique_ptr<opentelemetry::sdk::configuration::LogRecordExporterConfiguration> &model)
    const
{
  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> sdk;

  LogRecordExporterBuilder builder(this);
  model->Accept(&builder);
  sdk = std::move(builder.exporter);

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor>
SdkBuilder::CreateBatchLogRecordProcessor(
    const opentelemetry::sdk::configuration::BatchLogRecordProcessorConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor> sdk;
  opentelemetry::sdk::logs::BatchLogRecordProcessorOptions options;

  options.schedule_delay_millis = std::chrono::milliseconds(model->schedule_delay);
  options.max_queue_size        = model->max_queue_size;
  options.max_export_batch_size = model->max_export_batch_size;

  auto exporter_sdk = CreateLogRecordExporter(model->exporter);

  sdk = opentelemetry::sdk::logs::BatchLogRecordProcessorFactory::Create(std::move(exporter_sdk),
                                                                         options);

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor>
SdkBuilder::CreateSimpleLogRecordProcessor(
    const opentelemetry::sdk::configuration::SimpleLogRecordProcessorConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor> sdk;

  auto exporter_sdk = CreateLogRecordExporter(model->exporter);

  sdk = opentelemetry::sdk::logs::SimpleLogRecordProcessorFactory::Create(std::move(exporter_sdk));

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor>
SdkBuilder::CreateExtensionLogRecordProcessor(
    const opentelemetry::sdk::configuration::ExtensionLogRecordProcessorConfiguration *model) const
{
  std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor> sdk;
  std::string name = model->name;

  const ExtensionLogRecordProcessorBuilder *builder =
      registry_->GetExtensionLogRecordProcessorBuilder(name);

  if (builder != nullptr)
  {
    OTEL_INTERNAL_LOG_DEBUG("CreateExtensionLogRecordProcessor() using registered builder "
                            << name);
    sdk = builder->Build(model);
    return sdk;
  }

  std::string die("CreateExtensionLogRecordProcessor() no builder for ");
  die.append(name);
  throw UnsupportedException(die);
}

std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor> SdkBuilder::CreateLogRecordProcessor(
    const std::unique_ptr<opentelemetry::sdk::configuration::LogRecordProcessorConfiguration>
        &model) const
{
  std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor> sdk;

  LogRecordProcessorBuilder builder(this);
  model->Accept(&builder);
  sdk = std::move(builder.processor);

  return sdk;
}

std::unique_ptr<opentelemetry::sdk::logs::LoggerProvider> SdkBuilder::CreateLoggerProvider(
    const std::unique_ptr<opentelemetry::sdk::configuration::LoggerProviderConfiguration> &model,
    const opentelemetry::sdk::resource::Resource &resource) const
{
  std::unique_ptr<opentelemetry::sdk::logs::LoggerProvider> sdk;

  std::vector<std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor>> sdk_processors;

  for (const auto &processor_model : model->processors)
  {
    sdk_processors.push_back(CreateLogRecordProcessor(processor_model));
  }

  // FIXME-SDK: https://github.com/open-telemetry/opentelemetry-cpp/issues/3303
  // FIXME-SDK: use limits
  sdk =
      opentelemetry::sdk::logs::LoggerProviderFactory::Create(std::move(sdk_processors), resource);

  return sdk;
}

void SdkBuilder::SetResourceAttribute(
    opentelemetry::sdk::resource::ResourceAttributes &resource_attributes,
    const std::string &name,
    const opentelemetry::sdk::configuration::AttributeValueConfiguration *model) const
{
  ResourceAttributeValueSetter setter(resource_attributes, name);
  // Invokes resource_attributes.SetAttribute(name, <proper value from model>)
  model->Accept(&setter);
}

void SdkBuilder::SetResource(
    opentelemetry::sdk::resource::Resource &resource,
    const std::unique_ptr<opentelemetry::sdk::configuration::ResourceConfiguration> &opt_model)
    const
{
  if (opt_model)
  {
    opentelemetry::sdk::resource::ResourceAttributes sdk_attributes;

    // First, scan attributes_list, which has low priority.
    if (opt_model->attributes_list.size() != 0)
    {
      opentelemetry::common::KeyValueStringTokenizer tokenizer{opt_model->attributes_list};

      opentelemetry::nostd::string_view attribute_key;
      opentelemetry::nostd::string_view attribute_value;
      bool attribute_valid = true;

      while (tokenizer.next(attribute_valid, attribute_key, attribute_value))
      {
        if (attribute_valid)
        {
          opentelemetry::common::AttributeValue wrapped_attribute_value(attribute_value);
          sdk_attributes.SetAttribute(attribute_key, wrapped_attribute_value);
        }
        else
        {
          OTEL_INTERNAL_LOG_WARN("Found invalid key/value pair in attributes_list");
        }
      }
    }

    // Second, scan attributes, which has high priority.
    if (opt_model->attributes)
    {
      for (const auto &kv : opt_model->attributes->kv_map)
      {
        SetResourceAttribute(sdk_attributes, kv.first, kv.second.get());
      }
    }

    if (opt_model->detectors != nullptr)
    {
      // FIXME-SDK: https://github.com/open-telemetry/opentelemetry-cpp/issues/3548
      // FIXME-SDK: Implement resource detectors
      OTEL_INTERNAL_LOG_WARN("resource detectors not supported, ignoring");
    }

    auto sdk_resource =
        opentelemetry::sdk::resource::Resource::Create(sdk_attributes, opt_model->schema_url);
    resource = resource.Merge(sdk_resource);
  }
  else
  {
    resource = opentelemetry::sdk::resource::Resource::GetDefault();
  }
}

std::unique_ptr<ConfiguredSdk> SdkBuilder::CreateConfiguredSdk(
    const std::unique_ptr<opentelemetry::sdk::configuration::Configuration> &model) const
{
  auto sdk = std::make_unique<ConfiguredSdk>();

  if (!model->disabled)
  {
    SetResource(sdk->resource, model->resource);

    if (model->attribute_limits)
    {
      // FIXME-SDK: https://github.com/open-telemetry/opentelemetry-cpp/issues/3303
      // FIXME-SDK: Implement attribute limits
      OTEL_INTERNAL_LOG_WARN("attribute_limits not supported, ignoring");
    }

    if (model->tracer_provider)
    {
      sdk->tracer_provider = CreateTracerProvider(model->tracer_provider, sdk->resource);
    }

    if (model->propagator)
    {
      sdk->propagator = CreatePropagator(model->propagator);
    }

    if (model->meter_provider)
    {
      sdk->meter_provider = CreateMeterProvider(model->meter_provider, sdk->resource);
    }

    if (model->logger_provider)
    {
      sdk->logger_provider = CreateLoggerProvider(model->logger_provider, sdk->resource);
    }
  }

  return sdk;
}

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
