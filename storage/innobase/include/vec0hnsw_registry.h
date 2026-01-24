/**
  @file storage/innobase/include/vec0hnsw_registry.h

  HNSW Index Registry - Global singleton for managing table:column-to-index mappings.
  Supports multiple HNSW indexes per table (one per VECTOR column).
*/

#ifndef vec0hnsw_registry_h
#define vec0hnsw_registry_h

#include <mutex>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include "../vector/vec0hnsw.h"

namespace innodb_vector {

/**
  Global registry for HNSW indexes.
  Maps table:column keys to their corresponding HnswIndex instances.
  When column is empty, uses table name alone (backward compat).
  Thread-safe via internal mutex.
*/
class HnswIndexRegistry {
 public:
  static HnswIndexRegistry& instance();

  /**
    Register a new index for a table column.
    @param table_name  Table name
    @param column_name Column name (empty string for legacy single-index mode)
    @param dim         Vector dimensionality
    @param M           HNSW M parameter (connections per layer)
    @param ef_construction  HNSW ef parameter for construction
    @param metric      Distance metric (L2, COSINE, DOT_PRODUCT)
    @return true on success, false if index already exists
  */
  bool register_index(const std::string& table_name,
                      const std::string& column_name,
                      size_t dim,
                      size_t M = 16, size_t ef_construction = 200,
                      hnsw_metric_t metric = hnsw_metric_t::L2);

  /** Backward-compat overload (no column). */
  bool register_index(const std::string& table_name, size_t dim,
                      size_t M = 16, size_t ef_construction = 200,
                      hnsw_metric_t metric = hnsw_metric_t::L2) {
    return register_index(table_name, "", dim, M, ef_construction, metric);
  }

  /**
    Get an existing index for a table column.
    @param table_name  Table name
    @param column_name Column name (empty for legacy)
    @return Pointer to index, or nullptr if not found
  */
  HnswIndex* get_index(const std::string& table_name,
                        const std::string& column_name = "");

  /**
    Drop (remove) an index for a table column.
    @param table_name  Table name
    @param column_name Column name (empty for legacy)
    @return true if index was found and removed
  */
  bool drop_index(const std::string& table_name,
                  const std::string& column_name = "");

  /**
    Check if an index exists for a table column.
  */
  bool has_index(const std::string& table_name,
                 const std::string& column_name = "");

  /**
    Get list of all registered keys (table or table:column).
  */
  std::vector<std::string> list_indexes();

  /**
    Get all column names that have indexes for a given table.
    @param table_name  Table name
    @return Vector of column names (empty string entries for legacy indexes)
  */
  std::vector<std::string> get_columns_for_table(const std::string& table_name);

  /**
    Parse a metric string to enum value.
    @param metric_str  String: "l2", "cosine", "dot_product" (case-insensitive)
    @return Corresponding enum value, defaults to L2 for unknown strings
  */
  static hnsw_metric_t parse_metric(const std::string& metric_str);

  /**
    Convert metric enum to string representation.
  */
  static const char* metric_to_string(hnsw_metric_t metric);

 private:
  HnswIndexRegistry() = default;
  ~HnswIndexRegistry() = default;
  HnswIndexRegistry(const HnswIndexRegistry&) = delete;
  HnswIndexRegistry& operator=(const HnswIndexRegistry&) = delete;

  /** Build registry key from table and column names. */
  static std::string make_key(const std::string& table_name,
                              const std::string& column_name) {
    if (column_name.empty()) return table_name;
    return table_name + ":" + column_name;
  }

  std::mutex mutex_;
  std::unordered_map<std::string, std::unique_ptr<HnswIndex>> indexes_;
};

}  // namespace innodb_vector

#endif  // vec0hnsw_registry_h
