// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>

#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/sdk/common/custom_hash_equality.h"
#include "opentelemetry/sdk/metrics/view/attributes_processor.h"

using namespace opentelemetry::sdk::metrics;
using namespace opentelemetry::common;
using namespace opentelemetry::sdk::common;

TEST(AttributesProcessor, FilteringAttributesProcessor)
{
  const int kNumFilterAttributes                         = 3;
  opentelemetry::sdk::metrics::FilterAttributeMap filter = {
      {"attr2", true}, {"attr4", true}, {"attr6", true}};
  const int kNumAttributes              = 6;
  std::string keys[kNumAttributes]      = {"attr1", "attr2", "attr3", "attr4", "attr5", "attr6"};
  int values[kNumAttributes]            = {10, 20, 30, 40, 50, 60};
  std::map<std::string, int> attributes = {{keys[0], values[0]}, {keys[1], values[1]},
                                           {keys[2], values[2]}, {keys[3], values[3]},
                                           {keys[4], values[4]}, {keys[5], values[5]}};
  FilteringAttributesProcessor attributes_processor(filter);
  opentelemetry::common::KeyValueIterableView<std::map<std::string, int>> iterable(attributes);
  auto filtered_attributes = attributes_processor.process(iterable);
  for (auto &e : filtered_attributes)
  {
    EXPECT_FALSE(filter.find(e.first) == filter.end());
  }
  EXPECT_EQ(filter.size(), kNumFilterAttributes);
}

TEST(AttributesProcessor, FilteringAllAttributesProcessor)
{
  const int kNumFilterAttributes                         = 0;
  opentelemetry::sdk::metrics::FilterAttributeMap filter = {};
  const int kNumAttributes                               = 6;
  std::string keys[kNumAttributes]      = {"attr1", "attr2", "attr3", "attr4", "attr5", "attr6"};
  int values[kNumAttributes]            = {10, 20, 30, 40, 50, 60};
  std::map<std::string, int> attributes = {{keys[0], values[0]}, {keys[1], values[1]},
                                           {keys[2], values[2]}, {keys[3], values[3]},
                                           {keys[4], values[4]}, {keys[5], values[5]}};
  FilteringAttributesProcessor attributes_processor(filter);
  opentelemetry::common::KeyValueIterableView<std::map<std::string, int>> iterable(attributes);
  auto filtered_attributes = attributes_processor.process(iterable);
  EXPECT_EQ(filter.size(), kNumFilterAttributes);
}

TEST(AttributesProcessor, FilteringExcludeAttributesProcessor)
{
  opentelemetry::sdk::metrics::FilterAttributeMap filter = {
      {"attr2", true}, {"attr4", true}, {"attr6", true}};
  const int kNumAttributes              = 7;
  std::string keys[kNumAttributes]      = {"attr1", "attr2", "attr3", "attr4",
                                           "attr5", "attr6", "attr7"};
  int values[kNumAttributes]            = {10, 20, 30, 40, 50, 60, 70};
  std::map<std::string, int> attributes = {
      {keys[0], values[0]}, {keys[1], values[1]}, {keys[2], values[2]}, {keys[3], values[3]},
      {keys[4], values[4]}, {keys[5], values[5]}, {keys[6], values[6]}};
  FilteringExcludeAttributesProcessor attributes_processor(filter);
  opentelemetry::common::KeyValueIterableView<std::map<std::string, int>> iterable(attributes);
  auto filtered_attributes = attributes_processor.process(iterable);
  for (auto &e : filtered_attributes)
  {
    EXPECT_TRUE(filter.find(e.first) == filter.end());
  }
  int expected_filtered_attributes_size = 4;
  EXPECT_EQ(filtered_attributes.size(), expected_filtered_attributes_size);
}

TEST(AttributesProcessor, FilteringExcludeAllAttributesProcessor)
{
  opentelemetry::sdk::metrics::FilterAttributeMap filter = {};
  const int kNumAttributes                               = 6;
  std::string keys[kNumAttributes]      = {"attr1", "attr2", "attr3", "attr4", "attr5", "attr6"};
  int values[kNumAttributes]            = {10, 20, 30, 40, 50, 60};
  std::map<std::string, int> attributes = {{keys[0], values[0]}, {keys[1], values[1]},
                                           {keys[2], values[2]}, {keys[3], values[3]},
                                           {keys[4], values[4]}, {keys[5], values[5]}};
  FilteringExcludeAttributesProcessor attributes_processor(filter);
  opentelemetry::common::KeyValueIterableView<std::map<std::string, int>> iterable(attributes);
  auto filtered_attributes = attributes_processor.process(iterable);
  EXPECT_EQ(filtered_attributes.size(), kNumAttributes);
}
