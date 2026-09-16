// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "opentelemetry/sdk/configuration/base2_exponential_bucket_histogram_aggregation_configuration.h"
#include "opentelemetry/sdk/configuration/cardinality_limits_configuration.h"
#include "opentelemetry/sdk/configuration/configuration.h"
#include "opentelemetry/sdk/configuration/default_histogram_aggregation.h"
#include "opentelemetry/sdk/configuration/exemplar_filter.h"
#include "opentelemetry/sdk/configuration/explicit_bucket_histogram_aggregation_configuration.h"
#include "opentelemetry/sdk/configuration/grpc_tls_configuration.h"
#include "opentelemetry/sdk/configuration/headers_configuration.h"
#include "opentelemetry/sdk/configuration/http_tls_configuration.h"
#include "opentelemetry/sdk/configuration/include_exclude_configuration.h"
#include "opentelemetry/sdk/configuration/instrument_type.h"
#include "opentelemetry/sdk/configuration/meter_config_configuration.h"
#include "opentelemetry/sdk/configuration/meter_configurator_configuration.h"
#include "opentelemetry/sdk/configuration/meter_matcher_and_config_configuration.h"
#include "opentelemetry/sdk/configuration/meter_provider_configuration.h"
#include "opentelemetry/sdk/configuration/metric_reader_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_file_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_http_encoding.h"
#include "opentelemetry/sdk/configuration/otlp_http_push_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/periodic_metric_reader_configuration.h"
#include "opentelemetry/sdk/configuration/prometheus_pull_metric_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/pull_metric_reader_configuration.h"
#include "opentelemetry/sdk/configuration/string_array_configuration.h"
#include "opentelemetry/sdk/configuration/temporality_preference.h"
#include "opentelemetry/sdk/configuration/translation_strategy.h"
#include "opentelemetry/sdk/configuration/view_configuration.h"
#include "opentelemetry/sdk/configuration/view_selector_configuration.h"
#include "opentelemetry/sdk/configuration/view_stream_configuration.h"
#include "opentelemetry/sdk/configuration/yaml_configuration_parser.h"

static std::unique_ptr<opentelemetry::sdk::configuration::Configuration> DoParse(
    const std::string &yaml)
{
  static const std::string source("test");
  return opentelemetry::sdk::configuration::YamlConfigurationParser::ParseString(source, yaml);
}

TEST(YamlMetrics, no_readers)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlMetrics, empty_readers)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlMetrics, many_readers)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
    - periodic:
        exporter:
          console:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->readers.size(), 2);
}

TEST(YamlMetrics, default_periodic_reader)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->exemplar_filter,
            opentelemetry::sdk::configuration::ExemplarFilter::trace_based);
  ASSERT_EQ(config->meter_provider->readers.size(), 1);
  auto *reader = config->meter_provider->readers[0].get();
  ASSERT_NE(reader, nullptr);
  auto *periodic =
      reinterpret_cast<opentelemetry::sdk::configuration::PeriodicMetricReaderConfiguration *>(
          reader);
  ASSERT_EQ(periodic->interval, 5000);
  ASSERT_EQ(periodic->timeout, 30000);
  ASSERT_NE(periodic->exporter, nullptr);
  auto *exporter = periodic->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *cardinality_limits = periodic->cardinality_limits.get();
  ASSERT_EQ(cardinality_limits, nullptr);
}

TEST(YamlMetrics, periodic_reader)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        interval: 2500
        timeout: 15000
        exporter:
          console:
        cardinality_limits:
          default: 100
          counter: 200
          gauge: 300
          histogram: 400
          observable_counter: 500
          observable_gauge: 600
          observable_up_down_counter: 700
          up_down_counter: 800
  exemplar_filter: always_on
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->exemplar_filter,
            opentelemetry::sdk::configuration::ExemplarFilter::always_on);
  ASSERT_EQ(config->meter_provider->readers.size(), 1);
  auto *reader = config->meter_provider->readers[0].get();
  ASSERT_NE(reader, nullptr);
  auto *periodic =
      reinterpret_cast<opentelemetry::sdk::configuration::PeriodicMetricReaderConfiguration *>(
          reader);
  ASSERT_EQ(periodic->interval, 2500);
  ASSERT_EQ(periodic->timeout, 15000);
  ASSERT_NE(periodic->exporter, nullptr);
  auto *exporter = periodic->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *cardinality_limits = periodic->cardinality_limits.get();
  ASSERT_NE(cardinality_limits, nullptr);
  ASSERT_EQ(cardinality_limits->default_limit, 100);
  ASSERT_EQ(cardinality_limits->counter, 200);
  ASSERT_EQ(cardinality_limits->gauge, 300);
  ASSERT_EQ(cardinality_limits->histogram, 400);
  ASSERT_EQ(cardinality_limits->observable_counter, 500);
  ASSERT_EQ(cardinality_limits->observable_gauge, 600);
  ASSERT_EQ(cardinality_limits->observable_up_down_counter, 700);
  ASSERT_EQ(cardinality_limits->up_down_counter, 800);
}

