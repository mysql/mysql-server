// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "opentelemetry/sdk/configuration/batch_span_processor_configuration.h"
#include "opentelemetry/sdk/configuration/composable_always_off_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/composable_always_on_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/composable_parent_threshold_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/composable_probability_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/composable_rule_based_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/composable_rule_based_sampler_rule_attribute_patterns_configuration.h"
#include "opentelemetry/sdk/configuration/composable_rule_based_sampler_rule_attribute_values_configuration.h"
#include "opentelemetry/sdk/configuration/composable_rule_based_sampler_rule_configuration.h"
#include "opentelemetry/sdk/configuration/configuration.h"
#include "opentelemetry/sdk/configuration/grpc_tls_configuration.h"
#include "opentelemetry/sdk/configuration/headers_configuration.h"
#include "opentelemetry/sdk/configuration/http_tls_configuration.h"
#include "opentelemetry/sdk/configuration/jaeger_remote_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_file_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_http_encoding.h"
#include "opentelemetry/sdk/configuration/otlp_http_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/parent_based_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/sampler_configuration.h"
#include "opentelemetry/sdk/configuration/sampler_configuration_visitor.h"
#include "opentelemetry/sdk/configuration/simple_span_processor_configuration.h"
#include "opentelemetry/sdk/configuration/span_limits_configuration.h"
#include "opentelemetry/sdk/configuration/span_processor_configuration.h"
#include "opentelemetry/sdk/configuration/trace_id_ratio_based_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/tracer_config_configuration.h"
#include "opentelemetry/sdk/configuration/tracer_configurator_configuration.h"
#include "opentelemetry/sdk/configuration/tracer_matcher_and_config_configuration.h"
#include "opentelemetry/sdk/configuration/tracer_provider_configuration.h"
#include "opentelemetry/sdk/configuration/yaml_configuration_parser.h"

static std::unique_ptr<opentelemetry::sdk::configuration::Configuration> DoParse(
    const std::string &yaml)
{
  static const std::string source("test");
  return opentelemetry::sdk::configuration::YamlConfigurationParser::ParseString(source, yaml);
}

TEST(YamlTrace, no_processors)
{
  std::string yaml = R"(
file_format: "1.0-trace"
tracer_provider:
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlTrace, empty_processors)
{
  std::string yaml = R"(
file_format: "1.0-trace"
tracer_provider:
  processors:
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlTrace, many_processors)
{
  std::string yaml = R"(
file_format: "1.0-trace"
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
file_format: "1.0-trace"
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
file_format: "1.0-trace"
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
file_format: "1.0-trace"
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
file_format: "1.0-trace"
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
  ASSERT_EQ(otlp_http->tls, nullptr);
  ASSERT_EQ(otlp_http->headers, nullptr);
  ASSERT_EQ(otlp_http->headers_list, "");
  ASSERT_EQ(otlp_http->compression, "");
  ASSERT_EQ(otlp_http->timeout, 10000);
  ASSERT_EQ(otlp_http->encoding, opentelemetry::sdk::configuration::OtlpHttpEncoding::protobuf);
}

TEST(YamlTrace, otlp_http)
{
  std::string yaml = R"(
file_format: "1.0-trace"
tracer_provider:
  processors:
    - simple:
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
  ASSERT_EQ(otlp_http->encoding, opentelemetry::sdk::configuration::OtlpHttpEncoding::json);
}

TEST(YamlTrace, default_otlp_grpc)
{
  std::string yaml = R"(
file_format: "1.0-trace"
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
  ASSERT_EQ(otlp_grpc->tls, nullptr);
  ASSERT_EQ(otlp_grpc->headers, nullptr);
  ASSERT_EQ(otlp_grpc->headers_list, "");
  ASSERT_EQ(otlp_grpc->compression, "");
  ASSERT_EQ(otlp_grpc->timeout, 10000);
}

TEST(YamlTrace, otlp_grpc)
{
  std::string yaml = R"(
file_format: "1.0-trace"
tracer_provider:
  processors:
    - simple:
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
}

