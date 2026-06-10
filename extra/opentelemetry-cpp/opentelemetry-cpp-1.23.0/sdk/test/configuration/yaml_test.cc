// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stdlib.h>
#include <memory>
#include <string>

#include "opentelemetry/sdk/configuration/attribute_limits_configuration.h"
#include "opentelemetry/sdk/configuration/configuration.h"
#include "opentelemetry/sdk/configuration/trace_id_ratio_based_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/tracer_provider_configuration.h"
#include "opentelemetry/sdk/configuration/yaml_configuration_parser.h"

static std::unique_ptr<opentelemetry::sdk::configuration::Configuration> DoParse(
    const std::string &yaml)
{
  static const std::string source("test");
  return opentelemetry::sdk::configuration::YamlConfigurationParser::ParseString(source, yaml);
}

TEST(Yaml, empty)
{
  std::string yaml = "";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(Yaml, no_format)
{
  std::string yaml = R"(
file_format:
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(Yaml, just_format)
{
  std::string yaml = R"(
file_format: xx.yy
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->file_format, "xx.yy");
}

TEST(Yaml, disabled)
{
  std::string yaml = R"(
file_format: xx.yy
disabled: true
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->file_format, "xx.yy");
  ASSERT_EQ(config->disabled, true);
}

TEST(Yaml, enabled)
{
  std::string yaml = R"(
file_format: xx.yy
disabled: false
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->file_format, "xx.yy");
  ASSERT_EQ(config->disabled, false);
}

TEST(Yaml, enabled_by_default)
{
  std::string yaml = R"(
file_format: xx.yy
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->file_format, "xx.yy");
  ASSERT_EQ(config->disabled, false);
}

TEST(Yaml, no_attribute_limits)
{
  std::string yaml = R"(
file_format: xx.yy
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->attribute_limits, nullptr);
}

TEST(Yaml, empty_attribute_limits)
{
  std::string yaml = R"(
file_format: xx.yy
attribute_limits:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->file_format, "xx.yy");
  ASSERT_NE(config->attribute_limits, nullptr);
  ASSERT_EQ(config->attribute_limits->attribute_value_length_limit, 4096);
  ASSERT_EQ(config->attribute_limits->attribute_count_limit, 128);
}

TEST(Yaml, attribute_limits)
{
  std::string yaml = R"(
file_format: xx.yy
attribute_limits:
  attribute_value_length_limit: 1234
  attribute_count_limit: 5678
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->file_format, "xx.yy");
  ASSERT_NE(config->attribute_limits, nullptr);
  ASSERT_EQ(config->attribute_limits->attribute_value_length_limit, 1234);
  ASSERT_EQ(config->attribute_limits->attribute_count_limit, 5678);
}

TEST(Yaml, no_optional_boolean)
{
  std::string yaml = R"(
file_format: 0.0
disabled:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->disabled, false);
}

TEST(Yaml, illegal_boolean)
{
  std::string yaml = R"(
file_format: 0.0
disabled: illegal
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(Yaml, no_boolean_substitution)
{
  unsetenv("ENV_NAME");

  std::string yaml = R"(
file_format: 0.0
disabled: ${ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->disabled, false);
}

TEST(Yaml, no_boolean_substitution_env)
{
  unsetenv("ENV_NAME");

  std::string yaml = R"(
file_format: 0.0
disabled: ${env:ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->disabled, false);
}

TEST(Yaml, empty_boolean_substitution)
{
  setenv("ENV_NAME", "", 1);

  std::string yaml = R"(
file_format: 0.0
disabled: ${ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->disabled, false);
}

TEST(Yaml, empty_boolean_substitution_env)
{
  setenv("ENV_NAME", "", 1);

  std::string yaml = R"(
file_format: 0.0
disabled: ${env:ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->disabled, false);
}

TEST(Yaml, true_boolean_substitution)
{
  setenv("ENV_NAME", "true", 1);

  std::string yaml = R"(
file_format: 0.0
disabled: ${ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->disabled, true);
}

TEST(Yaml, false_boolean_substitution)
{
  setenv("ENV_NAME", "false", 1);

  std::string yaml = R"(
file_format: 0.0
disabled: ${ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->disabled, false);
}

TEST(Yaml, illegal_boolean_substitution)
{
  setenv("ENV_NAME", "illegal", 1);

  std::string yaml = R"(
file_format: 0.0
disabled: ${ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(Yaml, empty_boolean_substitution_fallback)
{
  unsetenv("ENV_NAME");

  std::string yaml = R"(
file_format: 0.0
disabled: ${ENV_NAME:-}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->disabled, false);
}

TEST(Yaml, true_boolean_substitution_fallback)
{
  unsetenv("ENV_NAME");

  std::string yaml = R"(
file_format: 0.0
disabled: ${ENV_NAME:-true}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->disabled, true);
}

TEST(Yaml, false_boolean_substitution_fallback)
{
  unsetenv("ENV_NAME");

  std::string yaml = R"(
file_format: 0.0
disabled: ${ENV_NAME:-false}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->disabled, false);
}

TEST(Yaml, illegal_boolean_substitution_fallback)
{
  unsetenv("ENV_NAME");

  std::string yaml = R"(
file_format: 0.0
disabled: ${ENV_NAME:-illegal}
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(Yaml, torture_boolean_substitution_fallback)
{
  setenv("env", "true", 1);

  std::string yaml = R"(
file_format: 0.0
disabled: ${env:-false}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->disabled, true);
}