TEST(YamlMetrics, default_pull_reader)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - pull:
        exporter:
          prometheus/development:
  exemplar_filter: always_off
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->exemplar_filter,
            opentelemetry::sdk::configuration::ExemplarFilter::always_off);
  ASSERT_EQ(config->meter_provider->readers.size(), 1);
  auto *reader = config->meter_provider->readers[0].get();
  ASSERT_NE(reader, nullptr);
  auto *pull =
      reinterpret_cast<opentelemetry::sdk::configuration::PullMetricReaderConfiguration *>(reader);
  ASSERT_NE(pull->exporter, nullptr);
  auto *exporter = pull->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *cardinality_limits = pull->cardinality_limits.get();
  ASSERT_EQ(cardinality_limits, nullptr);
}

TEST(YamlMetrics, pull_reader)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - pull:
        exporter:
          prometheus/development:
        cardinality_limits:
          default: 100
          counter: 200
          gauge: 300
          histogram: 400
          observable_counter: 500
          observable_gauge: 600
          observable_up_down_counter: 700
          up_down_counter: 800
  exemplar_filter: trace_based
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->exemplar_filter,
            opentelemetry::sdk::configuration::ExemplarFilter::trace_based);
  ASSERT_EQ(config->meter_provider->readers.size(), 1);
  auto *reader = config->meter_provider->readers[0].get();
  ASSERT_NE(reader, nullptr);
  auto *pull =
      reinterpret_cast<opentelemetry::sdk::configuration::PullMetricReaderConfiguration *>(reader);
  ASSERT_NE(pull->exporter, nullptr);
  auto *exporter = pull->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *cardinality_limits = pull->cardinality_limits.get();
  ASSERT_NE(cardinality_limits, nullptr);
  ASSERT_EQ(cardinality_limits->default_limit, 100);
  ASSERT_EQ(cardinality_limits->counter, 200);
  ASSERT_EQ(cardinality_limits->gauge, 300);
  ASSERT_EQ(cardinality_limits->histogram, 400);
  ASSERT_EQ(cardinality_limits->observable_counter, 500);
  ASSERT_EQ(cardinality_limits->observable_gauge, 600);
  ASSERT_EQ(cardinality_limits->observable_up_down_counter, 700);
  ASSERT_EQ(cardinality_limits->up_down_counter, 800);
}

TEST(YamlMetrics, default_otlp_http)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          otlp_http:
            endpoint: "somewhere"
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->readers.size(), 1);
  auto *reader = config->meter_provider->readers[0].get();
  ASSERT_NE(reader, nullptr);
  auto *periodic =
      reinterpret_cast<opentelemetry::sdk::configuration::PeriodicMetricReaderConfiguration *>(
          reader);
  ASSERT_NE(periodic->exporter, nullptr);
  auto *exporter = periodic->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *otlp_http = reinterpret_cast<
      opentelemetry::sdk::configuration::OtlpHttpPushMetricExporterConfiguration *>(exporter);
  ASSERT_EQ(otlp_http->endpoint, "somewhere");
  ASSERT_EQ(otlp_http->tls, nullptr);
  ASSERT_EQ(otlp_http->headers, nullptr);
  ASSERT_EQ(otlp_http->headers_list, "");
  ASSERT_EQ(otlp_http->compression, "");
  ASSERT_EQ(otlp_http->timeout, 10000);
  ASSERT_EQ(otlp_http->temporality_preference,
            opentelemetry::sdk::configuration::TemporalityPreference::cumulative);
  ASSERT_EQ(
      otlp_http->default_histogram_aggregation,
      opentelemetry::sdk::configuration::DefaultHistogramAggregation::explicit_bucket_histogram);
  ASSERT_EQ(otlp_http->encoding, opentelemetry::sdk::configuration::OtlpHttpEncoding::protobuf);
}

