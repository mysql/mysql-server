// Copyright 2026 Google LLC

/** @file vector/kmeans0partitioner.cc
 KMeans partitioner wrapper */

#include <vector>
#include <utility>

#include "vector0types.h"
#include "kmeans0types.h"
#include "kmeans0partitioner.h"

#include "include/mysqld_error.h"
#include "ut0dbg.h"
#include "ut0log.h"


namespace ib_vector {


index_err_t KMeansPartitioner::tokenize(const VectorDatapointPtr& dp,
                                      Partitions* result,
                                      uint32_t num_results) {
  ut_a(result != nullptr);

  // call the override API if the number of results, regardless of whether
  // it is through the num_results parameter.
  if (num_results && num_results != m_num_results) {
    auto* wrapper = m_partitioner->get_impl();
    auto* kmeans_partitioner =
        dynamic_cast<kmeans_wrapper::KMeansTreeLikePartitionerWrapper*>(wrapper);
    ut_a(kmeans_partitioner);

    std::vector<std::pair<uint32_t, float>> centroids;
    kmeans_partitioner->TokensForDatapointWithSpilling(
        dp, num_results, &centroids);

    result->clear();
    result->reserve(centroids.size());
    for (auto& c : centroids) {
      result->push_back((int32_t) c.first);
    }
  } else {
    m_partitioner->get_impl()->tokenize(dp, result);
  }

  return SUCCESS;
}

void KMeansPartitioner::set(std::shared_ptr<Partitioner> part) {
  ut_a(part);

  auto* wrapper = part->get_impl();
  auto* kmeans_partitioner =
      dynamic_cast<kmeans_wrapper::KMeansTreeLikePartitionerWrapper*>(wrapper);
  ut_a(kmeans_partitioner);

  m_partitioner = part;
  m_num_results = kmeans_partitioner->query_spilling_max_centers();
}

} /* namespace ib_vector */

