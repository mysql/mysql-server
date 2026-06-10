// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "opentelemetry/sdk/configuration/batch_span_processor_configuration.h"
#include "opentelemetry/sdk/configuration/configuration.h"
#include "opentelemetry/sdk/configuration/headers_configuration.h"
#include "opentelemetry/sdk/configuration/jaeger_remote_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_file_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_http_encoding.h"
#include "opentelemetry/sdk/configuration/otlp_http_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/parent_based_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/simple_span_processor_configuration.h"
#include "opentelemetry/sdk/configuration/span_limits_configuration.h"
#include "opentelemetry/sdk/configuration/span_processor_configuration.h"
#include "opentelemetry/sdk/configuration/trace_id_ratio_based_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/tracer_provider_configuration.h"
#include "opentelemetry/sdk/configuration/yaml_configuration_parser.h"
#include "opentelemetry/sdk/configuration/zipkin_span_exporter_configuration.h"

static std::unique_ptr<opentelemetry::sdk::configuration::Configuration> DoParse(
    const std::string &yaml)
{
  static const std::string source("test");
  return opentelemetry::sdk::configuration::YamlConfigurationParser::ParseString(source, yaml);
}

TEST(YamlTrace, no_processors)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlTrace, empty_processors)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlTrace, many_processors)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
    - simple:
        exporter:
          console:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_EQ(config->tracer_provider->processors.size(), 2);
}

TEST(YamlTrace, simple_processor)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_EQ(config->tracer_provider->processors.size(), 1);
  auto *processor = config->tracer_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *simple =
      reinterpret_cast<opentelemetry::sdk::configuration::SimpleSpanProcessorConfiguration *>(
          processor);
  ASSERT_NE(simple->exporter, nullptr);
  auto *exporter = simple->exporter.get();
  ASSERT_NE(exporter, nullptr);
}

TEST(YamlTrace, default_batch_processor)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - batch:
        exporter:
          console:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_EQ(config->tracer_provider->processors.size(), 1);
  auto *processor = config->tracer_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *batch =
      reinterpret_cast<opentelemetry::sdk::configuration::BatchSpanProcessorConfiguration *>(
          processor);
  ASSERT_EQ(batch->schedule_delay, 5000);
  ASSERT_EQ(batch->export_timeout, 30000);
  ASSERT_EQ(batch->max_queue_size, 2048);
  ASSERT_EQ(batch->max_export_batch_size, 512);
  ASSERT_NE(batch->exporter, nullptr);
  auto *exporter = batch->exporter.get();
  ASSERT_NE(exporter, nullptr);
}

TEST(YamlTrace, batch_processor)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - batch:
        schedule_delay: 5555
        export_timeout: 33333
        max_queue_size: 1234
        max_export_batch_size: 256
        exporter:
          console:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_EQ(config->tracer_provider->processors.size(), 1);
  auto *processor = config->tracer_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *batch =
      reinterpret_cast<opentelemetry::sdk::configuration::BatchSpanProcessorConfiguration *>(
          processor);
  ASSERT_EQ(batch->schedule_delay, 5555);
  ASSERT_EQ(batch->export_timeout, 33333);
  ASSERT_EQ(batch->max_queue_size, 1234);
  ASSERT_EQ(batch->max_export_batch_size, 256);
  ASSERT_NE(batch->exporter, nullptr);
  auto *exporter = batch->exporter.get();
  ASSERT_NE(exporter, nullptr);
}