TEST(YamlMetrics, otlp_http)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          otlp_http:
            endpoint: "somewhere"
            tls:
              ca_file: "ca_file"
              key_file: "key_file"
              cert_file: "cert_file"
            headers:
              - name: foo
                value: "123"
              - name: bar
                value: "456"
            headers_list: "baz=789"
            compression: "compression"
            timeout: 5000
            temporality_preference: delta
            default_histogram_aggregation: base2_exponential_bucket_histogram
            encoding: json
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->readers.size(), 1);
  auto *reader = config->meter_provider->readers[0].get();
  ASSERT_NE(reader, nullptr);
  auto *periodic =
      reinterpret_cast<opentelemetry::sdk::configuration::PeriodicMetricReaderConfiguration *>(
          reader);
  ASSERT_NE(periodic->exporter, nullptr);
  auto *exporter = periodic->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *otlp_http = reinterpret_cast<
      opentelemetry::sdk::configuration::OtlpHttpPushMetricExporterConfiguration *>(exporter);
  ASSERT_EQ(otlp_http->endpoint, "somewhere");
  ASSERT_NE(otlp_http->tls, nullptr);
  ASSERT_EQ(otlp_http->tls->ca_file, "ca_file");
  ASSERT_EQ(otlp_http->tls->key_file, "key_file");
  ASSERT_EQ(otlp_http->tls->cert_file, "cert_file");
  ASSERT_NE(otlp_http->headers, nullptr);
  ASSERT_EQ(otlp_http->headers->kv_map.size(), 2);
  ASSERT_EQ(otlp_http->headers->kv_map["foo"], "123");
  ASSERT_EQ(otlp_http->headers->kv_map["bar"], "456");
  ASSERT_EQ(otlp_http->headers_list, "baz=789");
  ASSERT_EQ(otlp_http->compression, "compression");
  ASSERT_EQ(otlp_http->timeout, 5000);
  ASSERT_EQ(otlp_http->temporality_preference,
            opentelemetry::sdk::configuration::TemporalityPreference::delta);
  ASSERT_EQ(otlp_http->default_histogram_aggregation,
            opentelemetry::sdk::configuration::DefaultHistogramAggregation::
                base2_exponential_bucket_histogram);
  ASSERT_EQ(otlp_http->encoding, opentelemetry::sdk::configuration::OtlpHttpEncoding::json);
}

TEST(YamlMetrics, default_otlp_grpc)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          otlp_grpc:
            endpoint: "somewhere"
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->readers.size(), 1);
  auto *reader = config->meter_provider->readers[0].get();
  ASSERT_NE(reader, nullptr);
  auto *periodic =
      reinterpret_cast<opentelemetry::sdk::configuration::PeriodicMetricReaderConfiguration *>(
          reader);
  ASSERT_NE(periodic->exporter, nullptr);
  auto *exporter = periodic->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *otlp_grpc = reinterpret_cast<
      opentelemetry::sdk::configuration::OtlpGrpcPushMetricExporterConfiguration *>(exporter);
  ASSERT_EQ(otlp_grpc->endpoint, "somewhere");
  ASSERT_EQ(otlp_grpc->tls, nullptr);
  ASSERT_EQ(otlp_grpc->headers, nullptr);
  ASSERT_EQ(otlp_grpc->headers_list, "");
  ASSERT_EQ(otlp_grpc->compression, "");
  ASSERT_EQ(otlp_grpc->timeout, 10000);
  ASSERT_EQ(otlp_grpc->temporality_preference,
            opentelemetry::sdk::configuration::TemporalityPreference::cumulative);
  ASSERT_EQ(
      otlp_grpc->default_histogram_aggregation,
      opentelemetry::sdk::configuration::DefaultHistogramAggregation::explicit_bucket_histogram);
}

