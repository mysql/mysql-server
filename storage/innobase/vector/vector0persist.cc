// Copyright 2026 Google LLC

/** @file kmeans/kmeans0persist.cc
 KMeans persist interface and factory function */

#include <memory>
#include <string>
#include <utility>

#include <vector0types.h>
#include <vector0persist.h>
#include <kmeans0types.h>
#include <kmeans0index.h>
#include <kmeans0table.h>

namespace ib_vector {

/** Factory method to create a PAGED Persister object
@param[in]      type            persistence type, which decides the ipml
@param[in]      index           the index to be persisted
@param[in]      thr             query thread (Not null when doing DML)
@param[in]      info            DDL info (Not null when doing backfill)
@return A base pointer pointing to a concrete object based on type */
template <>
std::shared_ptr<VectorPersist>
VectorPersist::create<PersistType::PAGED>(
    VectorIndex* index, que_thr_t* thr, CreateVectorIndexInfo* info) {
  IndexType idx_type = index->type();
  if (idx_type == IndexType::TREE_SQ) {
    auto kmeans_idx = dynamic_cast<KMeansIndex*>(index);
    ut_a(kmeans_idx);
    return std::shared_ptr<VectorPersist>(new KMeansTable(kmeans_idx, thr, info));
  } else {
    // persisting other index types to table is not supported yet
    return nullptr;
  }
}

/** Factory method to create a PAGED Persister object
@param[in]      type            persistence type, which decides the ipml
@param[in]      index           the index to be persisted
@param[in]      base_table      base table of index
@return A base pointer pointing to a concrete object based on type */
template <>
std::shared_ptr<VectorPersist>
VectorPersist::create<PersistType::PAGED>(
    VectorIndex* index, dict_table_t* base_table) {
  IndexType idx_type = index->type();
  if (idx_type == IndexType::TREE_SQ) {
    auto kmeans_idx = dynamic_cast<KMeansIndex*>(index);
    ut_a(kmeans_idx);
    return std::shared_ptr<VectorPersist>(new KMeansTable(kmeans_idx, base_table));
  } else {
    // persisting other index types to table is not supported yet
    return nullptr;
  }
}

} /* namespace ib_vector */
