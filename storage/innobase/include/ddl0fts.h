/*****************************************************************************

Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

/** @file include/ddl0fts.h
 Create Full Text Index with (parallel) merge sort.
 Created 10/13/2010 Jimmy Yang */

#ifndef ddl0fts_h
#define ddl0fts_h

#include "btr0load.h"
#include "data0data.h"
#include "ddl0impl-buffer.h"
#include "dict0types.h"
#include "fts0priv.h"
#include "fts0types.h"
#include "ut0mpmcbq.h"

/** The general architecture is that the work is done in two phases,
roughly the read and write phase. The scanner pushes the document to
a read handler queue for processing.

Phase I:
 Start several parsing/tokenization threads that read the document from
 a queue, parse the document, tokenize the document, add them to a buffer,
 sort the rows in the buffer and then write the buffer to a temporary file.
 There is one file per auxiliary table per parser instance. So, if you
 have 2 parse threads you will end up with:

   2 x FTS_NUM_AUX_INDEX files.

Phase 2:
 The temporary files generated during phase I are not closed but passed to
 the second (write) phase so that these temporary files can be merged and
 the rows inserted into the new FTS index. Using the example from above,
 create FTS_NUM_AUX_INDEX threads and each thread will merge 2 files. */

namespace ddl {
// Forward declaration
struct Builder;

/** Full text search index builder. */
struct FTS {
  /** Information about temporary files used in merge sort. This structure
  defines information the scan thread will fetch and put to the linked list
  for parallel tokenization/sort threads to process */
  struct Doc_item {
    /** Field contains document string */
    dfield_t *m_field{};

    /** Document ID */
    doc_id_t m_doc_id{};
  };

  /** Constructor.
  @param[in, out] ctx           DDL context.
  @param[in, out] index         DDL index.
  @param[in, out] table         DDL table. */
  FTS(Context &ctx, dict_index_t *index, dict_table_t *table) noexcept;

  /** ~Destructor. */
  ~FTS() noexcept;

  /** Create the internal data structures.
  @param[in] n_threads          Number of parse threads to create.
  @return DB_SUCCESS or error code. */
  dberr_t init(size_t n_threads) noexcept;

  /** @return the DDL index. */
  dict_index_t *index() noexcept { return m_index; }

  /** @return the temporary sort index. */
  dict_index_t *sort_index() noexcept { return m_dup.m_index; }

  /** Start the parsing, create the threads.
  @return DB_SUCCESS or error code. */
  dberr_t start_parse_threads(Builder *builder) noexcept;

  /** For sending the documents to parse to the parsing threads.
  @param[in,out] doc_item       Document to parse, takes ownership.
  @return DB_SUCCESS or error code, doc_item will be deleted either way. */
  dberr_t enqueue(Doc_item *doc_item) noexcept;

  /** Check for error status after the parsing has finished.
  @return DB_SUCCESS or error code. */
  dberr_t check_for_errors() noexcept;

  /** Start the merging and insert threads.
  @param[in,out] builder        Builder instance to use.
  @return DB_SUCCESS or error code. */
  dberr_t insert(Builder *builder) noexcept;

  /** Inform the parser threads that the scanning phase is complete so
  that they can shutdown after emptying the doc item queue.
  @param[in] err                Error status of the scanning thread(s).
  @return DB_SUCCESS or error code. */
  dberr_t scan_finished(dberr_t err) noexcept;

 private:
  /** Create the data structures required to build the FTS index.
  @param[in] n_threads          Number of parser threads.
  @return DB_SUCCESS or error code. */
  dberr_t create(size_t n_threads) noexcept;

  /** @return the number of parses. */
  size_t get_n_parsers() const noexcept { return m_parsers.size(); }

  /** Destroy the data structures and clean up. */
  void destroy() noexcept;

  /** Create a temporary "fts sort index" used to merge sort the
   tokenized doc string. The index has three "fields":

   1. Tokenized word,
   2. Doc ID
   3. Word's position in original 'doc'.

  @param[in,out] index          Index to sort.
  @param[in,out] table          Table that the FTS index is created on.
  @param[out] doc_id_32_bit       Whether to use 4 bytes instead of 7 bytes
                                integer to store the DOC ID during sort.
  @return dict_index_t structure for the fts sort index */
  [[nodiscard]] static dict_index_t *create_index(dict_index_t *index,
                                                  dict_table_t *table,
                                                  bool *doc_id_32_bit) noexcept;

  /** Setup the insert phase inoput files generated by the parsers.
  @return DB_SUCCESS or error code. */
  dberr_t setup_insert_phase() noexcept;

 private:
  // Forward declaration
  struct Parser;
  struct Inserter;

  using Threads = std::vector<std::thread>;
  using Parsers = std::vector<Parser *, ut::allocator<Parser *>>;

  /** For parsing the documents, there is one per thread. */
  Parsers m_parsers{};

  /** For inserting the rows parsed by the m_parsers. */
  Inserter *m_inserter{};

  /** DDL context. */
  Context &m_ctx;

  /** Duplicate key reporting. */
  Dup m_dup;

  /** true if document ID should be stored as a 32 bit instead of a 64 bit. */
  bool m_doc_id_32_bit{};

  /** DDL index instance. */
  dict_index_t *m_index{};

  /** DDL table instance. */
  dict_table_t *m_table{};

  /** Temporary index instance with relevant FTS columns. */
  dict_index_t *m_sort_index{};

  /** For tracking parser threads. */
  Threads m_threads{};
};

}  // namespace ddl
#endif /* ddl0fts_h */
