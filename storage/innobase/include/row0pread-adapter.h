/*****************************************************************************

Copyright (c) 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** Size of the buffer used to store InnoDB records and sent to the adapter*/
const uint64_t ADAPTER_SEND_BUFFER_SIZE = 2 * 1024 * 1024;

/** Traverse an index in the leaf page block list order and send records to
 * adapter. */
class Parallel_reader_adapter : public Key_reader {
  using Counter = ib_counter_t<size_t, 64, generic_indexer_t>;

  /** This callback is called by each parallel load thread at the beginning of
  the parallel load for the adapter scan.
  @param cookie      The cookie for this thread
  @param ncols       Number of columns in each row
  @param row_len     The size of a row in bytes
  @param col_offsets An array of size ncols, where each element represents
                     the offset of a column in the row data. The memory of
                     this array belongs to the caller and will be free-ed
                     after the pload_end_cbk call.
  @param nullbyte_offsets An array of size ncols, where each element
                     represents the offset of a column in the row data. The
                     memory of this array belongs to the caller and will be
                     free-ed after the pload_end_cbk call.
  @param null_bitmasks An array of size ncols, where each element
                     represents the bitmask required to get the null bit. The
                     memory of this array belongs to the caller and will be
                     free-ed after the pload_end_cbk call.
  @returns true if the is an error, false otherwise. */
  using pread_adapter_pload_init_cbk = std::function<bool(
      void *cookie, ulong ncols, ulong row_len, ulong *col_offsets,
      ulong *null_byte_offsets, ulong *null_bitmasks)>;

  /** This callback is called by each parallel load thread when processing
  of rows is required for the adapter scan.
  @param cookie    The cookie for this thread
  @param nrows     The nrows that are available
  @param rowdata   The mysql-in-memory row data buffer. It consistes of nrows
  number of records. */
  using pread_adapter_pload_row_cbk =
      std::function<bool(void *cookie, uint nrows, void *rowdata)>;

  /** This callback is called by each parallel load thread when processing
  of rows has eneded for the adapter scan.
  @param cookie    The cookie for this thread */
  using pread_adapter_pload_end_cbk = std::function<void(void *cookie)>;

 public:
  /** Constructor.
  @param[in]    table           Table to be read
  @param[in]    trx             Transaction used for parallel read
  @param[in]    index           Index in table to scan.
  @param[in]    n_threads       Maximum threads to use for reading
  @param[in]    prebuilt        InnoDB row prebuilt structure */
  Parallel_reader_adapter(dict_table_t *table, trx_t *trx, dict_index_t *index,
                          size_t n_threads, row_prebuilt_t *prebuilt)
      : Key_reader(table, trx, index, prebuilt, n_threads) {
    m_bufs.reserve(n_threads);

    for (size_t i = 0; i < n_threads; ++i) {
      m_bufs.push_back(
          static_cast<byte *>(ut_malloc_nokey(ADAPTER_SEND_BUFFER_SIZE)));
    }

    m_send_num_recs = ADAPTER_SEND_BUFFER_SIZE / m_prebuilt->mysql_row_len;
  }

  /** Destructor. */
  ~Parallel_reader_adapter() {
    for (auto buf : m_bufs) {
      ut_free(buf);
    }
  }

  /** Set callback info.
  @param[in]     thread_contexts context for each of the spawned threads
  @param[in]     load_init_fn    callback called by each parallel load thread
  at the beginning of the parallel load.
  @param[in]     load_rows_fn    callback called by each parallel load thread
  when processing of rows is required.
  @param[in]     load_end_fn     callback called by each parallel load thread
  when processing of rows has ended. */
  void set_callback(void **thread_contexts,
                    pread_adapter_pload_init_cbk load_init_fn,
                    pread_adapter_pload_row_cbk load_rows_fn,
                    pread_adapter_pload_end_cbk load_end_fn) {
    m_thread_contexts = thread_contexts;
    m_load_init = load_init_fn;
    m_load_rows = load_rows_fn;
    m_load_end = load_end_fn;
  }

  /** Convert the record in InnoDB format to MySQL format and send it to
  adapter.
  @param[in]      thread_id  Thread ID
  @param[in]      rec        InnoDB record
  @param[in]      index      InnoDB index which contains the record
  @param[in]      prebuilt   InnoDB row prebuilt structure
  @return error code */
  dberr_t process_rows(size_t thread_id, const rec_t *rec, dict_index_t *index,
                       row_prebuilt_t *prebuilt);

 protected:
  /** Counter to track number of records sent to adapter */
  Counter n_total_recs_sent;

 private:
  /** Poll for requests and execute.
  @param[in]      id         Thread ID
  @param[in,out]  ctxq       Queue with requests.
  @param[in]      f          Callback function. */
  dberr_t worker(size_t id, Queue &ctxq, Function &f);

