// Copyright 2026 Google LLC

/** @file kmeans/vector0cfg.cc
 Wrapper class to package vector related configurations/flags */

#include <pthread.h>
#include <vector0cfg.h>
#include <vector0types.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "my_rapidjson_size_t.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace ib_vector {

std::string VectorCfg::OptionLedger::to_verbose_string() const {
  ut_a(!m_edits.empty());
  std::string output = m_edits.rbegin()->second->name() + ": ";
  for (auto itr = m_edits.begin(); itr != m_edits.end(); itr++) {
    output += itr->second->value() +
              "(" + m_priority_table[itr->first].str + ")";
    if (std::next(itr) != m_edits.end()) {
      output += " -> ";
    }
  }
  return output;
}

void VectorCfg::OptionLedger::to_json_object(rapidjson::Document& doc) const {
  ut_a(!m_edits.empty());

  rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

  // create the object first
  rapidjson::Value node(rapidjson::kObjectType);
  for (auto itr = m_edits.begin(); itr != m_edits.end(); itr++) {
    // add nested values
    rapidjson::Value key(m_priority_table[itr->first].str, allocator);
    rapidjson::Value val(itr->second->value().c_str(), allocator);
    node.AddMember(key, val, allocator);
  }

  // insert the object to the json doc
  rapidjson::Value opt(m_edits.begin()->second->name().c_str(), allocator);
  doc.AddMember(opt, node, allocator);
}

VectorCfg::VectorCfg() {
  // Factory defaults are only applicable for build-time options.
  set(Options::INDEX_TYPE, IndexType::TREE_SQ, DEFAULT_SETTING);
  set(Options::NUM_NEIGHBORS, 10, DEFAULT_SETTING);
  set(Options::TREE_LEVELS, 1, DEFAULT_SETTING);
  set(Options::CLUSTERS_PER_BLOCK, 16, DEFAULT_SETTING);
  set(Options::MIN_CLUSTER_SIZE, (float)10.0, DEFAULT_SETTING);
  set(Options::MAX_CLUSTERING_ITERATION, 10, DEFAULT_SETTING);
  set(Options::VECTOR_DIMENSION, 768, DEFAULT_SETTING);
  set(Options::DIST_MEASURE, DistMeasure::L2_SQUARED, DEFAULT_SETTING);
}

VectorCfg::VectorCfg(const char* str) {
  rapidjson::Document doc;
  ut_a(!doc.Parse(str).HasParseError());

  for (auto itr = doc.MemberBegin(); itr != doc.MemberEnd(); itr++) {
    ut_a(itr->value.IsObject());
    auto option_str = itr->name.GetString();
    for (auto inner_itr = itr->value.MemberBegin();
         inner_itr != itr->value.MemberEnd(); inner_itr++) {
      auto priority_str = inner_itr->name.GetString();
      auto value_str = inner_itr->value.GetString();
      ut_a(!inner_itr->value.IsObject());

      VectorCfgPriority priority = OptionLedger::str_to_priority(priority_str);
      auto option_item = create_option(option_str, value_str);
      auto option = option_item->option();

      if (m_options.find(option) == m_options.end()) {
        m_options[option] = OptionLedger(option);
      }
      m_options[option].set(option_item, priority);
    }
  }
}

VectorCfg& VectorCfg::set(Options option,
                          std::any value,
                          VectorCfgPriority priority)
{
  auto item = create_option(option, value);
  if (m_options.find(option) == m_options.end()) {
    m_options[option] = OptionLedger(option);
  }
  m_options[option].set(item, priority);
  return *this;
}

std::string VectorCfg::to_string() const {
  std::string output;
  for (auto itr = m_options.begin(); itr != m_options.end(); itr++) {
    output += "{" + itr->second.to_verbose_string() + "}";
    if (std::next(itr) != m_options.end()) {
      output += ", ";
    }
  }
  return output;
}

std::string VectorCfg::to_json_string() const {
  rapidjson::Document doc;
  doc.SetObject();

  for (auto itr = m_options.begin(); itr != m_options.end(); itr++) {
    itr->second.to_json_object(doc);
  }

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

  doc.Accept(writer);

  return buffer.GetString();
}

