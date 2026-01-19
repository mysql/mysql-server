/**
  @file storage/innobase/vector/vec0hnsw.h
  
  HNSW (Hierarchical Navigable Small World) Index Implementation
  
  This module provides vector similarity search capabilities using the HNSW
  algorithm for approximate nearest neighbor (ANN) queries.
  
  Reference: https://arxiv.org/abs/1603.09320
  
  Created for MySQL Vector Extension - Phase 2
*/

#ifndef vec0hnsw_h
#define vec0hnsw_h

#include <cstdint>
#include <vector>
#include <random>
#include <mutex>

namespace innodb_vector {

/** HNSW index configuration parameters */
struct hnsw_config_t {
  uint32_t M;                 /**< Max connections per node per layer */
  uint32_t M0;                /**< Max connections at layer 0 (usually 2*M) */
  uint32_t ef_construction;   /**< Size of dynamic candidate list for construction */
  uint32_t ef_search;         /**< Size of dynamic candidate list for search */
  uint32_t max_elements;      /**< Maximum number of elements in index */
  uint32_t dimensions;        /**< Vector dimensionality */
  
  hnsw_config_t()
      : M(16), M0(32), ef_construction(200), ef_search(50),
        max_elements(1000000), dimensions(0) {}
};

/** Single node in the HNSW graph */
struct hnsw_node_t {
  uint64_t id;                           /**< Unique node identifier (row_id) */
  std::vector<float> vector;             /**< The vector data */
  std::vector<std::vector<uint64_t>> neighbors;  /**< Neighbors at each level */
  int32_t max_level;                     /**< Maximum level this node appears in */
};

/** Distance result for search operations */
struct hnsw_result_t {
  uint64_t id;      /**< Node identifier */
  double distance;  /**< Distance from query vector */
  
  bool operator<(const hnsw_result_t &other) const {
    return distance < other.distance;
  }
  bool operator>(const hnsw_result_t &other) const {
    return distance > other.distance;
  }
};

/** HNSW Index main class */
class HnswIndex {
 public:
  explicit HnswIndex(const hnsw_config_t &config);
  ~HnswIndex();
  
  /**
    Insert a vector into the index.
    @param id       Unique identifier for this vector
    @param vector   The vector data (must match configured dimensions)
    @return true on success, false on error
  */
  bool insert(uint64_t id, const std::vector<float> &vector);
  
  /**
    Search for k nearest neighbors.
    @param query    Query vector
    @param k        Number of neighbors to return
    @param ef       Search expansion factor (0 = use default)
    @return Vector of results sorted by distance (ascending)
  */
  std::vector<hnsw_result_t> search(const std::vector<float> &query,
                                     uint32_t k, uint32_t ef = 0);
  
  /**
    Get current number of elements in the index.
  */
  uint64_t size() const { return cur_elements_; }
  
  /**
    Get configuration.
  */
  const hnsw_config_t &config() const { return config_; }

 private:
  hnsw_config_t config_;
  std::vector<hnsw_node_t> nodes_;
  uint64_t cur_elements_;
  int32_t max_level_;
  uint64_t entry_point_;
  
  std::mt19937 rng_;
  mutable std::mutex index_mutex_;
  
  /** Calculate L2 distance between two vectors */
  double distance_l2(const std::vector<float> &a, const std::vector<float> &b);
  
  /** Generate random level for new node */
  int32_t random_level();
  
  /** Search layer for closest neighbors */
  std::vector<hnsw_result_t> search_layer(const std::vector<float> &query,
                                           uint64_t entry, uint32_t ef,
                                           int32_t level);
  
  /** Select neighbors using simple heuristic */
  std::vector<uint64_t> select_neighbors(const std::vector<hnsw_result_t> &candidates,
                                          uint32_t M);
};

}  // namespace innodb_vector

#endif  // vec0hnsw_h