TEST(YamlMetrics, otlp_grpc)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          otlp_grpc:
            endpoint: "somewhere"
            tls:
              ca_file: "ca_file"
              key_file: "key_file"
              cert_file: "cert_file"
              insecure: true
            headers:
              - name: foo
                value: "123"
              - name: bar
                value: "456"
            headers_list: "baz=789"
            compression: "compression"
            timeout: 5000
            temporality_preference: delta
            default_histogram_aggregation: base2_exponential_bucket_histogram
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->readers.size(), 1);
  auto *reader = config->meter_provider->readers[0].get();
  ASSERT_NE(reader, nullptr);
  auto *periodic =
      reinterpret_cast<opentelemetry::sdk::configuration::PeriodicMetricReaderConfiguration *>(
          reader);
  ASSERT_NE(periodic->exporter, nullptr);
  auto *exporter = periodic->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *otlp_grpc = reinterpret_cast<
      opentelemetry::sdk::configuration::OtlpGrpcPushMetricExporterConfiguration *>(exporter);
  ASSERT_EQ(otlp_grpc->endpoint, "somewhere");
  ASSERT_NE(otlp_grpc->tls, nullptr);
  ASSERT_EQ(otlp_grpc->tls->ca_file, "ca_file");
  ASSERT_EQ(otlp_grpc->tls->key_file, "key_file");
  ASSERT_EQ(otlp_grpc->tls->cert_file, "cert_file");
  ASSERT_EQ(otlp_grpc->tls->insecure, true);
  ASSERT_NE(otlp_grpc->headers, nullptr);
  ASSERT_EQ(otlp_grpc->headers->kv_map.size(), 2);
  ASSERT_EQ(otlp_grpc->headers->kv_map["foo"], "123");
  ASSERT_EQ(otlp_grpc->headers->kv_map["bar"], "456");
  ASSERT_EQ(otlp_grpc->headers_list, "baz=789");
  ASSERT_EQ(otlp_grpc->compression, "compression");
  ASSERT_EQ(otlp_grpc->timeout, 5000);
  ASSERT_EQ(otlp_grpc->temporality_preference,
            opentelemetry::sdk::configuration::TemporalityPreference::delta);
  ASSERT_EQ(otlp_grpc->default_histogram_aggregation,
            opentelemetry::sdk::configuration::DefaultHistogramAggregation::
                base2_exponential_bucket_histogram);
}

TEST(YamlMetrics, default_otlp_file)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          otlp_file/development:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->readers.size(), 1);
  auto *reader = config->meter_provider->readers[0].get();
  ASSERT_NE(reader, nullptr);
  auto *periodic =
      reinterpret_cast<opentelemetry::sdk::configuration::PeriodicMetricReaderConfiguration *>(
          reader);
  ASSERT_NE(periodic->exporter, nullptr);
  auto *exporter = periodic->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *otlp_file = reinterpret_cast<
      opentelemetry::sdk::configuration::OtlpFilePushMetricExporterConfiguration *>(exporter);
  ASSERT_EQ(otlp_file->output_stream, "");
  ASSERT_EQ(otlp_file->temporality_preference,
            opentelemetry::sdk::configuration::TemporalityPreference::cumulative);
  ASSERT_EQ(
      otlp_file->default_histogram_aggregation,
      opentelemetry::sdk::configuration::DefaultHistogramAggregation::explicit_bucket_histogram);
}

TEST(YamlMetrics, otlp_file)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          otlp_file/development:
            output_stream: "somewhere"
            temporality_preference: delta
            default_histogram_aggregation: base2_exponential_bucket_histogram
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->readers.size(), 1);
  auto *reader = config->meter_provider->readers[0].get();
  ASSERT_NE(reader, nullptr);
  auto *periodic =
      reinterpret_cast<opentelemetry::sdk::configuration::PeriodicMetricReaderConfiguration *>(
          reader);
  ASSERT_NE(periodic->exporter, nullptr);
  auto *exporter = periodic->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *otlp_file = reinterpret_cast<
      opentelemetry::sdk::configuration::OtlpFilePushMetricExporterConfiguration *>(exporter);
  ASSERT_EQ(otlp_file->output_stream, "somewhere");
  ASSERT_EQ(otlp_file->temporality_preference,
            opentelemetry::sdk::configuration::TemporalityPreference::delta);
  ASSERT_EQ(otlp_file->default_histogram_aggregation,
            opentelemetry::sdk::configuration::DefaultHistogramAggregation::
                base2_exponential_bucket_histogram);
}