TEST(YamlTrace, default_otlp_file)
{
  std::string yaml = R"(
file_format: "1.0-trace"
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
file_format: "1.0-trace"
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
file_format: "1.0-trace"
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

TEST(YamlTrace, no_limits)
{
  std::string yaml = R"(
file_format: "1.0-trace"
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
file_format: "1.0-trace"
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
file_format: "1.0-trace"
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
file_format: "1.0-trace"
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
file_format: "1.0-trace"
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
file_format: "1.0-trace"
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
file_format: "1.0-trace"
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
file_format: "1.0-trace"
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
file_format: "1.0-trace"
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
file_format: "1.0-trace"
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
file_format: "1.0-trace"
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
file_format: "1.0-trace"
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
file_format: "1.0-trace"
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

TEST(YamlTrace, no_tracer_configurator)
{
  std::string yaml = R"(
file_format: "1.0-trace"
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_EQ(config->tracer_provider->tracer_configurator, nullptr);
}

TEST(YamlTrace, tracer_configurator_default_only)
{
  std::string yaml = R"(
file_format: "1.0-trace"
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  tracer_configurator/development:
    default_config:
      enabled: false
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->tracer_configurator, nullptr);
  ASSERT_EQ(config->tracer_provider->tracer_configurator->default_config.enabled, false);
  ASSERT_EQ(config->tracer_provider->tracer_configurator->tracers.size(), 0);
}

TEST(YamlTrace, tracer_configurator_with_tracers)
{
  std::string yaml = R"(
file_format: "1.0-trace"
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  tracer_configurator/development:
    default_config:
      enabled: false
    tracers:
      - name: io.opentelemetry.contrib.*
        config:
          enabled: true
      - name: my.exact.tracer
        config:
          enabled: false
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->tracer_configurator, nullptr);

  auto &configurator = config->tracer_provider->tracer_configurator;
  ASSERT_EQ(configurator->default_config.enabled, false);
  ASSERT_EQ(configurator->tracers.size(), 2);

  ASSERT_EQ(configurator->tracers[0].name, "io.opentelemetry.contrib.*");
  ASSERT_EQ(configurator->tracers[0].config.enabled, true);

  ASSERT_EQ(configurator->tracers[1].name, "my.exact.tracer");
  ASSERT_EQ(configurator->tracers[1].config.enabled, false);
}

TEST(YamlTrace, tracer_configurator_default_enabled)
{
  std::string yaml = R"(
file_format: "1.0-trace"
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  tracer_configurator/development:
    default_config:
      enabled: true
    tracers:
      - name: noisy.library
        config:
          enabled: false
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->tracer_configurator, nullptr);

  auto &configurator = config->tracer_provider->tracer_configurator;
  ASSERT_EQ(configurator->default_config.enabled, true);
  ASSERT_EQ(configurator->tracers.size(), 1);
  ASSERT_EQ(configurator->tracers[0].name, "noisy.library");
  ASSERT_EQ(configurator->tracers[0].config.enabled, false);
}

namespace
{
enum class SamplerType : std::uint8_t
{
  kUnmatched                 = 0,
  kAlwaysOn                  = 1,
  kAlwaysOff                 = 2,
  kTraceIdRatioBased         = 3,
  kParentBased               = 4,
  kJaegerRemote              = 5,
  kComposableAlwaysOn        = 6,
  kComposableProbability     = 7,
  kComposableRuleBased       = 8,
  kComposableAlwaysOff       = 9,
  kComposableParentThreshold = 10
};

class TestSamplerVisitor : public opentelemetry::sdk::configuration::SamplerConfigurationVisitor
{
public:
  SamplerType type_matched = SamplerType::kUnmatched;
  double ratio             = -1.0;

  void VisitAlwaysOff(
      const opentelemetry::sdk::configuration::AlwaysOffSamplerConfiguration *) override
  {
    type_matched = SamplerType::kAlwaysOff;
  }
  void VisitAlwaysOn(
      const opentelemetry::sdk::configuration::AlwaysOnSamplerConfiguration *) override
  {
    type_matched = SamplerType::kAlwaysOn;
  }
  void VisitJaegerRemote(
      const opentelemetry::sdk::configuration::JaegerRemoteSamplerConfiguration *) override
  {
    type_matched = SamplerType::kJaegerRemote;
  }
  void VisitParentBased(
      const opentelemetry::sdk::configuration::ParentBasedSamplerConfiguration *) override
  {
    type_matched = SamplerType::kParentBased;
  }
  void VisitTraceIdRatioBased(
      const opentelemetry::sdk::configuration::TraceIdRatioBasedSamplerConfiguration *model)
      override
  {
    type_matched = SamplerType::kTraceIdRatioBased;
    ratio        = model->ratio;
  }
  void VisitExtension(
      const opentelemetry::sdk::configuration::ExtensionSamplerConfiguration *) override
  {}

  void VisitComposableAlwaysOff(
      const opentelemetry::sdk::configuration::ComposableAlwaysOffSamplerConfiguration *) override
  {
    type_matched = SamplerType::kComposableAlwaysOff;
  }
  void VisitComposableAlwaysOn(
      const opentelemetry::sdk::configuration::ComposableAlwaysOnSamplerConfiguration *) override
  {
    type_matched = SamplerType::kComposableAlwaysOn;
  }
  void VisitComposableProbability(
      const opentelemetry::sdk::configuration::ComposableProbabilitySamplerConfiguration *model)
      override
  {
    type_matched = SamplerType::kComposableProbability;
    ratio        = model->ratio;
  }
  void VisitComposableParentThreshold(
      const opentelemetry::sdk::configuration::ComposableParentThresholdSamplerConfiguration *)
      override
  {
    type_matched = SamplerType::kComposableParentThreshold;
  }
  void VisitComposableRuleBased(
      const opentelemetry::sdk::configuration::ComposableRuleBasedSamplerConfiguration *) override
  {
    type_matched = SamplerType::kComposableRuleBased;
  }
};
}  // namespace

