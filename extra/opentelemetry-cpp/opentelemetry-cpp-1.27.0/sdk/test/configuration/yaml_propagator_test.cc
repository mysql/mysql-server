// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "opentelemetry/sdk/configuration/configuration.h"
#include "opentelemetry/sdk/configuration/propagator_configuration.h"
#include "opentelemetry/sdk/configuration/yaml_configuration_parser.h"

static std::unique_ptr<opentelemetry::sdk::configuration::Configuration> DoParse(
    const std::string &yaml)
{
  static const std::string source("test");
  return opentelemetry::sdk::configuration::YamlConfigurationParser::ParseString(source, yaml);
}

TEST(YamlPropagator, empty_propagator)
{
  std::string yaml = R"(
file_format: "1.0-propagator"
propagator:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->propagator, nullptr);
  ASSERT_EQ(config->propagator->composite.size(), 0);
  ASSERT_EQ(config->propagator->composite_list, "");
}

TEST(YamlPropagator, empty_composite)
{
  std::string yaml = R"(
file_format: "1.0-propagator"
propagator:
  composite:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->propagator, nullptr);
  ASSERT_EQ(config->propagator->composite.size(), 0);
  ASSERT_EQ(config->propagator->composite_list, "");
}

TEST(YamlPropagator, old_propagator_1)
{
  // This is the old format, must fail
  std::string yaml = R"(
file_format: "1.0-propagator"
propagator:
  composite:
    - foo
    - bar
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlPropagator, old_propagator_2)
{
  // This is the old format, must fail
  std::string yaml = R"(
file_format: "1.0-propagator"
propagator:
  composite: [foo, bar]
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlPropagator, propagator_array_ok)
{
  std::string yaml = R"(
file_format: "1.0-propagator"
propagator:
  composite:
    - foo:
    - bar:
    - baz:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->propagator, nullptr);
  ASSERT_EQ(config->propagator->composite.size(), 3);
  ASSERT_EQ(config->propagator->composite[0], "foo");
  ASSERT_EQ(config->propagator->composite[1], "bar");
  ASSERT_EQ(config->propagator->composite[2], "baz");
  ASSERT_EQ(config->propagator->composite_list, "");
}

TEST(YamlPropagator, propagator_array_broken)
{
  std::string yaml = R"(
file_format: "1.0-propagator"
propagator:
  composite:
    - foo:
    - bar:
      baz:
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlPropagator, propagator_composite_list)
{
  std::string yaml = R"(
file_format: "1.0-propagator"
propagator:
  composite_list: "foo,bar,baz"
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->propagator, nullptr);
  ASSERT_EQ(config->propagator->composite.size(), 0);
  ASSERT_EQ(config->propagator->composite_list, "foo,bar,baz");
}

TEST(YamlPropagator, propagator_both)
{
  std::string yaml = R"(
file_format: "1.0-propagator"
propagator:
  composite:
    - aaa:
    - bbb:
    - ccc:
  composite_list: "ddd,eee,fff"
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->propagator, nullptr);
  ASSERT_EQ(config->propagator->composite.size(), 3);
  ASSERT_EQ(config->propagator->composite[0], "aaa");
  ASSERT_EQ(config->propagator->composite[1], "bbb");
  ASSERT_EQ(config->propagator->composite[2], "ccc");
  ASSERT_EQ(config->propagator->composite_list, "ddd,eee,fff");
}

TEST(YamlPropagator, propagator_duplicates)
{
  std::string yaml = R"(
file_format: "1.0-propagator"
propagator:
  composite:
    - aaa:
    - bbb:
    - bbb:
    - ccc:
  composite_list: "aaa,eee,eee,fff,ccc"
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  ASSERT_NE(config->propagator, nullptr);
  ASSERT_EQ(config->propagator->composite.size(), 4);
  ASSERT_EQ(config->propagator->composite[0], "aaa");
  ASSERT_EQ(config->propagator->composite[1], "bbb");
  ASSERT_EQ(config->propagator->composite[2], "bbb");
  ASSERT_EQ(config->propagator->composite[3], "ccc");
  ASSERT_EQ(config->propagator->composite_list, "aaa,eee,eee,fff,ccc");
}