TEST(YamlMetrics, default_console)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->readers.size(), 1);
  auto *reader = config->meter_provider->readers[0].get();
  ASSERT_NE(reader, nullptr);
  auto *periodic =
      reinterpret_cast<opentelemetry::sdk::configuration::PeriodicMetricReaderConfiguration *>(
          reader);
  ASSERT_NE(periodic->exporter, nullptr);
  auto *exporter = periodic->exporter.get();
  ASSERT_NE(exporter, nullptr);

#if 0
  auto *console =
      reinterpret_cast<opentelemetry::sdk::configuration::ConsolePushMetricExporterConfiguration *>(
          exporter);

  // FIXME-CONFIG: https://github.com/open-telemetry/opentelemetry-configuration/issues/242

  ASSERT_EQ(console->temporality_preference,
            opentelemetry::sdk::configuration::TemporalityPreference::cumulative);
  ASSERT_EQ(
      console->default_histogram_aggregation,
      opentelemetry::sdk::configuration::DefaultHistogramAggregation::explicit_bucket_histogram);
#endif
}

TEST(YamlMetrics, console)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
            temporality_preference: delta
            default_histogram_aggregation: base2_exponential_bucket_histogram
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->readers.size(), 1);
  auto *reader = config->meter_provider->readers[0].get();
  ASSERT_NE(reader, nullptr);
  auto *periodic =
      reinterpret_cast<opentelemetry::sdk::configuration::PeriodicMetricReaderConfiguration *>(
          reader);
  ASSERT_NE(periodic->exporter, nullptr);
  auto *exporter = periodic->exporter.get();
  ASSERT_NE(exporter, nullptr);

#if 0
  auto *console =
      reinterpret_cast<opentelemetry::sdk::configuration::ConsolePushMetricExporterConfiguration *>(
          exporter);

  // FIXME-CONFIG: https://github.com/open-telemetry/opentelemetry-configuration/issues/242

  ASSERT_EQ(console->temporality_preference,
            opentelemetry::sdk::configuration::TemporalityPreference::cumulative);
  ASSERT_EQ(
      console->default_histogram_aggregation,
      opentelemetry::sdk::configuration::DefaultHistogramAggregation::explicit_bucket_histogram);
#endif
}

TEST(YamlMetrics, default_prometheus)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - pull:
        exporter:
          prometheus/development:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->readers.size(), 1);
  auto *reader = config->meter_provider->readers[0].get();
  ASSERT_NE(reader, nullptr);
  auto *pull =
      reinterpret_cast<opentelemetry::sdk::configuration::PullMetricReaderConfiguration *>(reader);
  ASSERT_NE(pull->exporter, nullptr);
  auto *exporter = pull->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *prometheus = reinterpret_cast<
      opentelemetry::sdk::configuration::PrometheusPullMetricExporterConfiguration *>(exporter);
  ASSERT_EQ(prometheus->host, "localhost");
  ASSERT_EQ(prometheus->port, 9464);
  ASSERT_EQ(prometheus->without_scope_info, false);
  ASSERT_EQ(prometheus->without_target_info, false);
  ASSERT_EQ(prometheus->translation_strategy,
            opentelemetry::sdk::configuration::TranslationStrategy::UnderscoreEscapingWithSuffixes);
  ASSERT_EQ(prometheus->with_resource_constant_labels, nullptr);
}