std::shared_ptr<OptionItem> VectorCfg::create_option(
    Options opt, std::any val) {
  /* shared option pointer, what a name */
  using SOP = std::shared_ptr<OptionItem>;
  switch (opt) {
    case Options::INDEX_TYPE:
      return SOP(new Option<Options::INDEX_TYPE>(val));
    case Options::DIST_MEASURE:
      return SOP(new Option<Options::DIST_MEASURE>(val));
    case Options::TREE_LEVELS:
      return SOP(new Option<Options::TREE_LEVELS>(val));
    case Options::NUM_PARTITIONS:
      return SOP(new Option<Options::NUM_PARTITIONS>(val));
    case Options::NUM_NEIGHBORS:
      return SOP(new Option<Options::NUM_NEIGHBORS>(val));
    case Options::SAMPLE_SIZE:
      return SOP(new Option<Options::SAMPLE_SIZE>(val));
    case Options::CLUSTERS_PER_BLOCK:
      return SOP(new Option<Options::CLUSTERS_PER_BLOCK>(val));
    case Options::MIN_CLUSTER_SIZE:
      return SOP(new Option<Options::MIN_CLUSTER_SIZE>(val));
    case Options::MAX_CLUSTERING_ITERATION:
      return SOP(new Option<Options::MAX_CLUSTERING_ITERATION>(val));
    case Options::VECTOR_DIMENSION:
      return SOP(new Option<Options::VECTOR_DIMENSION>(val));
    case Options::NUM_DIMENSIONS_PER_BLOCK:
      return SOP(new Option<Options::NUM_DIMENSIONS_PER_BLOCK>(val));
    case Options::NUM_BLOCKS:
      return SOP(new Option<Options::NUM_BLOCKS>(val));
    case Options::NUM_QUERY_PARTITIONS:
      return SOP(new Option<Options::NUM_QUERY_PARTITIONS>(val));
    case Options::NUM_TLP:
      return SOP(new Option<Options::NUM_TLP>(val));
    case Options::NUM_SEARCH_TLP:
      return SOP(new Option<Options::NUM_SEARCH_TLP>(val));
    default:
      return nullptr;
  }
}

std::shared_ptr<OptionItem> VectorCfg::create_option(
    const std::string& opt_str, const std::string& val_str) {
  auto opt = OptionItem::str_to_option(opt_str);

  using SOP = std::shared_ptr<OptionItem>;
  switch (opt) {
    case Options::INDEX_TYPE:
      return SOP(new Option<Options::INDEX_TYPE>(val_str));
    case Options::DIST_MEASURE:
      return SOP(new Option<Options::DIST_MEASURE>(val_str));
    case Options::TREE_LEVELS:
      return SOP(new Option<Options::TREE_LEVELS>(val_str));
    case Options::NUM_PARTITIONS:
      return SOP(new Option<Options::NUM_PARTITIONS>(val_str));
    case Options::NUM_NEIGHBORS:
      return SOP(new Option<Options::NUM_NEIGHBORS>(val_str));
    case Options::SAMPLE_SIZE:
      return SOP(new Option<Options::SAMPLE_SIZE>(val_str));
    case Options::CLUSTERS_PER_BLOCK:
      return SOP(new Option<Options::CLUSTERS_PER_BLOCK>(val_str));
    case Options::MIN_CLUSTER_SIZE:
      return SOP(new Option<Options::MIN_CLUSTER_SIZE>(val_str));
    case Options::MAX_CLUSTERING_ITERATION:
      return SOP(new Option<Options::MAX_CLUSTERING_ITERATION>(val_str));
    case Options::VECTOR_DIMENSION:
      return SOP(new Option<Options::VECTOR_DIMENSION>(val_str));
    case Options::NUM_DIMENSIONS_PER_BLOCK:
      return SOP(new Option<Options::NUM_DIMENSIONS_PER_BLOCK>(val_str));
    case Options::NUM_BLOCKS:
      return SOP(new Option<Options::NUM_BLOCKS>(val_str));
    case Options::NUM_QUERY_PARTITIONS:
      return SOP(new Option<Options::NUM_QUERY_PARTITIONS>(val_str));
    case Options::NUM_TLP:
      return SOP(new Option<Options::NUM_TLP>(val_str));
    case Options::NUM_SEARCH_TLP:
      return SOP(new Option<Options::NUM_SEARCH_TLP>(val_str));
    default:
      return nullptr;
  }
}

} /* namespace ib_vector */