TEST(Yaml, no_required_string)
{
  std::string yaml = R"(
file_format:
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(Yaml, no_string_substitution)
{
  unsetenv("ENV_NAME");

  std::string yaml = R"(
file_format: ${ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(Yaml, no_string_substitution_env)
{
  unsetenv("ENV_NAME");

  std::string yaml = R"(
file_format: ${env:ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(Yaml, empty_string_substitution)
{
  setenv("ENV_NAME", "", 1);

  std::string yaml = R"(
file_format: ${ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(Yaml, empty_string_substitution_env)
{
  setenv("ENV_NAME", "", 1);

  std::string yaml = R"(
file_format: ${env:ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(Yaml, with_string_substitution)
{
  setenv("ENV_NAME", "foo.bar", 1);

  std::string yaml = R"(
file_format: ${ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->file_format, "foo.bar");
}

TEST(Yaml, with_string_substitution_env)
{
  setenv("ENV_NAME", "foo.bar", 1);

  std::string yaml = R"(
file_format: ${env:ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->file_format, "foo.bar");
}

TEST(Yaml, with_string_substitution_fallback)
{
  unsetenv("ENV_NAME");

  std::string yaml = R"(
file_format: ${env:ENV_NAME:-foo.bar}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->file_format, "foo.bar");
}

TEST(Yaml, multiple_string_substitution)
{
  setenv("PREFIX", "foo", 1);
  unsetenv("DOT");
  setenv("SUFFIX", "bar", 1);

  std::string yaml = R"(
file_format: ${env:PREFIX:-failed}${DOT:-.}${SUFFIX:-failed}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_EQ(config->file_format, "foo.bar");
}

TEST(Yaml, no_optional_integer)
{
  std::string yaml = R"(
file_format: 0.0
attribute_limits:
  attribute_count_limit:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->attribute_limits, nullptr);
  ASSERT_EQ(config->attribute_limits->attribute_count_limit, 128);
}

TEST(Yaml, illegal_integer)
{
  std::string yaml = R"(
file_format: 0.0
attribute_limits:
  attribute_count_limit: "just enough"
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(Yaml, no_integer_substitution)
{
  unsetenv("ENV_NAME");

  std::string yaml = R"(
file_format: 0.0
attribute_limits:
  attribute_count_limit: ${ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->attribute_limits, nullptr);
  ASSERT_EQ(config->attribute_limits->attribute_count_limit, 128);
}

TEST(Yaml, empty_integer_substitution)
{
  setenv("ENV_NAME", "", 1);

  std::string yaml = R"(
file_format: 0.0
attribute_limits:
  attribute_count_limit: ${ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->attribute_limits, nullptr);
  ASSERT_EQ(config->attribute_limits->attribute_count_limit, 128);
}

TEST(Yaml, with_integer_substitution)
{
  setenv("ENV_NAME", "7777", 1);

  std::string yaml = R"(
file_format: 0.0
attribute_limits:
  attribute_count_limit: ${ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->attribute_limits, nullptr);
  ASSERT_EQ(config->attribute_limits->attribute_count_limit, 7777);
}

TEST(Yaml, with_illegal_integer_substitution)
{
  setenv("ENV_NAME", "still not enough", 1);

  std::string yaml = R"(
file_format: 0.0
attribute_limits:
  attribute_count_limit: ${ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(Yaml, no_optional_double)
{
  std::string yaml = R"(
file_format: 0.0
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    trace_id_ratio_based:
      ratio:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->sampler, nullptr);
  auto sampler = config->tracer_provider->sampler.get();
  auto ratio_sampler =
      static_cast<opentelemetry::sdk::configuration::TraceIdRatioBasedSamplerConfiguration *>(
          sampler);
  ASSERT_EQ(ratio_sampler->ratio, 0.0);
}

TEST(Yaml, illegal_double)
{
  std::string yaml = R"(
file_format: 0.0
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    trace_id_ratio_based:
      ratio: something
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(Yaml, no_double_substitution)
{
  unsetenv("ENV_NAME");

  std::string yaml = R"(
file_format: 0.0
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    trace_id_ratio_based:
      ratio: ${ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->sampler, nullptr);
  auto sampler = config->tracer_provider->sampler.get();
  auto ratio_sampler =
      static_cast<opentelemetry::sdk::configuration::TraceIdRatioBasedSamplerConfiguration *>(
          sampler);
  ASSERT_EQ(ratio_sampler->ratio, 0.0);
}

TEST(Yaml, empty_double_substitution)
{
  setenv("ENV_NAME", "", 1);

  std::string yaml = R"(
file_format: 0.0
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    trace_id_ratio_based:
      ratio: ${ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->sampler, nullptr);
  auto sampler = config->tracer_provider->sampler.get();
  auto ratio_sampler =
      static_cast<opentelemetry::sdk::configuration::TraceIdRatioBasedSamplerConfiguration *>(
          sampler);
  ASSERT_EQ(ratio_sampler->ratio, 0.0);
}

TEST(Yaml, with_double_substitution)
{
  setenv("ENV_NAME", "3.14", 1);

  std::string yaml = R"(
file_format: 0.0
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    trace_id_ratio_based:
      ratio: ${ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->tracer_provider, nullptr);
  ASSERT_NE(config->tracer_provider->sampler, nullptr);
  auto sampler = config->tracer_provider->sampler.get();
  auto ratio_sampler =
      static_cast<opentelemetry::sdk::configuration::TraceIdRatioBasedSamplerConfiguration *>(
          sampler);
  ASSERT_EQ(ratio_sampler->ratio, 3.14);
}

TEST(Yaml, with_illegal_double_substitution)
{
  setenv("ENV_NAME", "something else", 1);

  std::string yaml = R"(
file_format: 0.0
tracer_provider:
  processors:
    - simple:
        exporter:
          console:
  sampler:
    trace_id_ratio_based:
      ratio: ${ENV_NAME}
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}