TEST(YamlMetrics, prometheus)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - pull:
        exporter:
          prometheus/development:
            host: "prometheus"
            port: 1234
            without_scope_info: true
            without_target_info: true
            translation_strategy: NoUTF8EscapingWithSuffixes
            with_resource_constant_labels:
              included:
                - "foo.in"
                - "bar.in"
              excluded:
                - "baz.ex"
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->readers.size(), 1);
  auto *reader = config->meter_provider->readers[0].get();
  ASSERT_NE(reader, nullptr);
  auto *pull =
      reinterpret_cast<opentelemetry::sdk::configuration::PullMetricReaderConfiguration *>(reader);
  ASSERT_NE(pull->exporter, nullptr);
  auto *exporter = pull->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *prometheus = reinterpret_cast<
      opentelemetry::sdk::configuration::PrometheusPullMetricExporterConfiguration *>(exporter);
  ASSERT_EQ(prometheus->host, "prometheus");
  ASSERT_EQ(prometheus->port, 1234);
  ASSERT_EQ(prometheus->without_scope_info, true);
  ASSERT_EQ(prometheus->without_target_info, true);
  ASSERT_EQ(prometheus->translation_strategy,
            opentelemetry::sdk::configuration::TranslationStrategy::NoUTF8EscapingWithSuffixes);
  ASSERT_NE(prometheus->with_resource_constant_labels, nullptr);
  ASSERT_NE(prometheus->with_resource_constant_labels->included, nullptr);
  ASSERT_EQ(prometheus->with_resource_constant_labels->included->string_array.size(), 2);
  ASSERT_EQ(prometheus->with_resource_constant_labels->included->string_array[0], "foo.in");
  ASSERT_EQ(prometheus->with_resource_constant_labels->included->string_array[1], "bar.in");
  ASSERT_NE(prometheus->with_resource_constant_labels->excluded, nullptr);
  ASSERT_EQ(prometheus->with_resource_constant_labels->excluded->string_array.size(), 1);
  ASSERT_EQ(prometheus->with_resource_constant_labels->excluded->string_array[0], "baz.ex");
}

TEST(YamlMetrics, empty_views)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
  views:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->views.size(), 0);
}

TEST(YamlMetrics, default_views)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
  views:
    - selector:
      stream:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->views.size(), 1);
  auto *view = config->meter_provider->views[0].get();
  ASSERT_NE(view, nullptr);
  ASSERT_NE(view->selector, nullptr);
  ASSERT_EQ(view->selector->instrument_name, "");
  ASSERT_EQ(view->selector->instrument_type,
            opentelemetry::sdk::configuration::InstrumentType::none);
  ASSERT_EQ(view->selector->unit, "");
  ASSERT_EQ(view->selector->meter_name, "");
  ASSERT_EQ(view->selector->meter_version, "");
  ASSERT_EQ(view->selector->meter_schema_url, "");
  ASSERT_NE(view->stream, nullptr);
  ASSERT_EQ(view->stream->name, "");
  ASSERT_EQ(view->stream->description, "");
  ASSERT_EQ(view->stream->aggregation_cardinality_limit, 0);
  ASSERT_EQ(view->stream->aggregation, nullptr);
  ASSERT_EQ(view->stream->attribute_keys, nullptr);
}

TEST(YamlMetrics, selector)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
  views:
    - selector:
        instrument_name: "instrument_name"
        instrument_type: "counter"
        unit: "unit"
        meter_name: "meter_name"
        meter_version: "meter_version"
        meter_schema_url: "meter_schema_url"
      stream:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->views.size(), 1);
  auto *view = config->meter_provider->views[0].get();
  ASSERT_NE(view, nullptr);
  ASSERT_NE(view->selector, nullptr);
  ASSERT_EQ(view->selector->instrument_name, "instrument_name");
  ASSERT_EQ(view->selector->instrument_type,
            opentelemetry::sdk::configuration::InstrumentType::counter);
  ASSERT_EQ(view->selector->unit, "unit");
  ASSERT_EQ(view->selector->meter_name, "meter_name");
  ASSERT_EQ(view->selector->meter_version, "meter_version");
  ASSERT_EQ(view->selector->meter_schema_url, "meter_schema_url");
  ASSERT_NE(view->stream, nullptr);
  ASSERT_EQ(view->stream->name, "");
  ASSERT_EQ(view->stream->description, "");
  ASSERT_EQ(view->stream->aggregation_cardinality_limit, 0);
  ASSERT_EQ(view->stream->aggregation, nullptr);
  ASSERT_EQ(view->stream->attribute_keys, nullptr);
}

