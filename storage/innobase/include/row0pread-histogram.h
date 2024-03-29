/*****************************************************************************

Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

/** @file include/row0pread-histogram.h
Parallel read histogram interface.

Created 2019-04-20 by Darshan M N. */

#ifndef row0pread_histogram_h
#define row0pread_histogram_h

#include <random>
#include "row0pread.h"
#include "ut0counter.h"

class Histogram_sampler {
 public:
  /** Constructor.
  @param[in]  max_threads         Maximum number of threads to use.
  @param[in]  sampling_seed       seed to be used for sampling
  @param[in]  sampling_percentage percentage of sampling that needs to be done
  @param[in]  sampling_method     sampling method to be used for sampling */
  explicit Histogram_sampler(size_t max_threads, int sampling_seed,
                             double sampling_percentage,
                             enum_sampling_method sampling_method);

  /** Destructor. */
  ~Histogram_sampler();

  /** Initialize the sampler context.
  @param[in]  trx   Transaction used for parallel read.
  @param[in]  index clustered index.
  @param[in]  prebuilt  prebuilt info
  @retval true on success. */
  bool init(trx_t *trx, dict_index_t *index, row_prebuilt_t *prebuilt);

  /** Buffer next row.
  @return error code */
  dberr_t buffer_next();

  /** End parallel read in case the reader thread is still active and wait for
  its exit. This can happen if we're ending sampling prematurely. */
  void buffer_end();

  /** Set the buffer.
  @param[in]  buf buffer to be used to store the row converted to MySQL
  format. */
  void set(byte *buf) { m_buf = buf; }

  /** Start the sampling process.
  @return DB_SUCCESS or error code. */
  dberr_t run();

  /** Check if the processing of the record needs to be skipped.
  In case of record belonging to non-leaf page, we decide if the child page
  pertaining to the record needs to be skipped.
  In case of record belonging to leaf page, we read the page regardless.
  @return true if it needs to be skipped, else false. */
  bool skip();

 private:
  /** Wait till there is a request to buffer the next row. */
  void wait_for_start_of_buffering();

  /** Wait till the buffering of the row is complete. */
  void wait_for_end_of_buffering();

  /** Signal that the next row needs to be buffered. */
  void signal_start_of_buffering();

  /** Signal that the buffering of the row is complete. */
  void signal_end_of_buffering();

  /** Set the error state.
  @param[in] err                Error state to set to. */
  void set_error_state(dberr_t err) { m_err = err; }

  /** @return true if in error state. */
  [[nodiscard]] bool is_error_set() const { return (m_err != DB_SUCCESS); }

  /** Each parallel reader thread's init function.
  @param[in]  reader_thread_ctx  context information related to the thread
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t start_callback(
      Parallel_reader::Thread_ctx *reader_thread_ctx);

  /** Each parallel reader thread's end function.
  @param[in]  reader_thread_ctx  context information related to the thread
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t finish_callback(
      Parallel_reader::Thread_ctx *reader_thread_ctx);

  /** Convert the row in InnoDB format to MySQL format and store in the buffer
  for server to use.
  @param[in]  ctx       Parallel read context.
  @param[in]  rec       record that needs to be converted
  @param[in]  offsets   offsets belonging to the record
  @param[in]  index     index of the record
  @param[in]  prebuilt  Row meta-data cache.
  @return DB_SUCCESS or error code. */
  dberr_t sample_rec(const Parallel_reader::Ctx *ctx, const rec_t *rec,
                     ulint *offsets, const dict_index_t *index,
                     row_prebuilt_t *prebuilt);

  /** For each record in a non-leaf block at level 1 (if leaf level is 0)
  check if the child page needs to be sampled and if so sample all the rows in
  the child page.
  @param[in]  ctx       Parallel read context.
  @param[in]  prebuilt  Row meta-data cache.
  @return error code */
  [[nodiscard]] dberr_t process_non_leaf_rec(const Parallel_reader::Ctx *ctx,
                                             row_prebuilt_t *prebuilt);

  /** Process the record in the leaf page. This would happen only when the root
  page is the leaf page and in such a case we process the page regardless of
  the sampling percentage.
  @param[in]  ctx       Parallel read context.
  @param[in]  prebuilt  Row meta-data cache.
  @return error code */
  [[nodiscard]] dberr_t process_leaf_rec(const Parallel_reader::Ctx *ctx,
                                         row_prebuilt_t *prebuilt);

 private:
  /** Buffer to store the sampled row which is in the MySQL format. */
  byte *m_buf{nullptr};

  /** Event to notify if the next row needs to be buffered. */
  os_event_t m_start_buffer_event;

  /** Event to notify if the next row has been buffered. */
  os_event_t m_end_buffer_event;

  /** Error code when the row was buffered. */
  dberr_t m_err{DB_SUCCESS};

  /** The parallel reader. */
  Parallel_reader m_parallel_reader;

  /** Random generator engine used to provide us random uniformly distributed
  values required to decide if the row in question needs to be sampled or
  not. */
  std::mt19937 m_random_generator;

  /** Uniform distribution used by the random generator. */
  static std::uniform_real_distribution<double> m_distribution;

  /** Sampling method to be used for sampling. */
  enum_sampling_method m_sampling_method{enum_sampling_method::NONE};

  /** Sampling percentage to be used for sampling */
  double m_sampling_percentage{};

  /** Sampling seed to be used for sampling */
  int m_sampling_seed{};

  /** Number of rows sampled */
  std::atomic_size_t m_n_sampled;
};

#endif /* !row0pread_histogram_h */
