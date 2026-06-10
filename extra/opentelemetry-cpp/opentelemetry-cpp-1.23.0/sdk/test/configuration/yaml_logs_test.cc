// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "opentelemetry/sdk/configuration/batch_log_record_processor_configuration.h"
#include "opentelemetry/sdk/configuration/configuration.h"
#include "opentelemetry/sdk/configuration/headers_configuration.h"
#include "opentelemetry/sdk/configuration/log_record_limits_configuration.h"
#include "opentelemetry/sdk/configuration/log_record_processor_configuration.h"
#include "opentelemetry/sdk/configuration/logger_provider_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_file_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/otlp_http_encoding.h"
#include "opentelemetry/sdk/configuration/otlp_http_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/simple_log_record_processor_configuration.h"
#include "opentelemetry/sdk/configuration/yaml_configuration_parser.h"

static std::unique_ptr<opentelemetry::sdk::configuration::Configuration> DoParse(
    const std::string &yaml)
{
  static const std::string source("test");
  return opentelemetry::sdk::configuration::YamlConfigurationParser::ParseString(source, yaml);
}

TEST(YamlLogs, no_processors)
{
  std::string yaml = R"(
file_format: xx.yy
logger_provider:
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlLogs, empty_processors)
{
  std::string yaml = R"(
file_format: xx.yy
logger_provider:
  processors:
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlLogs, many_processors)
{
  std::string yaml = R"(
file_format: xx.yy
logger_provider:
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
  ASSERT_NE(config->logger_provider, nullptr);
  ASSERT_EQ(config->logger_provider->processors.size(), 2);
}

TEST(YamlLogs, simple_processor)
{
  std::string yaml = R"(
file_format: xx.yy
logger_provider:
  processors:
    - simple:
        exporter:
          console:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->logger_provider, nullptr);
  ASSERT_EQ(config->logger_provider->processors.size(), 1);
  auto *processor = config->logger_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *simple =
      reinterpret_cast<opentelemetry::sdk::configuration::SimpleLogRecordProcessorConfiguration *>(
          processor);
  ASSERT_NE(simple->exporter, nullptr);
  auto *exporter = simple->exporter.get();
  ASSERT_NE(exporter, nullptr);
}

TEST(YamlLogs, default_batch_processor)
{
  std::string yaml = R"(
file_format: xx.yy
logger_provider:
  processors:
    - batch:
        exporter:
          console:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->logger_provider, nullptr);
  ASSERT_EQ(config->logger_provider->processors.size(), 1);
  auto *processor = config->logger_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *batch =
      reinterpret_cast<opentelemetry::sdk::configuration::BatchLogRecordProcessorConfiguration *>(
          processor);
  ASSERT_EQ(batch->schedule_delay, 5000);
  ASSERT_EQ(batch->export_timeout, 30000);
  ASSERT_EQ(batch->max_queue_size, 2048);
  ASSERT_EQ(batch->max_export_batch_size, 512);
  ASSERT_NE(batch->exporter, nullptr);
  auto *exporter = batch->exporter.get();
  ASSERT_NE(exporter, nullptr);
}

TEST(YamlLogs, batch_processor)
{
  std::string yaml = R"(
file_format: xx.yy
logger_provider:
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
  ASSERT_NE(config->logger_provider, nullptr);
  ASSERT_EQ(config->logger_provider->processors.size(), 1);
  auto *processor = config->logger_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *batch =
      reinterpret_cast<opentelemetry::sdk::configuration::BatchLogRecordProcessorConfiguration *>(
          processor);
  ASSERT_EQ(batch->schedule_delay, 5555);
  ASSERT_EQ(batch->export_timeout, 33333);
  ASSERT_EQ(batch->max_queue_size, 1234);
  ASSERT_EQ(batch->max_export_batch_size, 256);
  ASSERT_NE(batch->exporter, nullptr);
  auto *exporter = batch->exporter.get();
  ASSERT_NE(exporter, nullptr);
}

