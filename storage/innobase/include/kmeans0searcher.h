// Copyright 2026 Google LLC

/** @file include/kmeans0searcher.h
 KMeans searcher wrapper */

#ifndef _INCLUDE_KMEANS0SEARCHER_H_
#define _INCLUDE_KMEANS0SEARCHER_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <utility>

#include "vector0types.h"
#include "vector0cfg.h"
#include "kmeans0types.h"

#include "row0pread.h"
#include "data0data.h"
#include "db0err.h"
#include "dict0mem.h"
#include "mem0mem.h"
#include "trx0trx.h"
#include "ut0new.h"

#include "absl/synchronization/mutex.h"

namespace ib_vector {

/** Forward declaration */
class KMeansIndex;

/** KMeans Searcher
KMeansSearcher represents the (brute force) scanning of leaf nodes to serve ANN
queries. It is a simplified implementation of KMeans leaf searcher tailored to
the specific leaf data layout in InnoDB sub_tables.

After KMeansPartitioner picks the partitions to search, KMeansSearcher utilizes a
parallel ranged reader to scan those partitions. There is a callback function
provided to each scanning thread to process the vector data in each row (of
the sub_table).
    1. read the pk & quantized vector out
    2. calculate the distance between the query vector and this one
    3. push the pk and distance to a prirority queue (TopN)

The final results are the top N entries in the priority queue. */
class KMeansSearcher {
 public:
  // constructors & destructors

  /** Constructor
  @param[in]    kmeans_index        Pointer to the callingKMeansIndex object
  @param[in]    query             the query vector
  @param[in]    partitions        the partitions to search
  @param[in]    num_neighbors     the number of neighbors to return
  @param[in]    sub_table_index   cluster index of the sub_table to search
  @param[in]    trx               transaction of this query */
  KMeansSearcher(const KMeansIndex* kmeans_index, VectorDatapoint& query,
               const Partitions& partitions, size_t num_neighbors,
               dict_index_t* sub_table_index, trx_t* trx);

  /** Destructor */
  ~KMeansSearcher();

  // query interface

  /** Find the nearest neighbors
  @param[out]   neighbors         nearest neighbors
  @return error code or DB_SUCCESS */
  dberr_t get_neighbors(Neighbors& neighbors);

  /** Returns configured number of threads for parallel processing */
  size_t max_threads() const { return m_n_threads; }

  /** Returns base PK fixed length */
  size_t base_pk_fixed_len() const { return m_base_pk_fixed_len; }

  /** Returns quantized vector length */
  size_t vec_len() const { return m_dims; }

  /** Returns total length of content column including l2_norm */
  size_t content_len() const {
    return vec_len() + (m_dist_measure == DistMeasure::L2_SQUARED ? 4 : 0);
  }

  /** Returns sub_table index */
  const dict_index_t* sub_table_index() const { return m_sub_table_index; }

 protected:
  /** Initialize the searcher for a scan operation
  @remarks This function is called once before the scan starts. It
           sets up the searcher for the scan, including the number of threads
           to use for parallel processing, and the epsilon threshold to
           qualify a candidate for nearest neighbors. */
  void init_for_scan();

  /** Run the scan operation
  @return error code or DB_SUCCESS */
  dberr_t run_scan();

  /** Process the results from the scan operation
  @remarks This function is called after the scan operation is done. It
           processes the results from the scan operation and stores them in
           the return container.
  @param[out]   neighbors         the return container to store the results */
  void process_results(Neighbors& neighbors);

  /** Create a tuple based on partition id
  @param[in]    part              the partition id to search for
  @return a tuple that represents a sub_table row with given partition id */
  dtuple_t* build_tuple_from_partition_id(PartitionIdT part);

  /** Retrieve the vector data from a sub_table row
  @param[in]    ctx               the runtime ctx of a parallel ranged reader
  @remarks This function processes one record (retrieved by a parallel reader
           thread of a sub_table. It stores pointers to the PK and vector data
           in the page context. */
  void retrieve(const Parallel_reader::Ctx* ctx);

  /** Validate the partition_id column of the row record
  @param[in]    rec               the record to be validated
  @param[in]    index             cluster index of the sub_table
  @param[in]    tuple             the supposed value of partition_id column
  @param[in]    offsets           fields offsets from rec_get_offsets()
  @return true if the partition_id value is as expected */
  bool validate_row_partition_id(const rec_t* rec,
                                 const dict_index_t* index,
                                 const dtuple_t* tuple,
                                 const ulint* offsets);