TEST(YamlTrace, default_otlp_http)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          otlp_http:
            endpoint: "somewhere"
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_EQ(config->tracer_provider->processors.size(), 1);
  auto *processor = config->tracer_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *simple =
      reinterpret_cast<opentelemetry::sdk::configuration::SimpleSpanProcessorConfiguration *>(
          processor);
  ASSERT_NE(simple->exporter, nullptr);
  auto *exporter = simple->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *otlp_http =
      reinterpret_cast<opentelemetry::sdk::configuration::OtlpHttpSpanExporterConfiguration *>(
          exporter);
  ASSERT_EQ(otlp_http->endpoint, "somewhere");
  ASSERT_EQ(otlp_http->certificate_file, "");
  ASSERT_EQ(otlp_http->client_key_file, "");
  ASSERT_EQ(otlp_http->client_certificate_file, "");
  ASSERT_EQ(otlp_http->headers, nullptr);
  ASSERT_EQ(otlp_http->headers_list, "");
  ASSERT_EQ(otlp_http->compression, "");
  ASSERT_EQ(otlp_http->timeout, 10000);
  ASSERT_EQ(otlp_http->encoding, opentelemetry::sdk::configuration::OtlpHttpEncoding::protobuf);
}

TEST(YamlTrace, otlp_http)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
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
            encoding: json
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_EQ(config->tracer_provider->processors.size(), 1);
  auto *processor = config->tracer_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *simple =
      reinterpret_cast<opentelemetry::sdk::configuration::SimpleSpanProcessorConfiguration *>(
          processor);
  ASSERT_NE(simple->exporter, nullptr);
  auto *exporter = simple->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *otlp_http =
      reinterpret_cast<opentelemetry::sdk::configuration::OtlpHttpSpanExporterConfiguration *>(
          exporter);
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
  ASSERT_EQ(otlp_http->encoding, opentelemetry::sdk::configuration::OtlpHttpEncoding::json);
}

TEST(YamlTrace, default_otlp_grpc)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          otlp_grpc:
            endpoint: "somewhere"
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_EQ(config->tracer_provider->processors.size(), 1);
  auto *processor = config->tracer_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *simple =
      reinterpret_cast<opentelemetry::sdk::configuration::SimpleSpanProcessorConfiguration *>(
          processor);
  ASSERT_NE(simple->exporter, nullptr);
  auto *exporter = simple->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *otlp_grpc =
      reinterpret_cast<opentelemetry::sdk::configuration::OtlpGrpcSpanExporterConfiguration *>(
          exporter);
  ASSERT_EQ(otlp_grpc->endpoint, "somewhere");
  ASSERT_EQ(otlp_grpc->certificate_file, "");
  ASSERT_EQ(otlp_grpc->client_key_file, "");
  ASSERT_EQ(otlp_grpc->client_certificate_file, "");
  ASSERT_EQ(otlp_grpc->headers, nullptr);
  ASSERT_EQ(otlp_grpc->headers_list, "");
  ASSERT_EQ(otlp_grpc->compression, "");
  ASSERT_EQ(otlp_grpc->timeout, 10000);
  ASSERT_EQ(otlp_grpc->insecure, false);
}

TEST(YamlTrace, otlp_grpc)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
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
            insecure: true
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_EQ(config->tracer_provider->processors.size(), 1);
  auto *processor = config->tracer_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *simple =
      reinterpret_cast<opentelemetry::sdk::configuration::SimpleSpanProcessorConfiguration *>(
          processor);
  ASSERT_NE(simple->exporter, nullptr);
  auto *exporter = simple->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *otlp_grpc =
      reinterpret_cast<opentelemetry::sdk::configuration::OtlpGrpcSpanExporterConfiguration *>(
          exporter);
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
  ASSERT_EQ(otlp_grpc->insecure, true);
}

TEST(YamlTrace, default_otlp_file)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          otlp_file/development:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_EQ(config->tracer_provider->processors.size(), 1);
  auto *processor = config->tracer_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *simple =
      reinterpret_cast<opentelemetry::sdk::configuration::SimpleSpanProcessorConfiguration *>(
          processor);
  ASSERT_NE(simple->exporter, nullptr);
  auto *exporter = simple->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *otlp_file =
      reinterpret_cast<opentelemetry::sdk::configuration::OtlpFileSpanExporterConfiguration *>(
          exporter);
  ASSERT_EQ(otlp_file->output_stream, "");
}

