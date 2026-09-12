#ifndef ddl0sub_table_h
#define ddl0sub_table_h

#include <cstddef>
#include <cstdint>
#include <utility>

#include "data0type.h"
#include "db0err.h"
#include "ddl0ddl.h"
#include "ddl0impl-buffer.h"
#include "ddl0impl-builder.h"
#include "dict0mem.h"
#include "include/univ.i"
#include "mem0mem.h"
#include "vector0dd.h"
#include "ut0core.h"
#include "ut0dbg.h"

namespace ddl {

/** Extends the builder class, which is used to build secondary indexes, to
build sub_table in case of vector index.

In this class we add some new custom methods and also break a little bit of
encapsulation by passing a pointer to the clustered index builder to the
secondary index builder. The upstream code envisages a builder working
independently of each working on one secondary index.

In our case we have one clustered index of the sub_table to build and one
secondary index. The builder building the secondary index will need access to
the partition_id as well base_pk. Both these values are computed values based on
the base table PK and embedding. We want to use the same values for the
secondary index which we used for the clustered index. Primarily for two
reasons:

1. Performance: We don't want to do the expensive operation of recomputing the
partition_id and rereading the embedding for that.
2. Consistency: We want the same values for the clustered index and the
secondary index. Note that partition_id is based on embedding value and it is
possible in extremely rare cases that floating point rounding errors give us
different partition_id value for same embedding which will cause corruption in
our secondary index. */
struct VectorIndexBuilder : public Builder {
  /** Constructor.
  @param[in,out] ctx            DDL context.
  @param[in,out] loader         Owner of the instance.
  @param[in] i                  Index ordinal value.
  @param[in] clust_builder      Clustered index builder. nullptr if this is
                                the clustered index builder. */
  VectorIndexBuilder(ddl::Context &ctx, Loader &loader, size_t i,
                     VectorIndexBuilder *clust_builder) noexcept;

  /** Destructor/ */
  ~VectorIndexBuilder() noexcept {}

  /** Note that we have an array of Thread_ctx in each builder. The size of
  array is equal to number of parallel threads working on index build.

  This is an extension of the Thread_ctx class where each thread can allocate
  buffer to store base_pk and partition_id it is working on. The idea is that
  when a thread is working on the clustered index of the sub_table, it will
  compute these values and store them in it's thread_ctx. When it is working on
  the secondary index, it will use the values stored in the thread_ctx. On next
  iteration, it will clear the heap used for allocation. */
  struct SubTableThreadCtx : public Builder::Thread_ctx {
    /** Constructor.
    @param[in] id               Thread state ID.
    @param[in,out] key_buffer   Buffer for building the target index. Note, the
                                thread state will own the key buffer and is
                                responsible for deleting it.
    @param[in] clust_ctx        True if this is the clustered index builder. */
    explicit SubTableThreadCtx(size_t id, Key_sort_buffer *key_buffer,
                               bool clust_ctx) noexcept
        : Builder::Thread_ctx(id, key_buffer) {
      if (clust_ctx) {
        m_sub_table_heap.create(sizeof(mrec_buf_t), UT_LOCATION_HERE);
        ut_a(m_sub_table_heap.get() != nullptr);
      }
    }

    /** Reset the heap and the buffers. */
    void reset_sub_table_heap() noexcept {
      m_sub_table_heap.clear();
      m_pk_buf.first = nullptr;
      m_pk_buf.second = 0;
      m_partition_buf = nullptr;
    }

    /** Heap used to store partition_id and base_pk. */
    Scoped_heap m_sub_table_heap{};

    /* Serialized base table PK. */
    std::pair<byte *, ulint> m_pk_buf{nullptr, 0};

    /* Partition id. */
    byte *m_partition_buf{nullptr};
  };

  /** Get the serialized base table PK.
  @param[in]  thread_id   Thread state ID.
  @return pair of data and len. We should never get UNIV_SQL_NULL as len as a PK
  can never be NULL. */
  std::pair<byte *, ulint> get_pk_buf(size_t thread_id) const noexcept {
    ut_a(m_index->is_clustered());
    ut_a(thread_id < m_thread_ctxs.size());
    return static_cast<SubTableThreadCtx *>(m_thread_ctxs[thread_id])->m_pk_buf;
  }

  /** Get the partition id.
  @param[in]  thread_id   Thread state ID.
  @return pointer topartition id buffer. */
  const byte *get_partition_buf(size_t thread_id) const noexcept {
    ut_a(m_index->is_clustered());
    ut_a(thread_id < m_thread_ctxs.size());
    return static_cast<SubTableThreadCtx *>(m_thread_ctxs[thread_id])
        ->m_partition_buf;
  }

 private:
  /** Overrides the copy_row() method of the base class.
  @param[in,out] ctx         Copy context.
  @param[in] mv_rows_added   Not used.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] virtual dberr_t copy_row(
      Copy_ctx &ctx, size_t &mv_rows_added) noexcept override;

  /** Copy blobs to the tuple.
  @param[out] dtuple            Tuple to copy to.
  @param[in,out] offsets        Column offsets in the row.
  @param[in] mrec               Current row.
  @param[in,out] heap           Heap for the allocating tuple memory.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t dtuple_copy_blobs(dtuple_t *dtuple, ulint *offsets,
                                          const mrec_t *mrec,
                                          mem_heap_t *heap) noexcept override;

  /** Build the clustered index tuple for sub_table by looking at a record
  @param[in,out] ctx         Copy context pointing to the base table record.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t build_clust_index_tuple(Copy_ctx &ctx) noexcept;

  /** Build the secondary index tuple for sub_table by looking at a record
  @param[in,out] ctx         Copy context pointing to the base table record.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t build_sec_index_tuple(Copy_ctx &ctx) noexcept;

 private:
  /** Base table clustered index. */
  const dict_index_t *m_base_index{nullptr};

  /** Upstream code stores the trx_id and rollback_ptr as written in old_table.
  We don't want to do that. Instead we write the current transaction's id and
  an emptly rollback ptr indicating that it is a fresh insert. As these
  values don't change they are shared by all parallel threads building
  clustered index rows of the sub_table. */

  /** Buffer to store the transaction id. */
  byte m_trx_id_buf[DATA_TRX_ID_LEN];

  /** Buffer to store the rollback pointer. */
  byte m_roll_ptr_buf[DATA_ROLL_PTR_LEN];

  /** Clustered index builder. Set only in case of secondary index builder. */
  const VectorIndexBuilder *m_clust_builder{nullptr};
};

}  // namespace ddl

#endif /* !ddl0sub_table_h */