TEST(YamlMetrics, stream)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
  views:
    - selector:
      stream:
        name: "name"
        description: "description"
        aggregation_cardinality_limit: 1234
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->views.size(), 1);
  auto *view = config->meter_provider->views[0].get();
  ASSERT_NE(view, nullptr);
  ASSERT_NE(view->selector, nullptr);
  ASSERT_EQ(view->selector->instrument_name, "");
  ASSERT_EQ(view->selector->instrument_type,
            opentelemetry::sdk::configuration::InstrumentType::none);
  ASSERT_EQ(view->selector->unit, "");
  ASSERT_EQ(view->selector->meter_name, "");
  ASSERT_EQ(view->selector->meter_version, "");
  ASSERT_EQ(view->selector->meter_schema_url, "");
  ASSERT_NE(view->stream, nullptr);
  ASSERT_EQ(view->stream->name, "name");
  ASSERT_EQ(view->stream->description, "description");
  ASSERT_EQ(view->stream->aggregation_cardinality_limit, 1234);
  ASSERT_EQ(view->stream->aggregation, nullptr);
  ASSERT_EQ(view->stream->attribute_keys, nullptr);
}

TEST(YamlMetrics, stream_aggregation_default)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
  views:
    - selector:
      stream:
        aggregation:
          default:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->views.size(), 1);
  auto *view = config->meter_provider->views[0].get();
  ASSERT_NE(view, nullptr);
  ASSERT_NE(view->selector, nullptr);
  ASSERT_NE(view->stream, nullptr);
  ASSERT_NE(view->stream->aggregation, nullptr);
  ASSERT_EQ(view->stream->attribute_keys, nullptr);
}

TEST(YamlMetrics, stream_aggregation_drop)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
  views:
    - selector:
      stream:
        aggregation:
          drop:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->views.size(), 1);
  auto *view = config->meter_provider->views[0].get();
  ASSERT_NE(view, nullptr);
  ASSERT_NE(view->selector, nullptr);
  ASSERT_NE(view->stream, nullptr);
  ASSERT_NE(view->stream->aggregation, nullptr);
  ASSERT_EQ(view->stream->attribute_keys, nullptr);
}

TEST(YamlMetrics, stream_aggregation_explicit_bucket_histogram)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
  views:
    - selector:
      stream:
        aggregation:
          explicit_bucket_histogram:
            boundaries:
              - 10
              - 20
              - 30
            record_min_max: false
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->views.size(), 1);
  auto *view = config->meter_provider->views[0].get();
  ASSERT_NE(view, nullptr);
  ASSERT_NE(view->selector, nullptr);
  ASSERT_NE(view->stream, nullptr);
  ASSERT_NE(view->stream->aggregation, nullptr);
  auto *aggregation               = view->stream->aggregation.get();
  auto *explicit_bucket_histogram = reinterpret_cast<
      opentelemetry::sdk::configuration::ExplicitBucketHistogramAggregationConfiguration *>(
      aggregation);
  ASSERT_EQ(explicit_bucket_histogram->boundaries.size(), 3);
  ASSERT_EQ(explicit_bucket_histogram->boundaries[0], 10);
  ASSERT_EQ(explicit_bucket_histogram->boundaries[1], 20);
  ASSERT_EQ(explicit_bucket_histogram->boundaries[2], 30);
  ASSERT_EQ(explicit_bucket_histogram->record_min_max, false);
  ASSERT_EQ(view->stream->attribute_keys, nullptr);
}

TEST(YamlMetrics, stream_aggregation_base2_exponential_bucket_histogram)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
  views:
    - selector:
      stream:
        aggregation:
          base2_exponential_bucket_histogram:
            max_scale: 40
            max_size: 320
            record_min_max: false
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->views.size(), 1);
  auto *view = config->meter_provider->views[0].get();
  ASSERT_NE(view, nullptr);
  ASSERT_NE(view->selector, nullptr);
  ASSERT_NE(view->stream, nullptr);
  ASSERT_NE(view->stream->aggregation, nullptr);
  auto *aggregation                        = view->stream->aggregation.get();
  auto *base2_exponential_bucket_histogram = reinterpret_cast<
      opentelemetry::sdk::configuration::Base2ExponentialBucketHistogramAggregationConfiguration *>(
      aggregation);
  ASSERT_EQ(base2_exponential_bucket_histogram->max_scale, 40);
  ASSERT_EQ(base2_exponential_bucket_histogram->max_size, 320);
  ASSERT_EQ(base2_exponential_bucket_histogram->record_min_max, false);
  ASSERT_EQ(view->stream->attribute_keys, nullptr);
}

