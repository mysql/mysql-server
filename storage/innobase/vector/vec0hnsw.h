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
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <functional>

namespace innodb_vector {

/** Distance metric types */
enum class hnsw_metric_t : uint8_t {
  L2 = 0,           /**< Euclidean (L2) distance */
  COSINE = 1,       /**< Cosine distance (1 - cosine_similarity) */
  DOT_PRODUCT = 2   /**< Negative dot product (for max inner product search) */
};

/** HNSW index configuration parameters */
struct hnsw_config_t {
  uint32_t M;                 /**< Max connections per node per layer */
  uint32_t M0;                /**< Max connections at layer 0 (usually 2*M) */
  uint32_t ef_construction;   /**< Size of dynamic candidate list for construction */
  uint32_t ef_search;         /**< Size of dynamic candidate list for search */
  uint32_t max_elements;      /**< Maximum number of elements in index */
  uint32_t dimensions;        /**< Vector dimensionality */
  hnsw_metric_t metric;       /**< Distance metric to use */

  hnsw_config_t()
      : M(16), M0(32), ef_construction(200), ef_search(50),
        max_elements(1000000), dimensions(0), metric(hnsw_metric_t::L2) {}
};

/** Single node in the HNSW graph */
struct hnsw_node_t {
  uint64_t id;                           /**< Unique node identifier (row_id) */
  std::vector<float> vector;             /**< The vector data */
  std::vector<std::vector<uint64_t>> neighbors;  /**< Neighbors at each level (internal indices) */
  int32_t max_level;                     /**< Maximum level this node appears in */
  bool deleted;                          /**< Soft-delete flag */

  hnsw_node_t() : id(0), max_level(-1), deleted(false) {}
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
    Remove a vector from the index by external ID.
    Performs soft-delete and reconnects neighbors.
    @param id       External identifier to remove
    @return true if found and removed, false if not found
  */
  bool remove(uint64_t id);

  /**
    Update a vector in the index (remove + re-insert).
    @param id       External identifier to update
    @param vector   New vector data
    @return true on success, false on error
  */
  bool update(uint64_t id, const std::vector<float> &vector);

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
    Get current number of active (non-deleted) elements in the index.
  */
  uint64_t size() const { return active_elements_; }

  /**
    Get total allocated nodes (including deleted).
  */
  uint64_t total_nodes() const { return nodes_.size(); }

  /**
    Get number of deleted (soft-deleted) nodes.
  */
  uint64_t deleted_count() const { return deleted_count_; }

  /**
    Get configuration.
  */
  const hnsw_config_t &config() const { return config_; }

  /**
    Save the index to a binary file.
    @param path   File path to save to
    @return true on success, false on error
  */
  bool save_to_file(const char* path) const;

  /**
    Load the index from a binary file.
    @param path   File path to load from
    @return true on success, false on error
  */
  bool load_from_file(const char* path);

  /**
    Check if an external ID exists in the index.
  */
  bool contains(uint64_t id) const;

 private:
  hnsw_config_t config_;
  std::vector<hnsw_node_t> nodes_;
  uint64_t active_elements_;
  uint64_t deleted_count_;
  int32_t max_level_;
  uint64_t entry_point_;

  /** Mapping from external ID to internal node index */
  std::unordered_map<uint64_t, uint64_t> id_to_idx_;

  /** Free list of deleted node slots for reuse */
  std::vector<uint64_t> free_list_;

  std::mt19937 rng_;
  mutable std::shared_mutex index_mutex_;

  /** Compute distance between two vectors using configured metric */
  double compute_distance(const std::vector<float> &a,
                          const std::vector<float> &b) const;

  /** L2 (Euclidean) distance */
  static double distance_l2(const std::vector<float> &a,
                            const std::vector<float> &b);

  /** Cosine distance (1 - cosine_similarity) */
  static double distance_cosine(const std::vector<float> &a,
                                const std::vector<float> &b);

  /** Negative dot product distance (for MIPS) */
  static double distance_dot_product(const std::vector<float> &a,
                                     const std::vector<float> &b);

  /** Generate random level for new node */
  int32_t random_level();

  /** Search layer for closest neighbors (skips deleted nodes in results) */
  std::vector<hnsw_result_t> search_layer(const std::vector<float> &query,
                                           uint64_t entry, uint32_t ef,
                                           int32_t level);

  /** Select neighbors using simple heuristic */
  std::vector<uint64_t> select_neighbors(const std::vector<hnsw_result_t> &candidates,
                                          uint32_t M);

  /** Reconnect neighbors of a node being removed */
  void reconnect_neighbors(uint64_t internal_idx);

  /** Find a valid (non-deleted) entry point */
  bool find_valid_entry_point();

  /** Allocate a node slot (reuses free list or appends) */
  uint64_t allocate_node_slot();
};

}  // namespace innodb_vector

#endif  // vec0hnsw_h
