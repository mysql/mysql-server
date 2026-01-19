/**
  @file storage/innobase/vector/vec0hnsw.cc
  
  HNSW Index Implementation
  
  Implements the Hierarchical Navigable Small World algorithm for
  approximate nearest neighbor search.
*/

#include "vec0hnsw.h"
#include <algorithm>
#include <queue>
#include <cmath>
#include <limits>

namespace innodb_vector {

HnswIndex::HnswIndex(const hnsw_config_t &config)
    : config_(config),
      cur_elements_(0),
      max_level_(-1),
      entry_point_(0),
      rng_(std::random_device{}()) {
  nodes_.reserve(config_.max_elements);
}

HnswIndex::~HnswIndex() = default;

double HnswIndex::distance_l2(const std::vector<float> &a,
                               const std::vector<float> &b) {
  double sum = 0.0;
  for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
    double diff = static_cast<double>(a[i]) - static_cast<double>(b[i]);
    sum += diff * diff;
  }
  return std::sqrt(sum);
}

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
  
  // Simple selection: take closest M
  for (size_t i = 0; i < candidates.size() && result.size() < M; ++i) {
    result.push_back(candidates[i].id);
  }
  return result;
}

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
  
  std::vector<bool> visited(nodes_.size(), false);
  
  double d = distance_l2(query, nodes_[entry].vector);
  candidates.push({entry, d});
  results.push({entry, d});
  visited[entry] = true;
  
  while (!candidates.empty()) {
    hnsw_result_t current = candidates.top();
    candidates.pop();
    
    // If closest candidate is further than furthest result, stop
    if (current.distance > results.top().distance && results.size() >= ef) {
      break;
    }
    
    // Explore neighbors
    const auto &neighbors = nodes_[current.id].neighbors;
    if (level < static_cast<int32_t>(neighbors.size())) {
      for (uint64_t neighbor_id : neighbors[level]) {
        if (!visited[neighbor_id]) {
          visited[neighbor_id] = true;
          double dist = distance_l2(query, nodes_[neighbor_id].vector);
          
          if (results.size() < ef || dist < results.top().distance) {
            candidates.push({neighbor_id, dist});
            results.push({neighbor_id, dist});
            
            if (results.size() > ef) {
              results.pop();
            }
          }
        }
      }
    }
  }
  
  // Convert results to sorted vector
  std::vector<hnsw_result_t> result_vec;
  while (!results.empty()) {
    result_vec.push_back(results.top());
    results.pop();
  }
  std::sort(result_vec.begin(), result_vec.end());
  return result_vec;
}

bool HnswIndex::insert(uint64_t id, const std::vector<float> &vector) {
  std::lock_guard<std::mutex> lock(index_mutex_);
  
  if (cur_elements_ >= config_.max_elements) {
    return false;
  }
  
  if (vector.size() != config_.dimensions && config_.dimensions != 0) {
    return false;
  }
  
  // Set dimensions on first insert
  if (config_.dimensions == 0) {
    config_.dimensions = static_cast<uint32_t>(vector.size());
  }
  
  int32_t node_level = random_level();
  
  // Create new node
  hnsw_node_t new_node;
  new_node.id = id;
  new_node.vector = vector;
  new_node.max_level = node_level;
  new_node.neighbors.resize(node_level + 1);
  
  uint64_t node_idx = nodes_.size();
  nodes_.push_back(std::move(new_node));
  
  if (cur_elements_ == 0) {
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
        auto &neighbor_list = nodes_[neighbor_id].neighbors[l];
        neighbor_list.push_back(node_idx);
        
        // Prune if needed
        if (neighbor_list.size() > M_curr) {
          std::vector<hnsw_result_t> scored;
          for (uint64_t n : neighbor_list) {
            double d = distance_l2(nodes_[neighbor_id].vector, nodes_[n].vector);
            scored.push_back({n, d});
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
  
  ++cur_elements_;
  return true;
}

std::vector<hnsw_result_t> HnswIndex::search(const std::vector<float> &query,
                                              uint32_t k, uint32_t ef) {
  std::lock_guard<std::mutex> lock(index_mutex_);
  
  if (cur_elements_ == 0) {
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
  
  return results;
}

}  // namespace innodb_vector
