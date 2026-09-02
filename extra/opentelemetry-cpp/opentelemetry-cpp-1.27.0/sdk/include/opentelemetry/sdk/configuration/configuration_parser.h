// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/aggregation_configuration.h"
#include "opentelemetry/sdk/configuration/always_off_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/always_on_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/attribute_limits_configuration.h"
#include "opentelemetry/sdk/configuration/attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/attributes_configuration.h"
#include "opentelemetry/sdk/configuration/base2_exponential_bucket_histogram_aggregation_configuration.h"
#include "opentelemetry/sdk/configuration/batch_log_record_processor_configuration.h"
#include "opentelemetry/sdk/configuration/batch_span_processor_configuration.h"
#include "opentelemetry/sdk/configuration/boolean_array_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/boolean_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/cardinality_limits_configuration.h"
#include "opentelemetry/sdk/configuration/composable_always_off_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/composable_always_on_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/composable_parent_threshold_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/composable_probability_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/composable_rule_based_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/composable_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/configuration.h"
#include "opentelemetry/sdk/configuration/console_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/console_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/console_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/default_aggregation_configuration.h"
#include "opentelemetry/sdk/configuration/default_histogram_aggregation.h"
#include "opentelemetry/sdk/configuration/distribution_configuration.h"
#include "opentelemetry/sdk/configuration/document.h"
#include "opentelemetry/sdk/configuration/document_node.h"
#include "opentelemetry/sdk/configuration/double_array_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/double_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/drop_aggregation_configuration.h"
#include "opentelemetry/sdk/configuration/exemplar_filter.h"
#include "opentelemetry/sdk/configuration/explicit_bucket_histogram_aggregation_configuration.h"
#include "opentelemetry/sdk/configuration/extension_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/extension_log_record_processor_configuration.h"
#include "opentelemetry/sdk/configuration/extension_metric_producer_configuration.h"
#include "opentelemetry/sdk/configuration/extension_pull_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/extension_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/extension_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/extension_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/extension_span_processor_configuration.h"
#include "opentelemetry/sdk/configuration/headers_configuration.h"
#include "opentelemetry/sdk/configuration/include_exclude_configuration.h"
#include "opentelemetry/sdk/configuration/instrument_type.h"
#include "opentelemetry/sdk/configuration/integer_array_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/integer_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/invalid_schema_exception.h"
#include "opentelemetry/sdk/configuration/jaeger_remote_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/last_value_aggregation_configuration.h"
#include "opentelemetry/sdk/configuration/log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/log_record_limits_configuration.h"
#include "opentelemetry/sdk/configuration/log_record_processor_configuration.h"
#include "opentelemetry/sdk/configuration/logger_configurator_configuration.h"
#include "opentelemetry/sdk/configuration/logger_provider_configuration.h"
#include "opentelemetry/sdk/configuration/meter_configurator_configuration.h"
#include "opentelemetry/sdk/configuration/meter_provider_configuration.h"
#include "opentelemetry/sdk/configuration/metric_producer_configuration.h"
#include "opentelemetry/sdk/configuration/metric_reader_configuration.h"
#include "opentelemetry/sdk/configuration/open_census_metric_producer_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_file_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_file_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_file_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_http_encoding.h"
#include "opentelemetry/sdk/configuration/otlp_http_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_http_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_http_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/parent_based_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/periodic_metric_reader_configuration.h"
#include "opentelemetry/sdk/configuration/prometheus_pull_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/propagator_configuration.h"
#include "opentelemetry/sdk/configuration/pull_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/pull_metric_reader_configuration.h"
#include "opentelemetry/sdk/configuration/push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/resource_configuration.h"
#include "opentelemetry/sdk/configuration/sampler_configuration.h"
#include "opentelemetry/sdk/configuration/severity_number.h"
#include "opentelemetry/sdk/configuration/simple_log_record_processor_configuration.h"
#include "opentelemetry/sdk/configuration/simple_span_processor_configuration.h"
#include "opentelemetry/sdk/configuration/span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/span_limits_configuration.h"
#include "opentelemetry/sdk/configuration/span_processor_configuration.h"
#include "opentelemetry/sdk/configuration/string_array_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/string_array_configuration.h"
#include "opentelemetry/sdk/configuration/string_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/sum_aggregation_configuration.h"
#include "opentelemetry/sdk/configuration/temporality_preference.h"
#include "opentelemetry/sdk/configuration/trace_id_ratio_based_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/tracer_configurator_configuration.h"
#include "opentelemetry/sdk/configuration/tracer_provider_configuration.h"
#include "opentelemetry/sdk/configuration/translation_strategy.h"
#include "opentelemetry/sdk/configuration/view_configuration.h"
#include "opentelemetry/sdk/configuration/view_selector_configuration.h"
#include "opentelemetry/sdk/configuration/view_stream_configuration.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class ConfigurationParser
{
public:
  OtlpHttpEncoding ParseOtlpHttpEncoding(const std::unique_ptr<DocumentNode> &node,
                                         const std::string &name) const;

  SeverityNumber ParseSeverityNumber(const std::unique_ptr<DocumentNode> &node,
                                     const std::string &name) const;

  std::unique_ptr<StringArrayConfiguration> ParseStringArrayConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<IncludeExcludeConfiguration> ParseIncludeExcludeConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<HeadersConfiguration> ParseHeadersConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<AttributeLimitsConfiguration> ParseAttributeLimitsConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<HttpTlsConfiguration> ParseHttpTlsConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<GrpcTlsConfiguration> ParseGrpcTlsConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<OtlpHttpLogRecordExporterConfiguration>
  ParseOtlpHttpLogRecordExporterConfiguration(const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<OtlpGrpcLogRecordExporterConfiguration>
  ParseOtlpGrpcLogRecordExporterConfiguration(const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<OtlpFileLogRecordExporterConfiguration>
  ParseOtlpFileLogRecordExporterConfiguration(const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<ConsoleLogRecordExporterConfiguration> ParseConsoleLogRecordExporterConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<ExtensionLogRecordExporterConfiguration>
  ParseExtensionLogRecordExporterConfiguration(const std::string &name,
                                               std::unique_ptr<DocumentNode> node) const;

  std::unique_ptr<LogRecordExporterConfiguration> ParseLogRecordExporterConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<BatchLogRecordProcessorConfiguration> ParseBatchLogRecordProcessorConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<SimpleLogRecordProcessorConfiguration> ParseSimpleLogRecordProcessorConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<ExtensionLogRecordProcessorConfiguration>
  ParseExtensionLogRecordProcessorConfiguration(const std::string &name,
                                                std::unique_ptr<DocumentNode> node) const;

  std::unique_ptr<LogRecordProcessorConfiguration> ParseLogRecordProcessorConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<LogRecordLimitsConfiguration> ParseLogRecordLimitsConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<LoggerProviderConfiguration> ParseLoggerProviderConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  LoggerConfigConfiguration ParseLoggerConfigConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  LoggerMatcherAndConfigConfiguration ParseLoggerMatcherAndConfigConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<LoggerConfiguratorConfiguration> ParseLoggerConfiguratorConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  DefaultHistogramAggregation ParseDefaultHistogramAggregation(
      const std::unique_ptr<DocumentNode> &node,
      const std::string &name) const;

  TemporalityPreference ParseTemporalityPreference(const std::unique_ptr<DocumentNode> &node,
                                                   const std::string &name) const;

  std::unique_ptr<OtlpHttpPushMetricExporterConfiguration>
  ParseOtlpHttpPushMetricExporterConfiguration(const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<OtlpGrpcPushMetricExporterConfiguration>
  ParseOtlpGrpcPushMetricExporterConfiguration(const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<OtlpFilePushMetricExporterConfiguration>
  ParseOtlpFilePushMetricExporterConfiguration(const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<ConsolePushMetricExporterConfiguration>
  ParseConsolePushMetricExporterConfiguration(const std::unique_ptr<DocumentNode> &node) const;

  TranslationStrategy ParseTranslationStrategy(const std::unique_ptr<DocumentNode> &node,
                                               const std::string &name) const;

  std::unique_ptr<PrometheusPullMetricExporterConfiguration>
  ParsePrometheusPullMetricExporterConfiguration(const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<ExtensionPushMetricExporterConfiguration>
  ParsePushMetricExporterExtensionConfiguration(const std::string &name,
                                                std::unique_ptr<DocumentNode> node) const;

  std::unique_ptr<ExtensionPullMetricExporterConfiguration>
  ParsePullMetricExporterExtensionConfiguration(const std::string &name,
                                                std::unique_ptr<DocumentNode> node) const;

  std::unique_ptr<PushMetricExporterConfiguration> ParsePushMetricExporterConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<PullMetricExporterConfiguration> ParsePullMetricExporterConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<OpenCensusMetricProducerConfiguration> ParseOpenCensusMetricProducerConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<ExtensionMetricProducerConfiguration> ParseExtensionMetricProducerConfiguration(
      const std::string &name,
      std::unique_ptr<DocumentNode> node) const;

  std::unique_ptr<MetricProducerConfiguration> ParseMetricProducerConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<CardinalityLimitsConfiguration> ParseCardinalityLimitsConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<PeriodicMetricReaderConfiguration> ParsePeriodicMetricReaderConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<PullMetricReaderConfiguration> ParsePullMetricReaderConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<MetricReaderConfiguration> ParseMetricReaderConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  InstrumentType ParseInstrumentType(const std::unique_ptr<DocumentNode> &node,
                                     const std::string &name) const;

  ExemplarFilter ParseExemplarFilter(const std::unique_ptr<DocumentNode> &node,
                                     const std::string &name) const;

  std::unique_ptr<ViewSelectorConfiguration> ParseViewSelectorConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<DefaultAggregationConfiguration> ParseDefaultAggregationConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<DropAggregationConfiguration> ParseDropAggregationConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<ExplicitBucketHistogramAggregationConfiguration>
  ParseExplicitBucketHistogramAggregationConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<Base2ExponentialBucketHistogramAggregationConfiguration>
  ParseBase2ExponentialBucketHistogramAggregationConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<LastValueAggregationConfiguration> ParseLastValueAggregationConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<SumAggregationConfiguration> ParseSumAggregationConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<AggregationConfiguration> ParseAggregationConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<ViewStreamConfiguration> ParseViewStreamConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<ViewConfiguration> ParseViewConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<MeterProviderConfiguration> ParseMeterProviderConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  MeterConfigConfiguration ParseMeterConfigConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  MeterMatcherAndConfigConfiguration ParseMeterMatcherAndConfigConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<MeterConfiguratorConfiguration> ParseMeterConfiguratorConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<PropagatorConfiguration> ParsePropagatorConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<SpanLimitsConfiguration> ParseSpanLimitsConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<AlwaysOffSamplerConfiguration> ParseAlwaysOffSamplerConfiguration(
      const std::unique_ptr<DocumentNode> &node,
      size_t depth) const;

  std::unique_ptr<AlwaysOnSamplerConfiguration> ParseAlwaysOnSamplerConfiguration(
      const std::unique_ptr<DocumentNode> &node,
      size_t depth) const;

  std::unique_ptr<JaegerRemoteSamplerConfiguration> ParseJaegerRemoteSamplerConfiguration(
      const std::unique_ptr<DocumentNode> &node,
      size_t depth) const;

  std::unique_ptr<ParentBasedSamplerConfiguration> ParseParentBasedSamplerConfiguration(
      const std::unique_ptr<DocumentNode> &node,
      size_t depth) const;

  std::unique_ptr<TraceIdRatioBasedSamplerConfiguration> ParseTraceIdRatioBasedSamplerConfiguration(
      const std::unique_ptr<DocumentNode> &node,
      size_t depth) const;

  std::unique_ptr<ComposableAlwaysOffSamplerConfiguration>
  ParseComposableAlwaysOffSamplerConfiguration(const std::unique_ptr<DocumentNode> &node,
                                               size_t depth) const;

  std::unique_ptr<ComposableAlwaysOnSamplerConfiguration>
  ParseComposableAlwaysOnSamplerConfiguration(const std::unique_ptr<DocumentNode> &node,
                                              size_t depth) const;

  std::unique_ptr<ComposableProbabilitySamplerConfiguration>
  ParseComposableProbabilitySamplerConfiguration(const std::unique_ptr<DocumentNode> &node,
                                                 size_t depth) const;

  std::unique_ptr<ComposableParentThresholdSamplerConfiguration>
  ParseComposableParentThresholdSamplerConfiguration(const std::unique_ptr<DocumentNode> &node,
                                                     size_t depth) const;

  std::unique_ptr<ComposableRuleBasedSamplerRuleAttributeValuesConfiguration>
  ParseComposableRuleBasedSamplerRuleAttributeValuesConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<ComposableRuleBasedSamplerRuleAttributePatternsConfiguration>
  ParseComposableRuleBasedSamplerRuleAttributePatternsConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<ComposableRuleBasedSamplerRuleConfiguration>
  ParseComposableRuleBasedSamplerRuleConfiguration(const std::unique_ptr<DocumentNode> &node,
                                                   size_t depth) const;

  std::unique_ptr<ComposableRuleBasedSamplerConfiguration>
  ParseComposableRuleBasedSamplerConfiguration(const std::unique_ptr<DocumentNode> &node,
                                               size_t depth) const;

  std::unique_ptr<ComposableSamplerConfiguration> ParseComposableSamplerConfiguration(
      const std::unique_ptr<DocumentNode> &node,
      size_t depth) const;

  std::unique_ptr<ExtensionSamplerConfiguration> ParseSamplerExtensionConfiguration(
      const std::string &name,
      std::unique_ptr<DocumentNode> node,
      size_t depth) const;

  std::unique_ptr<SamplerConfiguration> ParseSamplerConfiguration(
      const std::unique_ptr<DocumentNode> &node,
      size_t depth) const;

  std::unique_ptr<OtlpHttpSpanExporterConfiguration> ParseOtlpHttpSpanExporterConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<OtlpGrpcSpanExporterConfiguration> ParseOtlpGrpcSpanExporterConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<OtlpFileSpanExporterConfiguration> ParseOtlpFileSpanExporterConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<ConsoleSpanExporterConfiguration> ParseConsoleSpanExporterConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<ExtensionSpanExporterConfiguration> ParseExtensionSpanExporterConfiguration(
      const std::string &name,
      std::unique_ptr<DocumentNode> node) const;

  std::unique_ptr<SpanExporterConfiguration> ParseSpanExporterConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<BatchSpanProcessorConfiguration> ParseBatchSpanProcessorConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<SimpleSpanProcessorConfiguration> ParseSimpleSpanProcessorConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<ExtensionSpanProcessorConfiguration> ParseExtensionSpanProcessorConfiguration(
      const std::string &name,
      std::unique_ptr<DocumentNode> node) const;

  std::unique_ptr<SpanProcessorConfiguration> ParseSpanProcessorConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<TracerProviderConfiguration> ParseTracerProviderConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  TracerConfigConfiguration ParseTracerConfigConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  TracerMatcherAndConfigConfiguration ParseTracerMatcherAndConfigConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<TracerConfiguratorConfiguration> ParseTracerConfiguratorConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<StringAttributeValueConfiguration> ParseStringAttributeValueConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<IntegerAttributeValueConfiguration> ParseIntegerAttributeValueConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<DoubleAttributeValueConfiguration> ParseDoubleAttributeValueConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<BooleanAttributeValueConfiguration> ParseBooleanAttributeValueConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<StringArrayAttributeValueConfiguration>
  ParseStringArrayAttributeValueConfiguration(const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<IntegerArrayAttributeValueConfiguration>
  ParseIntegerArrayAttributeValueConfiguration(const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<DoubleArrayAttributeValueConfiguration>
  ParseDoubleArrayAttributeValueConfiguration(const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<BooleanArrayAttributeValueConfiguration>
  ParseBooleanArrayAttributeValueConfiguration(const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<AttributesConfiguration> ParseAttributesConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<ResourceConfiguration> ParseResourceConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<DistributionConfiguration> ParseDistributionConfiguration(
      const std::unique_ptr<DocumentNode> &node) const;

  std::unique_ptr<Configuration> Parse(std::unique_ptr<Document> doc);

private:
  std::string version_;
  int version_major_;
  int version_minor_;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
