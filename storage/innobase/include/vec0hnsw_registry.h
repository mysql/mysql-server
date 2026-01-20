/**
  @file storage/innobase/include/vec0hnsw_registry.h

  HNSW Index Registry - Global singleton for managing table-to-index mappings.
*/

#ifndef vec0hnsw_registry_h
#define vec0hnsw_registry_h

#include <mutex>
#include <string>
#include <unordered_map>
#include <memory>
#include "../vector/vec0hnsw.h"

namespace innodb_vector {

/**
  Global registry for HNSW indexes.
  Maps table names to their corresponding HnswIndex instances.
  Thread-safe via internal mutex.
*/
class HnswIndexRegistry {
 public:
  static HnswIndexRegistry& instance();

  /**
    Register a new index for a table.
    @param table_name  Fully qualified table name (db.table)
    @param dim         Vector dimensionality
    @param M           HNSW M parameter (connections per layer)
    @param ef_construction  HNSW ef parameter for construction
    @return true on success, false if index already exists
  */
  bool register_index(const std::string& table_name, size_t dim, 
                      size_t M = 16, size_t ef_construction = 200);

  /**
    Get an existing index for a table.
    @param table_name  Fully qualified table name
    @return Pointer to index, or nullptr if not found
  */
  HnswIndex* get_index(const std::string& table_name);

  /**
    Drop (remove) an index for a table.
    @param table_name  Fully qualified table name
    @return true if index was found and removed
  */
  bool drop_index(const std::string& table_name);

  /**
    Check if an index exists for a table.
  */
  bool has_index(const std::string& table_name);

  /**
    Get list of all registered table names.
  */
  std::vector<std::string> list_indexes();

 private:
  HnswIndexRegistry() = default;
  ~HnswIndexRegistry() = default;
  HnswIndexRegistry(const HnswIndexRegistry&) = delete;
  HnswIndexRegistry& operator=(const HnswIndexRegistry&) = delete;

  std::mutex mutex_;
  std::unordered_map<std::string, std::unique_ptr<HnswIndex>> indexes_;
};

}  // namespace innodb_vector

#endif  // vec0hnsw_registry_h