TEST(YamlLogs, default_otlp_http)
{
  std::string yaml = R"(
file_format: xx.yy
logger_provider:
  processors:
    - simple:
        exporter:
          otlp_http:
            endpoint: "somewhere"
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->logger_provider, nullptr);
  ASSERT_EQ(config->logger_provider->processors.size(), 1);
  auto *processor = config->logger_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *simple =
      reinterpret_cast<opentelemetry::sdk::configuration::SimpleLogRecordProcessorConfiguration *>(
          processor);
  ASSERT_NE(simple->exporter, nullptr);
  auto *exporter = simple->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *otlp_http =
      reinterpret_cast<opentelemetry::sdk::configuration::OtlpHttpLogRecordExporterConfiguration *>(
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

TEST(YamlLogs, otlp_http)
{
  std::string yaml = R"(
file_format: xx.yy
logger_provider:
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
  ASSERT_NE(config->logger_provider, nullptr);
  ASSERT_EQ(config->logger_provider->processors.size(), 1);
  auto *processor = config->logger_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *simple =
      reinterpret_cast<opentelemetry::sdk::configuration::SimpleLogRecordProcessorConfiguration *>(
          processor);
  ASSERT_NE(simple->exporter, nullptr);
  auto *exporter = simple->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *otlp_http =
      reinterpret_cast<opentelemetry::sdk::configuration::OtlpHttpLogRecordExporterConfiguration *>(
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

TEST(YamlLogs, default_otlp_grpc)
{
  std::string yaml = R"(
file_format: xx.yy
logger_provider:
  processors:
    - simple:
        exporter:
          otlp_grpc:
            endpoint: "somewhere"
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->logger_provider, nullptr);
  ASSERT_EQ(config->logger_provider->processors.size(), 1);
  auto *processor = config->logger_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *simple =
      reinterpret_cast<opentelemetry::sdk::configuration::SimpleLogRecordProcessorConfiguration *>(
          processor);
  ASSERT_NE(simple->exporter, nullptr);
  auto *exporter = simple->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *otlp_grpc =
      reinterpret_cast<opentelemetry::sdk::configuration::OtlpGrpcLogRecordExporterConfiguration *>(
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

TEST(YamlLogs, otlp_grpc)
{
  std::string yaml = R"(
file_format: xx.yy
logger_provider:
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
  ASSERT_NE(config->logger_provider, nullptr);
  ASSERT_EQ(config->logger_provider->processors.size(), 1);
  auto *processor = config->logger_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *simple =
      reinterpret_cast<opentelemetry::sdk::configuration::SimpleLogRecordProcessorConfiguration *>(
          processor);
  ASSERT_NE(simple->exporter, nullptr);
  auto *exporter = simple->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *otlp_grpc =
      reinterpret_cast<opentelemetry::sdk::configuration::OtlpGrpcLogRecordExporterConfiguration *>(
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

TEST(YamlLogs, default_otlp_file)
{
  std::string yaml = R"(
file_format: xx.yy
logger_provider:
  processors:
    - simple:
        exporter:
          otlp_file/development:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->logger_provider, nullptr);
  ASSERT_EQ(config->logger_provider->processors.size(), 1);
  auto *processor = config->logger_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *simple =
      reinterpret_cast<opentelemetry::sdk::configuration::SimpleLogRecordProcessorConfiguration *>(
          processor);
  ASSERT_NE(simple->exporter, nullptr);
  auto *exporter = simple->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *otlp_file =
      reinterpret_cast<opentelemetry::sdk::configuration::OtlpFileLogRecordExporterConfiguration *>(
          exporter);
  ASSERT_EQ(otlp_file->output_stream, "");
}

TEST(YamlLogs, otlp_file)
{
  std::string yaml = R"(
file_format: xx.yy
logger_provider:
  processors:
    - simple:
        exporter:
          otlp_file/development:
            output_stream: "somewhere"
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->logger_provider, nullptr);
  ASSERT_EQ(config->logger_provider->processors.size(), 1);
  auto *processor = config->logger_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *simple =
      reinterpret_cast<opentelemetry::sdk::configuration::SimpleLogRecordProcessorConfiguration *>(
          processor);
  ASSERT_NE(simple->exporter, nullptr);
  auto *exporter = simple->exporter.get();
  ASSERT_NE(exporter, nullptr);
  auto *otlp_file =
      reinterpret_cast<opentelemetry::sdk::configuration::OtlpFileLogRecordExporterConfiguration *>(
          exporter);
  ASSERT_EQ(otlp_file->output_stream, "somewhere");
}

TEST(YamlLogs, otlp_console)
{
  std::string yaml = R"(
file_format: xx.yy
logger_provider:
  processors:
    - simple:
        exporter:
          console:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->logger_provider, nullptr);
  ASSERT_EQ(config->logger_provider->processors.size(), 1);
  auto *processor = config->logger_provider->processors[0].get();
  ASSERT_NE(processor, nullptr);
  auto *simple =
      reinterpret_cast<opentelemetry::sdk::configuration::SimpleLogRecordProcessorConfiguration *>(
          processor);
  ASSERT_NE(simple->exporter, nullptr);
  auto *exporter = simple->exporter.get();
  ASSERT_NE(exporter, nullptr);
}

TEST(YamlLogs, no_limits)
{
  std::string yaml = R"(
file_format: xx.yy
logger_provider:
  processors:
    - simple:
        exporter:
          console:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->logger_provider, nullptr);
  ASSERT_EQ(config->logger_provider->limits, nullptr);
}

TEST(YamlLogs, default_limits)
{
  std::string yaml = R"(
file_format: xx.yy
logger_provider:
  processors:
    - simple:
        exporter:
          console:
  limits:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->logger_provider, nullptr);
  ASSERT_NE(config->logger_provider->limits, nullptr);
  ASSERT_EQ(config->logger_provider->limits->attribute_value_length_limit, 4096);
  ASSERT_EQ(config->logger_provider->limits->attribute_count_limit, 128);
}

TEST(YamlLogs, limits)
{
  std::string yaml = R"(
file_format: xx.yy
logger_provider:
  processors:
    - simple:
        exporter:
          console:
  limits:
    attribute_value_length_limit: 1111
    attribute_count_limit: 2222
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->logger_provider, nullptr);
  ASSERT_NE(config->logger_provider->limits, nullptr);
  ASSERT_EQ(config->logger_provider->limits->attribute_value_length_limit, 1111);
  ASSERT_EQ(config->logger_provider->limits->attribute_count_limit, 2222);
}
