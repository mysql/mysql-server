// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "opentelemetry/sdk/configuration/configuration.h"
#include "opentelemetry/sdk/configuration/distribution_configuration.h"
#include "opentelemetry/sdk/configuration/distribution_entry_configuration.h"
#include "opentelemetry/sdk/configuration/document_node.h"
#include "opentelemetry/sdk/configuration/yaml_configuration_parser.h"

static std::unique_ptr<opentelemetry::sdk::configuration::Configuration> DoParse(
    const std::string &yaml)
{
  static const std::string source("test");
  return opentelemetry::sdk::configuration::YamlConfigurationParser::ParseString(source, yaml);
}

TEST(YamlTrace, empty_distribution)
{
  std::string yaml = R"(
file_format: "1.0-distribution"
distribution:
)";

  auto config = DoParse(yaml);
  ASSERT_EQ(config, nullptr);
}

TEST(YamlTrace, one_empty_distribution)
{
  std::string yaml = R"(
file_format: "1.0-distribution"
distribution:
  acme_vendor:
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  auto *distribution = config->distribution.get();
  ASSERT_NE(distribution, nullptr);
  ASSERT_EQ(distribution->entries.size(), 1);
  auto *entry = distribution->entries[0].get();
  ASSERT_NE(entry, nullptr);
  ASSERT_EQ(entry->name, "acme_vendor");
}

TEST(YamlTrace, many_distribution)
{
  std::string yaml = R"(
file_format: "1.0-distribution"
distribution:
  acme_vendor:
    a: 12
    b: 34
    c: 56
  other_vendor:
    xxx: 111
    yyy: 222
)";

  auto config = DoParse(yaml);
  ASSERT_NE(config, nullptr);
  auto *distribution = config->distribution.get();
  ASSERT_NE(distribution, nullptr);
  ASSERT_EQ(distribution->entries.size(), 2);
  opentelemetry::sdk::configuration::DocumentNode *node{nullptr};
  std::unique_ptr<opentelemetry::sdk::configuration::DocumentNode> property;
  std::string name;

  auto *entry_1 = distribution->entries[0].get();
  ASSERT_NE(entry_1, nullptr);
  ASSERT_EQ(entry_1->name, "acme_vendor");

  node = entry_1->node.get();
  ASSERT_NE(node, nullptr);
  auto it  = node->begin();
  property = (*it);
  ASSERT_NE(property, nullptr);
  name = property->Key();
  ASSERT_EQ(name, "a");
  ASSERT_EQ(property->AsInteger(), 12);
  ++it;
  property = (*it);
  ASSERT_NE(property, nullptr);
  name = property->Key();
  ASSERT_EQ(name, "b");
  ASSERT_EQ(property->AsInteger(), 34);
  ++it;
  property = (*it);
  ASSERT_NE(property, nullptr);
  name = property->Key();
  ASSERT_EQ(name, "c");
  ASSERT_EQ(property->AsInteger(), 56);
  ++it;
  ASSERT_EQ(node->end(), it);

  auto *entry_2 = distribution->entries[1].get();
  ASSERT_NE(entry_2, nullptr);
  ASSERT_EQ(entry_2->name, "other_vendor");

  node = entry_2->node.get();
  ASSERT_NE(node, nullptr);
}
