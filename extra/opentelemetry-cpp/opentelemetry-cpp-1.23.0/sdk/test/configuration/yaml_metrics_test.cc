// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "opentelemetry/sdk/configuration/base2_exponential_bucket_histogram_aggregation_configuration.h"
#include "opentelemetry/sdk/configuration/configuration.h"
#include "opentelemetry/sdk/configuration/default_histogram_aggregation.h"
#include "opentelemetry/sdk/configuration/explicit_bucket_histogram_aggregation_configuration.h"
#include "opentelemetry/sdk/configuration/headers_configuration.h"
#include "opentelemetry/sdk/configuration/include_exclude_configuration.h"
#include "opentelemetry/sdk/configuration/instrument_type.h"
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
file_format: xx.yy
meter_provider:
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlMetrics, empty_readers)
{
  std::string yaml = R"(
file_format: xx.yy
meter_provider:
  readers:
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlMetrics, many_readers)
{
  std::string yaml = R"(
file_format: xx.yy
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
file_format: xx.yy
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
  ASSERT_EQ(periodic->interval, 5000);
  ASSERT_EQ(periodic->timeout, 30000);
  ASSERT_NE(periodic->exporter, nullptr);
  auto *exporter = periodic->exporter.get();
  ASSERT_NE(exporter, nullptr);
}

TEST(YamlMetrics, periodic_reader)
{
  std::string yaml = R"(
file_format: xx.yy
meter_provider:
  readers:
    - periodic:
        interval: 2500
        timeout: 15000
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
  ASSERT_EQ(periodic->interval, 2500);
  ASSERT_EQ(periodic->timeout, 15000);
  ASSERT_NE(periodic->exporter, nullptr);
  auto *exporter = periodic->exporter.get();
  ASSERT_NE(exporter, nullptr);
}

TEST(YamlMetrics, pull_reader)
{
  std::string yaml = R"(
file_format: xx.yy
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
}

TEST(YamlMetrics, default_otlp_http)
{
  std::string yaml = R"(
file_format: xx.yy
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
  ASSERT_EQ(otlp_http->certificate_file, "");
  ASSERT_EQ(otlp_http->client_key_file, "");
  ASSERT_EQ(otlp_http->client_certificate_file, "");
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
file_format: xx.yy
meter_provider:
  readers:
    - periodic:
        exporter:
          otlp_http:
            endpoint: "somewhere"
            certificate_file: "certificate_file"
            client_key_file: "client_key_file"
            client_certificate_file: "client_certificate_file"
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
  ASSERT_EQ(otlp_http->certificate_file, "certificate_file");
  ASSERT_EQ(otlp_http->client_key_file, "client_key_file");
  ASSERT_EQ(otlp_http->client_certificate_file, "client_certificate_file");
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
file_format: xx.yy
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
  ASSERT_EQ(otlp_grpc->certificate_file, "");
  ASSERT_EQ(otlp_grpc->client_key_file, "");
  ASSERT_EQ(otlp_grpc->client_certificate_file, "");
  ASSERT_EQ(otlp_grpc->headers, nullptr);
  ASSERT_EQ(otlp_grpc->headers_list, "");
  ASSERT_EQ(otlp_grpc->compression, "");
  ASSERT_EQ(otlp_grpc->timeout, 10000);
  ASSERT_EQ(otlp_grpc->temporality_preference,
            opentelemetry::sdk::configuration::TemporalityPreference::cumulative);
  ASSERT_EQ(
      otlp_grpc->default_histogram_aggregation,
      opentelemetry::sdk::configuration::DefaultHistogramAggregation::explicit_bucket_histogram);
  ASSERT_EQ(otlp_grpc->insecure, false);
}

TEST(YamlMetrics, otlp_grpc)
{
  std::string yaml = R"(
file_format: xx.yy
meter_provider:
  readers:
    - periodic:
        exporter:
          otlp_grpc:
            endpoint: "somewhere"
            certificate_file: "certificate_file"
            client_key_file: "client_key_file"
            client_certificate_file: "client_certificate_file"
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
            insecure: true
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
  ASSERT_EQ(otlp_grpc->certificate_file, "certificate_file");
  ASSERT_EQ(otlp_grpc->client_key_file, "client_key_file");
  ASSERT_EQ(otlp_grpc->client_certificate_file, "client_certificate_file");
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
  ASSERT_EQ(otlp_grpc->insecure, true);
}

TEST(YamlMetrics, default_otlp_file)
{
  std::string yaml = R"(
file_format: xx.yy
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
file_format: xx.yy
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
file_format: xx.yy
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
file_format: xx.yy
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
file_format: xx.yy
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
  ASSERT_EQ(prometheus->without_units, false);
  ASSERT_EQ(prometheus->without_type_suffix, false);
  ASSERT_EQ(prometheus->without_scope_info, false);
}

TEST(YamlMetrics, prometheus)
{
  std::string yaml = R"(
file_format: xx.yy
meter_provider:
  readers:
    - pull:
        exporter:
          prometheus/development:
            host: "prometheus"
            port: 1234
            without_units: true
            without_type_suffix: true
            without_scope_info: true
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
  ASSERT_EQ(prometheus->without_units, true);
  ASSERT_EQ(prometheus->without_type_suffix, true);
  ASSERT_EQ(prometheus->without_scope_info, true);
}

TEST(YamlMetrics, empty_views)
{
  std::string yaml = R"(
file_format: xx.yy
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
file_format: xx.yy
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
file_format: xx.yy
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
file_format: xx.yy
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
file_format: xx.yy
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
file_format: xx.yy
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
file_format: xx.yy
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
file_format: xx.yy
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
file_format: xx.yy
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
file_format: xx.yy
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
file_format: xx.yy
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