  /** Counter to track number of records processed by each row. */
  Counter n_recs;

  /* adapter context for each of the spawned threads. */
  void **m_thread_contexts{nullptr};

  /** adapter callback called by each parallel load thread at the
  beginning of the parallel load for the adapter scan. */
  pread_adapter_pload_init_cbk m_load_init;

  /** adapter callback called by each parallel load thread when
  processing of rows is required for the adapter scan. */
  pread_adapter_pload_row_cbk m_load_rows;

  /** adapter callback called by each parallel load thread when
  processing of rows has ended for the adapter scan. */
  pread_adapter_pload_end_cbk m_load_end;

  /** Number of records to be sent across to adapter. */
  uint64_t m_send_num_recs;

  /** Buffer to store records to be sent to adapter. */
  std::vector<byte *> m_bufs;
};

/** Traverse all the indexes of a partitioned table in the leaf page block list
order and send records to adapter */
class Parallel_partition_reader_adapter : public Parallel_reader_adapter {
  using Counter = ib_counter_t<size_t, 64, generic_indexer_t>;

  /** This callback is called by each parallel load thread at the beginning of
  the parallel load for the adapter scan.
  @param cookie      The cookie for this thread
  @param ncols       Number of columns in each row
  @param row_len     The size of a row in bytes
  @param col_offsets An array of size ncols, where each element represents
                     the offset of a column in the row data. The memory of
                     this array belongs to the caller and will be free-ed
                     after the pload_end_cbk call.
  @param nullbyte_offsets An array of size ncols, where each element
                     represents the offset of a column in the row data. The
                     memory of this array belongs to the caller and will be
                     free-ed after the pload_end_cbk call.
  @param null_bitmasks An array of size ncols, where each element
                     represents the bitmask required to get the null bit. The
                     memory of this array belongs to the caller and will be
                     free-ed after the pload_end_cbk call. */
  using pread_adapter_pload_init_cbk = std::function<bool(
      void *cookie, ulong ncols, ulong row_len, ulong *col_offsets,
      ulong *null_byte_offsets, ulong *null_bitmasks)>;

  /** This callback is called by each parallel load thread when processing
  of rows is required for the adapter scan.
  @param cookie    The cookie for this thread
  @param nrows     The nrows that are available
  @param rowdata   The mysql-in-memory row data buffer. It consistes of nrows
  number of records. */
  using pread_adapter_pload_row_cbk =
      std::function<bool(void *cookie, uint nrows, void *rowdata)>;

  /** This callback is called by each parallel load thread when processing
  of rows has eneded for the adapter scan.
  @param cookie    The cookie for this thread */
  using pread_adapter_pload_end_cbk = std::function<void(void *cookie)>;

 public:
  /** Constructor.
  @param[in]    table           Table to be read
  @param[in]    trx             Transaction used for parallel read
  @param[in]    index           Index in table to scan.
  @param[in]    n_threads       Maximum threads to use for reading
  @param[in]    prebuilt        InnoDB row prebuilt structure
  @param[in]    num_parts       total number of partitions present in a table */
  Parallel_partition_reader_adapter(dict_table_t *table, trx_t *trx,
                                    dict_index_t *index, size_t n_threads,
                                    row_prebuilt_t *prebuilt,
                                    uint64_t num_parts)
      : Parallel_reader_adapter(table, trx, index, n_threads, prebuilt),
        m_num_parts(num_parts) {}

  /** Destructor */
  ~Parallel_partition_reader_adapter() {}

  /** Fetch number of threads that would be spawned for the parallel read.
  @return number of threads */
  size_t calc_num_threads();

  /** Fill info.
  @param[in]   table   table to be read
  @param[in]   index   index in table to scan
  @param[in]   trx     trx used for parallel read
  @param[in]   prebuilt        InnoDB row prebuilt structure */
  void set_info(dict_table_t *table, dict_index_t *index, trx_t *trx,
                row_prebuilt_t *prebuilt);

  /** Start the threads to do the parallel read.
  @param[in,out]  f    Callback to process the rows.
  @return error code. */
  dberr_t read(Function &&f);

 private:
  using Ranges = std::vector<Range, ut_allocator<Range>>;

  /** Vector of tables to read. */
  std::vector<dict_table_t *> m_table;

  /** Vector of indexes of all the partition in a table. */
  std::vector<dict_index_t *> m_index;

  /** Vector of trx's belonging to partitions. */
  std::vector<trx_t *> m_trx;

  /** Vector of row prebuilt structure belonging to partitions. */
  std::vector<row_prebuilt_t *> m_prebuilt;

  /** Range values of all the partitions in a table. */
  std::vector<Ranges> m_partitions;

  /** Total number of partitions present in a table. */
  uint64_t m_num_parts;
};

#endif /* !row0pread_adapter_h */
