// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "opentelemetry/sdk/configuration/attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/attributes_configuration.h"
#include "opentelemetry/sdk/configuration/configuration.h"
#include "opentelemetry/sdk/configuration/include_exclude_configuration.h"
#include "opentelemetry/sdk/configuration/resource_configuration.h"
#include "opentelemetry/sdk/configuration/string_array_configuration.h"
#include "opentelemetry/sdk/configuration/string_attribute_value_configuration.h"
#include "opentelemetry/sdk/configuration/yaml_configuration_parser.h"

static std::unique_ptr<opentelemetry::sdk::configuration::Configuration> DoParse(
    const std::string &yaml)
{
  static const std::string source("test");
  return opentelemetry::sdk::configuration::YamlConfigurationParser::ParseString(source, yaml);
}

TEST(YamlResource, empty_resource)
{
  std::string yaml = R"(
file_format: "1.0-resource"
resource:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->resource, nullptr);
  ASSERT_EQ(config->resource->attributes, nullptr);
  ASSERT_EQ(config->resource->attributes_list, "");
  ASSERT_EQ(config->resource->detectors, nullptr);
}

TEST(YamlResource, empty_attributes)
{
  std::string yaml = R"(
file_format: "1.0-resource"
resource:
  attributes:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->resource, nullptr);
  ASSERT_NE(config->resource->attributes, nullptr);
  ASSERT_EQ(config->resource->attributes->kv_map.size(), 0);
}

TEST(YamlResource, some_attributes_0_10)
{
  // This is the old 0.10 format, must fail
  std::string yaml = R"(
file_format: "1.0-resource"
resource:
  attributes:
    foo: "1234"
    bar: "5678"
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlResource, some_attributes_0_30)
{
  // This is the new 0.30 format, must pass
  std::string yaml = R"(
file_format: "1.0-resource"
resource:
  attributes:
   - name: foo
     value: "1234"
   - name: bar
     value: "5678"
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->resource, nullptr);
  ASSERT_NE(config->resource->attributes, nullptr);
  ASSERT_EQ(config->resource->attributes->kv_map.size(), 2);

  {
    const auto &value = config->resource->attributes->kv_map["foo"];
    ASSERT_NE(value, nullptr);
    auto *value_ptr = value.get();
    auto *string_value =
        reinterpret_cast<opentelemetry::sdk::configuration::StringAttributeValueConfiguration *>(
            value_ptr);
    ASSERT_EQ(string_value->value, "1234");
  }

  {
    const auto &value = config->resource->attributes->kv_map["bar"];
    ASSERT_NE(value, nullptr);
    auto *value_ptr = value.get();
    auto *string_value =
        reinterpret_cast<opentelemetry::sdk::configuration::StringAttributeValueConfiguration *>(
            value_ptr);
    ASSERT_EQ(string_value->value, "5678");
  }
}

TEST(YamlResource, empty_attributes_list)
{
  std::string yaml = R"(
file_format: "1.0-resource"
resource:
  attributes_list:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->resource, nullptr);
  ASSERT_EQ(config->resource->attributes_list, "");
}

TEST(YamlResource, some_attributes_list)
{
  std::string yaml = R"(
file_format: "1.0-resource"
resource:
  attributes_list: "foo=1234,bar=5678"
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->resource, nullptr);
  ASSERT_EQ(config->resource->attributes_list, "foo=1234,bar=5678");
}