TEST(YamlMetrics, stream_aggregation_last_value)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
  views:
    - selector:
      stream:
        aggregation:
          last_value:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->views.size(), 1);
  auto *view = config->meter_provider->views[0].get();
  ASSERT_NE(view, nullptr);
  ASSERT_NE(view->selector, nullptr);
  ASSERT_NE(view->stream, nullptr);
  ASSERT_NE(view->stream->aggregation, nullptr);
  ASSERT_EQ(view->stream->attribute_keys, nullptr);
}

TEST(YamlMetrics, stream_aggregation_sum)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
  views:
    - selector:
      stream:
        aggregation:
          sum:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->views.size(), 1);
  auto *view = config->meter_provider->views[0].get();
  ASSERT_NE(view, nullptr);
  ASSERT_NE(view->selector, nullptr);
  ASSERT_NE(view->stream, nullptr);
  ASSERT_NE(view->stream->aggregation, nullptr);
  ASSERT_EQ(view->stream->attribute_keys, nullptr);
}

TEST(YamlMetrics, stream_attribute_keys)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
  views:
    - selector:
      stream:
        attribute_keys:
          included:
            - foo.in
            - bar.in
          excluded:
            - foo.ex
            - bar.ex
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->views.size(), 1);
  auto *view = config->meter_provider->views[0].get();
  ASSERT_NE(view, nullptr);
  ASSERT_NE(view->selector, nullptr);
  ASSERT_NE(view->stream, nullptr);
  ASSERT_EQ(view->stream->aggregation, nullptr);
  ASSERT_NE(view->stream->attribute_keys, nullptr);
  ASSERT_EQ(view->stream->attribute_keys->included->string_array.size(), 2);
  ASSERT_EQ(view->stream->attribute_keys->included->string_array[0], "foo.in");
  ASSERT_EQ(view->stream->attribute_keys->included->string_array[1], "bar.in");
  ASSERT_NE(view->stream->attribute_keys->excluded, nullptr);
  ASSERT_EQ(view->stream->attribute_keys->excluded->string_array.size(), 2);
  ASSERT_EQ(view->stream->attribute_keys->excluded->string_array[0], "foo.ex");
  ASSERT_EQ(view->stream->attribute_keys->excluded->string_array[1], "bar.ex");
}

TEST(YamlMetrics, no_meter_configurator)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_EQ(config->meter_provider->meter_configurator, nullptr);
}

TEST(YamlMetrics, meter_configurator_default_only)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
  meter_configurator/development:
    default_config:
      enabled: false
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_NE(config->meter_provider->meter_configurator, nullptr);
  ASSERT_EQ(config->meter_provider->meter_configurator->default_config.enabled, false);
  ASSERT_EQ(config->meter_provider->meter_configurator->meters.size(), 0);
}

TEST(YamlMetrics, meter_configurator_with_meters)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
  meter_configurator/development:
    default_config:
      enabled: false
    meters:
      - name: io.opentelemetry.contrib.*
        config:
          enabled: true
      - name: my.exact.meter
        config:
          enabled: false
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_NE(config->meter_provider->meter_configurator, nullptr);

  auto &configurator = config->meter_provider->meter_configurator;
  ASSERT_EQ(configurator->default_config.enabled, false);
  ASSERT_EQ(configurator->meters.size(), 2);

  ASSERT_EQ(configurator->meters[0].name, "io.opentelemetry.contrib.*");
  ASSERT_EQ(configurator->meters[0].config.enabled, true);

  ASSERT_EQ(configurator->meters[1].name, "my.exact.meter");
  ASSERT_EQ(configurator->meters[1].config.enabled, false);
}

TEST(YamlMetrics, meter_configurator_default_enabled)
{
  std::string yaml = R"(
file_format: "1.0-metrics"
meter_provider:
  readers:
    - periodic:
        exporter:
          console:
  meter_configurator/development:
    default_config:
      enabled: true
    meters:
      - name: noisy.library
        config:
          enabled: false
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->meter_provider, nullptr);
  ASSERT_NE(config->meter_provider->meter_configurator, nullptr);

  auto &configurator = config->meter_provider->meter_configurator;
  ASSERT_EQ(configurator->default_config.enabled, true);
  ASSERT_EQ(configurator->meters.size(), 1);
  ASSERT_EQ(configurator->meters[0].name, "noisy.library");
  ASSERT_EQ(configurator->meters[0].config.enabled, false);
}