TEST(YamlTrace, otlp_file)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          otlp_file/development:
            output_stream: "somewhere"
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_EQ(config->tracer_provider->processors.size(), 1);
  auto *processor = config->tracer_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *simple =
      reinterpret_cast<opentelemetry::sdk::configuration::SimpleSpanProcessorConfiguration *>(
          processor);
  ASSERT_NE(simple->exporter, nullptr);
  auto *exporter = simple->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *otlp_file =
      reinterpret_cast<opentelemetry::sdk::configuration::OtlpFileSpanExporterConfiguration *>(
          exporter);
  ASSERT_EQ(otlp_file->output_stream, "somewhere");
}

TEST(YamlTrace, otlp_console)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_EQ(config->tracer_provider->processors.size(), 1);
  auto *processor = config->tracer_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *simple =
      reinterpret_cast<opentelemetry::sdk::configuration::SimpleSpanProcessorConfiguration *>(
          processor);
  ASSERT_NE(simple->exporter, nullptr);
  auto *exporter = simple->exporter.get();
  ASSERT_NE(exporter, nullptr);
}

TEST(YamlTrace, default_otlp_zipkin)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          zipkin:
            endpoint: "zipkin"
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_EQ(config->tracer_provider->processors.size(), 1);
  auto *processor = config->tracer_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *simple =
      reinterpret_cast<opentelemetry::sdk::configuration::SimpleSpanProcessorConfiguration *>(
          processor);
  ASSERT_NE(simple->exporter, nullptr);
  auto *exporter = simple->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *zipkin =
      reinterpret_cast<opentelemetry::sdk::configuration::ZipkinSpanExporterConfiguration *>(
          exporter);
  ASSERT_EQ(zipkin->endpoint, "zipkin");
  ASSERT_EQ(zipkin->timeout, 10000);
}

TEST(YamlTrace, otlp_zipkin)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          zipkin:
            endpoint: "zipkin"
            timeout: 5000
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_EQ(config->tracer_provider->processors.size(), 1);
  auto *processor = config->tracer_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *simple =
      reinterpret_cast<opentelemetry::sdk::configuration::SimpleSpanProcessorConfiguration *>(
          processor);
  ASSERT_NE(simple->exporter, nullptr);
  auto *exporter = simple->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *zipkin =
      reinterpret_cast<opentelemetry::sdk::configuration::ZipkinSpanExporterConfiguration *>(
          exporter);
  ASSERT_EQ(zipkin->endpoint, "zipkin");
  ASSERT_EQ(zipkin->timeout, 5000);
}

TEST(YamlTrace, no_limits)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_EQ(config->tracer_provider->limits, nullptr);
}

TEST(YamlTrace, default_limits)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  limits:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->limits, nullptr);
  ASSERT_EQ(config->tracer_provider->limits->attribute_value_length_limit, 4096);
  ASSERT_EQ(config->tracer_provider->limits->attribute_count_limit, 128);
  ASSERT_EQ(config->tracer_provider->limits->event_count_limit, 128);
  ASSERT_EQ(config->tracer_provider->limits->link_count_limit, 128);
  ASSERT_EQ(config->tracer_provider->limits->event_attribute_count_limit, 128);
  ASSERT_EQ(config->tracer_provider->limits->link_attribute_count_limit, 128);
}

TEST(YamlTrace, limits)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  limits:
    attribute_value_length_limit: 1111
    attribute_count_limit: 2222
    event_count_limit: 3333
    link_count_limit: 4444
    event_attribute_count_limit: 5555
    link_attribute_count_limit: 6666
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->limits, nullptr);
  ASSERT_EQ(config->tracer_provider->limits->attribute_value_length_limit, 1111);
  ASSERT_EQ(config->tracer_provider->limits->attribute_count_limit, 2222);
  ASSERT_EQ(config->tracer_provider->limits->event_count_limit, 3333);
  ASSERT_EQ(config->tracer_provider->limits->link_count_limit, 4444);
  ASSERT_EQ(config->tracer_provider->limits->event_attribute_count_limit, 5555);
  ASSERT_EQ(config->tracer_provider->limits->link_attribute_count_limit, 6666);
}

