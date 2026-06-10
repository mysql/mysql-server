// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <map>
#include <string>

#include "opentelemetry/sdk/configuration/console_log_record_exporter_builder.h"
#include "opentelemetry/sdk/configuration/console_push_metric_exporter_builder.h"
#include "opentelemetry/sdk/configuration/console_span_exporter_builder.h"
#include "opentelemetry/sdk/configuration/extension_log_record_exporter_builder.h"
#include "opentelemetry/sdk/configuration/extension_log_record_processor_builder.h"
#include "opentelemetry/sdk/configuration/extension_pull_metric_exporter_builder.h"
#include "opentelemetry/sdk/configuration/extension_push_metric_exporter_builder.h"
#include "opentelemetry/sdk/configuration/extension_sampler_builder.h"
#include "opentelemetry/sdk/configuration/extension_span_exporter_builder.h"
#include "opentelemetry/sdk/configuration/extension_span_processor_builder.h"
#include "opentelemetry/sdk/configuration/otlp_file_log_record_exporter_builder.h"
#include "opentelemetry/sdk/configuration/otlp_file_push_metric_exporter_builder.h"
#include "opentelemetry/sdk/configuration/otlp_file_span_exporter_builder.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_log_record_exporter_builder.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_push_metric_exporter_builder.h"
#include "opentelemetry/sdk/configuration/otlp_grpc_span_exporter_builder.h"
#include "opentelemetry/sdk/configuration/otlp_http_log_record_exporter_builder.h"
#include "opentelemetry/sdk/configuration/otlp_http_push_metric_exporter_builder.h"
#include "opentelemetry/sdk/configuration/otlp_http_span_exporter_builder.h"
#include "opentelemetry/sdk/configuration/prometheus_pull_metric_exporter_builder.h"
#include "opentelemetry/sdk/configuration/text_map_propagator_builder.h"
#include "opentelemetry/sdk/configuration/zipkin_span_exporter_builder.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class Registry
{
public:
  Registry();
  Registry(Registry &&)                      = delete;
  Registry(const Registry &)                 = delete;
  Registry &operator=(Registry &&)           = delete;
  Registry &operator=(const Registry &other) = delete;

  ~Registry() = default;

  /* Core optional components. */

  const OtlpHttpSpanExporterBuilder *GetOtlpHttpSpanBuilder() const
  {
    return otlp_http_span_builder_.get();
  }

  void SetOtlpHttpSpanBuilder(std::unique_ptr<OtlpHttpSpanExporterBuilder> &&builder)
  {
    otlp_http_span_builder_ = std::move(builder);
  }

  const OtlpGrpcSpanExporterBuilder *GetOtlpGrpcSpanBuilder() const
  {
    return otlp_grpc_span_builder_.get();
  }

  void SetOtlpGrpcSpanBuilder(std::unique_ptr<OtlpGrpcSpanExporterBuilder> &&builder)
  {
    otlp_grpc_span_builder_ = std::move(builder);
  }

  const OtlpFileSpanExporterBuilder *GetOtlpFileSpanBuilder() const
  {
    return otlp_file_span_builder_.get();
  }

  void SetOtlpFileSpanBuilder(std::unique_ptr<OtlpFileSpanExporterBuilder> &&builder)
  {
    otlp_file_span_builder_ = std::move(builder);
  }

  const ConsoleSpanExporterBuilder *GetConsoleSpanBuilder() const
  {
    return console_span_builder_.get();
  }

  void SetConsoleSpanBuilder(std::unique_ptr<ConsoleSpanExporterBuilder> &&builder)
  {
    console_span_builder_ = std::move(builder);
  }

  const ZipkinSpanExporterBuilder *GetZipkinSpanBuilder() const
  {
    return zipkin_span_builder_.get();
  }

  void SetZipkinSpanBuilder(std::unique_ptr<ZipkinSpanExporterBuilder> &&builder)
  {
    zipkin_span_builder_ = std::move(builder);
  }

  const OtlpHttpPushMetricExporterBuilder *GetOtlpHttpPushMetricExporterBuilder() const
  {
    return otlp_http_push_metric_builder_.get();
  }

  void SetOtlpHttpPushMetricExporterBuilder(
      std::unique_ptr<OtlpHttpPushMetricExporterBuilder> &&builder)
  {
    otlp_http_push_metric_builder_ = std::move(builder);
  }

  const OtlpGrpcPushMetricExporterBuilder *GetOtlpGrpcPushMetricExporterBuilder() const
  {
    return otlp_grpc_push_metric_builder_.get();
  }

  void SetOtlpGrpcPushMetricExporterBuilder(
      std::unique_ptr<OtlpGrpcPushMetricExporterBuilder> &&builder)
  {
    otlp_grpc_push_metric_builder_ = std::move(builder);
  }

  const OtlpFilePushMetricExporterBuilder *GetOtlpFilePushMetricExporterBuilder() const
  {
    return otlp_file_push_metric_builder_.get();
  }

  void SetOtlpFilePushMetricExporterBuilder(
      std::unique_ptr<OtlpFilePushMetricExporterBuilder> &&builder)
  {
    otlp_file_push_metric_builder_ = std::move(builder);
  }

  const ConsolePushMetricExporterBuilder *GetConsolePushMetricExporterBuilder() const
  {
    return console_metric_builder_.get();
  }

  void SetConsolePushMetricExporterBuilder(
      std::unique_ptr<ConsolePushMetricExporterBuilder> &&builder)
  {
    console_metric_builder_ = std::move(builder);
  }

  const PrometheusPullMetricExporterBuilder *GetPrometheusPullMetricExporterBuilder() const
  {
    return prometheus_metric_builder_.get();
  }

  void SetPrometheusPullMetricExporterBuilder(
      std::unique_ptr<PrometheusPullMetricExporterBuilder> &&builder)
  {
    prometheus_metric_builder_ = std::move(builder);
  }

  const OtlpHttpLogRecordExporterBuilder *GetOtlpHttpLogRecordBuilder() const
  {
    return otlp_http_log_record_builder_.get();
  }

  void SetOtlpHttpLogRecordBuilder(std::unique_ptr<OtlpHttpLogRecordExporterBuilder> &&builder)
  {
    otlp_http_log_record_builder_ = std::move(builder);
  }

  const OtlpGrpcLogRecordExporterBuilder *GetOtlpGrpcLogRecordBuilder() const
  {
    return otlp_grpc_log_record_builder_.get();
  }

  void SetOtlpGrpcLogRecordBuilder(std::unique_ptr<OtlpGrpcLogRecordExporterBuilder> &&builder)
  {
    otlp_grpc_log_record_builder_ = std::move(builder);
  }

  const OtlpFileLogRecordExporterBuilder *GetOtlpFileLogRecordBuilder() const
  {
    return otlp_file_log_record_builder_.get();
  }

  void SetOtlpFileLogRecordBuilder(std::unique_ptr<OtlpFileLogRecordExporterBuilder> &&builder)
  {
    otlp_file_log_record_builder_ = std::move(builder);
  }

  const ConsoleLogRecordExporterBuilder *GetConsoleLogRecordBuilder() const
  {
    return console_log_record_builder_.get();
  }

  void SetConsoleLogRecordBuilder(std::unique_ptr<ConsoleLogRecordExporterBuilder> &&builder)
  {
    console_log_record_builder_ = std::move(builder);
  }

  /* Extension points */

  const TextMapPropagatorBuilder *GetTextMapPropagatorBuilder(const std::string &name) const;

  void SetTextMapPropagatorBuilder(const std::string &name,
                                   std::unique_ptr<TextMapPropagatorBuilder> &&builder);

  const ExtensionSamplerBuilder *GetExtensionSamplerBuilder(const std::string &name) const;

  void SetExtensionSamplerBuilder(const std::string &name,
                                  std::unique_ptr<ExtensionSamplerBuilder> &&builder);

  const ExtensionSpanExporterBuilder *GetExtensionSpanExporterBuilder(
      const std::string &name) const;

  void SetExtensionSpanExporterBuilder(const std::string &name,
                                       std::unique_ptr<ExtensionSpanExporterBuilder> &&builder);

  const ExtensionSpanProcessorBuilder *GetExtensionSpanProcessorBuilder(
      const std::string &name) const;

  void SetExtensionSpanProcessorBuilder(const std::string &name,
                                        std::unique_ptr<ExtensionSpanProcessorBuilder> &&builder);

  const ExtensionPushMetricExporterBuilder *GetExtensionPushMetricExporterBuilder(
      const std::string &name) const;

  void SetExtensionPushMetricExporterBuilder(
      const std::string &name,
      std::unique_ptr<ExtensionPushMetricExporterBuilder> &&builder);

  const ExtensionPullMetricExporterBuilder *GetExtensionPullMetricExporterBuilder(
      const std::string &name) const;

  void SetExtensionPullMetricExporterBuilder(
      const std::string &name,
      std::unique_ptr<ExtensionPullMetricExporterBuilder> &&builder);

  const ExtensionLogRecordExporterBuilder *GetExtensionLogRecordExporterBuilder(
      const std::string &name) const;

  void SetExtensionLogRecordExporterBuilder(
      const std::string &name,
      std::unique_ptr<ExtensionLogRecordExporterBuilder> &&builder);

  const ExtensionLogRecordProcessorBuilder *GetExtensionLogRecordProcessorBuilder(
      const std::string &name) const;

  void SetExtensionLogRecordProcessorBuilder(
      const std::string &name,
      std::unique_ptr<ExtensionLogRecordProcessorBuilder> &&builder);

private:
  std::unique_ptr<OtlpHttpSpanExporterBuilder> otlp_http_span_builder_;
  std::unique_ptr<OtlpGrpcSpanExporterBuilder> otlp_grpc_span_builder_;
  std::unique_ptr<OtlpFileSpanExporterBuilder> otlp_file_span_builder_;
  std::unique_ptr<ConsoleSpanExporterBuilder> console_span_builder_;
  std::unique_ptr<ZipkinSpanExporterBuilder> zipkin_span_builder_;

  std::unique_ptr<OtlpHttpPushMetricExporterBuilder> otlp_http_push_metric_builder_;
  std::unique_ptr<OtlpGrpcPushMetricExporterBuilder> otlp_grpc_push_metric_builder_;
  std::unique_ptr<OtlpFilePushMetricExporterBuilder> otlp_file_push_metric_builder_;
  std::unique_ptr<ConsolePushMetricExporterBuilder> console_metric_builder_;
  std::unique_ptr<PrometheusPullMetricExporterBuilder> prometheus_metric_builder_;

  std::unique_ptr<OtlpHttpLogRecordExporterBuilder> otlp_http_log_record_builder_;
  std::unique_ptr<OtlpGrpcLogRecordExporterBuilder> otlp_grpc_log_record_builder_;
  std::unique_ptr<OtlpFileLogRecordExporterBuilder> otlp_file_log_record_builder_;
  std::unique_ptr<ConsoleLogRecordExporterBuilder> console_log_record_builder_;

  std::map<std::string, std::unique_ptr<TextMapPropagatorBuilder>> propagator_builders_;
  std::map<std::string, std::unique_ptr<ExtensionSamplerBuilder>> sampler_builders_;
  std::map<std::string, std::unique_ptr<ExtensionSpanExporterBuilder>> span_exporter_builders_;
  std::map<std::string, std::unique_ptr<ExtensionSpanProcessorBuilder>> span_processor_builders_;
  std::map<std::string, std::unique_ptr<ExtensionPushMetricExporterBuilder>>
      push_metric_exporter_builders_;
  std::map<std::string, std::unique_ptr<ExtensionPullMetricExporterBuilder>>
      pull_metric_exporter_builders_;
  std::map<std::string, std::unique_ptr<ExtensionLogRecordExporterBuilder>>
      log_record_exporter_builders_;
  std::map<std::string, std::unique_ptr<ExtensionLogRecordProcessorBuilder>>
      log_record_processor_builders_;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
