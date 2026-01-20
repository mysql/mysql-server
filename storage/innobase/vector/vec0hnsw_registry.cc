/**
  @file storage/innobase/vector/vec0hnsw_registry.cc

  HNSW Index Registry Implementation.
*/

#include "../include/vec0hnsw_registry.h"

namespace innodb_vector {

HnswIndexRegistry& HnswIndexRegistry::instance() {
  static HnswIndexRegistry registry;
  return registry;
}

bool HnswIndexRegistry::register_index(const std::string& table_name, 
                                        size_t dim, size_t M, 
                                        size_t ef_construction) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (indexes_.find(table_name) != indexes_.end()) {
    return false;  // Index already exists
  }
  
  hnsw_config_t config;
  config.dimensions = dim;
  config.M = M;
  config.ef_construction = ef_construction;
  indexes_[table_name] = std::make_unique<HnswIndex>(config);
  return true;
}

HnswIndex* HnswIndexRegistry::get_index(const std::string& table_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  auto it = indexes_.find(table_name);
  if (it == indexes_.end()) {
    return nullptr;
  }
  return it->second.get();
}

bool HnswIndexRegistry::drop_index(const std::string& table_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  return indexes_.erase(table_name) > 0;
}

bool HnswIndexRegistry::has_index(const std::string& table_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  return indexes_.find(table_name) != indexes_.end();
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

}  // namespace innodb_vector