  /** Check if given distance is close enough to make into the TopN
  @param[in]    dist              the distance to be qualified
  @return true if it qualifies TopN */
  bool is_close_enough(VectorDistT dist) { return dist <= m_epsilon.load(); }

  /** Record the distance measurement data
  @param[in]    pk              pk of the datapoint (in base table)
  @param[in]    distance        distance b/w this datapoint & the query vector
  @remarks The distance information is added to a priority queue, which
           automatically takes care of the ordering. */
  void add_neighbor_measurement(std::shared_ptr<std::string> pk,
                                VectorDistT distance);

  /** Returns number of threads to use for parallel processing. Zero if
  processing should be done inline. */
  size_t get_parallelism() const;

  /** Context for each page being scanned. We use this to store the pointers
  to PK and vector data for each row. At page boundary, we process all the
  rows in the page. This happens before we release the page latch in the scan
  code.  */
  struct Page_ctx {
    /** Constructor
    @param[in]    base_pk_fixed_len  True if base table PK is fixed length. */
    explicit Page_ctx(bool base_pk_fixed_len);

    ~Page_ctx() {
      if (m_current_index > 0) {
        ib::warn() << "Unprocessed rows in ANN search scan. Number of rows: "
                   << m_current_index;
        ut_d(ut_error);
      }

      if (m_pk_len_buf) {
        ut::free(m_pk_len_buf);
      }

      if (m_pk_ptrs) {
        ut::free(m_pk_ptrs);
      }

      if (m_vec_ptrs) {
        ut::free(m_vec_ptrs);
      }
    }

    /** Current index of the record being processed */
    size_t m_current_index{0};

    /** Store pointers to the quantized vector */
    byte** m_vec_ptrs{nullptr};

    /** Store pointers to the PK data */
    byte** m_pk_ptrs{nullptr};

    /** Buffer to store PK length. Used only if the PK is variable length */
    uint16_t* m_pk_len_buf{nullptr};

    /** Maximum number of rows we can potentially have in a page. Note that
    we have 8 byte partition_id, 6 byte trx_id and 7 byte rollback pointer apart
    from the PK and vector data. Setting this to 1024 is pretty safe with 16KB
    page size.  */
    static constexpr size_t kMaxRowsPerPage = 1024;
  };
  using Page_ctx_ptr = std::shared_ptr<Page_ctx>;

  /** Callback function called at end of each page and also at the end of the
  scan.
  @param[in]    ctx      the thread context of the parallel reader */
  void page_finish_cbk(const Parallel_reader::Thread_ctx* ctx);

  /** Process the rows in the page. This is called at the end of each page. */
  void get_distances(Page_ctx_ptr ctx);

 private:
  /** Search query */
  VectorDatapoint& m_query;

  /** L2 norm of the query vector */
  VectorDistT m_query_l2_norm{0.0};

  /** If the PK of the base table is fixed size then the length of PK in
  sub_table is stored here. */
  const size_t m_base_pk_fixed_len{0};

  /** The index dist measure */
  const DistMeasure m_dist_measure;

  /** Index dimensionality */
  const int m_dims;

  /** The total # of partitions in the whole index */
  const int m_num_total_partitions;

  /** The number of neighbors to return */
  const size_t m_num_neighbors;

  /** The partition list (in desc order of search priority) */
  const Partitions& m_partitions;

  /** Cluster index of the sub_table for the vector index */
  dict_index_t* m_sub_table_index;

  /** Transaction of this query */
  trx_t* m_trx;

  using TopN = FastTopNeighbors<VectorDistT, std::shared_ptr<std::string>>;

  /** Priority queue to store the vector & distance */
  TopN m_neighbors;

  /** Mutator of the top N neighbor priority queue */
  TopN::Mutator m_neighbors_mutator;

  /** The dist threshold to qualify a candidate for nearest neighbors */
  std::atomic<VectorDistT> m_epsilon {0};

  /** The mutex protecting the exclusive access of m_neighbors_mutator */
  absl::Mutex m_mutex;

  /** Number of threads to use for parallel processing */
  size_t m_n_threads{0};

  /** Heap for range tuples */
  mem_heap_t* m_heap;

  /** Per-thread heap for ranged partition scan */
  std::vector<mem_heap_t*, ut::allocator<mem_heap_t*>> m_thread_heaps;

  /** Per-thread page contexts */
  std::vector<Page_ctx_ptr, ut::allocator<Page_ctx_ptr>> m_page_ctxs;

  /** Count for scanned record for one query */
  std::atomic<uint64_t> m_scanned {0};
};

} /* namespace ib_vector */

#endif  /* !_INCLUDE_KMEANS0SEARCHER_H_ */
