/*****************************************************************************

Copyright (c) 2018, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/row0pread-adapter.h
Parallel read adapter interface.

Created 2018-02-28 by Darshan M N. */

#ifndef row0pread_adapter_h
#define row0pread_adapter_h

#include "row0pread.h"
#include "ut0counter.h"

#include "handler.h"

/** Traverse an index in the leaf page block list order and send records to
adapter. */
class Parallel_reader_adapter {
  /** Size of the buffer used to store InnoDB records and sent to the adapter*/
  static constexpr size_t ADAPTER_SEND_BUFFER_SIZE = 2 * 1024 * 1024;

  /** Forward declaration. */
  struct Thread_ctx;

 public:
  using Load_fn = handler::Load_cbk;

  using End_fn = handler::Load_end_cbk;

  using Init_fn = handler::Load_init_cbk;

  /** Constructor.
  @param[in]  max_threads       Maximum threads to use for all scan contexts.
  @param[in]  rowlen            Row length.  */
  Parallel_reader_adapter(size_t max_threads, ulint rowlen);

  /** Destructor. */
  ~Parallel_reader_adapter() = default;

  /** Add scan context.
  @param[in]  trx               Transaction used for parallel read.
  @param[in]  config            (Cluster) Index scan configuration.
  @param[in]  f                 Callback function.
  @retval error. */
  [[nodiscard]] dberr_t add_scan(trx_t *trx,
                                 const Parallel_reader::Config &config,
                                 Parallel_reader::F &&f);

  /** Run the parallel scan.
  @param[in]  thread_contexts   Context for each of the spawned threads
  @param[in]  init_fn           Callback called by each parallel load thread
                                at the beginning of the parallel load.
  @param[in]  load_fn           Callback called by each parallel load thread
                                when processing of rows is required.
  @param[in]  end_fn            Callback called by each parallel load thread
                                when processing of rows has ended.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t run(void **thread_contexts, Init_fn init_fn,
                            Load_fn load_fn, End_fn end_fn);

  /** Convert the record in InnoDB format to MySQL format and send them.
  @param[in]  reader_ctx  Parallel read context.
  @return error code */
  [[nodiscard]] dberr_t process_rows(const Parallel_reader::Ctx *reader_ctx);

  /** Set up the query processing state cache.
  @param[in]  prebuilt           The prebuilt cache for the query. */
  void set(row_prebuilt_t *prebuilt);

 private:
  /** Each parallel reader thread's init function.
  @param[in]  reader_thread_ctx  context info related to the
  current thread
  @param[in]  prebuilt           prebuilt cache
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t init(Parallel_reader::Thread_ctx *reader_thread_ctx,
                             row_prebuilt_t *prebuilt);

  /** Each parallel reader thread's end function.
  @param[in]  reader_thread_ctx  context info related to the current thread
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t end(Parallel_reader::Thread_ctx *reader_thread_ctx);

  /** Send a batch of records.
  @param[in]  reader_thread_ctx reader threads related thread context info
  @param[in]  partition_id      partition ID of the index the record belongs to
  @param[in]  n_recs            Number of records to send.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t send_batch(
      Parallel_reader::Thread_ctx *reader_thread_ctx, size_t partition_id,
      uint64_t n_recs);

  /** Get the number of rows buffered but not sent.
  @param[in]  ctx  adapter related thread context information.
  @return number of buffered items. */
  [[nodiscard]] size_t pending(Thread_ctx *ctx) const {
    return (ctx->m_n_read - ctx->m_n_sent);
  }

  /** Check if the buffer is full.
  @param[in]  ctx  adapter related thread context information.
  @return true if the buffer is full. */
  [[nodiscard]] bool is_buffer_full(Thread_ctx *ctx) const {
    return ctx->m_n_read > 0 && ctx->m_n_read % m_batch_size == 0;
  }

 private:
  /** Adapter context for each of the spawned threads. We don't know the
  type of the context it's passed to us as a void *.  */
  void **m_thread_ctxs{};

  /** Callback called by each parallel load thread at the
  beginning of the parallel load for the scan. */
  Init_fn m_init_fn{};

  /** Callback called by each parallel load thread when
  processing of rows is required for the scan. */
  Load_fn m_load_fn{};

  /** Callback called by each parallel load thread when
  processing of rows has ended for the scan. */
  End_fn m_end_fn{};

  /** Number of records to be sent across to the caller in a batch. */
  uint64_t m_batch_size{};

  /** MySQL row meta data. This is common across partitions. */
  struct MySQL_row {
    using Column_meta_data = std::vector<ulong, ut::allocator<ulong>>;

    /** Column offsets. */
    Column_meta_data m_offsets{};

    /** Column null bit masks. */
    Column_meta_data m_null_bit_mask{};

    /** Column null bit offsets. */
    Column_meta_data m_null_bit_offsets{};

    /** Maximum row length. */
    ulong m_max_len{};
  };

  /** Row meta data per scan context. */
  MySQL_row m_mysql_row{};

  /** Callback thread context for each of the spawned threads. */
  struct Thread_ctx {
    /** Constructor. */
    Thread_ctx();

    /** Destructor. */
    ~Thread_ctx() = default;

    /** Number of records read. */
    size_t m_n_read{};

    /** Number of records sent to the adapter. */
    size_t m_n_sent{};

    /** Partition ID for the records in buffer. Must be set when adding more
    records to be sent i.e. while incrementing m_n_read. */
    size_t m_partition_id{std::numeric_limits<size_t>::max()};

    /** Buffer to store records to be sent to the adapter. */
    std::vector<byte, ut::allocator<byte>> m_buffer;
  };

  /** Prebuilt to use for conversion to MySQL row format.
  NOTE: We are sharing this because we don't convert BLOBs yet. There are
  data members in row_prebuilt_t that cannot be accessed in multi-threaded
  mode e.g., blob_heap.

  row_prebuilt_t is designed for single threaded access and to share
  it among threads is not recommended unless "you know what you are doing".
  This is very fragile code as it stands.

  To solve the blob heap issue in prebuilt we use per thread m_blob_heaps.
  Pass the blob heap to the InnoDB to MySQL row format conversion function. */
  row_prebuilt_t *m_prebuilt{};

  /** Parallel reader to use. */
  Parallel_reader m_parallel_reader;
};

#endif /* !row0pread_adapter_h */
