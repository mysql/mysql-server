/**
  @file storage/innobase/vector/vec0hnsw.cc

  HNSW Index Implementation

  Implements the Hierarchical Navigable Small World algorithm for
  approximate nearest neighbor search.

  Improvements over Phase 2:
  - shared_mutex for concurrent reads
  - Multiple distance metrics (L2, Cosine, Dot Product)
  - Soft-delete with neighbor reconnection
  - External-to-internal ID mapping with free list reuse
  - update() operation (remove + re-insert)
*/

#include "vec0hnsw.h"
#include <algorithm>
#include <queue>
#include <cmath>
#include <limits>
#include <fstream>
#include <cstring>
#include <numeric>

namespace innodb_vector {

HnswIndex::HnswIndex(const hnsw_config_t &config)
    : config_(config),
      active_elements_(0),
      deleted_count_(0),
      max_level_(-1),
      entry_point_(0),
      rng_(std::random_device{}()) {
  nodes_.reserve(config_.max_elements);
}

HnswIndex::~HnswIndex() = default;

// ============================================================================
// Distance Functions
// ============================================================================

double HnswIndex::distance_l2(const std::vector<float> &a,
                               const std::vector<float> &b) {
  double sum = 0.0;
  size_t n = std::min(a.size(), b.size());
  for (size_t i = 0; i < n; ++i) {
    double diff = static_cast<double>(a[i]) - static_cast<double>(b[i]);
    sum += diff * diff;
  }
  return std::sqrt(sum);
}

double HnswIndex::distance_cosine(const std::vector<float> &a,
                                   const std::vector<float> &b) {
  double dot = 0.0, norm_a = 0.0, norm_b = 0.0;
  size_t n = std::min(a.size(), b.size());
  for (size_t i = 0; i < n; ++i) {
    double ai = static_cast<double>(a[i]);
    double bi = static_cast<double>(b[i]);
    dot += ai * bi;
    norm_a += ai * ai;
    norm_b += bi * bi;
  }
  double denom = std::sqrt(norm_a) * std::sqrt(norm_b);
  if (denom < 1e-10) return 1.0;  // Avoid division by zero
  return 1.0 - (dot / denom);
}

double HnswIndex::distance_dot_product(const std::vector<float> &a,
                                        const std::vector<float> &b) {
  double dot = 0.0;
  size_t n = std::min(a.size(), b.size());
  for (size_t i = 0; i < n; ++i) {
    dot += static_cast<double>(a[i]) * static_cast<double>(b[i]);
  }
  // Negative because HNSW minimizes distance; higher dot = more similar
  return -dot;
}

double HnswIndex::compute_distance(const std::vector<float> &a,
                                    const std::vector<float> &b) const {
  switch (config_.metric) {
    case hnsw_metric_t::COSINE:
      return distance_cosine(a, b);
    case hnsw_metric_t::DOT_PRODUCT:
      return distance_dot_product(a, b);
    case hnsw_metric_t::L2:
    default:
      return distance_l2(a, b);
  }
}

// ============================================================================
// Internal Helpers
// ============================================================================

int32_t HnswIndex::random_level() {
  std::uniform_real_distribution<double> dist(0.0, 1.0);
  double r = dist(rng_);
  double mL = 1.0 / std::log(static_cast<double>(config_.M));
  return static_cast<int32_t>(std::floor(-std::log(r) * mL));
}

std::vector<uint64_t> HnswIndex::select_neighbors(
    const std::vector<hnsw_result_t> &candidates, uint32_t M) {
  std::vector<uint64_t> result;
  result.reserve(M);

  // Simple selection: take closest M non-deleted nodes
  for (size_t i = 0; i < candidates.size() && result.size() < M; ++i) {
    uint64_t idx = candidates[i].id;
    if (idx < nodes_.size() && !nodes_[idx].deleted) {
      result.push_back(idx);
    }
  }
  return result;
}

uint64_t HnswIndex::allocate_node_slot() {
  if (!free_list_.empty()) {
    uint64_t slot = free_list_.back();
    free_list_.pop_back();
    return slot;
  }
  uint64_t slot = nodes_.size();
  nodes_.emplace_back();
  return slot;
}

bool HnswIndex::find_valid_entry_point() {
  if (active_elements_ == 0) return false;

  // If current entry point is valid, keep it
  if (entry_point_ < nodes_.size() && !nodes_[entry_point_].deleted) {
    return true;
  }

  // Search for a non-deleted node with the highest level
  int32_t best_level = -1;
  uint64_t best_idx = 0;
  for (uint64_t i = 0; i < nodes_.size(); ++i) {
    if (!nodes_[i].deleted && nodes_[i].max_level > best_level) {
      best_level = nodes_[i].max_level;
      best_idx = i;
    }
  }

  if (best_level >= 0) {
    entry_point_ = best_idx;
    max_level_ = best_level;
    return true;
  }
  return false;
}

void HnswIndex::reconnect_neighbors(uint64_t internal_idx) {
  const auto &node = nodes_[internal_idx];

  // For each level this node participates in
  for (int32_t l = 0; l <= node.max_level; ++l) {
    if (l >= static_cast<int32_t>(node.neighbors.size())) break;

    const auto &level_neighbors = node.neighbors[l];

    // Remove this node from all its neighbors' lists
    for (uint64_t neighbor_idx : level_neighbors) {
      if (neighbor_idx >= nodes_.size() || nodes_[neighbor_idx].deleted) continue;

      auto &nb_list = nodes_[neighbor_idx].neighbors[l];
      nb_list.erase(
          std::remove(nb_list.begin(), nb_list.end(), internal_idx),
          nb_list.end());

      // Try to connect this neighbor to other neighbors of the deleted node
      // to maintain graph connectivity
      uint32_t M_curr = (l == 0) ? config_.M0 : config_.M;
      if (nb_list.size() < M_curr / 2) {
        for (uint64_t other_idx : level_neighbors) {
          if (other_idx == neighbor_idx || other_idx >= nodes_.size() ||
              nodes_[other_idx].deleted) continue;
          // Check if already connected
          if (std::find(nb_list.begin(), nb_list.end(), other_idx) != nb_list.end())
            continue;
          if (nb_list.size() >= M_curr) break;
          nb_list.push_back(other_idx);
          // Add bidirectional
          auto &other_list = nodes_[other_idx].neighbors[l];
          if (other_list.size() < M_curr) {
            other_list.push_back(neighbor_idx);
          }
        }
      }
    }
  }
}

// ============================================================================
// Search Layer
// ============================================================================

std::vector<hnsw_result_t> HnswIndex::search_layer(
    const std::vector<float> &query, uint64_t entry, uint32_t ef,
    int32_t level) {

  // Min-heap for candidates (closest first)
  auto cmp_min = [](const hnsw_result_t &a, const hnsw_result_t &b) {
    return a.distance > b.distance;
  };
  std::priority_queue<hnsw_result_t, std::vector<hnsw_result_t>,
                      decltype(cmp_min)> candidates(cmp_min);

  // Max-heap for results (furthest first, to maintain top-ef)
  auto cmp_max = [](const hnsw_result_t &a, const hnsw_result_t &b) {
    return a.distance < b.distance;
  };
  std::priority_queue<hnsw_result_t, std::vector<hnsw_result_t>,
                      decltype(cmp_max)> results(cmp_max);

  std::unordered_set<uint64_t> visited;
  visited.reserve(ef * 2);

  double d = compute_distance(query, nodes_[entry].vector);
  candidates.push({entry, d});
  if (!nodes_[entry].deleted) {
    results.push({entry, d});
  }
  visited.insert(entry);

  while (!candidates.empty()) {
    hnsw_result_t current = candidates.top();
    candidates.pop();

    // If closest candidate is further than furthest result, stop
    if (!results.empty() && current.distance > results.top().distance &&
        results.size() >= ef) {
      break;
    }

    // Explore neighbors
    if (current.id < nodes_.size()) {
      const auto &neighbors = nodes_[current.id].neighbors;
      if (level < static_cast<int32_t>(neighbors.size())) {
        for (uint64_t neighbor_id : neighbors[level]) {
          if (visited.count(neighbor_id)) continue;
          visited.insert(neighbor_id);

          if (neighbor_id >= nodes_.size()) continue;

          double dist = compute_distance(query, nodes_[neighbor_id].vector);

          bool should_add = results.size() < ef ||
                            dist < results.top().distance;
          if (should_add) {
            candidates.push({neighbor_id, dist});
            // Only add non-deleted nodes to results
            if (!nodes_[neighbor_id].deleted) {
              results.push({neighbor_id, dist});
              if (results.size() > ef) {
                results.pop();
              }
            }
          }
        }
      }
    }
  }

  // Convert results to sorted vector
  std::vector<hnsw_result_t> result_vec;
  result_vec.reserve(results.size());
  while (!results.empty()) {
    result_vec.push_back(results.top());
    results.pop();
  }
  std::sort(result_vec.begin(), result_vec.end());
  return result_vec;
}

// ============================================================================
// Insert
// ============================================================================

bool HnswIndex::insert(uint64_t id, const std::vector<float> &vector) {
  std::unique_lock<std::shared_mutex> lock(index_mutex_);

  if (active_elements_ >= config_.max_elements) {
    return false;
  }

  if (vector.size() != config_.dimensions && config_.dimensions != 0) {
    return false;
  }

  // Check for duplicate external ID
  if (id_to_idx_.count(id)) {
    return false;
  }

  // Set dimensions on first insert
  if (config_.dimensions == 0) {
    config_.dimensions = static_cast<uint32_t>(vector.size());
  }

  int32_t node_level = random_level();

  // Allocate node slot (reuse from free list if available)
  uint64_t node_idx = allocate_node_slot();

  // Initialize node
  hnsw_node_t &new_node = nodes_[node_idx];
  new_node.id = id;
  new_node.vector = vector;
  new_node.max_level = node_level;
  new_node.neighbors.clear();
  new_node.neighbors.resize(node_level + 1);
  new_node.deleted = false;

  // Register in ID map
  id_to_idx_[id] = node_idx;

  if (active_elements_ == 0) {
    // First element
    entry_point_ = node_idx;
    max_level_ = node_level;
  } else {
    uint64_t curr_entry = entry_point_;

    // Traverse from top to node_level+1
    for (int32_t l = max_level_; l > node_level; --l) {
      auto results = search_layer(vector, curr_entry, 1, l);
      if (!results.empty()) {
        curr_entry = results[0].id;
      }
    }

    // Build connections at each level
    for (int32_t l = std::min(node_level, max_level_); l >= 0; --l) {
      uint32_t M_curr = (l == 0) ? config_.M0 : config_.M;
      auto candidates = search_layer(vector, curr_entry, config_.ef_construction, l);
      auto neighbors = select_neighbors(candidates, M_curr);

      nodes_[node_idx].neighbors[l] = neighbors;

      // Add bidirectional connections
      for (uint64_t neighbor_id : neighbors) {
        if (neighbor_id >= nodes_.size() || nodes_[neighbor_id].deleted) continue;
        auto &neighbor_list = nodes_[neighbor_id].neighbors[l];
        neighbor_list.push_back(node_idx);

        // Prune if needed
        if (neighbor_list.size() > M_curr) {
          std::vector<hnsw_result_t> scored;
          scored.reserve(neighbor_list.size());
          for (uint64_t n : neighbor_list) {
            if (n < nodes_.size() && !nodes_[n].deleted) {
              double d = compute_distance(nodes_[neighbor_id].vector, nodes_[n].vector);
              scored.push_back({n, d});
            }
          }
          std::sort(scored.begin(), scored.end());
          neighbor_list = select_neighbors(scored, M_curr);
        }
      }

      if (!candidates.empty()) {
        curr_entry = candidates[0].id;
      }
    }

    if (node_level > max_level_) {
      entry_point_ = node_idx;
      max_level_ = node_level;
    }
  }

  ++active_elements_;
  return true;
}

// ============================================================================
// Remove
// ============================================================================

bool HnswIndex::remove(uint64_t id) {
  std::unique_lock<std::shared_mutex> lock(index_mutex_);

  auto it = id_to_idx_.find(id);
  if (it == id_to_idx_.end()) {
    return false;
  }

  uint64_t internal_idx = it->second;
  if (internal_idx >= nodes_.size() || nodes_[internal_idx].deleted) {
    return false;
  }

  // Reconnect neighbors before marking as deleted
  reconnect_neighbors(internal_idx);

  // Mark as deleted
  nodes_[internal_idx].deleted = true;
  nodes_[internal_idx].vector.clear();
  nodes_[internal_idx].vector.shrink_to_fit();
  nodes_[internal_idx].neighbors.clear();

  // Remove from ID map and add to free list
  id_to_idx_.erase(it);
  free_list_.push_back(internal_idx);

  --active_elements_;
  ++deleted_count_;

  // Update entry point if we deleted it
  if (internal_idx == entry_point_) {
    find_valid_entry_point();
  }

  return true;
}

// ============================================================================
// Update
// ============================================================================

bool HnswIndex::update(uint64_t id, const std::vector<float> &vector) {
  // Note: We hold the write lock across both operations to ensure atomicity.
  // remove() and insert() each acquire the lock, so we do it manually here.
  std::unique_lock<std::shared_mutex> lock(index_mutex_);

  // Remove the old entry (inline without lock)
  auto it = id_to_idx_.find(id);
  if (it != id_to_idx_.end()) {
    uint64_t internal_idx = it->second;
    if (internal_idx < nodes_.size() && !nodes_[internal_idx].deleted) {
      reconnect_neighbors(internal_idx);
      nodes_[internal_idx].deleted = true;
      nodes_[internal_idx].vector.clear();
      nodes_[internal_idx].vector.shrink_to_fit();
      nodes_[internal_idx].neighbors.clear();
      id_to_idx_.erase(it);
      free_list_.push_back(internal_idx);
      --active_elements_;
      ++deleted_count_;
      if (internal_idx == entry_point_) {
        find_valid_entry_point();
      }
    }
  }

  // Re-insert with the same external ID (inline without lock)
  if (active_elements_ >= config_.max_elements) {
    return false;
  }
  if (vector.size() != config_.dimensions && config_.dimensions != 0) {
    return false;
  }

  int32_t node_level = random_level();
  uint64_t node_idx = allocate_node_slot();

  hnsw_node_t &new_node = nodes_[node_idx];
  new_node.id = id;
  new_node.vector = vector;
  new_node.max_level = node_level;
  new_node.neighbors.clear();
  new_node.neighbors.resize(node_level + 1);
  new_node.deleted = false;

  id_to_idx_[id] = node_idx;

  if (active_elements_ == 0) {
    entry_point_ = node_idx;
    max_level_ = node_level;
  } else {
    uint64_t curr_entry = entry_point_;

    for (int32_t l = max_level_; l > node_level; --l) {
      auto results = search_layer(vector, curr_entry, 1, l);
      if (!results.empty()) {
        curr_entry = results[0].id;
      }
    }

    for (int32_t l = std::min(node_level, max_level_); l >= 0; --l) {
      uint32_t M_curr = (l == 0) ? config_.M0 : config_.M;
      auto candidates = search_layer(vector, curr_entry, config_.ef_construction, l);
      auto neighbors = select_neighbors(candidates, M_curr);

      nodes_[node_idx].neighbors[l] = neighbors;

      for (uint64_t neighbor_id : neighbors) {
        if (neighbor_id >= nodes_.size() || nodes_[neighbor_id].deleted) continue;
        auto &neighbor_list = nodes_[neighbor_id].neighbors[l];
        neighbor_list.push_back(node_idx);

        if (neighbor_list.size() > M_curr) {
          std::vector<hnsw_result_t> scored;
          scored.reserve(neighbor_list.size());
          for (uint64_t n : neighbor_list) {
            if (n < nodes_.size() && !nodes_[n].deleted) {
              double d = compute_distance(nodes_[neighbor_id].vector, nodes_[n].vector);
              scored.push_back({n, d});
            }
          }
          std::sort(scored.begin(), scored.end());
          neighbor_list = select_neighbors(scored, M_curr);
        }
      }

      if (!candidates.empty()) {
        curr_entry = candidates[0].id;
      }
    }

    if (node_level > max_level_) {
      entry_point_ = node_idx;
      max_level_ = node_level;
    }
  }

  ++active_elements_;
  return true;
}

// ============================================================================
// Search
// ============================================================================

std::vector<hnsw_result_t> HnswIndex::search(const std::vector<float> &query,
                                              uint32_t k, uint32_t ef) {
  std::shared_lock<std::shared_mutex> lock(index_mutex_);

  if (active_elements_ == 0) {
    return {};
  }

  if (ef == 0) {
    ef = config_.ef_search;
  }
  ef = std::max(ef, k);

  uint64_t curr_entry = entry_point_;

  // Traverse from top to level 1
  for (int32_t l = max_level_; l > 0; --l) {
    auto results = search_layer(query, curr_entry, 1, l);
    if (!results.empty()) {
      curr_entry = results[0].id;
    }
  }

  // Search at level 0
  auto results = search_layer(query, curr_entry, ef, 0);

  // Return top-k
  if (results.size() > k) {
    results.resize(k);
  }

  // Map internal indices to external IDs
  for (auto &result : results) {
    result.id = nodes_[result.id].id;
  }

  return results;
}

// ============================================================================
// Contains
// ============================================================================

bool HnswIndex::contains(uint64_t id) const {
  std::shared_lock<std::shared_mutex> lock(index_mutex_);
  return id_to_idx_.count(id) > 0;
}

// ============================================================================
// Persistence - Save
// ============================================================================

bool HnswIndex::save_to_file(const char* path) const {
  std::shared_lock<std::shared_mutex> lock(index_mutex_);

  std::ofstream file(path, std::ios::binary);
  if (!file) return false;

  // Write header/magic + version
  const char magic[] = "HNSW";
  file.write(magic, 4);
  uint32_t version = 2;  // Version 2: includes metric and deleted support
  file.write(reinterpret_cast<const char*>(&version), sizeof(version));

  // Write config
  file.write(reinterpret_cast<const char*>(&config_), sizeof(config_));

  // Write state
  file.write(reinterpret_cast<const char*>(&active_elements_), sizeof(active_elements_));
  file.write(reinterpret_cast<const char*>(&deleted_count_), sizeof(deleted_count_));
  file.write(reinterpret_cast<const char*>(&max_level_), sizeof(max_level_));
  file.write(reinterpret_cast<const char*>(&entry_point_), sizeof(entry_point_));

  // Write nodes (only non-deleted)
  uint64_t save_count = active_elements_;
  file.write(reinterpret_cast<const char*>(&save_count), sizeof(save_count));

  for (const auto& node : nodes_) {
    if (node.deleted) continue;

    file.write(reinterpret_cast<const char*>(&node.id), sizeof(node.id));
    file.write(reinterpret_cast<const char*>(&node.max_level), sizeof(node.max_level));

    // Write vector
    uint32_t vec_size = static_cast<uint32_t>(node.vector.size());
    file.write(reinterpret_cast<const char*>(&vec_size), sizeof(vec_size));
    file.write(reinterpret_cast<const char*>(node.vector.data()), vec_size * sizeof(float));

    // Write neighbors per level (map internal indices to external IDs for portability)
    uint32_t level_count = static_cast<uint32_t>(node.neighbors.size());
    file.write(reinterpret_cast<const char*>(&level_count), sizeof(level_count));
    for (const auto& level_neighbors : node.neighbors) {
      // Filter out deleted neighbors and write external IDs
      std::vector<uint64_t> valid_neighbors;
      for (uint64_t idx : level_neighbors) {
        if (idx < nodes_.size() && !nodes_[idx].deleted) {
          valid_neighbors.push_back(nodes_[idx].id);  // Store external ID
        }
      }
      uint32_t neighbor_count = static_cast<uint32_t>(valid_neighbors.size());
      file.write(reinterpret_cast<const char*>(&neighbor_count), sizeof(neighbor_count));
      file.write(reinterpret_cast<const char*>(valid_neighbors.data()),
                 neighbor_count * sizeof(uint64_t));
    }
  }

  return file.good();
}

// ============================================================================
// Persistence - Load
// ============================================================================

bool HnswIndex::load_from_file(const char* path) {
  std::unique_lock<std::shared_mutex> lock(index_mutex_);

  std::ifstream file(path, std::ios::binary);
  if (!file) return false;

  // Check magic
  char magic[4];
  file.read(magic, 4);
  if (std::strncmp(magic, "HNSW", 4) != 0) return false;

  // Read version
  uint32_t version;
  file.read(reinterpret_cast<char*>(&version), sizeof(version));

  if (version == 2) {
    // Version 2: full format with metric support
    file.read(reinterpret_cast<char*>(&config_), sizeof(config_));
    file.read(reinterpret_cast<char*>(&active_elements_), sizeof(active_elements_));
    file.read(reinterpret_cast<char*>(&deleted_count_), sizeof(deleted_count_));
    file.read(reinterpret_cast<char*>(&max_level_), sizeof(max_level_));
    file.read(reinterpret_cast<char*>(&entry_point_), sizeof(entry_point_));
  } else {
    // Version 1 (legacy): no version field was written, rewind and read old format
    file.seekg(4);  // After magic
    file.read(reinterpret_cast<char*>(&config_), sizeof(config_));
    uint64_t cur_elements;
    file.read(reinterpret_cast<char*>(&cur_elements), sizeof(cur_elements));
    active_elements_ = cur_elements;
    deleted_count_ = 0;
    file.read(reinterpret_cast<char*>(&max_level_), sizeof(max_level_));
    file.read(reinterpret_cast<char*>(&entry_point_), sizeof(entry_point_));
  }

  // Read nodes
  uint64_t node_count;
  file.read(reinterpret_cast<char*>(&node_count), sizeof(node_count));

  nodes_.clear();
  nodes_.reserve(node_count);
  id_to_idx_.clear();
  id_to_idx_.reserve(node_count);
  free_list_.clear();
  deleted_count_ = 0;

  // First pass: read all nodes and build ID mapping
  // Neighbors stored as external IDs in v2, internal indices in v1
  struct LoadedNode {
    uint64_t id;
    int32_t max_level;
    std::vector<float> vector;
    std::vector<std::vector<uint64_t>> neighbor_ids;  // external IDs (v2) or indices (v1)
  };
  std::vector<LoadedNode> loaded;
  loaded.reserve(node_count);

  for (uint64_t i = 0; i < node_count; ++i) {
    LoadedNode ln;
    file.read(reinterpret_cast<char*>(&ln.id), sizeof(ln.id));
    file.read(reinterpret_cast<char*>(&ln.max_level), sizeof(ln.max_level));

    uint32_t vec_size;
    file.read(reinterpret_cast<char*>(&vec_size), sizeof(vec_size));
    ln.vector.resize(vec_size);
    file.read(reinterpret_cast<char*>(ln.vector.data()), vec_size * sizeof(float));

    uint32_t level_count;
    file.read(reinterpret_cast<char*>(&level_count), sizeof(level_count));
    ln.neighbor_ids.resize(level_count);
    for (uint32_t l = 0; l < level_count; ++l) {
      uint32_t neighbor_count;
      file.read(reinterpret_cast<char*>(&neighbor_count), sizeof(neighbor_count));
      ln.neighbor_ids[l].resize(neighbor_count);
      file.read(reinterpret_cast<char*>(ln.neighbor_ids[l].data()),
                neighbor_count * sizeof(uint64_t));
    }
    loaded.push_back(std::move(ln));
  }

  if (!file.good()) return false;

  // Build nodes with correct internal indices
  for (uint64_t i = 0; i < loaded.size(); ++i) {
    hnsw_node_t node;
    node.id = loaded[i].id;
    node.vector = std::move(loaded[i].vector);
    node.max_level = loaded[i].max_level;
    node.deleted = false;
    node.neighbors.resize(loaded[i].neighbor_ids.size());
    nodes_.push_back(std::move(node));
    id_to_idx_[loaded[i].id] = i;
  }

  // Resolve neighbor references
  if (version == 2) {
    // Version 2: neighbors are external IDs, resolve to internal indices
    for (uint64_t i = 0; i < loaded.size(); ++i) {
      for (uint32_t l = 0; l < loaded[i].neighbor_ids.size(); ++l) {
        auto &nb_list = nodes_[i].neighbors[l];
        for (uint64_t ext_id : loaded[i].neighbor_ids[l]) {
          auto it = id_to_idx_.find(ext_id);
          if (it != id_to_idx_.end()) {
            nb_list.push_back(it->second);
          }
        }
      }
    }
  } else {
    // Version 1: neighbors are already internal indices
    for (uint64_t i = 0; i < loaded.size(); ++i) {
      for (uint32_t l = 0; l < loaded[i].neighbor_ids.size(); ++l) {
        nodes_[i].neighbors[l] = std::move(loaded[i].neighbor_ids[l]);
      }
    }
  }

  active_elements_ = node_count;
  deleted_count_ = 0;

  // Find valid entry point
  if (!nodes_.empty()) {
    find_valid_entry_point();
  }

  return true;
}

}  // namespace innodb_vector
