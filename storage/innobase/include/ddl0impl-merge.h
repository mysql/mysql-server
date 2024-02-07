/*****************************************************************************

Copyright (c) 2020, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/ddl0impl-merge.h
 DDL cluster merge sort data structures.
 Created 2020-11-01 by Sunny Bains. */

#ifndef ddl0impl_merge_h
#define ddl0impl_merge_h

#include "ddl0impl-file-reader.h"

namespace ddl {

// Forward declaration.
struct Builder;

/** Merge the blocks in the file. */
struct Merge_file_sort {
  // Forward declarations.
  struct Cursor;
  struct Output_file;

  /** The design is generalized as an N way merge, however we stick
  with 2 for now. */
  static constexpr size_t N_WAY_MERGE = 2;

  /** Context to use for merging the files/runs. */
  struct Context {
    /** File to sort. */
    ddl::file_t *m_file{};

    /** For reporting duplicates, it has the index instance too. */
    Dup *m_dup{};

    /** Number of scan threads used, for memory buffer calculation. */
    size_t m_n_threads{};

    /** PFS progress monitoring. */
    Alter_stage *m_stage{};
  };

  /** Start of the record lists to merge. */
  using Range = std::pair<os_offset_t, os_offset_t>;

  /** Constructor.
  @param[in,out] merge_ctx      Data blocks merge meta data. */
  explicit Merge_file_sort(Context *merge_ctx) noexcept
      : m_merge_ctx(merge_ctx) {}

  /** Merge the the blocks.
  @param[in,out] builder        Builder instance used for building index.
  @param[in,out] offsets        Offsets from where to start the merge.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t sort(Builder *builder, Merge_offsets &offsets) noexcept;

  /** @return the number of rows in the sorted file. */
  [[nodiscard]] uint64_t get_n_rows() const noexcept { return m_n_rows; }

 private:
  /** Merge the rows.
  @param[in,out] cursor         To iterate over the rows to merge.
  @param[in,out] output_file    Output file to write the merged rows.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t merge_rows(Cursor &cursor,
                                   Output_file &output_file) noexcept;

  /** Merge the blocks in the ranges.
  @param[in,out] cursor         To iterate over the rows to merge.
  @param[in,out] offsets        Starting offsets of record lists to merge.
  @param[in,out] output_file    Output file to write the merged rows.
  @param[in] buffer_size        IO buffer size for reads.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t merge_ranges(Cursor &cursor, Merge_offsets &offsets,
                                     Output_file &output_file,
                                     size_t buffer_size) noexcept;

  /** Move to the next range of pages to merge.
  @param[in,out] offsets         Current offsets to start the merge from.
  @return the next range to merge. */
  Range next_range(Merge_offsets &offsets) noexcept;

 private:
  /** To check and report duplicates. */
  Dup *m_dup{};

  /** Meta data for merging blocks. */
  Context *m_merge_ctx{};

  /** Page numbers to merge for the next pass. */
  Merge_offsets m_next_offsets{};

  /** Number of rows in the sorted file. */
  uint64_t m_n_rows{};
};

}  // namespace ddl

#endif /* !ddl0impl_merge_h */
