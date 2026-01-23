/**
  @file storage/innobase/vector/vec0hnsw_registry.cc

  HNSW Index Registry Implementation.
  Supports multiple indexes per table via table:column composite keys.
*/

#include "../include/vec0hnsw_registry.h"

namespace innodb_vector {

HnswIndexRegistry& HnswIndexRegistry::instance() {
  static HnswIndexRegistry registry;
  return registry;
}

bool HnswIndexRegistry::register_index(const std::string& table_name,
                                        const std::string& column_name,
                                        size_t dim, size_t M,
                                        size_t ef_construction) {
  std::lock_guard<std::mutex> lock(mutex_);

  std::string key = make_key(table_name, column_name);
  if (indexes_.find(key) != indexes_.end()) {
    return false;  // Index already exists
  }

  hnsw_config_t config;
  config.dimensions = dim;
  config.M = M;
  config.ef_construction = ef_construction;
  indexes_[key] = std::make_unique<HnswIndex>(config);
  return true;
}

HnswIndex* HnswIndexRegistry::get_index(const std::string& table_name,
                                          const std::string& column_name) {
  std::lock_guard<std::mutex> lock(mutex_);

  std::string key = make_key(table_name, column_name);
  auto it = indexes_.find(key);
  if (it == indexes_.end()) {
    return nullptr;
  }
  return it->second.get();
}

bool HnswIndexRegistry::drop_index(const std::string& table_name,
                                    const std::string& column_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string key = make_key(table_name, column_name);
  return indexes_.erase(key) > 0;
}

bool HnswIndexRegistry::has_index(const std::string& table_name,
                                   const std::string& column_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::string key = make_key(table_name, column_name);
  return indexes_.find(key) != indexes_.end();
}

std::vector<std::string> HnswIndexRegistry::list_indexes() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::string> result;
  result.reserve(indexes_.size());
  for (const auto& pair : indexes_) {
    result.push_back(pair.first);
  }
  return result;
}

std::vector<std::string> HnswIndexRegistry::get_columns_for_table(
    const std::string& table_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::string> result;
  std::string prefix = table_name + ":";

  for (const auto& pair : indexes_) {
    if (pair.first == table_name) {
      // Legacy entry (no column)
      result.push_back("");
    } else if (pair.first.compare(0, prefix.size(), prefix) == 0) {
      // table:column entry
      result.push_back(pair.first.substr(prefix.size()));
    }
  }
  return result;
}

}  // namespace innodb_vector