TEST(YamlResource, both_0_10)
{
  // This is the old 0.10 format, must fail
  std::string yaml = R"(
file_format: "1.0-resource"
resource:
  attributes:
    foo: "1234"
    bar: "5678"
  attributes_list: "foo=aaaa,bar=bbbb"
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlResource, both_0_30)
{
  // This is the new 0.30 format, must pass
  std::string yaml = R"(
file_format: "1.0-resource"
resource:
  attributes:
   - name: foo
     value: "1234"
   - name: bar
     value: "5678"
  attributes_list: "foo=aaaa,bar=bbbb"
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->resource, nullptr);
  ASSERT_NE(config->resource->attributes, nullptr);
  ASSERT_EQ(config->resource->attributes->kv_map.size(), 2);

  {
    const auto &value = config->resource->attributes->kv_map["foo"];
    ASSERT_NE(value, nullptr);
    auto *value_ptr = value.get();
    auto *string_value =
        reinterpret_cast<opentelemetry::sdk::configuration::StringAttributeValueConfiguration *>(
            value_ptr);
    ASSERT_EQ(string_value->value, "1234");
  }

  {
    const auto &value = config->resource->attributes->kv_map["bar"];
    ASSERT_NE(value, nullptr);
    auto *value_ptr = value.get();
    auto *string_value =
        reinterpret_cast<opentelemetry::sdk::configuration::StringAttributeValueConfiguration *>(
            value_ptr);
    ASSERT_EQ(string_value->value, "5678");
  }

  ASSERT_EQ(config->resource->attributes_list, "foo=aaaa,bar=bbbb");
}

TEST(YamlResource, empty_detectors)
{
  std::string yaml = R"(
file_format: "1.0-resource"
resource:
  detectors:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->resource, nullptr);
  ASSERT_NE(config->resource->detectors, nullptr);
  ASSERT_EQ(config->resource->detectors->included, nullptr);
  ASSERT_EQ(config->resource->detectors->excluded, nullptr);
}

TEST(YamlResource, empty_included_detectors)
{
  std::string yaml = R"(
file_format: "1.0-resource"
resource:
  detectors:
    included:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->resource, nullptr);
  ASSERT_NE(config->resource->detectors, nullptr);
  ASSERT_NE(config->resource->detectors->included, nullptr);
  ASSERT_EQ(config->resource->detectors->included->string_array.size(), 0);
  ASSERT_EQ(config->resource->detectors->excluded, nullptr);
}

TEST(YamlResource, some_included_detectors)
{
  std::string yaml = R"(
file_format: "1.0-resource"
resource:
  detectors:
    included:
      - foo
      - bar
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->resource, nullptr);
  ASSERT_NE(config->resource->detectors, nullptr);
  ASSERT_NE(config->resource->detectors->included, nullptr);
  ASSERT_EQ(config->resource->detectors->included->string_array.size(), 2);
  ASSERT_EQ(config->resource->detectors->included->string_array[0], "foo");
  ASSERT_EQ(config->resource->detectors->included->string_array[1], "bar");
  ASSERT_EQ(config->resource->detectors->excluded, nullptr);
}

TEST(YamlResource, some_excluded_detectors)
{
  std::string yaml = R"(
file_format: "1.0-resource"
resource:
  detectors:
    excluded:
      - foo
      - bar
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->resource, nullptr);
  ASSERT_NE(config->resource->detectors, nullptr);
  ASSERT_EQ(config->resource->detectors->included, nullptr);
  ASSERT_NE(config->resource->detectors->excluded, nullptr);
  ASSERT_EQ(config->resource->detectors->excluded->string_array.size(), 2);
  ASSERT_EQ(config->resource->detectors->excluded->string_array[0], "foo");
  ASSERT_EQ(config->resource->detectors->excluded->string_array[1], "bar");
}

TEST(YamlResource, some_detectors)
{
  std::string yaml = R"(
file_format: "1.0-resource"
resource:
  detectors:
    included:
      - foo.in
      - bar.in
    excluded:
      - foo.ex
      - bar.ex
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->resource, nullptr);
  ASSERT_NE(config->resource->detectors, nullptr);
  ASSERT_NE(config->resource->detectors->included, nullptr);
  ASSERT_EQ(config->resource->detectors->included->string_array.size(), 2);
  ASSERT_EQ(config->resource->detectors->included->string_array[0], "foo.in");
  ASSERT_EQ(config->resource->detectors->included->string_array[1], "bar.in");
  ASSERT_NE(config->resource->detectors->excluded, nullptr);
  ASSERT_EQ(config->resource->detectors->excluded->string_array.size(), 2);
  ASSERT_EQ(config->resource->detectors->excluded->string_array[0], "foo.ex");
  ASSERT_EQ(config->resource->detectors->excluded->string_array[1], "bar.ex");
}