TEST(YamlTrace, composable_always_on_sampler)
{
  std::string yaml = R"(
file_format: "1.0-trace"
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    composite/development:
      always_on:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->sampler, nullptr);

  TestSamplerVisitor visitor;
  config->tracer_provider->sampler->Accept(&visitor);
  EXPECT_EQ(visitor.type_matched, SamplerType::kComposableAlwaysOn);
}

TEST(YamlTrace, composable_probability_sampler)
{
  std::string yaml = R"(
file_format: "1.0-trace"
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    composite/development:
      probability:
        ratio: 0.25
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->sampler, nullptr);

  TestSamplerVisitor visitor;
  config->tracer_provider->sampler->Accept(&visitor);

  EXPECT_EQ(visitor.type_matched, SamplerType::kComposableProbability);
  EXPECT_DOUBLE_EQ(visitor.ratio, 0.25);
}

TEST(YamlTrace, composable_rule_based_sampler)
{
  std::string yaml = R"(
file_format: "1.0-trace"
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    composite/development:
      rule_based:
        rules:
          - sampler:
              always_on:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->sampler, nullptr);

  TestSamplerVisitor visitor;
  config->tracer_provider->sampler->Accept(&visitor);
  EXPECT_EQ(visitor.type_matched, SamplerType::kComposableRuleBased);
}

TEST(YamlTrace, composable_always_off_sampler)
{
  std::string yaml = R"(
file_format: "1.0-trace"
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    composite/development:
      always_off:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->sampler, nullptr);

  TestSamplerVisitor visitor;
  config->tracer_provider->sampler->Accept(&visitor);
  EXPECT_EQ(visitor.type_matched, SamplerType::kComposableAlwaysOff);
}

TEST(YamlTrace, composable_parent_threshold_sampler)
{
  std::string yaml = R"(
file_format: "1.0-trace"
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    composite/development:
      parent_threshold:
        root:
          always_on:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->sampler, nullptr);

  TestSamplerVisitor visitor;
  config->tracer_provider->sampler->Accept(&visitor);
  EXPECT_EQ(visitor.type_matched, SamplerType::kComposableParentThreshold);
}

TEST(YamlTrace, composable_rule_based_sampler_comprehensive)
{
  std::string yaml = R"(
file_format: "1.0-trace"
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    composite/development:
      rule_based:
        rules:
          - attribute_values:
              key: "http.method"
              values:
                - "GET"
                - "POST"
            attribute_patterns:
              key: "http.url"
              included:
                - ".*/api/.*"
              excluded:
                - ".*/healthz"
            parent:
              - remote
              - local
            span_kinds:
              - server
              - client
            sampler:
              always_on:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->sampler, nullptr);

  TestSamplerVisitor visitor;
  config->tracer_provider->sampler->Accept(&visitor);
  ASSERT_EQ(visitor.type_matched, SamplerType::kComposableRuleBased);

  auto *rule_based_sampler =
      static_cast<opentelemetry::sdk::configuration::ComposableRuleBasedSamplerConfiguration *>(
          config->tracer_provider->sampler.get());
  ASSERT_NE(rule_based_sampler, nullptr);
  ASSERT_EQ(rule_based_sampler->rules.size(), 1);

  const auto &rule = rule_based_sampler->rules[0];

  // 1. Check attribute_values
  ASSERT_NE(rule->attribute_values, nullptr);
  EXPECT_EQ(rule->attribute_values->key, "http.method");
  ASSERT_EQ(rule->attribute_values->values.size(), 2);
  EXPECT_EQ(rule->attribute_values->values[0], "GET");

  // 2. Check attribute_patterns
  ASSERT_NE(rule->attribute_patterns, nullptr);
  EXPECT_EQ(rule->attribute_patterns->key, "http.url");
  ASSERT_EQ(rule->attribute_patterns->included.size(), 1);
  EXPECT_EQ(rule->attribute_patterns->included[0], ".*/api/.*");
  ASSERT_EQ(rule->attribute_patterns->excluded.size(), 1);
  EXPECT_EQ(rule->attribute_patterns->excluded[0], ".*/healthz");

  // 3. Check parent flags
  EXPECT_TRUE(rule->match_parent_remote);
  EXPECT_TRUE(rule->match_parent_local);
  EXPECT_FALSE(rule->match_parent_none);

  // 4. Check span kinds
  EXPECT_TRUE(rule->match_span_kind_server);
  EXPECT_TRUE(rule->match_span_kind_client);
  EXPECT_FALSE(rule->match_span_kind_internal);

  // 5. Check inner sampler
  ASSERT_NE(rule->sampler, nullptr);
}

TEST(YamlTrace, composable_rule_based_sampler_illegal_parent)
{
  std::string yaml = R"(
file_format: "1.0-trace"
tracer_provider:
  sampler:
    composite/development:
      rule_based:
        rules:
          - parent:
              - illegal_parent_value
            sampler:
              always_on:
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlTrace, composable_rule_based_sampler_illegal_spankind)
{
  std::string yaml = R"(
file_format: "1.0-trace"
tracer_provider:
  sampler:
    composite/development:
      rule_based:
        rules:
          - span_kinds:
              - illegal_span_kind
            sampler:
              always_on:
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}