TEST(YamlTrace, no_sampler)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_EQ(config->tracer_provider->sampler, nullptr);
}

TEST(YamlTrace, empty_sampler)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlTrace, many_sampler)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    foo:
    bar:
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlTrace, always_off_sampler)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    always_off:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->sampler, nullptr);
}

TEST(YamlTrace, always_on_sampler)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    always_on:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->sampler, nullptr);
}

TEST(YamlTrace, jaeger_remote_sampler)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    jaeger_remote:
      endpoint: jaeger
      interval: 1234
      initial_sampler:
        always_off:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->sampler, nullptr);
  auto *sampler = config->tracer_provider->sampler.get();
  auto *jaeger =
      reinterpret_cast<opentelemetry::sdk::configuration::JaegerRemoteSamplerConfiguration *>(
          sampler);
  ASSERT_EQ(jaeger->endpoint, "jaeger");
  ASSERT_EQ(jaeger->interval, 1234);
  ASSERT_NE(jaeger->initial_sampler, nullptr);
}

TEST(YamlTrace, default_parent_based_sampler)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    parent_based:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->sampler, nullptr);
  auto *sampler = config->tracer_provider->sampler.get();
  auto *parent =
      reinterpret_cast<opentelemetry::sdk::configuration::ParentBasedSamplerConfiguration *>(
          sampler);
  ASSERT_EQ(parent->root, nullptr);
  ASSERT_EQ(parent->remote_parent_sampled, nullptr);
  ASSERT_EQ(parent->remote_parent_not_sampled, nullptr);
  ASSERT_EQ(parent->local_parent_sampled, nullptr);
  ASSERT_EQ(parent->local_parent_not_sampled, nullptr);
}

TEST(YamlTrace, parent_based_sampler)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    parent_based:
      root:
        always_off:
      remote_parent_sampled:
        always_off:
      remote_parent_not_sampled:
        always_off:
      local_parent_sampled:
        always_off:
      local_parent_not_sampled:
        always_off:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->sampler, nullptr);
  auto *sampler = config->tracer_provider->sampler.get();
  auto *parent =
      reinterpret_cast<opentelemetry::sdk::configuration::ParentBasedSamplerConfiguration *>(
          sampler);
  ASSERT_NE(parent->root, nullptr);
  ASSERT_NE(parent->remote_parent_sampled, nullptr);
  ASSERT_NE(parent->remote_parent_not_sampled, nullptr);
  ASSERT_NE(parent->local_parent_sampled, nullptr);
  ASSERT_NE(parent->local_parent_not_sampled, nullptr);
}

TEST(YamlTrace, default_trace_id_ratio_based_sampler)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    trace_id_ratio_based:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->sampler, nullptr);
  auto *sampler = config->tracer_provider->sampler.get();
  auto *ratio =
      reinterpret_cast<opentelemetry::sdk::configuration::TraceIdRatioBasedSamplerConfiguration *>(
          sampler);
  ASSERT_EQ(ratio->ratio, 0.0);
}

TEST(YamlTrace, trace_id_ratio_based_sampler)
{
  std::string yaml = R"(
file_format: xx.yy
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    trace_id_ratio_based:
      ratio: 3.14
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->sampler, nullptr);
  auto *sampler = config->tracer_provider->sampler.get();
  auto *ratio =
      reinterpret_cast<opentelemetry::sdk::configuration::TraceIdRatioBasedSamplerConfiguration *>(
          sampler);
  ASSERT_EQ(ratio->ratio, 3.14);
}
