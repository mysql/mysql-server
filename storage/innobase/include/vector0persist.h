// Copyright 2026 Google LLC

/** @file include/kmeans0persist.h
 Interface definition for KMeans index persistence */

#ifndef _INCLUDE_VECTOR0PERSIST_H_
#define _INCLUDE_VECTOR0PERSIST_H_

#include <memory>

#include "vector0types.h"

#include <db0err.h>
#include <que0que.h>

namespace ib_vector {

class VectorIndex;

/** Type of persistence */
enum class PersistType { SIMPLE, PAGED, };

/** Interface for KMeans index persistence.
This class defines the interface for persist related APIs.
They are separated out from KMeansIndex class so that different implementations
can be easily incorporated for different persistence scheme.

At this point, we will have:
1. File-based persistence, for the coming (2024.02) MVP. The content of the
   index will be persisted onto a disk file.
2. Table-based persistence, as a long-term prod goal, to persist the index in
   database table(s), presumably optimized for paging and caching. */
class VectorPersist {
 public:
  VectorPersist() = default;
  virtual ~VectorPersist() = default;

  // Disable copy/move since the persist configurations can be quite dynamic,
  // therefore only meaningful during the exact moment when persitence happens.
  // Moving & copying expose risks of stale data/configurations.
  VectorPersist(const VectorPersist&&) = delete;
  VectorPersist(const VectorPersist&) = delete;
  VectorPersist& operator = (VectorPersist&) = delete;
  VectorPersist& operator = (VectorPersist&&) = delete;

  /** Persist the index
  @return DB_SUCCESS or error code */
  virtual dberr_t persist() = 0;

  /** Load persisted KMeans index data and create a new index object
  @return a native KMeans index, or nullptr if error occurs and db error code.
  @remarks The caller has to assume the ownership of the returned object.
           Setting the return type to (void*) is intentional, based on the
           choice between 2 possible syntatic designs:
               unique_ptr<KMeansIndex> index = VectorPersist::load(...);
                 or
               unique_ptr<KMeansIndex> index(...);
               index->load(...)
           Between those 2, the 2nd option is preferred for it provides an
           easier way to query the index internal state especially when some
           of the creation steps can take quite amount of time (such as
           training, building, reloading, etc).

           With option 2), the return object is KMeans native. Making it strongly
           typed hinders us in
               a. keeping track of different KMeans indexes (vary based on cfg)
               b. introducing name conflicts b/w some google3 libs and mysql
           Therefore, returning (void*) is picked as an easy alternative. */
  virtual std::pair<dberr_t, void*> load() = 0;

  /** Sync up with the mutation on vector data (of base table)
  @param[in]    thr             query thread that mutates the base table
  @return DB_SUCCESS or error code */
  virtual dberr_t sync_mutation(que_thr_t* thr) = 0;

  /** Factory method to create a Persister object
  @param[in]    type            persistence type, which decides the ipml
  @param[in]    index           the index to be persisted
  @param[in]    args            implementation specific arguments
  @return A base pointer pointing to a concrete object based on type */
  template<PersistType T, typename... Args>
  static std::shared_ptr<VectorPersist>
  create(VectorIndex* index, Args... args);

  /** Check if persistence is avaiable for a certain index type
  @param[in]    type            type of the index
  @return true if it can be persisted */
  static bool persistable(IndexType type) {
    bool persistable = false;
    switch (type) {
      case IndexType::TREE_SQ :
        persistable = true;
        break;
      default:
        break;
    }
    return persistable;
  }
};

} /* namespace ib_vector */

#endif /* _INCLUDE_VECTOR0PERSIST_H_ */
