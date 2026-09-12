// Copyright 2026 Google LLC

/** @file include/kmeans0partitioner.h
 KMeans partitioner wrapper */

#ifndef _INCLUDE_KMEANS0PARTITIONER_H_
#define _INCLUDE_KMEANS0PARTITIONER_H_

#include <memory>
#include <string>

#include "vector0types.h"
#include "kmeans0types.h"

namespace ib_vector {

/** KMeans Partitioner
In KMeans's terminology, a partitioner maps a given datapoint, i.e., vector, to
partition(s). It is often used interchangeably with 'tokenizer'. It is used in
2 scenarios:
  1. Query time: It maps a query datapoint to more than one partitions. The
     partitioners selected are those "close enough" to the query datapoint.
     Vectors in those partitions are going to be further exam'ed to find the
     nearest neighbors.
  2. Mutate time: It is also called "database spilling" in KMeans, which maps
     a datapoint to partition(s) it belongs to, based on the trained
     centroids. This happens when the vector index is constructed, or mutated.
This class packages KMeans's partitioner implementation. */
class KMeansPartitioner {
 public:
  // constructors

  /** Default constructor
  Used to create an empty object since the owner class (KMeansIndex) won't
  have partitioner data until the training is done. */
  KMeansPartitioner() : m_partitioner(nullptr), m_num_results(0) {}

  /**Construct a partitioner from a native KMeans partitioner
  @param[in]    part               the native KMeans partitioner */
  KMeansPartitioner(std::shared_ptr<Partitioner> part) { set(part); }

  // operators

  /** Assign a KMeans native partitioner to this wrapper class
  @param[in]    part              the KMeans native partitioner
  @return this reference for syntax only */
  KMeansPartitioner& operator = (std::shared_ptr<Partitioner> part) {
    set(part);
    return *this;
  }

  std::shared_ptr<Partitioner> operator -> () const { return m_partitioner; }

  /** Cast this object to a (void) pointer type
  @return the address of the underlying KMeans partitioner object.
  @remarks This is designed for sanity check, e.g.
           KMeansPartitioner p1(native_kmeans_partitioner), p2;
           ut_ad(p1 && !p2); */
  operator const void* () const {
    return reinterpret_cast<const void*>(m_partitioner.get()); }

  // api

  /** Map a given datapoint to KMeans partition(s)
  @param[in]    dp                the datapoint to be mapped (tokenized)
  @param[out]   result            id(s) of the partition(s) dp is mapped to
  @param[in]    num_results       override of the number of results, or 0
                                  if no overriding */
  index_err_t tokenize(const VectorDatapointPtr& dp, Partitions* result,
                       uint32_t num_results = 0);

  // modidifers

  /** Get the inner raw partitioner object
  @return the raw pointer */
  Partitioner* get() { return m_partitioner ? m_partitioner.get() : nullptr; }

  /** Release the internal KMeans partitioner object */
  void release() { m_partitioner = nullptr; }

 protected:
  /** Set the underlying partitioner object
  @param[in]    part              a native KMeans partitioner
  @remarks This only supports KMeansTreePartitioner derived classes atm. The
           input type remains Partitioner for simplicity cause most of the
           KMeans partitioner factory calls return the interface (Partitioner)
           typed pointers. */
  void set(std::shared_ptr<Partitioner> part);

 private:
  /** The underlying partitioner object created by KMeans library */
  std::shared_ptr<Partitioner> m_partitioner;

  /** # of centroids returned by default by m_partitioner's tokenize calls */
  uint32_t m_num_results;
};

} /* namespace ib_vector */

#endif /* _INCLUDE_KMEANS0PARTITIONER_H_ */

