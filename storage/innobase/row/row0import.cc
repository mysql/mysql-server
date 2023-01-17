/*****************************************************************************

Copyright (c) 2012, 2023, Oracle and/or its affiliates.

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

/** @file row/row0import.cc
 Import a tablespace to a running instance.

 Created 2012-02-08 by Sunny Bains.
 *******************************************************/

#include <errno.h>
#include <my_aes.h>
#include <sys/types.h>
#include <memory>
#include <vector>

#include "sql/dd/types/column_type_element.h"

#include "btr0pcur.h"
#include "dict0boot.h"
#include "dict0crea.h"
#include "dict0dd.h"
#include "dict0upgrade.h"
#include "ha_prototypes.h"
#include "ibuf0ibuf.h"
#include "lob0first.h"
#include "lob0impl.h"
#include "lob0lob.h"
#include "lob0pages.h"
#include "log0chkp.h"
#include "pars0pars.h"
#include "que0que.h"
#include "row0import.h"
#include "row0mysql.h"
#include "row0quiesce.h"
#include "row0sel.h"
#include "row0upd.h"
#include "sql/mysqld.h"
#include "srv0start.h"
#include "ut0new.h"
#include "zlob0first.h"

#include <vector>

#include "my_aes.h"
#include "my_dbug.h"

/** The size of the buffer to use for IO. Note: os_file_read() doesn't expect
reads to fail. If you set the buffer size to be greater than a multiple of the
file size then it will assert. TODO: Fix this limitation of the IO functions.
@param  m       page size of the tablespace.
@param  n       page size of the tablespace.
@retval number of pages */
inline size_t IO_BUFFER_SIZE(size_t m, size_t n) { return m / n; }

/** For gathering stats on records during phase I */
struct row_stats_t {
  ulint m_n_deleted; /*!< Number of deleted records
                     found in the index */

  ulint m_n_purged; /*!< Number of records purged
                    optimistically */

  ulint m_n_rows; /*!< Number of rows */

  ulint m_n_purge_failed; /*!< Number of deleted rows
                          that could not be purged */
};

/** Index information required by IMPORT. */
struct row_index_t {
  space_index_t m_id; /*!< Index id of the table
                      in the exporting server */
  byte *m_name;       /*!< Index name */

  space_id_t m_space; /*!< Space where it is placed */

  page_no_t m_page_no; /*!< Root page number */

  ulint m_type; /*!< Index type */

  ulint m_trx_id_offset; /*!< Relevant only for clustered
                         indexes, offset of transaction
                         id system column */

  ulint m_n_user_defined_cols; /*!< User defined columns */

  ulint m_n_uniq; /*!< Number of columns that can
                  uniquely identify the row */

  ulint m_n_nullable; /*!< Number of nullable
                      columns */

  ulint m_n_fields; /*!< Total number of fields */

  dict_field_t *m_fields; /*!< Index fields */

  const dict_index_t *m_srv_index; /*!< Index instance in the
                                   importing server */

  row_stats_t m_stats; /*!< Statistics gathered during
                       the import phase */
};

/** Meta data required by IMPORT. */
struct row_import {
 public:
  row_import() UNIV_NOTHROW : m_table(),
                              m_version(),
                              m_hostname(),
                              m_table_name(),
                              m_heap(nullptr),
                              m_autoinc(),
                              m_page_size(0, 0, false),
                              m_flags(),
                              m_n_cols(),
                              m_n_instant_cols(0),
                              m_n_instant_nullable(0),
                              m_cols(nullptr),
                              m_col_names(nullptr),
                              m_n_indexes(),
                              m_indexes(nullptr),
                              m_missing(true),
                              m_has_sdi(false),
                              m_cfp_missing(true) {}

  ~row_import() UNIV_NOTHROW;

  /** Find the index entry in in the indexes array.
  @param name index name
  @return instance if found else 0. */
  row_index_t *get_index(const char *name) const UNIV_NOTHROW;

  /** Get the number of rows in the index.
  @param name index name
  @return number of rows (doesn't include delete marked rows). */
  ulint get_n_rows(const char *name) const UNIV_NOTHROW;

  /** Find the ordinal value of the column name in the cfg table columns.
  @param name of column to look for.
  @return ULINT_UNDEFINED if not found. */
  ulint find_col(const char *name) const UNIV_NOTHROW;

  /** Get the number of rows for which purge failed during the
  convert phase.
  @param name index name
  @return number of rows for which purge failed. */
  ulint get_n_purge_failed(const char *name) const UNIV_NOTHROW;

  /** Check if the index is clean. ie. no delete-marked records
  @param name index name
  @return true if index needs to be purged. */
  bool requires_purge(const char *name) const UNIV_NOTHROW {
    return (get_n_purge_failed(name) > 0);
  }

  /** Set the index root <space, pageno> using the index name */
  void set_root_by_name() UNIV_NOTHROW;

  /** Set the index root <space, pageno> using a heuristic
  @return DB_SUCCESS or error code */
  dberr_t set_root_by_heuristic() UNIV_NOTHROW;

  /** Check if the index schema that was read from the .cfg file
  matches the in memory index definition.
  Note: It will update row_import_t::m_srv_index to map the meta-data
  read from the .cfg file to the server index instance.
  @return DB_SUCCESS or error code. */
  dberr_t match_index_columns(THD *thd, const dict_index_t *index) UNIV_NOTHROW;

  /** Check if the column default values of table schema that was
  read from the .cfg file matches the in memory column definition.
  @param[in]    thd             MySQL session variable
  @param[in]    dd_table        dd::Table
  @return       DB_SUCCESS or error code. */
  dberr_t match_col_default_values(THD *thd,
                                   const dd::Table *dd_table) UNIV_NOTHROW;

  /** Check if the table schema that was read from the .cfg file matches the
  in memory table definition.
  @param[in]    thd             MySQL session variable
  @param[in]    dd_table        dd::Table
  @return DB_SUCCESS or error code. */
  dberr_t match_compression_type_option(THD *thd,
                                        const dd::Table *dd_table) UNIV_NOTHROW;

  /** Check if the table schema that was read from the .cfg file
  matches the in memory table definition.
  @param thd MySQL session variable
  @return DB_SUCCESS or error code. */
  dberr_t match_table_columns(THD *thd) UNIV_NOTHROW;

  /** Check if the table (and index) schema that was read from the
  .cfg file matches the in memory table definition.
  @param[in]    thd             MySQL session variable
  @param[in]    dd_table        dd::Table
  @return DB_SUCCESS or error code. */
  dberr_t match_schema(THD *thd, const dd::Table *dd_table) UNIV_NOTHROW;

  /** Check if table being imported has INSTANT ADD/DROP columns
  @return true if table has INSTANT ADD/DROP columns */
  bool has_row_versions() {
    return (m_total_column_count > m_initial_column_count ||
            m_total_column_count > m_current_column_count);
  }

 private:
  /** Set the instant ADD COLUMN information to the table */
  dberr_t set_instant_info(THD *thd, const dd::Table *dd_table) UNIV_NOTHROW;

  /** Set the instant ADD/DROP COLUMN information to the table
  @param[in]            thd             MySQL session
  @param[in,out]        dd_table        target table definition
  @return DB_SUCCESS or error code. */
  dberr_t set_instant_info_v2(THD *thd, const dd::Table *dd_table) UNIV_NOTHROW;

  /** Match INSTANT metadata of CFG file and target table when both source and
  target table has INSTANT columns.
  @param[in]            thd             MySQL session
  @return DB_SUCCESS or error code. */
  dberr_t match_instant_metadata_in_target_table(THD *thd);

  /** Update INSTANT metadata into target table when only source has INSTANT
  columns.
  @param[in]            thd             MySQL session
  @param[in,out]        dd_table        target table definition
  @return DB_SUCCESS or error code. */
  dberr_t adjust_instant_metadata_in_taregt_table(THD *thd,
                                                  const dd::Table *dd_table);

  /** Add INSTANT DROP columns to target table innodb cache.
  @param[in] target_table       target table in InnoDB cache.
  @return DB_SUCCESS or error code. */
  dberr_t add_instant_dropped_columns(dict_table_t *target_table);

 public:
  dict_table_t *m_table; /*!< Table instance */

  uint32_t m_version; /*!< Version of config file */

  byte *m_hostname;   /*!< Hostname where the
                      tablespace was exported */
  byte *m_table_name; /*!< Exporting instance table
                      name */

  mem_heap_t *m_heap; /*!< Memory heap for default
                      value of instant columns */

  uint64_t m_autoinc; /*!< Next autoinc value */

  page_size_t m_page_size; /*!< Tablespace page size */

  uint32_t m_flags; /*!< Table flags */

  ulint m_n_cols; /*!< Number of columns in the
                  meta-data file */

  /* Column counts for table */
  uint32_t m_initial_column_count{0};
  uint32_t m_current_column_count{0};
  uint32_t m_total_column_count{0};
  uint32_t m_n_instant_drop_cols{0};
  uint32_t m_current_row_version{0};

  uint16_t m_n_instant_cols; /*!< Number of columns before
                             first instant ADD COLUMN in
                             the meta-data file */

  uint32_t m_n_instant_nullable;

  dict_col_t *m_cols; /*!< Column data */

  byte **m_col_names; /*!< Column names, we store the
                      column names separately because
                      there is no field to store the
                      value in dict_col_t */

  ulint m_n_indexes; /*!< Number of indexes,
                     including clustered index */

  row_index_t *m_indexes; /*!< Index meta data */

  bool m_missing; /*!< true if a .cfg file was
                  found and was readable */
  bool m_has_sdi; /*!< true if tablespace has
                  SDI */

  bool m_cfp_missing; /*!< true if a .cfp file was
                      found and was readable */

  /** Compression type in the meta-data file */
  Compression::Type m_compression_type{};

  /** Encryption settings */
  Encryption_metadata m_encryption_metadata{};
};

/** Use the page cursor to iterate over records in a block. */
class RecIterator {
 public:
  /** Default constructor */
  RecIterator() UNIV_NOTHROW = default;

  /** Position the cursor on the first user record. */
  void open(buf_block_t *block) UNIV_NOTHROW {
    page_cur_set_before_first(block, &m_cur);

    if (!end()) {
      next();
    }
  }

  /** Move to the next record. */
  void next() UNIV_NOTHROW { page_cur_move_to_next(&m_cur); }

  /**
  @return the current record */
  rec_t *current() UNIV_NOTHROW {
    ut_ad(!end());
    return (page_cur_get_rec(&m_cur));
  }

  /**
  @return true if cursor is at the end */
  bool end() UNIV_NOTHROW { return (page_cur_is_after_last(&m_cur) == true); }

  /** Remove the current record
  @return true on success */
  bool remove(const dict_index_t *index, ulint *offsets) UNIV_NOTHROW {
    /* We can't end up with an empty page unless it is root. */
    if (page_get_n_recs(m_cur.block->frame) <= 1) {
      return (false);
    }

    return (page_delete_rec(index, &m_cur, offsets));
  }

 private:
  page_cur_t m_cur;
};

/** Class that purges delete marked records from indexes, both secondary
and cluster. It does a pessimistic delete. This should only be done if we
couldn't purge the delete marked records during Phase I. */
class IndexPurge {
 public:
  /** Constructor
  @param trx the user transaction covering the import tablespace
  @param index to be imported. */
  IndexPurge(trx_t *trx, dict_index_t *index) UNIV_NOTHROW : m_trx(trx),
                                                             m_index(index),
                                                             m_n_rows(0) {
    ib::info(ER_IB_MSG_934)
        << "Phase II - Purge records from index " << index->name;
  }

  /** Destructor */
  ~IndexPurge() UNIV_NOTHROW = default;

  /** Purge delete marked records.
  @return DB_SUCCESS or error code. */
  dberr_t garbage_collect() UNIV_NOTHROW;

  /** The number of records that are not delete marked.
  @return total records in the index after purge */
  ulint get_n_rows() const UNIV_NOTHROW { return (m_n_rows); }

 private:
  /** Begin import, position the cursor on the first record. */
  void open() UNIV_NOTHROW;

  /** Close the persistent curosr and commit the mini-transaction. */
  void close() UNIV_NOTHROW;

  /** Position the cursor on the next record.
  @return DB_SUCCESS or error code */
  dberr_t next() UNIV_NOTHROW;

  /** Store the persistent cursor position and reopen the
  B-tree cursor in BTR_MODIFY_TREE mode, because the
  tree structure may be changed during a pessimistic delete. */
  void purge_pessimistic_delete() UNIV_NOTHROW;

  /** Purge delete-marked records. */
  void purge() UNIV_NOTHROW;

 protected:
  // Disable copying
  IndexPurge();
  IndexPurge(const IndexPurge &);
  IndexPurge &operator=(const IndexPurge &);

 private:
  trx_t *m_trx;          /*!< User transaction */
  mtr_t m_mtr;           /*!< Mini-transaction */
  btr_pcur_t m_pcur;     /*!< Persistent cursor */
  dict_index_t *m_index; /*!< Index to be processed */
  ulint m_n_rows;        /*!< Records in index */
};

/** Functor that is called for each physical page that is read from the
tablespace file.  */
class AbstractCallback : public PageCallback {
 public:
  /** Constructor
  @param trx covering transaction */
  AbstractCallback(trx_t *trx)
      : m_trx(trx),
        m_space(SPACE_UNKNOWN),
        m_xdes(),
        m_xdes_page_no(FIL_NULL),
        m_space_flags(UINT32_UNDEFINED),
        m_table_flags(UINT32_UNDEFINED) UNIV_NOTHROW {}

  /** Free any extent descriptor instance */
  ~AbstractCallback() override { ut::delete_arr(m_xdes); }

  /** Determine the page size to use for traversing the tablespace
  @param file_size size of the tablespace file in bytes
  @param block contents of the first page in the tablespace file.
  @retval DB_SUCCESS or error code. */
  dberr_t init(os_offset_t file_size,
               const buf_block_t *block) override UNIV_NOTHROW;

  /** @return true if compressed table. */
  bool is_compressed_table() const UNIV_NOTHROW {
    return (get_page_size().is_compressed());
  }

 protected:
  /** Get the data page depending on the table type, compressed or not.
  @param block block read from disk
  @retval the buffer frame */
  buf_frame_t *get_frame(buf_block_t *block) const UNIV_NOTHROW {
    if (is_compressed_table()) {
      return (block->page.zip.data);
    }

    return (buf_block_get_frame(block));
  }

  /** Check for session interrupt. If required we could
  even flush to disk here every N pages.
  @retval DB_SUCCESS or error code */
  dberr_t periodic_check() UNIV_NOTHROW {
    if (trx_is_interrupted(m_trx)) {
      return (DB_INTERRUPTED);
    }

    return (DB_SUCCESS);
  }

  /** Get the physical offset of the extent descriptor within the page.
  @param page_no page number of the extent descriptor
  @param page contents of the page containing the extent descriptor.
  @return the start of the xdes array in a page */
  const xdes_t *xdes(ulint page_no, const page_t *page) const UNIV_NOTHROW {
    ulint offset;

    offset = xdes_calc_descriptor_index(get_page_size(), page_no);

    return (page + XDES_ARR_OFFSET + XDES_SIZE * offset);
  }

  /** Set the current page directory (xdes). If the extent descriptor is
  marked as free then free the current extent descriptor and set it to
  0. This implies that all pages that are covered by this extent
  descriptor are also freed.

  @param page_no offset of page within the file
  @param page page contents
  @return DB_SUCCESS or error code. */
  dberr_t set_current_xdes(page_no_t page_no, const page_t *page) UNIV_NOTHROW {
    m_xdes_page_no = page_no;

    ut::delete_arr(m_xdes);
    m_xdes = nullptr;

    ulint state;
    const xdes_t *xdesc = page + XDES_ARR_OFFSET;

    state = mach_read_ulint(xdesc + XDES_STATE, MLOG_4BYTES);

    if (state != XDES_FREE) {
      m_xdes = ut::new_arr_withkey<xdes_t>(UT_NEW_THIS_FILE_PSI_KEY,
                                           ut::Count{m_page_size.physical()});

      /* Trigger OOM */
      DBUG_EXECUTE_IF("ib_import_OOM_13", ut::delete_arr(m_xdes);
                      m_xdes = nullptr;);

      if (m_xdes == nullptr) {
        return (DB_OUT_OF_MEMORY);
      }

      memcpy(m_xdes, page, m_page_size.physical());
    }

    return (DB_SUCCESS);
  }

  /**
  @return true if it is a root page */
  bool is_root_page(const page_t *page) const UNIV_NOTHROW {
    ut_ad(fil_page_index_page_check(page));

    return (mach_read_from_4(page + FIL_PAGE_NEXT) == FIL_NULL &&
            mach_read_from_4(page + FIL_PAGE_PREV) == FIL_NULL);
  }

  /** Check if the page is marked as free in the extent descriptor.
  @param page_no page number to check in the extent descriptor.
  @return true if the page is marked as free */
  bool is_free(page_no_t page_no) const UNIV_NOTHROW {
    ut_a(xdes_calc_descriptor_page(get_page_size(), page_no) == m_xdes_page_no);

    if (m_xdes != nullptr) {
      const xdes_t *xdesc = xdes(page_no, m_xdes);
      page_no_t pos = page_no % FSP_EXTENT_SIZE;

      return (xdes_get_bit(xdesc, XDES_FREE_BIT, pos));
    }

    /* If the current xdes was free, the page must be free. */
    return (true);
  }

 protected:
  /** Covering transaction. */
  trx_t *m_trx;

  /** Space id of the file being iterated over. */
  space_id_t m_space;

  /** Minimum page number for which the free list has not been
  initialized: the pages >= this limit are, by definition, free;
  note that in a single-table tablespace where size < 64 pages,
  this number is 64, i.e., we have initialized the space about
  the first extent, but have not physically allocated those pages
  to the file. @see FSP_LIMIT. */
  page_no_t m_free_limit;

  /** Current size of the space in pages */
  page_no_t m_size;

  /** Current extent descriptor page */
  xdes_t *m_xdes;

  /** Physical page offset in the file of the extent descriptor */
  page_no_t m_xdes_page_no;

  /** Flags value read from the header page */
  uint32_t m_space_flags;

  /** Derived from m_space_flags and row format type, the row format
  type is determined from the page header. */
  uint32_t m_table_flags;
};

/** Determine the page size to use for traversing the tablespace
@param file_size size of the tablespace file in bytes
@param block contents of the first page in the tablespace file.
@retval DB_SUCCESS or error code. */
dberr_t AbstractCallback::init(os_offset_t file_size,
                               const buf_block_t *block) UNIV_NOTHROW {
  const page_t *page = block->frame;

  m_space_flags = fsp_header_get_flags(page);

  /* Since we don't know whether it is a compressed table
  or not, the data is always read into the block->frame. */

  set_page_size(block->frame);

  /* Set the page size used to traverse the tablespace. */

  if (!is_compressed_table() && !m_page_size.equals_to(univ_page_size)) {
    ib::error(ER_IB_MSG_935)
        << "Page size " << m_page_size.physical()
        << " of ibd file is not the same as the server page"
           " size "
        << univ_page_size.physical();

    return (DB_CORRUPTION);

  } else if (file_size % m_page_size.physical() != 0) {
    ib::error(ER_IB_MSG_936) << "File size " << file_size
                             << " is not a"
                                " multiple of the page size "
                             << m_page_size.physical();

    return (DB_CORRUPTION);
  }

  ut_a(m_space == SPACE_UNKNOWN);

  m_size = mach_read_from_4(page + FSP_SIZE);
  m_free_limit = mach_read_from_4(page + FSP_FREE_LIMIT);
  m_space = fsp_header_get_field(page, FSP_SPACE_ID);
  dberr_t err = set_current_xdes(0, page);

  return (err);
}

/**
Try and determine the index root pages by checking if the next/prev
pointers are both FIL_NULL. We need to ensure that skip deleted pages. */
struct FetchIndexRootPages : public AbstractCallback {
  /** Index information gathered from the .ibd file. */
  struct Index {
    Index(space_index_t id, page_no_t page_no) : m_id(id), m_page_no(page_no) {}

    space_index_t m_id;  /*!< Index id */
    page_no_t m_page_no; /*!< Root page number */
  };

  typedef std::vector<Index, ut::allocator<Index>> Indexes;

  /** Constructor
  @param trx covering (user) transaction
  @param table table definition in server .*/
  FetchIndexRootPages(const dict_table_t *table, trx_t *trx)
      : AbstractCallback(trx), m_table(table) UNIV_NOTHROW {}

  /** Destructor */
  ~FetchIndexRootPages() UNIV_NOTHROW override = default;

  /**
  @retval the space id of the tablespace being iterated over */
  space_id_t get_space_id() const UNIV_NOTHROW override { return (m_space); }

  /**
  @retval the space flags of the tablespace being iterated over */
  ulint get_space_flags() const UNIV_NOTHROW override {
    return (m_space_flags);
  }

  /** Check if the .ibd file row format is the same as the table's.
  @param ibd_table_flags determined from space and page.
  @return DB_SUCCESS or error code. */
  dberr_t check_row_format(uint32_t ibd_table_flags) UNIV_NOTHROW {
    dberr_t err;
    rec_format_t ibd_rec_format;
    rec_format_t table_rec_format;

    if (!dict_tf_is_valid(ibd_table_flags)) {
      ib_errf(m_trx->mysql_thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              ".ibd file has invalid table flags: %x", ibd_table_flags);

      return (DB_CORRUPTION);
    }

    ibd_rec_format = dict_tf_get_rec_format(ibd_table_flags);
    table_rec_format = dict_tf_get_rec_format(m_table->flags);

    if (table_rec_format != ibd_rec_format) {
      ib_errf(m_trx->mysql_thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Table has %s row format, .ibd"
              " file has %s row format.",
              dict_tf_to_row_format_string(m_table->flags),
              dict_tf_to_row_format_string(ibd_table_flags));

      err = DB_CORRUPTION;
    } else {
      err = DB_SUCCESS;
    }

    return (err);
  }

  /** Called for each block as it is read from the file. Check index pages to
  determine the exact row format. We can't get that from the tablespace
  header flags alone.

  @param offset physical offset in the file
  @param block block to convert, it is not from the buffer pool.
  @retval DB_SUCCESS or error code. */
  dberr_t operator()(os_offset_t offset,
                     buf_block_t *block) override UNIV_NOTHROW;

  /** Update the import configuration that will be used to import
  the tablespace. */
  dberr_t build_row_import(row_import *cfg) const UNIV_NOTHROW;

  /** Table definition in server. */
  const dict_table_t *m_table;

  /** Index information */
  Indexes m_indexes;
};

/** Called for each block as it is read from the file. Check index pages to
determine the exact row format. We can't get that from the tablespace
header flags alone.

@param offset physical offset in the file
@param block block to convert, it is not from the buffer pool.
@retval DB_SUCCESS or error code. */
dberr_t FetchIndexRootPages::operator()(os_offset_t offset,
                                        buf_block_t *block) UNIV_NOTHROW {
  dberr_t err;

  if ((err = periodic_check()) != DB_SUCCESS) {
    return (err);
  }

  const page_t *page = get_frame(block);

  ulint page_type = fil_page_get_type(page);

  if (block->page.id.page_no() * m_page_size.physical() != offset) {
    ib::error(ER_IB_MSG_937)
        << "Page offset doesn't match file offset:"
           " page offset: "
        << block->page.id.page_no()
        << ", file offset: " << (offset / m_page_size.physical());

    err = DB_CORRUPTION;
  } else if (page_type == FIL_PAGE_TYPE_XDES) {
    err = set_current_xdes(block->page.id.page_no(), page);
  } else if (fil_page_index_page_check(page) &&
             !is_free(block->page.id.page_no()) && is_root_page(page)) {
    space_index_t id = btr_page_get_index_id(page);

    m_indexes.push_back(Index(id, block->page.id.page_no()));

    /* Since there are SDI Indexes before normal indexes, we
    check for FIL_PAGE_INDEX type. */
    if (page_type == FIL_PAGE_INDEX) {
      m_table_flags = fsp_flags_to_dict_tf(m_space_flags, page_is_comp(page));

      err = check_row_format(m_table_flags);
    }
  }

  return (err);
}

/**
Update the import configuration that will be used to import the tablespace.
@return error code or DB_SUCCESS */
dberr_t FetchIndexRootPages::build_row_import(row_import *cfg) const
    UNIV_NOTHROW {
  Indexes::const_iterator end = m_indexes.end();

  ut_a(cfg->m_table == m_table);
  cfg->m_page_size.copy_from(m_page_size);
  cfg->m_n_indexes = m_indexes.size();
  cfg->m_has_sdi = FSP_FLAGS_HAS_SDI(m_space_flags);
  cfg->m_flags = m_table->flags;

  if (cfg->m_n_indexes == 0) {
    ib::error(ER_IB_MSG_938) << "No B+Tree found in tablespace";

    return (DB_CORRUPTION);
  }

  cfg->m_indexes = ut::new_arr_withkey<row_index_t>(
      UT_NEW_THIS_FILE_PSI_KEY, ut::Count{cfg->m_n_indexes});

  /* Trigger OOM */
  DBUG_EXECUTE_IF("ib_import_OOM_11", ut::delete_arr(cfg->m_indexes);
                  cfg->m_indexes = nullptr;);

  if (cfg->m_indexes == nullptr) {
    return (DB_OUT_OF_MEMORY);
  }

  memset(cfg->m_indexes, 0x0, sizeof(*cfg->m_indexes) * cfg->m_n_indexes);

  row_index_t *cfg_index = cfg->m_indexes;

  for (Indexes::const_iterator it = m_indexes.begin(); it != end;
       ++it, ++cfg_index) {
    char name[BUFSIZ];

    snprintf(name, sizeof(name), "index" IB_ID_FMT, it->m_id);

    ulint len = strlen(name) + 1;

    cfg_index->m_name =
        ut::new_arr_withkey<byte>(UT_NEW_THIS_FILE_PSI_KEY, ut::Count{len});

    /* Trigger OOM */
    DBUG_EXECUTE_IF("ib_import_OOM_12", ut::delete_arr(cfg_index->m_name);
                    cfg_index->m_name = nullptr;);

    if (cfg_index->m_name == nullptr) {
      return (DB_OUT_OF_MEMORY);
    }

    memcpy(cfg_index->m_name, name, len);

    cfg_index->m_id = it->m_id;

    cfg_index->m_space = m_space;

    cfg_index->m_page_no = it->m_page_no;
  }

  return (DB_SUCCESS);
}

/* Functor that is called for each physical page that is read from the
tablespace file.

  1. Check each page for corruption.

  2. Update the space id and LSN on every page
    - For the header page
       - Validate the flags
       - Update the LSN

  3. On Btree pages
    - Set the index id
    - Update the max trx id
    - In a cluster index, update the system columns
    - In a cluster index, update the BLOB ptr, set the space id
    - Purge delete marked records, but only if they can be easily
       removed from the page
    - Keep a counter of number of rows, ie. non-delete-marked rows
    - Keep a counter of number of delete marked rows
    - Keep a counter of number of purge failure
    - If a page is stamped with an index id that isn't in the .cfg file
       we assume it is deleted and the page can be ignored.

   4. Set the page state to dirty so that it will be written to disk.
*/
class PageConverter : public AbstractCallback {
 public:
  /** Constructor
  @param cfg config of table being imported.
  @param trx transaction covering the import */
  PageConverter(row_import *cfg, trx_t *trx) UNIV_NOTHROW;

  ~PageConverter() UNIV_NOTHROW override {
    if (m_heap != nullptr) {
      mem_heap_free(m_heap);
    }
  }

  /**
  @retval the server space id of the tablespace being iterated over */
  space_id_t get_space_id() const UNIV_NOTHROW override {
    return (m_cfg->m_table->space);
  }

  /**
  @retval the space flags of the tablespace being iterated over */
  ulint get_space_flags() const UNIV_NOTHROW override {
    return (m_space_flags);
  }

  /** Called for every page in the tablespace. If the page was not
  updated then its state must be set to BUF_PAGE_NOT_USED.
  @param offset physical offset within the file
  @param block block read from file, note it is not from the buffer pool
  @retval DB_SUCCESS or error code. */
  dberr_t operator()(os_offset_t offset,
                     buf_block_t *block) override UNIV_NOTHROW;

 private:
  /** Status returned by PageConverter::validate() */
  enum import_page_status_t {
    IMPORT_PAGE_STATUS_OK,       /*!< Page is OK */
    IMPORT_PAGE_STATUS_ALL_ZERO, /*!< Page is all zeros */
    IMPORT_PAGE_STATUS_CORRUPTED /*!< Page is corrupted */
  };

  /** Update the page, set the space id, max trx id and index id.
  @param block block read from file
  @param page_type type of the page
  @retval DB_SUCCESS or error code */
  dberr_t update_page(buf_block_t *block, ulint &page_type) UNIV_NOTHROW;

#ifdef UNIV_DEBUG
  /**
  @return true error condition is enabled. */
  bool trigger_corruption() UNIV_NOTHROW { return (false); }
#else
#define trigger_corruption() (false)
#endif /* UNIV_DEBUG */

  /** Update the space, index id, trx id.
  @param block block to convert
  @return DB_SUCCESS or error code */
  dberr_t update_index_page(buf_block_t *block) UNIV_NOTHROW;

  /** Update the BLOB references and write UNDO log entries for
  rows that can't be purged optimistically.
  @param block block to update
  @retval DB_SUCCESS or error code */
  dberr_t update_records(buf_block_t *block) UNIV_NOTHROW;

  /** Validate the page, check for corruption.
  @param        offset  physical offset within file.
  @param        block   page read from file.
  @return 0 on success, 1 if all zero, 2 if corrupted */
  import_page_status_t validate(os_offset_t offset,
                                buf_block_t *block) UNIV_NOTHROW;

  /** Validate the space flags and update tablespace header page.
  @param block block read from file, not from the buffer pool.
  @retval DB_SUCCESS or error code */
  dberr_t update_header(buf_block_t *block) UNIV_NOTHROW;

  /** Adjust the BLOB reference for a single column that is externally stored
  @param rec record to update
  @param offsets column offsets for the record
  @param i column ordinal value
  @return DB_SUCCESS or error code */
  dberr_t adjust_cluster_index_blob_column(rec_t *rec, const ulint *offsets,
                                           ulint i) UNIV_NOTHROW;

  /** Adjusts the BLOB reference in the clustered index row for all
  externally stored columns.
  @param rec record to update
  @param offsets column offsets for the record
  @return DB_SUCCESS or error code */
  dberr_t adjust_cluster_index_blob_columns(rec_t *rec,
                                            const ulint *offsets) UNIV_NOTHROW;

  /** In the clustered index, adjist the BLOB pointers as needed.
  Also update the BLOB reference, write the new space id.
  @param rec record to update
  @param offsets column offsets for the record
  @return DB_SUCCESS or error code */
  dberr_t adjust_cluster_index_blob_ref(rec_t *rec,
                                        const ulint *offsets) UNIV_NOTHROW;

  /** Purge delete-marked records, only if it is possible to do so without
  re-organising the B+tree.
  @return true if purge succeeded */
  bool purge() UNIV_NOTHROW;

  /** Adjust the BLOB references and sys fields for the current record.
  @param index the index being converted
  @param rec record to update
  @param offsets column offsets for the record
  @return DB_SUCCESS or error code. */
  dberr_t adjust_cluster_record(const dict_index_t *index, rec_t *rec,
                                const ulint *offsets) UNIV_NOTHROW;

  /** Find an index with the matching id.
  @return row_index_t* instance or 0 */
  row_index_t *find_index(space_index_t id) UNIV_NOTHROW {
    row_index_t *index = &m_cfg->m_indexes[0];

    for (ulint i = 0; i < m_cfg->m_n_indexes; ++i, ++index) {
      if (id == index->m_id) {
        return (index);
      }
    }

    return (nullptr);
  }

 private:
  /** Config for table that is being imported. */
  row_import *m_cfg;

  /** Current index whose pages are being imported */
  row_index_t *m_index;

  /** Current system LSN */
  lsn_t m_current_lsn;

  /** Alias for m_page_zip, only set for compressed pages. */
  page_zip_des_t *m_page_zip_ptr;

  /** Iterator over records in a block */
  RecIterator m_rec_iter;

  /** Record offset */
  ulint m_offsets_[REC_OFFS_NORMAL_SIZE];

  /** Pointer to m_offsets_ */
  ulint *m_offsets;

  /** Memory heap for the record offsets */
  mem_heap_t *m_heap;

  /** Cluster index instance */
  dict_index_t *m_cluster_index;
};

/**
row_import destructor. */
row_import::~row_import() UNIV_NOTHROW {
  for (ulint i = 0; m_indexes != nullptr && i < m_n_indexes; ++i) {
    ut::delete_arr(m_indexes[i].m_name);

    if (m_indexes[i].m_fields == nullptr) {
      continue;
    }

    dict_field_t *fields = m_indexes[i].m_fields;
    ulint n_fields = m_indexes[i].m_n_fields;

    for (ulint j = 0; j < n_fields; ++j) {
      ut::delete_arr(const_cast<char *>(fields[j].name()));
    }

    ut::delete_arr(fields);
  }

  for (ulint i = 0; m_col_names != nullptr && i < m_n_cols; ++i) {
    ut::delete_arr(m_col_names[i]);
  }

  ut::delete_arr(m_cols);
  ut::delete_arr(m_indexes);
  ut::delete_arr(m_col_names);
  ut::delete_arr(m_table_name);
  ut::delete_arr(m_hostname);

  if (m_heap != nullptr) {
    mem_heap_free(m_heap);
  }
}

/** Find the index entry in in the indexes array.
@param name index name
@return instance if found else 0. */
row_index_t *row_import::get_index(const char *name) const UNIV_NOTHROW {
  for (ulint i = 0; i < m_n_indexes; ++i) {
    const char *index_name;
    row_index_t *index = &m_indexes[i];

    index_name = reinterpret_cast<const char *>(index->m_name);

    if (strcmp(index_name, name) == 0) {
      return (index);
    }
  }

  return (nullptr);
}

/** Get the number of rows in the index.
@param name index name
@return number of rows (doesn't include delete marked rows). */
ulint row_import::get_n_rows(const char *name) const UNIV_NOTHROW {
  const row_index_t *index = get_index(name);

  ut_a(name != nullptr);

  return (index->m_stats.m_n_rows);
}

/** Get the number of rows for which purge failed uding the convert phase.
@param name index name
@return number of rows for which purge failed. */
ulint row_import::get_n_purge_failed(const char *name) const UNIV_NOTHROW {
  const row_index_t *index = get_index(name);

  ut_a(name != nullptr);

  return (index->m_stats.m_n_purge_failed);
}

/** Find the ordinal value of the column name in the cfg table columns.
@param name of column to look for.
@return ULINT_UNDEFINED if not found. */
ulint row_import::find_col(const char *name) const UNIV_NOTHROW {
  for (ulint i = 0; i < m_n_cols; ++i) {
    const char *col_name;

    col_name = reinterpret_cast<const char *>(m_col_names[i]);

    if (strcmp(col_name, name) == 0) {
      return (i);
    }
  }

  return (ULINT_UNDEFINED);
}

/**
Check if the index schema that was read from the .cfg file matches the
in memory index definition.
@return DB_SUCCESS or error code. */
dberr_t row_import::match_index_columns(THD *thd, const dict_index_t *index)
    UNIV_NOTHROW {
  row_index_t *cfg_index;
  dberr_t err = DB_SUCCESS;

  cfg_index = get_index(index->name);

  if (cfg_index == nullptr) {
    ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
            "Index %s not found in tablespace meta-data file.", index->name());

    return (DB_ERROR);
  }

  if (cfg_index->m_n_fields != index->n_fields) {
    ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
            "Index field count %lu doesn't match"
            " tablespace metadata file value %lu",
            (ulong)index->n_fields, (ulong)cfg_index->m_n_fields);

    return (DB_ERROR);
  }

  cfg_index->m_srv_index = index;

  const dict_field_t *field = index->fields;
  const dict_field_t *cfg_field = cfg_index->m_fields;

  for (ulint i = 0; i < index->n_fields; ++i, ++field, ++cfg_field) {
    if (strcmp(field->name(), cfg_field->name()) != 0) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Index field name %s doesn't match tablespace metadata"
              " field name %s for field position %lu",
              field->name(), cfg_field->name(), (ulong)i);

      err = DB_ERROR;
    }

    if (cfg_field->prefix_len != field->prefix_len) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Index %s field %s prefix len %lu doesn't match metadata"
              " file value %lu",
              index->name(), field->name(), (ulong)field->prefix_len,
              (ulong)cfg_field->prefix_len);

      err = DB_ERROR;
    }

    if (cfg_field->fixed_len != field->fixed_len) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Index %s field %s fixed len %lu doesn't match metadata"
              " file value %lu",
              index->name(), field->name(), (ulong)field->fixed_len,
              (ulong)cfg_field->fixed_len);

      err = DB_ERROR;
    }

    constexpr char asc[] = "ascending";
    constexpr char desc[] = "descending";

    if (cfg_field->is_ascending != field->is_ascending) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Index %s field %s is %s which does not match metadata"
              " file which is %s",
              index->name(), field->name(), (field->is_ascending ? asc : desc),
              (cfg_field->is_ascending ? asc : desc));

      err = DB_SCHEMA_MISMATCH;
    }
  }

  return (err);
}

/** Check if the column default values of table schema that was
read from the .cfg file matches the in memory column definition.
@param[in]      thd             MySQL session variable
@param[in]      dd_table        dd::Table
@return DB_SUCCESS or error code. */
dberr_t row_import::match_col_default_values(
    THD *thd, const dd::Table *dd_table) UNIV_NOTHROW {
  dberr_t err = DB_SUCCESS;

  ut_ad(dd_table_is_partitioned(*dd_table) == dict_table_is_partition(m_table));

  err = set_instant_info(thd, dd_table);

  if (err != DB_SUCCESS) {
    return (err);
  }

  /* Only check instant partitioned table. Because different partitions
  may have different number of default values, make sure the default
  values of this imported table match the default values which are
  already remembered in server.
  Also if the table in server is not instant, then all fine, just
  store the new default values */
  if (!m_table->has_instant_cols() || !dict_table_is_partition(m_table) ||
      !dd_table_has_instant_cols(*dd_table)) {
    return (err);
  }

  for (uint16_t i = 0; i < m_table->get_n_user_cols(); ++i) {
    dict_col_t *col = m_table->get_col(i);
    if (col->instant_default == nullptr) {
      continue;
    }

    const dd::Column *dd_col =
        dd_find_column(dd_table, m_table->get_col_name(i));

    if (!dd_match_default_value(dd_col, col)) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Default values of instant column %s mismatch",
              dd_col->name().c_str());

      err = DB_ERROR;
      break;
    }
  }

  return (err);
}

/** Check if the table schema that was read from the .cfg file matches the
in memory table definition.
@param[in]      thd             MySQL session variable
@param[in]      dd_table        dd::Table
@return DB_SUCCESS or error code. */
dberr_t row_import::match_compression_type_option(
    THD *thd, const dd::Table *dd_table) UNIV_NOTHROW {
  dd::String_type compress_option;
  auto &options = dd_table->options();

  if (options.exists("compress")) {
    options.get("compress", &compress_option);
  } else {
    compress_option = "none";
  }

  if (innobase_strcasecmp(Compression::to_string(m_compression_type),
                          compress_option.c_str()) != 0) {
    ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
            "Compression option does not match");
    return DB_ERROR;
  }

  return DB_SUCCESS;
}

/** Check if the table schema that was read from the .cfg file matches the
in memory table definition.
@param thd MySQL session variable
@return DB_SUCCESS or error code. */
dberr_t row_import::match_table_columns(THD *thd) UNIV_NOTHROW {
  dberr_t err = DB_SUCCESS;
  const dict_col_t *col = m_table->cols;
  uint32_t n_sys_cols = 0;

  if (m_version >= IB_EXPORT_CFG_VERSION_V7 && m_table->has_row_versions()) {
    /* Only target table has row versions, ERROR */
    if (!has_row_versions()) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "The .cfg file indicates no INSTANT column in the source table"
              " whereas the metadata in data dictionary says there are instant"
              " columns in the target table");

      return (DB_ERROR);
    }

    if (m_table->current_row_version != m_current_row_version) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Table has instant column but current row version didn't match.");
      return (DB_ERROR);
    }

    if ((m_table->initial_col_count != m_initial_column_count) ||
        (m_table->current_col_count != m_current_column_count) ||
        (m_table->total_col_count != m_total_column_count)) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Table has instant column but column counts didn't match.");
      return (DB_ERROR);
    }
  }

  /* Following loop makes sure all the columns present in target table are
  accounted for */
  for (ulint i = 0; i < m_table->n_cols; ++i, ++col) {
    const char *col_name;
    ulint cfg_col_index;

    col_name = m_table->get_col_name(dict_col_get_no(col));

    if ((strcmp(col_name, "DB_ROW_ID") == 0) ||
        (strcmp(col_name, "DB_TRX_ID") == 0) ||
        (strcmp(col_name, "DB_ROLL_PTR") == 0)) {
      n_sys_cols += 1;
    }

    cfg_col_index = find_col(col_name);

    if (cfg_col_index == ULINT_UNDEFINED) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Column %s not found in tablespace.", col_name);

      err = DB_ERROR;
    } else if (cfg_col_index != col->ind) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Column %s ordinal value mismatch, it's at"
              " %lu in the table and %lu in the tablespace"
              " meta-data file",
              col_name, (ulong)col->ind, (ulong)cfg_col_index);

      err = DB_ERROR;
    } else {
      const dict_col_t *cfg_col;

      cfg_col = &m_cols[cfg_col_index];
      ut_a(cfg_col->ind == cfg_col_index);

      if (cfg_col->prtype != col->prtype) {
        ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
                "Column %s precise type mismatch.", col_name);
        err = DB_ERROR;
      }

      if (cfg_col->mtype != col->mtype) {
        ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
                "Column %s main type mismatch.", col_name);
        err = DB_ERROR;
      }

      if (cfg_col->len != col->len) {
        ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
                "Column %s length mismatch.", col_name);
        err = DB_ERROR;
      }

      if (cfg_col->mbminmaxlen != col->mbminmaxlen) {
        ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
                "Column %s multi-byte len mismatch.", col_name);
        err = DB_ERROR;
      }

      if (cfg_col->ind != col->ind) {
        err = DB_ERROR;
      }

      if (cfg_col->ord_part != col->ord_part) {
        ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
                "Column %s ordering mismatch.", col_name);
        err = DB_ERROR;
      }

      if (cfg_col->max_prefix != col->max_prefix) {
        ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
                "Column %s max prefix mismatch.", col_name);
        err = DB_ERROR;
      }
    }
  }

  /* Following check makes sure all the columns present in config file are
  accounted for */
  if (m_version >= IB_EXPORT_CFG_VERSION_V7 && has_row_versions()) {
    if (!(m_table->n_cols - n_sys_cols == m_current_column_count)) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Found %u columns in destination table whereas cfg file has %u"
              " columns.",
              (m_table->n_cols - n_sys_cols), m_current_column_count);
      err = DB_ERROR;
    }
  } else {
    if (!(m_table->n_cols == m_n_cols)) {
      ib_errf(
          thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
          "Found %u columns in destination table whereas cfg file has " ULINTPF
          " columns.",
          (m_table->n_cols - n_sys_cols), (m_n_cols - n_sys_cols));
      err = DB_ERROR;
    }
  }

  return (err);
}

/** Check if the table (and index) schema that was read from the .cfg file
matches the in memory table definition.
@param[in]      thd             MySQL session variable
@param[in]      dd_table        dd::Table
@return DB_SUCCESS or error code. */
dberr_t row_import::match_schema(THD *thd,
                                 const dd::Table *dd_table) UNIV_NOTHROW {
  /* Do some simple checks. */

  if (m_flags != m_table->flags) {
    if (dict_tf_to_row_format_string(m_flags) !=
        dict_tf_to_row_format_string(m_table->flags)) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Table flags don't match, server table has %s"
              " and the meta-data file has %s",
              (const char *)dict_tf_to_row_format_string(m_table->flags),
              (const char *)dict_tf_to_row_format_string(m_flags));
    } else if (DICT_TF_HAS_DATA_DIR(m_flags) !=
               DICT_TF_HAS_DATA_DIR(m_table->flags)) {
      /* If the meta-data flag is set for data_dir, but table flag is not set
      for data_dir or vice versa then return error. */
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Table location flags do not match. The source table %s a "
              "DATA DIRECTORY but the destination table %s.",
              (DICT_TF_HAS_DATA_DIR(m_flags) ? "uses" : "does not use"),
              (DICT_TF_HAS_DATA_DIR(m_table->flags) ? "does" : "does not"));
    } else {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Table flags don't match");
    }
    return (DB_ERROR);
  } else if (m_table->n_cols != m_n_cols - m_n_instant_drop_cols) {
    ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
            "Number of columns don't match, table has %lu"
            " columns but the tablespace meta-data file has"
            " %lu columns",
            (ulong)m_table->n_cols, (ulong)(m_n_cols - m_n_instant_drop_cols));

    return (DB_ERROR);
  } else if (UT_LIST_GET_LEN(m_table->indexes) + (m_has_sdi ? 1 : 0) !=
             m_n_indexes) {
    /* If the number of indexes don't match then it is better
    to abort the IMPORT. It is easy for the user to create a
    table matching the IMPORT definition. */

    ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
            "Number of indexes don't match, table has %lu"
            " indexes but the tablespace meta-data file has"
            " %lu indexes",
            (ulong)UT_LIST_GET_LEN(m_table->indexes), (ulong)m_n_indexes);

    return (DB_ERROR);
  }

  dberr_t err;

  if (m_version >= IB_EXPORT_CFG_VERSION_V6) {
    err = match_compression_type_option(thd, dd_table);

    if (err != DB_SUCCESS) {
      return err;
    }
  }

  err = match_table_columns(thd);

  if (err != DB_SUCCESS) {
    return (err);
  }

  err = match_col_default_values(thd, dd_table);

  if (err != DB_SUCCESS) {
    return (err);
  }

  /* Check if the SDI index definitions match */

  if (m_has_sdi) {
    const dict_index_t *index;
    dict_mutex_enter_for_mysql();

    index = dict_sdi_get_index(m_table->space);

    if (index == nullptr) {
      dict_sdi_create_idx_in_mem(m_table->space, true,
                                 dict_tf_to_fsp_flags(m_flags), false);

      index = dict_sdi_get_index(m_table->space);
    }

    dict_mutex_exit_for_mysql();

    ut_ad(index != nullptr);

    dberr_t index_err = match_index_columns(thd, index);

    if (index_err != DB_SUCCESS) {
      err = index_err;
    }
  }

  if (err != DB_SUCCESS) {
    return (err);
  }

  /* Check if the index definitions match. */
  for (auto index : m_table->indexes) {
    dberr_t index_err;

    index_err = match_index_columns(thd, index);

    if (index_err != DB_SUCCESS) {
      err = index_err;
    }
  }

  return (err);
}

/**
Set the index root <space, pageno>, using index name. */
void row_import::set_root_by_name() UNIV_NOTHROW {
  row_index_t *cfg_index = m_indexes;
  dict_index_t *index;
  ulint normal_indexes_count = m_has_sdi ? (m_n_indexes - 1) : m_n_indexes;

  if (m_has_sdi) {
    dict_mutex_enter_for_mysql();
    index = dict_sdi_get_index(m_table->space);
    dict_mutex_exit_for_mysql();

    ut_ad(index != nullptr);
    index->space = m_table->space;
    index->page = cfg_index->m_page_no;
    ++cfg_index;
  }

  for (uint32_t i = 0; i < normal_indexes_count; ++i, ++cfg_index) {
    const char *index_name;

    index_name = reinterpret_cast<const char *>(cfg_index->m_name);

    index = dict_table_get_index_on_name(m_table, index_name);

    /* We've already checked that it exists. */
    ut_a(index != nullptr);

    /* Set the root page number and space id. */
    index->space = m_table->space;
    index->page = cfg_index->m_page_no;
  }
}

/**
Set the index root <space, pageno>, using a heuristic.
@return DB_SUCCESS or error code */
dberr_t row_import::set_root_by_heuristic() UNIV_NOTHROW {
  row_index_t *cfg_index = m_indexes;

  ut_a(m_n_indexes > 0);

  // TODO: For now use brute force, based on ordinality

  ulint num_indexes = UT_LIST_GET_LEN(m_table->indexes) + (m_has_sdi ? 1 : 0);
  if (num_indexes != m_n_indexes) {
    ib::warn(ER_IB_MSG_939)
        << "Table " << m_table->name << " should have " << num_indexes
        << " indexes but"
           " the tablespace has "
        << m_n_indexes << " indexes";
  }

  dict_mutex_enter_for_mysql();

  ulint i = 0;
  dberr_t err = DB_SUCCESS;

  if (m_has_sdi) {
    dict_index_t *index = dict_sdi_get_index(m_table->space);
    if (index == nullptr) {
      dict_sdi_create_idx_in_mem(m_table->space, true,
                                 dict_tf_to_fsp_flags(m_flags), false);

      index = dict_sdi_get_index(m_table->space);
    }

    ut_ad(index != nullptr);
    ut::delete_arr(cfg_index[i].m_name);

    ulint len = strlen(index->name) + 1;

    cfg_index[i].m_name =
        ut::new_arr_withkey<byte>(UT_NEW_THIS_FILE_PSI_KEY, ut::Count{len});

    if (cfg_index[i].m_name == nullptr) {
      err = DB_OUT_OF_MEMORY;
      dict_mutex_exit_for_mysql();
      return (err);
    }

    memcpy(cfg_index[i].m_name, index->name, len);

    cfg_index[i].m_srv_index = index;

    index->space = m_table->space;
    index->page = cfg_index[i].m_page_no;
    ++i;
  }

  for (auto index : m_table->indexes) {
    if (index->type & DICT_FTS) {
      dict_set_corrupted(index);
      ib::warn(ER_IB_MSG_940) << "Skipping FTS index: " << index->name;
    } else if (i < m_n_indexes) {
      ut::delete_arr(cfg_index[i].m_name);

      ulint len = strlen(index->name) + 1;

      cfg_index[i].m_name =
          ut::new_arr_withkey<byte>(UT_NEW_THIS_FILE_PSI_KEY, ut::Count{len});

      /* Trigger OOM */
      DBUG_EXECUTE_IF("ib_import_OOM_14", ut::delete_arr(cfg_index[i].m_name);
                      cfg_index[i].m_name = nullptr;);

      if (cfg_index[i].m_name == nullptr) {
        err = DB_OUT_OF_MEMORY;
        break;
      }

      memcpy(cfg_index[i].m_name, index->name, len);

      cfg_index[i].m_srv_index = index;

      index->space = m_table->space;
      index->page = cfg_index[i].m_page_no;
      ++i;
    }
  }

  dict_mutex_exit_for_mysql();

  return (err);
}

dberr_t row_import::match_instant_metadata_in_target_table(THD *thd) {
  if (m_table->current_row_version != m_current_row_version) {
    /* It must have already been checked in match_table_columns */
    ut_ad(false);
    ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
            "Target table also has instant column but current row version"
            " didn't match with the configuration file.");
    return (DB_ERROR);
  }

  if ((m_table->initial_col_count != m_initial_column_count) ||
      (m_table->current_col_count != m_current_column_count) ||
      (m_table->total_col_count != m_total_column_count)) {
    /* It must have already been checked in match_table_columns */
    ut_ad(false);
    ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
            "Target table also has instant columns but column counts didn't"
            " match with the configuration file.");
    return (DB_ERROR);
  }

  for (uint32_t i = 0; i < m_n_cols; i++) {
    dict_col_t *cfg_col = &m_cols[i];
    ut_ad(cfg_col != nullptr);

    const char *col_name = (char *)m_col_names[i];

    /* Search for this column in target table */
    dict_col_t *target_col = m_table->get_col_by_name(col_name);

    if (target_col == nullptr) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Instant metadata didn't match for dropped column");
      return DB_ERROR;
    }

    ut_a(target_col != nullptr);

    if (!cfg_col->is_version_added_match(target_col) ||
        !cfg_col->is_version_dropped_match(target_col) ||
        (cfg_col->ind != target_col->ind) ||
        (cfg_col->get_phy_pos() != target_col->get_phy_pos())) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Instant metadata didn't match for column %s", col_name);
      return DB_ERROR;
    }

    if (cfg_col->instant_default == nullptr &&
        target_col->instant_default == nullptr) {
      /* This isn't an INSTANT ADD columns or this column has been dropped. */
      ut_ad(!cfg_col->is_instant_added() || cfg_col->is_instant_dropped());
      continue;
    }

    if (cfg_col->instant_default == nullptr &&
        target_col->instant_default != nullptr) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "The metadata in the data dictionary and the .cfg file contain"
              " different default values for column %s!",
              col_name);
      return DB_ERROR;
    }

    if (cfg_col->instant_default != nullptr &&
        target_col->instant_default == nullptr) {
      /* set the value from .cfg file. */
      target_col->set_default(cfg_col->instant_default->value,
                              cfg_col->instant_default->len, m_table->heap);
    }

    /* If instant_default values are different, error */
    if (*target_col->instant_default != *cfg_col->instant_default) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "The metadata in the data dictionary and the .cfg file contain"
              " different default values for column %s!",
              col_name);
      return DB_ERROR;
    }
  }

  return DB_SUCCESS;
}

dberr_t row_import::add_instant_dropped_columns(dict_table_t *target_table) {
  dict_index_t *index = m_table->first_index();
  ut_ad(index->is_clustered());

  /* NOTE : Generated columns can't be part of clustered index so all the
  fields have to be pointing to cols in table->cols. */
  uint16_t *mapping = ut::new_arr_withkey<uint16_t>(UT_NEW_THIS_FILE_PSI_KEY,
                                                    ut::Count{index->n_fields});
  for (size_t i = 0; i < index->n_fields; i++) {
    mapping[i] = index->get_field(i)->col->ind;
  }

  /* Get the table->heap size in saved_heap_size */
  size_t old_heap_size =
      mem_heap_get_size(target_table->heap) + mem_heap_get_size(index->heap);

  uint32_t n_dropped_cols = m_total_column_count - m_current_column_count;

  /* Allocate memory for n_cols (table->n_cols + n_dropped_cols) */
  {
    dict_col_t *cols = target_table->cols;
    uint32_t total_cols = target_table->n_cols + n_dropped_cols;
    target_table->cols = (dict_col_t *)mem_heap_alloc(
        target_table->heap, total_cols * sizeof(dict_col_t));
    memcpy(target_table->cols, cols, target_table->n_cols * sizeof(dict_col_t));
  }

  /* Allocate memory for n_fields (index->n_fields + n_dropped_cols) */
  {
    dict_field_t *fields = index->fields;
    uint32_t total_fields = index->n_fields + n_dropped_cols;
    index->fields = (dict_field_t *)mem_heap_alloc(
        index->heap, 1 + (total_fields) * sizeof(dict_field_t));
    memcpy(index->fields, fields, 1 + index->n_fields * sizeof(dict_field_t));

    /* Fix field->col pointers with the mapping created. */
    for (size_t i = 0; i < index->n_fields; i++) {
      index->get_field(i)->col = target_table->get_col(mapping[i]);
    }
  }
  ut::delete_arr(mapping);

  /* Set initial/current/total_col_count for table */
  target_table->initial_col_count = m_initial_column_count;
  target_table->current_col_count = m_current_column_count;
  target_table->total_col_count = m_total_column_count;

  /* Take a temp heap and add columns */
  mem_heap_t *heap = mem_heap_create(1000, UT_LOCATION_HERE);
  for (size_t i = 0; i < m_n_cols; i++) {
    dict_col_t *cfg_col = &m_cols[i];
    ut_ad(cfg_col != nullptr);

    if (cfg_col->is_instant_dropped()) {
      uint8_t v_added = cfg_col->is_instant_added()
                            ? cfg_col->get_version_added()
                            : UINT8_UNDEFINED;
      uint8_t v_dropped = cfg_col->get_version_dropped();
      uint32_t phy_pos = cfg_col->get_phy_pos();
      std::string col_name = (char *)m_col_names[i];

      dict_mem_table_add_col(m_table, heap, col_name.c_str(), cfg_col->mtype,
                             cfg_col->prtype, cfg_col->len, false, phy_pos,
                             v_added, v_dropped);
    }
  }
  mem_heap_free(heap);

  for (size_t i = 0; i < m_n_cols; i++) {
    dict_col_t *cfg_col = &m_cols[i];
    ut_ad(cfg_col != nullptr);

    if (cfg_col->is_instant_dropped()) {
      std::string col_name = (char *)m_col_names[i];
      /* Add this field into clustered index fields */
      dict_col_t *col = m_table->get_col_by_name(col_name.c_str());
      ut_ad(col->mtype != DATA_SYS);
      /* Physical position must have already been set */
      ut_ad(col->get_phy_pos() != UINT32_UNDEFINED);

      dict_index_add_col(index, m_table, col, 0, true);

      index->n_total_fields++;
    }
  }

  /* index->fields_array doesn't take space in index->heap. It will be updated
  in the caller. */

  /* Update the size change in dict_sys */
  size_t new_heap_size =
      mem_heap_get_size(target_table->heap) + mem_heap_get_size(index->heap);
  if (new_heap_size > old_heap_size) {
    mutex_enter(&dict_sys->mutex);
    dict_sys->size += new_heap_size - old_heap_size;
    mutex_exit(&dict_sys->mutex);
  }

  return DB_SUCCESS;
}

/** Set the instant ADD/DROP COLUMN information to the table.
@return DB_SUCCESS if successful, or error code */
dberr_t row_import::set_instant_info_v2(THD *thd, const dd::Table *dd_table)
    UNIV_NOTHROW {
  dberr_t err = DB_SUCCESS;

  bool src_has_row_versions = has_row_versions();
  bool dst_has_row_versions = m_table->has_row_versions();

  /* None of the table has INSTANT columns. Return success. */
  if (!src_has_row_versions && !dst_has_row_versions) {
    return (DB_SUCCESS);
  }

  /* Only target table has INSTANT columns, ERROR */
  /* It must have already been checked in match_table_columns */
  ut_ad(!(!src_has_row_versions && dst_has_row_versions));
  if (!src_has_row_versions && dst_has_row_versions) {
    ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
            "The .cfg file indicates no INSTANT column in the source table"
            " whereas the metadata in data dictionary says there are instant"
            " columns in the target table");

    return (DB_ERROR);
  }

  /* Only source table has INSTANT columns. */
  if (src_has_row_versions && !dst_has_row_versions) {
    /* Update INSTANT metadata in target table. */
    return (adjust_instant_metadata_in_taregt_table(thd, dd_table));
  }

  /* Both the tables have INSTANT columns. */
  if (src_has_row_versions && dst_has_row_versions) {
    /* INSTANT metadata must match. */
    return (match_instant_metadata_in_target_table(thd));
  }

  return (err);
}

dberr_t row_import::adjust_instant_metadata_in_taregt_table(
    THD *thd, const dd::Table *dd_table) {
  dberr_t err = DB_SUCCESS;

  /* It must have already been checked in match_table_columns */
  ut_ad(m_table->get_n_user_cols() == m_current_column_count);
  if (m_table->get_n_user_cols() != m_current_column_count) {
    ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
            "Source table has INSTANT columns. Target table column count"
            " didn't match with the configuration file.");
    return (DB_ERROR);
  }

  bool has_instant_drop_cols = m_total_column_count > m_current_column_count;
  if (has_instant_drop_cols) {
    /* Add dropped columns to target table definition. */
    add_instant_dropped_columns(m_table);
  }

  size_t old_size = mem_heap_get_size(m_table->heap);

  for (uint32_t i = 0; i < m_n_cols; i++) {
    dict_col_t *cfg_col = &m_cols[i];
    ut_ad(cfg_col != nullptr);

    std::string col_name = (char *)m_col_names[i];

    /* Search for this column in target table */
    dict_col_t *target_col = m_table->get_col_by_name(col_name.c_str());

    /* Normal column */
    if (!cfg_col->is_instant_added() && !cfg_col->is_instant_dropped()) {
      ut_ad(target_col != nullptr);
      ut_ad(!target_col->is_instant_added() &&
            !target_col->is_instant_dropped());
      ut_ad(cfg_col->instant_default == nullptr);

      /* We need to adjust phy_pos for column here */
      target_col->set_phy_pos(cfg_col->get_phy_pos());
      continue;
    }

    /* INSTANT DROP column */
    if (cfg_col->is_instant_dropped()) {
      ut_ad(dict_col_t::is_instant_dropped_name(col_name));

      /* This columns must have already been added to table cache in
      add_instant_dropped_columns() */
      ut_ad(target_col != nullptr);
      ut_ad(target_col->get_phy_pos() == cfg_col->get_phy_pos());
      ut_ad(target_col->is_version_added_match(cfg_col));
      ut_ad(target_col->is_instant_dropped());
      ut_ad(target_col->is_version_dropped_match(cfg_col));

      /* This column must have already been added to DD::Columns while
      reading columns data from CFG file in row_import_read_columns(). */
      ut_ad(nullptr != dd_find_column(dd_table, col_name.c_str()));

      continue;
    }

    /* INSTANT ADD column */
    if (cfg_col->is_instant_added()) {
      ut_ad(!cfg_col->is_instant_dropped());
      /* This must be present in target. */
      ut_ad(target_col != nullptr);
      if (target_col == nullptr) {
        ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
                "The column %s isn't found in target table.", col_name.c_str());
        err = DB_ERROR;
        break;
      }

      /* Update version_added/phy_pos for column. */
      target_col->set_version_added(cfg_col->get_version_added());
      target_col->set_phy_pos(cfg_col->get_phy_pos());

      /* Set default value from .cfg file. */
      ut_ad(cfg_col->instant_default != nullptr);
      target_col->set_default(cfg_col->instant_default->value,
                              cfg_col->instant_default->len, m_table->heap);
    }

    /* Note: these info has to be updated in DD as well in
    dd_import_instant_add_columns(). */
  }

  size_t new_size = mem_heap_get_size(m_table->heap);
  if (new_size > old_size) {
    mutex_enter(&dict_sys->mutex);
    dict_sys->size += new_size - old_size;
    mutex_exit(&dict_sys->mutex);
  }

  if (err != DB_SUCCESS) {
    return (err);
  }

  m_table->initial_col_count = m_initial_column_count;
  m_table->current_col_count = m_current_column_count;
  m_table->total_col_count = m_total_column_count;
  m_table->current_row_version = m_current_row_version;

  ut_ad(m_table->has_row_versions());
  dict_index_t &first_index = *m_table->first_index();
  first_index.row_versions = true;
  first_index.rec_cache.offsets = nullptr;
  first_index.rec_cache.nullable_cols = 0;
  /* Recreate fields array for clustered index */
  first_index.create_fields_array();
  first_index.create_nullables(m_table->current_row_version);

  /* FIXME: Force to discard the table, in case of any rollback later. */
  //    m_table->discard_after_ddl = true;

  return (err);
}

/** Set the instant ADD COLUMN information to the table.
@return DB_SUCCESS if all instant columns are trailing columns, or error code */
dberr_t row_import::set_instant_info(THD *thd,
                                     const dd::Table *dd_table) UNIV_NOTHROW {
  if (m_version >= IB_EXPORT_CFG_VERSION_V7) {
    return set_instant_info_v2(thd, dd_table);
  }

  dberr_t error = DB_SUCCESS;
  dict_col_t *col = m_table->cols;
  uint16_t instants = 0;
  uint64_t old_size;
  uint64_t new_size;

  /* If .cfg file indicates no INSTANT column in source table. */
  if (m_n_instant_cols == 0) {
    /* But if target table has INSTANT columns, report error. */
    if (m_table->has_instant_cols()) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "The .cfg file indicates no INSTANT column in the source table"
              " whereas the metadata in data dictionary says there are instant"
              " columns in the target table");

      return (DB_ERROR);
    }

    /* All good. Return success. */
    m_table->set_instant_cols(m_table->get_n_user_cols());
    ut_ad(!m_table->has_instant_cols());
    return (DB_SUCCESS);
  }

  /* Do not allow IMPORT if target table also have INSTANT columns. As after
  the implementation of row versions in table
  - IMPORT allowed with INSTANT columns in target table iff metadata matches
    exactly with source table.
  - The source table is from earlier release so metadata can't match. */
  if (m_table->has_row_versions()) {
    ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
            "Target table has INSTANT columns but the .cfg file is from earlier"
            " release with INSTANT column in the source table. Instant metadata"
            " can't match. Please create target table with no INSTANT column"
            " and try IMPORT.");
    return (DB_ERROR);
  }

  old_size = mem_heap_get_size(m_table->heap);

  for (ulint i = 0; i < m_table->get_n_user_cols(); ++i, ++col) {
    const char *col_name;
    ulint cfg_col_index;

    col_name = m_table->get_col_name(dict_col_get_no(col));

    cfg_col_index = find_col(col_name);
    ut_ad(cfg_col_index != ULINT_UNDEFINED);

    const dict_col_t *cfg_col = &m_cols[cfg_col_index];

    if (cfg_col->instant_default == nullptr) {
      if (instants > 0) {
        ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
                "Instant columns read from meta-data"
                " file mismatch, because there are"
                " some columns which were not instantly"
                " added after columns which were"
                " instantly added");

        error = DB_ERROR;
        break;
      }

      continue;
    }

    ++instants;

    /* If the data dictionary does not contain a default for this column set
    the value from .cfg file. */
    if (col->instant_default == nullptr) {
      col->set_default(cfg_col->instant_default->value,
                       cfg_col->instant_default->len, m_table->heap);
    }
    /* If the instant_default field is equal in the .cfg and DD just continue,
    through the loop. Otherwise there's a collision, return an error here. */
    else if (*col->instant_default != *cfg_col->instant_default) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "The metadata in the data dictionary and the .cfg file contain"
              " different default values for column %s!",
              col_name);

      error = DB_ERROR;
      break;
    }
  }

  new_size = mem_heap_get_size(m_table->heap);
  if (new_size > old_size) {
    dict_sys_mutex_enter();
    dict_sys->size += new_size - old_size;
    dict_sys_mutex_exit();
  }

  if (error == DB_SUCCESS && instants != m_n_instant_cols) {
    ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
            "Number of instant columns don't match, table has"
            " %lu instant columns record in meta-data file but"
            " there are %lu columns with default value",
            static_cast<ulong>(m_n_instant_cols), static_cast<ulong>(instants));

    error = DB_ERROR;
  }

  if (error != DB_SUCCESS) {
    return (error);
  }

  m_table->set_instant_cols(m_table->get_n_user_cols() - m_n_instant_cols);
  ut_ad(m_table->has_instant_cols());
  m_table->set_upgraded_instant();

  dict_index_t &first_index = *m_table->first_index();
  first_index.instant_cols = true;
  first_index.rec_cache.offsets = nullptr;
  first_index.rec_cache.nullable_cols = 0;
  first_index.set_instant_nullable(m_n_instant_nullable);
  /* FIXME: Force to discard the table, in case of any rollback later. */
  //    m_table->discard_after_ddl = true;

  return (DB_SUCCESS);
}

/**
Purge delete marked records.
@return DB_SUCCESS or error code. */
dberr_t IndexPurge::garbage_collect() UNIV_NOTHROW {
  dberr_t err;
  auto comp = dict_table_is_comp(m_index->table);

  /* Open the persistent cursor and start the mini-transaction. */

  open();
  import_ctx_t import_ctx = {false};
  m_pcur.import_ctx = &import_ctx;

  while ((err = next()) == DB_SUCCESS) {
    rec_t *rec = m_pcur.get_rec();
    auto deleted = rec_get_deleted_flag(rec, comp);

    if (!deleted) {
      ++m_n_rows;
    } else {
      purge();
    }
  }

  /* Close the persistent cursor and commit the mini-transaction. */

  close();
  if (m_pcur.import_ctx->is_error == true) {
    m_pcur.import_ctx = nullptr;
    return DB_TABLE_CORRUPT;
  }

  m_pcur.import_ctx = nullptr;

  return (err == DB_END_OF_INDEX ? DB_SUCCESS : err);
}

/**
Begin import, position the cursor on the first record. */
void IndexPurge::open() UNIV_NOTHROW {
  mtr_start(&m_mtr);
  mtr_set_log_mode(&m_mtr, MTR_LOG_NO_REDO);

  m_pcur.open_at_side(true, m_index, BTR_MODIFY_LEAF, true, 0, &m_mtr);
}

/**
Close the persistent curosr and commit the mini-transaction. */
void IndexPurge::close() UNIV_NOTHROW {
  m_pcur.close();
  mtr_commit(&m_mtr);
}

/**
Position the cursor on the next record.
@return DB_SUCCESS or error code */
dberr_t IndexPurge::next() UNIV_NOTHROW {
  m_pcur.move_to_next_on_page();

  /* When switching pages, commit the mini-transaction
  in order to release the latch on the old page. */

  if (!m_pcur.is_after_last_on_page()) {
    return (DB_SUCCESS);
  } else if (trx_is_interrupted(m_trx)) {
    /* Check after every page because the check
    is expensive. */
    return (DB_INTERRUPTED);
  }

  m_pcur.store_position(&m_mtr);

  mtr_commit(&m_mtr);

  mtr_start(&m_mtr);
  mtr_set_log_mode(&m_mtr, MTR_LOG_NO_REDO);

  m_pcur.restore_position(BTR_MODIFY_LEAF, &m_mtr, UT_LOCATION_HERE);

  if (m_pcur.move_to_next_user_rec(&m_mtr) != DB_SUCCESS) {
    return (DB_END_OF_INDEX);
  }

  return (DB_SUCCESS);
}

/**
Store the persistent cursor position and reopen the
B-tree cursor in BTR_MODIFY_TREE mode, because the
tree structure may be changed during a pessimistic delete. */
void IndexPurge::purge_pessimistic_delete() UNIV_NOTHROW {
  dberr_t err;

  m_pcur.restore_position(BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE, &m_mtr,
                          UT_LOCATION_HERE);

  ut_ad(rec_get_deleted_flag(m_pcur.get_rec(),
                             dict_table_is_comp(m_index->table)));

  btr_cur_pessimistic_delete(&err, false, m_pcur.get_btr_cur(), 0, false, 0, 0,
                             0, &m_mtr, &m_pcur, nullptr);

  ut_a(err == DB_SUCCESS);

  /* Reopen the B-tree cursor in BTR_MODIFY_LEAF mode */
  mtr_commit(&m_mtr);
}

/**
Purge delete-marked records. */
void IndexPurge::purge() UNIV_NOTHROW {
  m_pcur.store_position(&m_mtr);

  purge_pessimistic_delete();

  mtr_start(&m_mtr);
  mtr_set_log_mode(&m_mtr, MTR_LOG_NO_REDO);

  m_pcur.restore_position(BTR_MODIFY_LEAF, &m_mtr, UT_LOCATION_HERE);
}

/** Constructor
@param cfg config of table being imported.
@param trx transaction covering the import */
PageConverter::PageConverter(row_import *cfg, trx_t *trx)
    : AbstractCallback(trx),
      m_cfg(cfg),
      m_page_zip_ptr(nullptr),
      m_heap(nullptr) UNIV_NOTHROW {
  m_index = m_cfg->m_indexes;

  m_current_lsn = log_sys->flushed_to_disk_lsn;
  ut_a(m_current_lsn > 0);

  m_offsets = m_offsets_;
  rec_offs_init(m_offsets_);

  m_cluster_index = m_cfg->m_table->first_index();
}

/** Adjust the BLOB reference for a single column that is externally stored
@param rec record to update
@param offsets column offsets for the record
@param i column ordinal value
@return DB_SUCCESS or error code */
dberr_t PageConverter::adjust_cluster_index_blob_column(rec_t *rec,
                                                        const ulint *offsets,
                                                        ulint i) UNIV_NOTHROW {
  ulint len;
  byte *field;

  field = rec_get_nth_field(m_cluster_index, rec, offsets, i, &len);

  DBUG_EXECUTE_IF("ib_import_trigger_corruption_2",
                  len = BTR_EXTERN_FIELD_REF_SIZE - 1;);

  if (len < BTR_EXTERN_FIELD_REF_SIZE) {
    ib_errf(m_trx->mysql_thd, IB_LOG_LEVEL_ERROR, ER_INNODB_INDEX_CORRUPT,
            "Externally stored column(%lu) has a reference"
            " length of %lu in the cluster index %s",
            (ulong)i, (ulong)len, m_cluster_index->name());

    return (DB_CORRUPTION);
  }

  field += lob::BTR_EXTERN_SPACE_ID - BTR_EXTERN_FIELD_REF_SIZE + len;

  if (is_compressed_table()) {
    mach_write_to_4(field, get_space_id());

    ut_ad(m_index->m_srv_index != nullptr);
    ut_ad(m_index->m_srv_index->is_clustered());

    page_zip_write_blob_ptr(m_page_zip_ptr, rec, m_index->m_srv_index, offsets,
                            i, nullptr);

  } else {
    mlog_write_ulint(field, get_space_id(), MLOG_4BYTES, nullptr);
  }

  return (DB_SUCCESS);
}

/** Adjusts the BLOB reference in the clustered index row for all externally
stored columns.
@param rec record to update
@param offsets column offsets for the record
@return DB_SUCCESS or error code */
dberr_t PageConverter::adjust_cluster_index_blob_columns(
    rec_t *rec, const ulint *offsets) UNIV_NOTHROW {
  ut_ad(rec_offs_any_extern(offsets));

  /* Adjust the space_id in the BLOB pointers. */

  for (ulint i = 0; i < rec_offs_n_fields(offsets); ++i) {
    /* Only if the column is stored "externally". */

    if (rec_offs_nth_extern(m_cluster_index, offsets, i)) {
      dberr_t err;

      err = adjust_cluster_index_blob_column(rec, offsets, i);

      if (err != DB_SUCCESS) {
        return (err);
      }
    }
  }

  return (DB_SUCCESS);
}

/** In the clustered index, adjust BLOB pointers as needed. Also update the
BLOB reference, write the new space id.
@param rec record to update
@param offsets column offsets for the record
@return DB_SUCCESS or error code */
dberr_t PageConverter::adjust_cluster_index_blob_ref(
    rec_t *rec, const ulint *offsets) UNIV_NOTHROW {
  if (rec_offs_any_extern(offsets)) {
    dberr_t err;

    err = adjust_cluster_index_blob_columns(rec, offsets);

    if (err != DB_SUCCESS) {
      return (err);
    }
  }

  return (DB_SUCCESS);
}

bool PageConverter::purge() UNIV_NOTHROW {
  const dict_index_t *index = m_index->m_srv_index;

  /* We can't have a page that is empty and not root. */
  if (m_rec_iter.remove(index, m_offsets)) {
    ++m_index->m_stats.m_n_purged;

    return (true);
  } else {
    ++m_index->m_stats.m_n_purge_failed;
  }

  return (false);
}

dberr_t PageConverter::adjust_cluster_record(
    const dict_index_t *index, rec_t *rec, const ulint *offsets) UNIV_NOTHROW {
  dberr_t err;

  ut_ad(index->is_clustered());

  if ((err = adjust_cluster_index_blob_ref(rec, offsets)) == DB_SUCCESS) {
    /* Reset DB_TRX_ID and DB_ROLL_PTR.  Normally, these fields
    are only written in conjunction with other changes to the
    record. */

    row_upd_rec_sys_fields(rec, m_page_zip_ptr, index, m_offsets, m_trx, 0);
  }

  return (err);
}

/** Update the BLOB references and write UNDO log entries for
rows that can't be purged optimistically.
@param block block to update
@retval DB_SUCCESS or error code */
dberr_t PageConverter::update_records(buf_block_t *block) UNIV_NOTHROW {
  auto comp = dict_table_is_comp(m_cfg->m_table);
  bool clust_index = (m_index->m_srv_index == m_cluster_index) ||
                     dict_index_is_sdi(m_index->m_srv_index);

  /* This will also position the cursor on the first user record. */

  m_rec_iter.open(block);

  while (!m_rec_iter.end()) {
    rec_t *rec = m_rec_iter.current();

    auto has_version =
        (comp ? rec_new_is_versioned(rec) : rec_old_is_versioned(rec));

    /* CFG file is required to process records having version */

    if (m_cfg->m_missing && has_version) {
      return (DB_SCHEMA_MISMATCH);
    }

    auto deleted = rec_get_deleted_flag(rec, comp);
    /* For the clustered index we have to adjust the BLOB
    reference and the system fields irrespective of the
    delete marked flag. The adjustment of delete marked
    cluster records is required for purge to work later. */

    if (deleted || clust_index) {
      m_offsets = rec_get_offsets(rec, m_index->m_srv_index, m_offsets,
                                  ULINT_UNDEFINED, UT_LOCATION_HERE, &m_heap);
    }

    if (clust_index) {
      dberr_t err = adjust_cluster_record(m_index->m_srv_index, rec, m_offsets);

      if (err != DB_SUCCESS) {
        return (err);
      }
    }

    /* If it is a delete marked record then try an
    optimistic delete. */

    if (deleted) {
      /* A successful purge will move the cursor to the
      next record. */

      if (!purge()) {
        m_rec_iter.next();
      }

      ++m_index->m_stats.m_n_deleted;
    } else {
      ++m_index->m_stats.m_n_rows;
      m_rec_iter.next();
    }
  }

  return (DB_SUCCESS);
}

/** Update the space, index id, trx id.
@return DB_SUCCESS or error code */
dberr_t PageConverter::update_index_page(buf_block_t *block) UNIV_NOTHROW {
  space_index_t id;
  buf_frame_t *page = block->frame;

  if (is_free(block->page.id.page_no())) {
    return (DB_SUCCESS);
  } else if ((id = btr_page_get_index_id(page)) != m_index->m_id) {
    row_index_t *index = find_index(id);

    if (index == nullptr) {
      m_index = nullptr;
      return (DB_CORRUPTION);
    }

    /* Update current index */
    m_index = index;
  }

  /* If the .cfg file is missing and there is an index mismatch
  then ignore the error. */
  if (m_cfg->m_missing &&
      (m_index == nullptr || m_index->m_srv_index == nullptr)) {
    return (DB_SUCCESS);
  }

#ifdef UNIV_ZIP_DEBUG
  ut_a(!is_compressed_table() ||
       page_zip_validate(m_page_zip_ptr, page, m_index->m_srv_index));
#endif /* UNIV_ZIP_DEBUG */

  /* This has to be written to uncompressed index header. Set it to
  the current index id. */
  btr_page_set_index_id(page, m_page_zip_ptr, m_index->m_srv_index->id,
                        nullptr);

  page_set_max_trx_id(block, m_page_zip_ptr, m_trx->id, nullptr);

  if (page_is_empty(block->frame)) {
    /* Only a root page can be empty. */
    if (!is_root_page(block->frame)) {
      // TODO: We should relax this and skip secondary
      // indexes. Mark them as corrupt because they can
      // always be rebuilt.
      return (DB_CORRUPTION);
    }

    return (DB_SUCCESS);
  }

  if (!page_is_leaf(block->frame)) {
    return (DB_SUCCESS);
  }

  return (update_records(block));
}

/** Validate the space flags and update tablespace header page.
@param block block read from file, not from the buffer pool.
@retval DB_SUCCESS or error code */
dberr_t PageConverter::update_header(buf_block_t *block) UNIV_NOTHROW {
  /* Check for valid header */
  switch (fsp_header_get_space_id(get_frame(block))) {
    case 0:
      return (DB_CORRUPTION);
    case SPACE_UNKNOWN:
      ib::warn(ER_IB_MSG_941) << "Space id check in the header failed: ignored";
  }

  uint32_t space_flags = fsp_header_get_flags(get_frame(block));

  if (!fsp_flags_is_valid(space_flags)) {
    ib::error(ER_IB_MSG_942) << "Unsupported tablespace format " << space_flags;

    return (DB_UNSUPPORTED);
  }

  /* Write space_id to the tablespace header, page 0. */
  fsp_header_set_field(get_frame(block), FSP_SPACE_ID, get_space_id());

  /* This is on every page in the tablespace. */
  mach_write_to_4(get_frame(block) + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID,
                  get_space_id());

  return (DB_SUCCESS);
}

/** Update the page, set the space id, max trx id and index id.
@param block block read from file
@param page_type type of the page
@retval DB_SUCCESS or error code */
dberr_t PageConverter::update_page(buf_block_t *block,
                                   ulint &page_type) UNIV_NOTHROW {
  dberr_t err = DB_SUCCESS;

  switch (page_type = fil_page_get_type(get_frame(block))) {
    case FIL_PAGE_TYPE_FSP_HDR:
      /* Work directly on the uncompressed page headers. */
      ut_a(block->page.id.page_no() == 0);
      return (update_header(block));

    case FIL_PAGE_INDEX:
    case FIL_PAGE_RTREE:
    case FIL_PAGE_SDI:
      /* We need to decompress the contents into block->frame
      before we can do any thing with Btree pages. */

      if (is_compressed_table() && !buf_zip_decompress(block, true)) {
        return (DB_CORRUPTION);
      }

      /* This is on every page in the tablespace. */
      mach_write_to_4(get_frame(block) + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID,
                      get_space_id());

      /* Only update the Btree nodes. */
      return (update_index_page(block));

    case FIL_PAGE_TYPE_SYS:
      /* This is page 0 in the system tablespace. */
      return (DB_CORRUPTION);

    case FIL_PAGE_TYPE_LOB_FIRST: {
      lob::first_page_t first_page(block);
      first_page.import(m_trx->id);
      first_page.set_space_id_no_redo(get_space_id());
      return (err);
    }

    case FIL_PAGE_TYPE_LOB_INDEX: {
      lob::node_page_t node_page(block);
      node_page.import(m_trx->id);
      node_page.set_space_id_no_redo(get_space_id());
      return (err);
    }

    case FIL_PAGE_TYPE_LOB_DATA: {
      lob::data_page_t data_page(block);
      data_page.set_trx_id_no_redo(m_trx->id);
      data_page.set_space_id_no_redo(get_space_id());
      return (err);
    }

    case FIL_PAGE_TYPE_ZLOB_FIRST: {
      dict_index_t *index = const_cast<dict_index_t *>(m_index->m_srv_index);
      lob::z_first_page_t first_page(block, nullptr, index);
      first_page.import(m_trx->id);
      byte *ptr = get_frame(block) + FIL_PAGE_SPACE_ID;
      mach_write_to_4(ptr, get_space_id());
      return (err);
    }

    case FIL_PAGE_TYPE_ZLOB_DATA: {
      lob::z_data_page_t dpage(block);
      dpage.set_trx_id_no_redo(m_trx->id);
      byte *ptr = get_frame(block) + FIL_PAGE_SPACE_ID;
      mach_write_to_4(ptr, get_space_id());
      return (err);
    }

    case FIL_PAGE_TYPE_ZLOB_INDEX: {
      lob::z_index_page_t ipage(
          block, const_cast<dict_index_t *>(m_index->m_srv_index));
      ipage.import(m_trx->id);
      byte *ptr = get_frame(block) + FIL_PAGE_SPACE_ID;
      mach_write_to_4(ptr, get_space_id());
      return (err);
    }

    case FIL_PAGE_TYPE_ZLOB_FRAG: {
      byte *ptr = get_frame(block) + FIL_PAGE_SPACE_ID;
      mach_write_to_4(ptr, get_space_id());
      return (err);
    }

    case FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY: {
      byte *ptr = get_frame(block) + FIL_PAGE_SPACE_ID;
      mach_write_to_4(ptr, get_space_id());
      return (err);
    }

    case FIL_PAGE_TYPE_XDES:
      err = set_current_xdes(block->page.id.page_no(), get_frame(block));
      [[fallthrough]];
    case FIL_PAGE_INODE:
    case FIL_PAGE_TYPE_TRX_SYS:
    case FIL_PAGE_IBUF_FREE_LIST:
    case FIL_PAGE_TYPE_ALLOCATED:
    case FIL_PAGE_IBUF_BITMAP:
    case FIL_PAGE_TYPE_BLOB:
    case FIL_PAGE_TYPE_ZBLOB:
    case FIL_PAGE_TYPE_ZBLOB2:
    case FIL_PAGE_SDI_BLOB:
    case FIL_PAGE_SDI_ZBLOB:
    case FIL_PAGE_TYPE_RSEG_ARRAY:

      /* Work directly on the uncompressed page headers. */
      /* This is on every page in the tablespace. */
      mach_write_to_4(get_frame(block) + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID,
                      get_space_id());

      return (err);
  }

  ib::warn(ER_IB_MSG_943) << "Unknown page type (" << page_type << ")";

  return (DB_CORRUPTION);
}

/** Validate the page, check for corruption.
@param  offset  physical offset within file.
@param  block   page read from file.
@return 0 on success, 1 if all zero, 2 if corrupted */
PageConverter::import_page_status_t PageConverter::validate(
    os_offset_t offset, buf_block_t *block) UNIV_NOTHROW {
  buf_frame_t *page = get_frame(block);

  /* Check that the page number corresponds to the offset in
  the file. Flag as corrupt if it doesn't. Disable the check
  for LSN in buf_page_is_corrupted() */

  BlockReporter reporter(false, page, get_page_size(),
                         fsp_is_checksum_disabled(block->page.id.space()));

  if (reporter.is_corrupted() ||
      (page_get_page_no(page) != offset / m_page_size.physical() &&
       page_get_page_no(page) != 0)) {
    return (IMPORT_PAGE_STATUS_CORRUPTED);

  } else if (offset > 0 && page_get_page_no(page) == 0) {
    /* The page is all zero: do nothing. We already checked
    for all NULs in buf_page_is_corrupted() */
    return (IMPORT_PAGE_STATUS_ALL_ZERO);
  }

  return (IMPORT_PAGE_STATUS_OK);
}

/** Called for every page in the tablespace. If the page was not
updated then its state must be set to BUF_PAGE_NOT_USED.
@param offset physical offset within the file
@param block block read from file, note it is not from the buffer pool
@retval DB_SUCCESS or error code. */
dberr_t PageConverter::operator()(os_offset_t offset,
                                  buf_block_t *block) UNIV_NOTHROW {
  ulint page_type;
  dberr_t err = DB_SUCCESS;

  if ((err = periodic_check()) != DB_SUCCESS) {
    return (err);
  }

  if (is_compressed_table()) {
    m_page_zip_ptr = &block->page.zip;
  } else {
    ut_ad(m_page_zip_ptr == nullptr);
  }

  switch (validate(offset, block)) {
    case IMPORT_PAGE_STATUS_OK:

      /* We have to decompress the compressed pages before
      we can work on them */

      if ((err = update_page(block, page_type)) != DB_SUCCESS) {
        return (err);
      }

      /* Note: For compressed pages this function will write to the
      zip descriptor and for uncompressed pages it will write to
      page (ie. the block->frame). Therefore the caller should write
      out the descriptor contents and not block->frame for compressed
      pages. */

      if (!is_compressed_table() || fil_page_type_is_index(page_type)) {
        buf_flush_init_for_writing(
            !is_compressed_table() ? block : nullptr,
            !is_compressed_table() ? block->frame : block->page.zip.data,
            !is_compressed_table() ? nullptr : m_page_zip_ptr, m_current_lsn,
            fsp_is_checksum_disabled(block->page.id.space()),
            true /* skip_lsn_check */);
      } else {
        /* Calculate and update the checksum of non-btree
        pages for compressed tables explicitly here. */

        buf_flush_update_zip_checksum(get_frame(block),
                                      get_page_size().physical(), m_current_lsn,
                                      true /* skip_lsn_check */);
      }

      break;

    case IMPORT_PAGE_STATUS_ALL_ZERO:
      /* The page is all zero: leave it as is. */
      break;

    case IMPORT_PAGE_STATUS_CORRUPTED:

      ib::warn(ER_IB_MSG_944)
          << "Page " << (offset / m_page_size.physical()) << " at offset "
          << offset << " looks corrupted in file " << m_filepath;

      return (DB_CORRUPTION);
  }

  return (err);
}

/** Clean up after import tablespace failure, this function will acquire
 the dictionary latches on behalf of the transaction if the transaction
 hasn't already acquired them. */
static void row_import_discard_changes(
    row_prebuilt_t *prebuilt, /*!< in/out: prebuilt from handler */
    trx_t *trx,               /*!< in/out: transaction for import */
    dberr_t err)              /*!< in: error code */
{
  dict_table_t *table = prebuilt->table;

  ut_a(err != DB_SUCCESS);

  prebuilt->trx->error_index = nullptr;

  ib::info(ER_IB_MSG_945) << "Failed to import tablespace of table '"
                          << prebuilt->table->name.m_name
                          << (err == DB_UNSUPPORTED
                                  ? "': the CFG file version is "
                                  : "': ")
                          << ut_strerr(err);

  if (trx->dict_operation_lock_mode != RW_X_LATCH) {
    ut_a(trx->dict_operation_lock_mode == 0);
    row_mysql_lock_data_dictionary(trx, UT_LOCATION_HERE);
  }

  ut_a(trx->dict_operation_lock_mode == RW_X_LATCH);

  /* Since we update the index root page numbers on disk after
  we've done a successful import. The table will not be loadable.
  However, we need to ensure that the in memory root page numbers
  are reset to "NULL". We assume these indexes were not added to AHI, otherwise
  the btr_search_drop_page_hash_index() will fail for these indexes. */

  for (auto index : table->indexes) {
    index->page = FIL_NULL;
    index->space = FIL_NULL;
  }

  table->ibd_file_missing = true;

  err = fil_close_tablespace(table->space);
  ut_a(err == DB_SUCCESS || err == DB_TABLESPACE_NOT_FOUND);
}

/** Clean up after import tablespace. */
[[nodiscard]] static dberr_t row_import_cleanup(
    row_prebuilt_t *prebuilt, /*!< in/out: prebuilt from handler */
    trx_t *trx,               /*!< in/out: transaction for import */
    dberr_t err)              /*!< in: error code */
{
  ut_a(prebuilt->trx != trx);

  if (err != DB_SUCCESS) {
    row_import_discard_changes(prebuilt, trx, err);
  }

  ut_a(trx->dict_operation_lock_mode == RW_X_LATCH);

  DBUG_EXECUTE_IF("ib_import_before_commit_crash", DBUG_SUICIDE(););

  trx_commit_for_mysql(trx);

  row_mysql_unlock_data_dictionary(trx);

  trx_free_for_mysql(trx);

  prebuilt->trx->op_info = "";

  DBUG_EXECUTE_IF("ib_import_before_checkpoint_crash", DBUG_SUICIDE(););

  log_make_latest_checkpoint();

  return (err);
}

/** Report error during tablespace import. */
[[nodiscard]] static dberr_t row_import_error(
    row_prebuilt_t *prebuilt, /*!< in/out: prebuilt from handler */
    trx_t *trx,               /*!< in/out: transaction for import */
    dberr_t err)              /*!< in: error code */
{
  if (!trx_is_interrupted(trx)) {
    char table_name[MAX_FULL_NAME_LEN + 1];

    innobase_format_name(table_name, sizeof(table_name),
                         prebuilt->table->name.m_name);

    ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_WARN, ER_INNODB_IMPORT_ERROR,
                table_name, (ulong)err, ut_strerr(err));
  }

  return (row_import_cleanup(prebuilt, trx, err));
}

/** Adjust the root page index node and leaf node segment headers, update
 with the new space id. For all the table's secondary indexes.
 @return error code */
[[nodiscard]] static dberr_t row_import_adjust_root_pages_of_secondary_indexes(
    trx_t *trx,            /*!< in: transaction used for
                           the import */
    dict_table_t *table,   /*!< in: table the indexes
                           belong to */
    const row_import &cfg) /*!< Import context */
{
  dict_index_t *index;
  ulint n_rows_in_table;
  dberr_t err = DB_SUCCESS;

  /* Skip the clustered index. */
  index = table->first_index();

  n_rows_in_table = cfg.get_n_rows(index->name);

  /* Adjust the root pages of the secondary indexes only. */
  while ((index = index->next()) != nullptr) {
    ut_a(!index->is_clustered());

    if (!index->is_corrupted() && index->space != FIL_NULL &&
        index->page != FIL_NULL) {
      /* Update the Btree segment headers for index node and
      leaf nodes in the root page. Set the new space id. */

      err = btr_root_adjust_on_import(index);
    } else {
      ib::warn(ER_IB_MSG_946) << "Skip adjustment of root pages for"
                                 " index "
                              << index->name << ".";

      err = DB_CORRUPTION;
    }

    if (err != DB_SUCCESS) {
      if (index->type & DICT_CLUSTERED) {
        break;
      }

      ib_errf(trx->mysql_thd, IB_LOG_LEVEL_WARN, ER_INNODB_INDEX_CORRUPT,
              "Index %s not found or corrupt,"
              " you should recreate this index.",
              index->name());

      /* Do not bail out, so that the data
      can be recovered. */

      err = DB_SUCCESS;
      dict_set_corrupted(index);
      continue;
    }

    /* If we failed to purge any records in the index then
    do it the hard way.

    TODO: We can do this in the first pass by generating UNDO log
    records for the failed rows. */

    if (!cfg.requires_purge(index->name)) {
      continue;
    }

    IndexPurge purge(trx, index);

    trx->op_info = "secondary: purge delete marked records";

    err = purge.garbage_collect();

    trx->op_info = "";

    if (err != DB_SUCCESS) {
      break;
    } else if (purge.get_n_rows() != n_rows_in_table) {
      ib_errf(trx->mysql_thd, IB_LOG_LEVEL_WARN, ER_INNODB_INDEX_CORRUPT,
              "Index %s contains %lu entries,"
              " should be %lu, you should recreate"
              " this index.",
              index->name(), (ulong)purge.get_n_rows(), (ulong)n_rows_in_table);

      dict_set_corrupted(index);

      /* Do not bail out, so that the data
      can be recovered. */

      err = DB_SUCCESS;
    }
  }

  return (err);
}

/** Ensure that dict_sys->row_id exceeds SELECT MAX(DB_ROW_ID).
 @return error code */
[[nodiscard]] static dberr_t row_import_set_sys_max_row_id(
    row_prebuilt_t *prebuilt, /*!< in/out: prebuilt from
                              handler */
    dict_table_t *table)      /*!< in: table to import */
{
  dberr_t err;
  const rec_t *rec;
  mtr_t mtr;
  btr_pcur_t pcur;
  row_id_t row_id = 0;
  dict_index_t *index;

  index = table->first_index();
  ut_a(index->is_clustered());

  mtr_start(&mtr);

  mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);

  pcur.open_at_side(false,  // High end
                    index, BTR_SEARCH_LEAF,
                    true,  // Init cursor
                    0,     // Leaf level
                    &mtr);

  pcur.move_to_prev_on_page();
  rec = pcur.get_rec();

  /* Check for empty table. */
  if (!page_rec_is_infimum(rec)) {
    ulint len;
    const byte *field;
    mem_heap_t *heap = nullptr;
    ulint offsets_[1 + REC_OFFS_HEADER_SIZE];
    ulint *offsets;

    rec_offs_init(offsets_);

    offsets = rec_get_offsets(rec, index, offsets_, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &heap);

    field = rec_get_nth_field(index, rec, offsets,
                              index->get_sys_col_pos(DATA_ROW_ID), &len);

    if (len == DATA_ROW_ID_LEN) {
      row_id = mach_read_from_6(field);
      err = DB_SUCCESS;
    } else {
      err = DB_CORRUPTION;
    }

    if (heap != nullptr) {
      mem_heap_free(heap);
    }
  } else {
    /* The table is empty. */
    err = DB_SUCCESS;
  }

  pcur.close();
  mtr_commit(&mtr);

  DBUG_EXECUTE_IF("ib_import_set_max_rowid_failure", err = DB_CORRUPTION;);

  if (err != DB_SUCCESS) {
    ib_errf(prebuilt->trx->mysql_thd, IB_LOG_LEVEL_WARN,
            ER_INNODB_INDEX_CORRUPT,
            "Index `%s` corruption detected, invalid DB_ROW_ID"
            " in index.",
            index->name());

    return (err);

  } else if (row_id > 0) {
    /* Update the system row id if the imported index row id is
    greater than the max system row id. */

    dict_sys_mutex_enter();

    if (row_id >= dict_sys->row_id) {
      dict_sys->row_id = row_id + 1;
      dict_hdr_flush_row_id();
    }

    dict_sys_mutex_exit();
  }

  return (DB_SUCCESS);
}

/** Read the a string from the meta data file.
 @return DB_SUCCESS or error code. */
static dberr_t row_import_cfg_read_string(
    FILE *file,    /*!< in/out: File to read from */
    byte *ptr,     /*!< out: string to read */
    ulint max_len) /*!< in: maximum length of the output
                   buffer in bytes */
{
  DBUG_EXECUTE_IF("ib_import_string_read_error", errno = EINVAL;
                  return (DB_IO_ERROR););

  ulint len = 0;

  while (!feof(file)) {
    int ch = fgetc(file);

    if (ch == EOF) {
      break;
    } else if (ch != 0) {
      if (len < max_len) {
        ptr[len++] = ch;
      } else {
        break;
      }
      /* max_len includes the NUL byte */
    } else if (len != max_len - 1) {
      break;
    } else {
      ptr[len] = 0;
      return (DB_SUCCESS);
    }
  }

  errno = EINVAL;

  return (DB_IO_ERROR);
}

/** Write the meta data (index user fields) config file.
 @return DB_SUCCESS or error code. */
[[nodiscard]] static dberr_t row_import_cfg_read_index_fields(
    FILE *file,         /*!< in: file to write to */
    THD *thd,           /*!< in/out: session */
    row_index_t *index, /*!< Index being read in */
    row_import *cfg)    /*!< in/out: meta-data read */
{
  /* v4 row will have prefix_len, fixed_len, is_ascending, name length */
  byte row[sizeof(uint32_t) * 4];
  size_t row_len = sizeof(row);
  if (cfg->m_version < IB_EXPORT_CFG_VERSION_V4) {
    /* v3 row will have prefix_len, fixed_len, name length */
    row_len = sizeof(uint32_t) * 3;
  }

  ulint n_fields = index->m_n_fields;

  index->m_fields = ut::new_arr_withkey<dict_field_t>(UT_NEW_THIS_FILE_PSI_KEY,
                                                      ut::Count{n_fields});

  /* Trigger OOM */
  DBUG_EXECUTE_IF("ib_import_OOM_4", ut::delete_arr(index->m_fields);
                  index->m_fields = nullptr;);

  if (index->m_fields == nullptr) {
    return (DB_OUT_OF_MEMORY);
  }

  dict_field_t *field = index->m_fields;

  std::uninitialized_fill_n(field, n_fields, dict_field_t());

  for (ulint i = 0; i < n_fields; ++i, ++field) {
    byte *ptr = row;

    /* Trigger EOF */
    DBUG_EXECUTE_IF("ib_import_io_read_error_1",
                    (void)fseek(file, 0L, SEEK_END););

    if (fread(row, 1, row_len, file) != row_len) {
      ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                  strerror(errno), "while reading index fields.");

      return (DB_IO_ERROR);
    }

    field->prefix_len = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    field->fixed_len = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    if (cfg->m_version >= IB_EXPORT_CFG_VERSION_V4) {
      field->is_ascending = mach_read_from_4(ptr);
      ptr += sizeof(uint32_t);
    } else {
      /* Previous to CFG version 4 the DESC key was not recorded.
      Assume the index column is ascending.
      This flag became available in v8.0. */
      field->is_ascending = true;
    }

    /* Include the NUL byte in the length. */
    ulint len = mach_read_from_4(ptr);

    byte *name =
        ut::new_arr_withkey<byte>(UT_NEW_THIS_FILE_PSI_KEY, ut::Count{len});

    /* Trigger OOM */
    DBUG_EXECUTE_IF("ib_import_OOM_5", ut::delete_arr(name); name = nullptr;);

    if (name == nullptr) {
      return (DB_OUT_OF_MEMORY);
    }

    field->name = reinterpret_cast<const char *>(name);

    dberr_t err = row_import_cfg_read_string(file, name, len);

    if (err != DB_SUCCESS) {
      ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                  strerror(errno), "while parsing table name.");

      return (err);
    }
  }

  return (DB_SUCCESS);
}

/** Read the index names and root page numbers of the indexes and set the
 values. Row format [root_page_no, len of str, str ... ]
 @return DB_SUCCESS or error code. */
[[nodiscard]] static dberr_t row_import_read_index_data(
    FILE *file,      /*!< in: File to read from */
    THD *thd,        /*!< in: session */
    row_import *cfg) /*!< in/out: meta-data read */
{
  byte *ptr;
  row_index_t *cfg_index;
  byte row[sizeof(space_index_t) + sizeof(uint32_t) * 9];

  /* FIXME: What is the max value? */
  ut_a(cfg->m_n_indexes > 0);
  ut_a(cfg->m_n_indexes < 1024);

  cfg->m_indexes = ut::new_arr_withkey<row_index_t>(
      UT_NEW_THIS_FILE_PSI_KEY, ut::Count{cfg->m_n_indexes});

  /* Trigger OOM */
  DBUG_EXECUTE_IF("ib_import_OOM_6", ut::delete_arr(cfg->m_indexes);
                  cfg->m_indexes = nullptr;);

  if (cfg->m_indexes == nullptr) {
    return (DB_OUT_OF_MEMORY);
  }

  memset(cfg->m_indexes, 0x0, sizeof(*cfg->m_indexes) * cfg->m_n_indexes);

  cfg_index = cfg->m_indexes;

  for (ulint i = 0; i < cfg->m_n_indexes; ++i, ++cfg_index) {
    /* Trigger EOF */
    DBUG_EXECUTE_IF("ib_import_io_read_error_2",
                    (void)fseek(file, 0L, SEEK_END););

    /* Read the index data. */
    size_t n_bytes = fread(row, 1, sizeof(row), file);

    /* Trigger EOF */
    DBUG_EXECUTE_IF("ib_import_io_read_error",
                    (void)fseek(file, 0L, SEEK_END););

    if (n_bytes != sizeof(row)) {
      char msg[BUFSIZ];

      snprintf(msg, sizeof(msg),
               "while reading index meta-data, expected"
               " to read %lu bytes but read only %lu"
               " bytes",
               (ulong)sizeof(row), (ulong)n_bytes);

      ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                  strerror(errno), msg);

      ib::error(ER_IB_MSG_947) << "IO Error: " << msg;

      return (DB_IO_ERROR);
    }

    ptr = row;

    cfg_index->m_id = mach_read_from_8(ptr);
    ptr += sizeof(space_index_t);

    cfg_index->m_space = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    cfg_index->m_page_no = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    cfg_index->m_type = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    cfg_index->m_trx_id_offset = mach_read_from_4(ptr);
    if (cfg_index->m_trx_id_offset != mach_read_from_4(ptr)) {
      ut_d(ut_error);
      /* Overflow. Pretend that the clustered index
      has a variable-length PRIMARY KEY. */
      ut_o(cfg_index->m_trx_id_offset = 0);
    }
    ptr += sizeof(uint32_t);

    cfg_index->m_n_user_defined_cols = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    cfg_index->m_n_uniq = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    cfg_index->m_n_nullable = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    cfg_index->m_n_fields = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    /* The NUL byte is included in the name length. */
    ulint len = mach_read_from_4(ptr);

    if (len > OS_FILE_MAX_PATH) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_INNODB_INDEX_CORRUPT,
              "Index name length (" ULINTPF
              ") is too long,"
              " the meta-data is corrupt",
              len);

      return (DB_CORRUPTION);
    }

    cfg_index->m_name =
        ut::new_arr_withkey<byte>(UT_NEW_THIS_FILE_PSI_KEY, ut::Count{len});

    /* Trigger OOM */
    DBUG_EXECUTE_IF("ib_import_OOM_7", ut::delete_arr(cfg_index->m_name);
                    cfg_index->m_name = nullptr;);

    if (cfg_index->m_name == nullptr) {
      return (DB_OUT_OF_MEMORY);
    }

    dberr_t err;

    err = row_import_cfg_read_string(file, cfg_index->m_name, len);

    if (err != DB_SUCCESS) {
      ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                  strerror(errno), "while parsing index name.");

      return (err);
    }

    err = row_import_cfg_read_index_fields(file, thd, cfg_index, cfg);

    if (err != DB_SUCCESS) {
      return (err);
    }
  }

  return (DB_SUCCESS);
}

/** Set the index root page number for v1 format.
 @return DB_SUCCESS or error code. */
static dberr_t row_import_read_indexes(
    FILE *file,      /*!< in: File to read from */
    THD *thd,        /*!< in: session */
    row_import *cfg) /*!< in/out: meta-data read */
{
  byte row[sizeof(uint32_t)];

  /* Trigger EOF */
  DBUG_EXECUTE_IF("ib_import_io_read_error_3",
                  (void)fseek(file, 0L, SEEK_END););

  /* Read the number of indexes. */
  if (fread(row, 1, sizeof(row), file) != sizeof(row)) {
    ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                strerror(errno), "while reading number of indexes.");

    return (DB_IO_ERROR);
  }

  cfg->m_n_indexes = mach_read_from_4(row);

  if (cfg->m_n_indexes == 0) {
    ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                strerror(errno), "Number of indexes in meta-data file is 0");

    return (DB_CORRUPTION);

  } else if (cfg->m_n_indexes > 1024) {
    // FIXME: What is the upper limit? */
    ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                strerror(errno),
                "Number of indexes in meta-data file is too high: %lu",
                (ulong)cfg->m_n_indexes);
    cfg->m_n_indexes = 0;

    return (DB_CORRUPTION);
  }

  return (row_import_read_index_data(file, thd, cfg));
}

/** Read specified bytes from the meta data file.
@param[in]      file    file to read from
@param[in]      length  length of bytes to read
@return the bytes stream, caller has to free the memory if not nullptr */
[[nodiscard]] static byte *row_import_read_bytes(FILE *file, size_t length) {
  size_t read = 0;
  byte *r =
      ut::new_arr_withkey<byte>(UT_NEW_THIS_FILE_PSI_KEY, ut::Count{length});

  if (length == 0) {
    return (r);
  }

  while (!feof(file)) {
    int ch = fgetc(file);

    if (ch == EOF) {
      break;
    }

    r[read++] = ch;
    if (read == length) {
      return (r);
    }
  }

  errno = EINVAL;

  ut::delete_arr(r);

  return (nullptr);
}

/** Read the metadata config file. Deserialise the contents of
dict_col_t::instant_default if exists.
Refer to row_quiesce_write_default_value() for the format details.
@param[in]      file    file to read from
@param[in,out]  col     column whose default value to read
@param[in,out]  heap    memory heap to store default value
@param[in,out]  read    true if default value read */
[[nodiscard]] static dberr_t row_import_read_default_values(FILE *file,
                                                            dict_col_t *col,
                                                            mem_heap_t **heap,
                                                            bool *read) {
  byte *str;

  /* Instant or not byte */
  if ((str = row_import_read_bytes(file, 1)) == nullptr) {
    return (DB_IO_ERROR);
  }

  if (str[0] == 0) {
    ut::delete_arr(str);
    *read = false;
    return (DB_SUCCESS);
  }

  *read = true;

  ut::delete_arr(str);

  /* Null byte */
  if ((str = row_import_read_bytes(file, 1)) == nullptr) {
    return (DB_IO_ERROR);
  }

  if (*heap == nullptr) {
    *heap = mem_heap_create(100, UT_LOCATION_HERE);
  }

  if (str[0] == 1) {
    ut::delete_arr(str);
    col->set_default(nullptr, UNIV_SQL_NULL, *heap);
    return (DB_SUCCESS);
  } else {
    ut::delete_arr(str);

    /* Length bytes */
    if ((str = row_import_read_bytes(file, 4)) == nullptr) {
      return (DB_IO_ERROR);
    }

    size_t length = mach_read_from_4(str);

    ut::delete_arr(str);

    /* Value bytes */
    if (((str = row_import_read_bytes(file, length)) == nullptr) &&
        (length != 0)) {
      return (DB_IO_ERROR);
    }

    col->set_default(str, length, *heap);

    ut::delete_arr(str);

    return (DB_SUCCESS);
  }
}

/** Read dd::Column metadata for the dropped table.
@param[in,out]  table_def       Table definition
@param[in]      file            file to read from
@param[in]      thd             session
@param[in]      col             dict_col_t
@param[in]      col_name        name of the columns */
static dberr_t row_import_read_dropped_col_metadata(dd::Table *table_def,
                                                    FILE *file, THD *thd,
                                                    dict_col_t *col,
                                                    const char *col_name) {
  ut_ad(col->is_instant_dropped());

  /* Total metadata to be written
    1 byte for is NULLABLE
    1 byte for is_unsigned
    4 bytes for char_length
    4 bytes for column type
    4 bytes for numeric scale
    8 bytes for collation id */
  constexpr size_t METADATA_SIZE = 22;

  byte row[METADATA_SIZE];

  /* Read column's v_added, v_dropped, phy_pos  */
  if (fread(row, 1, sizeof(row), file) != sizeof(row)) {
    ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                strerror(errno), "while reading dropped column meta-data.");
    return (DB_IO_ERROR);
  }

  byte *ptr = row;

  /* 1 byte for is NULLABLE */
  bool is_nullable = mach_read_from_1(ptr);
  ptr += 1;

  /* 1 byte for is_unsigned */
  bool is_unsigned = mach_read_from_1(ptr);
  ptr += 1;

  /* 4 bytes for char_length */
  uint32_t char_length = mach_read_from_4(ptr);
  ptr += sizeof(uint32_t);

  /* 4 bytes for column type */
  uint32_t col_type = mach_read_from_4(ptr);
  ptr += sizeof(uint32_t);

  /* 4 bytes for numeric scale */
  uint32_t numeric_scale = mach_read_from_4(ptr);
  ptr += sizeof(uint32_t);

  /* 8 bytes for collation id */
  uint64_t collation_id = mach_read_from_8(ptr);
  ptr += sizeof(uint64_t);

  /* Read elements for enum column type.
  [4]     bytes : number of elements
  For each element
    [4]     bytes : element name length (len+1)
    [len+1] bytes : element name */
  std::vector<dd::String_type> enum_names;
  if ((dd::enum_column_types)col_type == dd::enum_column_types::ENUM ||
      (dd::enum_column_types)col_type == dd::enum_column_types::SET) {
    byte _row[4];

    /* Read element count */
    if (fread(_row, 1, sizeof(_row), file) != sizeof(_row)) {
      ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                  strerror(errno), "while reading dropped column meta-data.");
      return (DB_IO_ERROR);
    }
    size_t n_elem = mach_read_from_4(_row);

    for (size_t i = 0; i < n_elem; i++) {
      /* Read element name length */
      if (fread(_row, 1, sizeof(_row), file) != sizeof(_row)) {
        ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                    strerror(errno), "while reading dropped column meta-data.");
        return (DB_IO_ERROR);
      }
      uint32_t len = mach_read_from_4(_row);

      if (len == 0) {
        ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                    strerror(errno),
                    "Enum element name length %lu, is invalid for column %s",
                    (ulong)len, col_name);

        return (DB_CORRUPTION);
      }

      /* Read element name */
      byte *elem_name =
          ut::new_arr_withkey<byte>(UT_NEW_THIS_FILE_PSI_KEY, ut::Count{len});
      dberr_t err = row_import_cfg_read_string(file, elem_name, len);
      if (err != DB_SUCCESS) {
        ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                    strerror(errno),
                    "Error reading Enum element name for column %s", col_name);

        return (DB_CORRUPTION);
      }
      enum_names.push_back((const char *)elem_name);
      ut::delete_arr(elem_name);
    }
  }

  dd::Column *new_column =
      const_cast<dd::Column *>(dd_find_column(table_def, col_name));
  /* If the INSTANT DROP column already exists in target table DD, confirm it's
  metadata is matching which the CFG. */
  if (new_column != nullptr) {
    ut_ad(new_column->is_se_hidden());

    bool err = false;

    {
      /* Match version added */
      if (col->is_instant_added() && dd_column_is_added(new_column)) {
        uint32_t v = dd_column_get_version_added(new_column);
        err = (v != (uint32_t)col->get_version_added());
      } else if (col->is_instant_added() || dd_column_is_added(new_column)) {
        err = true;
      }

      /* Match version dropped */
      if (!err) {
        if (dd_column_is_dropped(new_column)) {
          uint32_t v = dd_column_get_version_dropped(new_column);
          err = (v != (uint32_t)col->get_version_dropped());
        } else {
          err = true;
        }
      }

      /* Match phy_pos */
      if (!err) {
        const char *s = dd_column_key_strings[DD_INSTANT_PHYSICAL_POS];
        uint32_t v = 0;
        new_column->se_private_data().get(s, &v);
        err = (v != col->get_phy_pos());
      }
    }

    auto match_enum_values = [&](dd::Column *col) {
      if (enum_names.size() == 0) {
        ut_ad(col->type() != dd::enum_column_types::ENUM &&
              col->type() != dd::enum_column_types::SET);
        return false;
      }

      size_t i = 0;
      for (const auto &elem : col->elements()) {
        if (enum_names[i++].compare(elem->name()) != 0) {
          return true;
        }
      }

      return false;
    };

    if (err || new_column->is_nullable() != is_nullable ||
        new_column->is_unsigned() != is_unsigned ||
        new_column->char_length() != char_length ||
        new_column->numeric_scale() != numeric_scale ||
        new_column->collation_id() != collation_id ||
        match_enum_values(new_column)) {
      ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                  strerror(errno),
                  "DD metadata for INSTNAT DROP column %s in"
                  " target table doesn't match with CFG.",
                  col_name);
      return DB_ERROR;
    }

    return DB_SUCCESS;
  }

  /* Add this column as a SE_HIDDEN column in dest table def */
  new_column = dd_add_hidden_column(table_def, col_name, char_length,
                                    (dd::enum_column_types)col_type);
  ut_ad(new_column != nullptr);

  /* Set SE Private data of newly added hidden column here */
  {
    auto set = [&](const char *s, uint32_t v) {
      new_column->se_private_data().set(s, v);
    };

    new_column->se_private_data().clear();
    if (col->is_instant_added()) {
      set(dd_column_key_strings[DD_INSTANT_VERSION_ADDED],
          (uint32_t)col->get_version_added());
    }

    set(dd_column_key_strings[DD_INSTANT_VERSION_DROPPED],
        (uint32_t)col->get_version_dropped());
    set(dd_column_key_strings[DD_INSTANT_PHYSICAL_POS], col->get_phy_pos());
  }

  new_column->set_nullable(is_nullable);
  new_column->set_unsigned(is_unsigned);
  new_column->set_char_length(char_length);
  new_column->set_numeric_scale(numeric_scale);
  new_column->set_collation_id(collation_id);
  new_column->set_type((dd::enum_column_types)col_type);
  /* Elements for enum columns */
  if ((dd::enum_column_types)col_type == dd::enum_column_types::ENUM ||
      (dd::enum_column_types)col_type == dd::enum_column_types::SET) {
    for (auto &name : enum_names) {
      auto *elem_obj = new_column->add_element();
      elem_obj->set_name(name.c_str());
    }
  }

  return DB_SUCCESS;
}

/** Read the meta data (table columns) config file. Deserialise the contents of
dict_col_t structure, along with the column name.
@param[in,out]  table_def       Table definition
@param[in]      file            file to read from
@param[in]      thd             session
@param[in]      cfg             meta-data read */
[[nodiscard]] static dberr_t row_import_read_columns(dd::Table *table_def,
                                                     FILE *file, THD *thd,
                                                     row_import *cfg) {
  /* FIXME: What should the upper limit be? */
  ut_a(cfg->m_n_cols > 0);
  ut_a(cfg->m_n_cols < 1024);

  /* Allocate array of columns */
  cfg->m_cols = ut::new_arr_withkey<dict_col_t>(UT_NEW_THIS_FILE_PSI_KEY,
                                                ut::Count{cfg->m_n_cols});

  /* Trigger OOM */
  DBUG_EXECUTE_IF("ib_import_OOM_8", ut::delete_arr(cfg->m_cols);
                  cfg->m_cols = nullptr;);

  if (cfg->m_cols == nullptr) {
    return (DB_OUT_OF_MEMORY);
  }

  // memset(cfg->m_cols, 0x0, sizeof(*cfg->m_cols) * cfg->m_n_cols);

  /* Allocated array to store name of the columns */
  cfg->m_col_names = ut::new_arr_withkey<byte *>(UT_NEW_THIS_FILE_PSI_KEY,
                                                 ut::Count{cfg->m_n_cols});

  /* Trigger OOM */
  DBUG_EXECUTE_IF("ib_import_OOM_9", ut::delete_arr(cfg->m_col_names);
                  cfg->m_col_names = nullptr;);

  if (cfg->m_col_names == nullptr) {
    return (DB_OUT_OF_MEMORY);
  }

  memset(cfg->m_col_names, 0x0, sizeof(cfg->m_col_names) * cfg->m_n_cols);

  dict_col_t *col = cfg->m_cols;
  byte row[sizeof(uint32_t) * 8];

  for (ulint i = 0; i < cfg->m_n_cols; ++i, ++col) {
    byte *ptr = row;

    /* Trigger EOF */
    DBUG_EXECUTE_IF("ib_import_io_read_error_4",
                    (void)fseek(file, 0L, SEEK_END););

    if (fread(row, 1, sizeof(row), file) != sizeof(row)) {
      ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                  strerror(errno), "while reading table column meta-data.");

      return (DB_IO_ERROR);
    }

    col->prtype = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    col->mtype = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    col->len = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    col->mbminmaxlen = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    col->ind = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    col->ord_part = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    col->max_prefix = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    /* Read in the column name as [len, byte array]. The len
    includes the NUL byte. */

    ulint len = mach_read_from_4(ptr);

    /* FIXME: What is the maximum column name length? */
    if (len == 0 || len > 128) {
      ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                  strerror(errno), "Column name length %lu, is invalid",
                  (ulong)len);

      return (DB_CORRUPTION);
    }

    cfg->m_col_names[i] =
        ut::new_arr_withkey<byte>(UT_NEW_THIS_FILE_PSI_KEY, ut::Count{len});

    /* Trigger OOM */
    DBUG_EXECUTE_IF("ib_import_OOM_10", ut::delete_arr(cfg->m_col_names[i]);
                    cfg->m_col_names[i] = nullptr;);

    if (cfg->m_col_names[i] == nullptr) {
      return (DB_OUT_OF_MEMORY);
    }

    dberr_t err;

    err = row_import_cfg_read_string(file, cfg->m_col_names[i], len);

    if (err != DB_SUCCESS) {
      ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                  strerror(errno), "while parsing table column name.");

      return (err);
    }

    /* Read INSTANT metadata of column */
    if (cfg->m_version >= IB_EXPORT_CFG_VERSION_V7) {
      byte row[2 + sizeof(uint32_t)];

      /* Read column's v_added, v_dropped, phy_pos  */
      if (fread(row, 1, sizeof(row), file) != sizeof(row)) {
        ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                    strerror(errno),
                    "while reading table column INSTANT meta-data.");
        return (DB_IO_ERROR);
      }

      byte *ptr = row;
      uint8_t v = mach_read_from_1(ptr);
      col->set_version_added(v);
      ptr++;

      v = mach_read_from_1(ptr);
      col->set_version_dropped(v);
      ptr++;

      col->set_phy_pos(mach_read_from_4(ptr));

      if (col->is_instant_dropped()) {
        const char *col_name = (const char *)cfg->m_col_names[i];
        ut_ad(dict_col_t::is_instant_dropped_name(std::string(col_name)));

        /* Read dropped col dd::Column metadata and add it to dd::Table */
        dberr_t err = row_import_read_dropped_col_metadata(table_def, file, thd,
                                                           col, col_name);

        if (err != DB_SUCCESS) {
          return err;
        }
      }
    }

    if (cfg->m_version >= IB_EXPORT_CFG_VERSION_V3) {
      bool read = false;
      dberr_t err;

      err = row_import_read_default_values(file, col, &cfg->m_heap, &read);

      if (err != DB_SUCCESS) {
        ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                    strerror(errno),
                    "while reading table column"
                    " default value.");
        return (err);
      }

      if (read) {
        ++cfg->m_n_instant_cols;
      }
    }
  }

  return (DB_SUCCESS);
}

/** Read the contents of the @<tablespace@>.cfg file.
 @return DB_SUCCESS or error code. */
[[nodiscard]] static dberr_t row_import_read_v1(
    FILE *file,      /*!< in: File to read from */
    THD *thd,        /*!< in: session */
    row_import *cfg) /*!< out: meta data */
{
  byte value[sizeof(uint32_t)];

  /* Trigger EOF */
  DBUG_EXECUTE_IF("ib_import_io_read_error_5",
                  (void)fseek(file, 0L, SEEK_END););

  /* Read the hostname where the tablespace was exported. */
  if (fread(value, 1, sizeof(value), file) != sizeof(value)) {
    ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                strerror(errno),
                "while reading meta-data export hostname length.");

    return (DB_IO_ERROR);
  }

  ulint len = mach_read_from_4(value);

  /* NUL byte is part of name length. */
  cfg->m_hostname =
      ut::new_arr_withkey<byte>(UT_NEW_THIS_FILE_PSI_KEY, ut::Count{len});

  /* Trigger OOM */
  DBUG_EXECUTE_IF("ib_import_OOM_1", ut::delete_arr(cfg->m_hostname);
                  cfg->m_hostname = nullptr;);

  if (cfg->m_hostname == nullptr) {
    return (DB_OUT_OF_MEMORY);
  }

  dberr_t err = row_import_cfg_read_string(file, cfg->m_hostname, len);

  if (err != DB_SUCCESS) {
    ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                strerror(errno), "while parsing export hostname.");

    return (err);
  }

  /* Trigger EOF */
  DBUG_EXECUTE_IF("ib_import_io_read_error_6",
                  (void)fseek(file, 0L, SEEK_END););

  /* Read the table name of tablespace that was exported. */
  if (fread(value, 1, sizeof(value), file) != sizeof(value)) {
    ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                strerror(errno), "while reading meta-data table name length.");

    return (DB_IO_ERROR);
  }

  len = mach_read_from_4(value);

  /* NUL byte is part of name length. */
  cfg->m_table_name =
      ut::new_arr_withkey<byte>(UT_NEW_THIS_FILE_PSI_KEY, ut::Count{len});

  /* Trigger OOM */
  DBUG_EXECUTE_IF("ib_import_OOM_2", ut::delete_arr(cfg->m_table_name);
                  cfg->m_table_name = nullptr;);

  if (cfg->m_table_name == nullptr) {
    return (DB_OUT_OF_MEMORY);
  }

  err = row_import_cfg_read_string(file, cfg->m_table_name, len);

  if (err != DB_SUCCESS) {
    ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                strerror(errno), "while parsing table name.");

    return (err);
  }

  ib::info(ER_IB_MSG_948) << "Importing tablespace for table '"
                          << cfg->m_table_name
                          << "' that was exported from host '"
                          << cfg->m_hostname << "'";

  byte row[sizeof(uint32_t) * 3];

  /* Trigger EOF */
  DBUG_EXECUTE_IF("ib_import_io_read_error_7",
                  (void)fseek(file, 0L, SEEK_END););

  /* Read the autoinc value. */
  if (fread(row, 1, sizeof(uint64_t), file) != sizeof(uint64_t)) {
    ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                strerror(errno), "while reading autoinc value.");

    return (DB_IO_ERROR);
  }

  cfg->m_autoinc = mach_read_from_8(row);

  /* Trigger EOF */
  DBUG_EXECUTE_IF("ib_import_io_read_error_8",
                  (void)fseek(file, 0L, SEEK_END););

  /* Read the tablespace page size. */
  if (fread(row, 1, sizeof(row), file) != sizeof(row)) {
    ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                strerror(errno), "while reading meta-data header.");

    return (DB_IO_ERROR);
  }

  byte *ptr = row;

  const ulint logical_page_size = mach_read_from_4(ptr);
  ptr += sizeof(uint32_t);

  if (logical_page_size != univ_page_size.logical()) {
    ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
            "Tablespace to be imported has a different"
            " page size than this server. Server page size"
            " is %zu, whereas tablespace page size"
            " is " ULINTPF,
            univ_page_size.logical(), logical_page_size);

    return (DB_ERROR);
  }

  cfg->m_flags = mach_read_from_4(ptr);
  ptr += sizeof(uint32_t);

  cfg->m_page_size.copy_from(dict_tf_get_page_size(cfg->m_flags));

  ut_a(logical_page_size == cfg->m_page_size.logical());

  /* Read Total number of columns in table */
  cfg->m_n_cols = mach_read_from_4(ptr);

  if (!dict_tf_is_valid(cfg->m_flags)) {
    return (DB_CORRUPTION);
  }

  if (cfg->m_version >= IB_EXPORT_CFG_VERSION_V5) {
    /* Read the nullable field before first instant column */
    if (fread(value, 1, sizeof(value), file) != sizeof(value)) {
      ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                  strerror(errno),
                  "while reading meta-data nullable column"
                  " before first instant column.");

      return (DB_IO_ERROR);
    }

    cfg->m_n_instant_nullable = mach_read_from_4(value);
  } else {
    cfg->m_n_instant_nullable = 0;
  }

  if (cfg->m_version >= IB_EXPORT_CFG_VERSION_V7) {
    byte row[sizeof(uint32_t) * 5];

    /* Read column's count for the table  */
    if (fread(row, 1, sizeof(row), file) != sizeof(row)) {
      ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                  strerror(errno), "while reading table column counts.");
      return (DB_IO_ERROR);
    }

    byte *ptr = row;
    cfg->m_initial_column_count = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    cfg->m_current_column_count = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    cfg->m_total_column_count = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    cfg->m_n_instant_drop_cols = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);

    cfg->m_current_row_version = mach_read_from_4(ptr);
    ptr += sizeof(uint32_t);
  }

  return (err);
}

/** Read tablespace flags and compression type info from @<tablespace@>.cfg
file.
@param[in]      file    File to read from
@param[in]      thd     session
@param[in,out]  cfg     meta data
@return DB_SUCCESS or error code. */
[[nodiscard]] static MY_ATTRIBUTE((nonnull)) dberr_t
    row_import_read_v2(FILE *file, THD *thd, row_import *cfg) {
  byte value[sizeof(uint32_t)];

  /* Read the tablespace flags */
  if (fread(value, 1, sizeof(value), file) != sizeof(value)) {
    ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                strerror(errno), "while reading meta-data tablespace flags.");

    return DB_IO_ERROR;
  }

  ulint space_flags = mach_read_from_4(value);
  ut_ad(space_flags != UINT32_UNDEFINED);
  cfg->m_has_sdi = FSP_FLAGS_HAS_SDI(space_flags);

  if (cfg->m_version >= IB_EXPORT_CFG_VERSION_V6) {
    /* Read the compression type info. */
    if (fread(value, 1, sizeof(uint8_t), file) != sizeof(uint8_t)) {
      ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                  strerror(errno), "while reading compression type info.");

      return DB_IO_ERROR;
    }

    auto compression_type =
        static_cast<Compression::Type>(mach_read_from_1(value));

    ut_ad(Compression::validate(compression_type));

    cfg->m_compression_type = compression_type;
  }

  return DB_SUCCESS;
}

/** Read the contents of the @<tablespace@>.cfg file
@param[in]      table_def Table definition
@param[in]      file    file to read from
@param[in]      thd     session
@param[in,out]  cfg     meta data
@return DB_SUCCESS or error code. */
[[nodiscard]] static MY_ATTRIBUTE((nonnull)) dberr_t
    row_import_read_common(dd::Table *table_def, FILE *file, THD *thd,
                           row_import *cfg) {
  dberr_t err;
  if ((err = row_import_read_columns(table_def, file, thd, cfg)) !=
      DB_SUCCESS) {
    return (err);

  } else if ((err = row_import_read_indexes(file, thd, cfg)) != DB_SUCCESS) {
    return (err);
  }

  ut_a(err == DB_SUCCESS);
  return (err);
}

/**
Read the contents of the @<tablespace@>.cfg file.
@param[in]      table           dict table
@param[in]      table_def       Table definition
@param[in]      file            File to read from
@param[in]      thd             session
@param[out]     cfg             contents of the .cfg file
@return DB_SUCCESS or error code. */
[[nodiscard]] static dberr_t row_import_read_meta_data(dict_table_t *table,
                                                       dd::Table *table_def,
                                                       FILE *file, THD *thd,
                                                       row_import &cfg) {
  byte row[sizeof(uint32_t)];

  /* Trigger EOF */
  DBUG_EXECUTE_IF("ib_import_io_read_error_9",
                  (void)fseek(file, 0L, SEEK_END););

  if (fread(&row, 1, sizeof(row), file) != sizeof(row)) {
    ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                strerror(errno), "while reading meta-data version.");

    return (DB_IO_ERROR);
  }

  cfg.m_version = mach_read_from_4(row);

  /* Check the version number. */
  switch (cfg.m_version) {
    dberr_t err;
    case IB_EXPORT_CFG_VERSION_V1:
      err = row_import_read_v1(file, thd, &cfg);
      if (err == DB_SUCCESS) {
        err = row_import_read_common(table_def, file, thd, &cfg);
      }
      return (err);

    case IB_EXPORT_CFG_VERSION_V2:
    case IB_EXPORT_CFG_VERSION_V3:
    case IB_EXPORT_CFG_VERSION_V4:
    case IB_EXPORT_CFG_VERSION_V5:
    case IB_EXPORT_CFG_VERSION_V6:
    case IB_EXPORT_CFG_VERSION_V7:
      err = row_import_read_v1(file, thd, &cfg);

      if (err == DB_SUCCESS) {
        err = row_import_read_v2(file, thd, &cfg);
      }

      if (err == DB_SUCCESS) {
        err = row_import_read_common(table_def, file, thd, &cfg);
      }
      return (err);
    default:
      my_error(ER_IMP_INCOMPATIBLE_CFG_VERSION, MYF(0), table->name.m_name,
               unsigned{cfg.m_version}, unsigned{IB_EXPORT_CFG_VERSION_V5});
  }

  return (DB_UNSUPPORTED);
}

/**
Read the contents of the @<tablename@>.cfg file.
@param[in]      table           table
@param[in]      table_def       dd table
@param[in]      thd             session
@param[in,out]  cfg             contents of the .cfg file
@return DB_SUCCESS or error code. */
[[nodiscard]] static dberr_t row_import_read_cfg(dict_table_t *table,
                                                 dd::Table *table_def, THD *thd,
                                                 row_import &cfg) {
  dberr_t err;
  char name[OS_FILE_MAX_PATH];

  cfg.m_table = table;

  dd_get_meta_data_filename(table, table_def, name, sizeof(name));

  fil_adjust_name_import(table, name, CFG);

  FILE *file = fopen(name, "rb");

  if (file == nullptr) {
    char msg[BUFSIZ];

    snprintf(msg, sizeof(msg),
             "Error opening '%s', will attempt to import"
             " without schema verification",
             name);

    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_READ_ERROR, errno,
                strerror(errno), msg);

    cfg.m_missing = true;

    err = DB_FAIL;
  } else {
    cfg.m_missing = false;

    err = row_import_read_meta_data(table, table_def, file, thd, cfg);
    fclose(file);
  }

  return (err);
}

/** Read the contents of the .cfp file.
@param[out]     cfg             the encryption key will be stored to it
@param[in]      file            file to read from
@param[in]      thd             session
@return DB_SUCCESS or error code. */
static dberr_t row_import_read_encryption_data(row_import &cfg, FILE *file,
                                               THD *thd) {
  byte row[sizeof(uint32_t)];
  ulint key_size;
  byte transfer_key[Encryption::KEY_LEN];
  byte encryption_key[Encryption::KEY_LEN];
  byte encryption_iv[Encryption::KEY_LEN];
  lint elen;

  if (fread(&row, 1, sizeof(row), file) != sizeof(row)) {
    ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                strerror(errno), "while reading encrypton key size.");

    return (DB_IO_ERROR);
  }

  key_size = mach_read_from_4(row);
  if (key_size != Encryption::KEY_LEN) {
    ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                strerror(errno), "while parsing encryption key size.");

    return (DB_IO_ERROR);
  }

  /* Read the transfer key. */
  if (fread(transfer_key, 1, Encryption::KEY_LEN, file) !=
      Encryption::KEY_LEN) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                strerror(errno), "while reading tranfer key.");

    return (DB_IO_ERROR);
  }

  /* Read the encrypted key. */
  if (fread(encryption_key, 1, Encryption::KEY_LEN, file) !=
      Encryption::KEY_LEN) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                strerror(errno), "while reading encryption key.");

    return (DB_IO_ERROR);
  }

  /* Read the encrypted iv. */
  if (fread(encryption_iv, 1, Encryption::KEY_LEN, file) !=
      Encryption::KEY_LEN) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_IO_WRITE_ERROR, errno,
                strerror(errno), "while reading encryption iv.");

    return (DB_IO_ERROR);
  }
  /* Decrypt tablespace key and iv. */
  elen = my_aes_decrypt(encryption_key, Encryption::KEY_LEN,
                        cfg.m_encryption_metadata.m_key, transfer_key,
                        Encryption::KEY_LEN, my_aes_256_ecb, nullptr, false);

  if (elen == MY_AES_BAD_DATA) {
    ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                strerror(errno), "while decrypt encryption key.");

    return (DB_IO_ERROR);
  }

  elen = my_aes_decrypt(encryption_iv, Encryption::KEY_LEN,
                        cfg.m_encryption_metadata.m_iv, transfer_key,
                        Encryption::KEY_LEN, my_aes_256_ecb, nullptr, false);

  if (elen == MY_AES_BAD_DATA) {
    ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_IO_READ_ERROR, errno,
                strerror(errno), "while decrypt encryption iv.");

    return (DB_IO_ERROR);
  }
  cfg.m_encryption_metadata.m_type = Encryption::Type::AES;
  cfg.m_encryption_metadata.m_key_len = Encryption::KEY_LEN;

  return (DB_SUCCESS);
}

/** Read the contents of the .cfp file.
@param[in]      table           table
@param[in]      thd             session
@param[in,out]  import          meta data
@return DB_SUCCESS or error code. */
static dberr_t row_import_read_cfp(dict_table_t *table, THD *thd,
                                   row_import &import) {
  dberr_t err;
  char name[OS_FILE_MAX_PATH];

  /* Clear table encryption information. */
  import.m_encryption_metadata.m_type = Encryption::Type::NONE;

  srv_get_encryption_data_filename(table, name, sizeof(name));

  fil_adjust_name_import(table, name, CFP);

  FILE *file = fopen(name, "rb");

  if (file != nullptr) {
    import.m_cfp_missing = false;
    err = row_import_read_encryption_data(import, file, thd);
    fclose(file);
  } else {
    /* If there's no cfp file, we assume it's not an
    encrpyted table. return directly. */
    import.m_cfp_missing = true;
    err = DB_SUCCESS;
  }
  return (err);
}

/** Check the correctness of clustered index of imported table.
Once there is corruption found, the IMPORT would be refused. This can
help to detect the missing .cfg file for a table with instant added columns.
@param[in,out]  table           InnoDB table object
@param[in,out]  thd             MySQL session variable
@param[in]      missing         true if .cfg file is missing
@return DB_SUCCESS or error code. */
dberr_t row_import_check_corruption(dict_table_t *table, THD *thd,
                                    bool missing) {
  dberr_t err = DB_SUCCESS;
  if (!btr_validate_index(table->first_index(), nullptr, false)) {
    err = DB_CORRUPTION;
    if (missing) {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Clustered index validation failed. Because"
              " the .cfg file is missing, table definition"
              " of the IBD file could be different. Or"
              " the data file itself is already corrupted.");
    } else {
      ib_errf(thd, IB_LOG_LEVEL_ERROR, ER_INNODB_INDEX_CORRUPT,
              "Clustered index validation failed, due to"
              " data file corruption.");
    }
  }

  return (err);
}

/** Imports a tablespace. The space id in the .ibd file must match the space id
of the table in the data dictionary.
@param[in]      table           table
@param[in]      table_def       dd table
@param[in]      prebuilt        prebuilt struct in MySQL
@return error code or DB_SUCCESS */
dberr_t row_import_for_mysql(dict_table_t *table, dd::Table *table_def,
                             row_prebuilt_t *prebuilt) {
  dberr_t err;
  trx_t *trx;
  uint64_t autoinc = 0;
  char *filepath = nullptr;

  /* The caller assured that this is not read_only_mode and that no
  temporary tablespace is being imported. */
  ut_ad(!srv_read_only_mode);
  ut_ad(!table->is_temporary());

  ut_a(table->space);
  ut_ad(prebuilt->trx);
  ut_a(table->ibd_file_missing);

  ibuf_delete_for_discarded_space(table->space);

  trx_start_if_not_started(prebuilt->trx, true, UT_LOCATION_HERE);

  trx = trx_allocate_for_mysql();

  /* So that the table is not DROPped during recovery. */
  trx_set_dict_operation(trx, TRX_DICT_OP_INDEX);

  trx_start_if_not_started(trx, true, UT_LOCATION_HERE);

  /* So that we can send error messages to the user. */
  trx->mysql_thd = prebuilt->trx->mysql_thd;

  /* Assign an undo segment for the transaction, so that the
  transaction will be recovered after a crash. */

  mutex_enter(&trx->undo_mutex);

  /* IMPORT tablespace is blocked for temp-tables and so we don't
  need to assign temporary rollback segment for this trx. */
  err = trx_undo_assign_undo(trx, &trx->rsegs.m_redo, TRX_UNDO_UPDATE);

  mutex_exit(&trx->undo_mutex);

  if (err != DB_SUCCESS) {
    return (row_import_cleanup(prebuilt, trx, err));

  } else if (trx->rsegs.m_redo.update_undo == nullptr) {
    err = DB_TOO_MANY_CONCURRENT_TRXS;
    return (row_import_cleanup(prebuilt, trx, err));
  }

  ut_a(err == DB_SUCCESS);

  prebuilt->trx->op_info = "read meta-data file";

  /* Prevent DDL operations while we are checking. */
  rw_lock_s_lock_func(dict_operation_lock, 0, UT_LOCATION_HERE);

  row_import cfg{};
  ulint space_flags = 0;

  /* Read CFP file */
  if (dd_is_table_in_encrypted_tablespace(table)) {
    /* First try to read CFP file here. */
    err = row_import_read_cfp(table, trx->mysql_thd, cfg);
    ut_ad(cfg.m_cfp_missing || err == DB_SUCCESS);

    if (err != DB_SUCCESS) {
      rw_lock_s_unlock_gen(dict_operation_lock, 0);
      return (row_import_error(prebuilt, trx, err));
    }

    /* If table is encrypted, but can't find cfp file, return error. */
    if (cfg.m_cfp_missing) {
      ib_errf(trx->mysql_thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Table is in an encrypted tablespace, but the encryption"
              " meta-data file cannot be found while importing.");
      err = DB_ERROR;
      rw_lock_s_unlock_gen(dict_operation_lock, 0);
      return (row_import_error(prebuilt, trx, err));
    } else {
      /* If CFP file is read, encryption_key must have been populted. */
      ut_ad(cfg.m_encryption_metadata.can_encrypt());
    }
  }

  /* Read CFG file */
  err = row_import_read_cfg(table, table_def, trx->mysql_thd, cfg);

  /* Check if the table column definitions match the contents
  of the config file. */

  if (err == DB_SUCCESS) {
    /* We have a schema file, try and match it with our
    data dictionary. */

    if (err == DB_SUCCESS) {
      err = cfg.match_schema(trx->mysql_thd, table_def);
    }

    /* Update index->page and SYS_INDEXES.PAGE_NO to match the
    B-tree root page numbers in the tablespace. Use the index
    name from the .cfg file to find match. */

    if (err == DB_SUCCESS) {
      cfg.set_root_by_name();
      autoinc = cfg.m_autoinc;
    }

    rw_lock_s_unlock_gen(dict_operation_lock, 0);

  } else if (cfg.m_missing) {
    rw_lock_s_unlock_gen(dict_operation_lock, 0);

    /* We don't have a schema file, we will have to discover
    the index root pages from the .ibd file and skip the schema
    matching step. */

    ut_a(err == DB_FAIL);

    cfg.m_page_size.copy_from(univ_page_size);

    /* Check and store compression type. */
    Compression compression;

    err = Compression::check(prebuilt->m_mysql_table->s->compress.str,
                             &compression);

    ut_a(err == DB_SUCCESS);
    cfg.m_compression_type = compression.m_type;

    FetchIndexRootPages fetchIndexRootPages(table, trx);

    err = fil_tablespace_iterate(
        cfg.m_encryption_metadata, table,
        IO_BUFFER_SIZE(cfg.m_page_size.physical(), cfg.m_page_size.physical()),
        cfg.m_compression_type, fetchIndexRootPages);

    if (err == DB_SCHEMA_MISMATCH) {
      ib_errf(trx->mysql_thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "CFG file is missing and source table is found to have row "
              "versions. CFG file is must to IMPORT tables with row versions.");
      return (row_import_cleanup(prebuilt, trx, err));
    }

    if (err == DB_SUCCESS) {
      err = fetchIndexRootPages.build_row_import(&cfg);

      /* Update index->page and SYS_INDEXES.PAGE_NO
      to match the B-tree root page numbers in the
      tablespace. */

      if (err == DB_SUCCESS) {
        err = cfg.set_root_by_heuristic();
      }
    }

    space_flags = fetchIndexRootPages.get_space_flags();

    /* If the fsp flag is set for data_dir, but table flag is not set
    for data_dir or vice versa then return error. */
    if (err == DB_SUCCESS && FSP_FLAGS_HAS_DATA_DIR(space_flags) !=
                                 DICT_TF_HAS_DATA_DIR(table->flags)) {
      ib_errf(trx->mysql_thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
              "Table location flags do not match. The source table %s a "
              "DATA DIRECTORY but the destination table %s.",
              (FSP_FLAGS_HAS_DATA_DIR(space_flags) ? "uses" : "does not use"),
              (DICT_TF_HAS_DATA_DIR(table->flags) ? "does" : "does not"));
      err = DB_ERROR;
      return (row_import_error(prebuilt, trx, err));
    }
  } else {
    rw_lock_s_unlock_gen(dict_operation_lock, 0);
  }

  if (err != DB_SUCCESS) {
    if (err == DB_IO_NO_ENCRYPT_TABLESPACE) {
      ib_errf(
          trx->mysql_thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
          "Encryption attribute in the file does not match the dictionary.");

      return (row_import_cleanup(prebuilt, trx, err));
    }
    return (row_import_error(prebuilt, trx, err));
  }

  /* At this point, all required information has been collected for IMPORT. */

  prebuilt->trx->op_info = "importing tablespace";

  ib::info(ER_IB_MSG_949) << "Phase I - Update all pages";

  /* Iterate over all the pages and do the sanity checking and
  the conversion required to import the tablespace. */

  PageConverter converter(&cfg, trx);

  /* Set the IO buffer size in pages. */

  err = fil_tablespace_iterate(
      cfg.m_encryption_metadata, table,
      IO_BUFFER_SIZE(cfg.m_page_size.physical(), cfg.m_page_size.physical()),
      cfg.m_compression_type, converter);

  if (err == DB_SCHEMA_MISMATCH) {
    ib_errf(trx->mysql_thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
            "CFG file is missing and source table is found to have row "
            "versions. CFG file is must to IMPORT tables with row versions.");
    return (row_import_cleanup(prebuilt, trx, err));
  }

  DBUG_EXECUTE_IF("ib_import_reset_space_and_lsn_failure",
                  err = DB_TOO_MANY_CONCURRENT_TRXS;);

  if (err == DB_IO_NO_ENCRYPT_TABLESPACE) {
    ib_errf(trx->mysql_thd, IB_LOG_LEVEL_ERROR, ER_TABLE_SCHEMA_MISMATCH,
            "Encryption attribute in the file does not match the dictionary.");

    return (row_import_cleanup(prebuilt, trx, err));
  }

  if (err != DB_SUCCESS) {
    char table_name[MAX_FULL_NAME_LEN + 1];

    innobase_format_name(table_name, sizeof(table_name), table->name.m_name);

    ib_errf(trx->mysql_thd, IB_LOG_LEVEL_ERROR, ER_INTERNAL_ERROR,
            "Cannot reset LSNs in table %s : %s", table_name, ut_strerr(err));

    return (row_import_cleanup(prebuilt, trx, err));
  }

  row_mysql_lock_data_dictionary(trx, UT_LOCATION_HERE);

  if (table->has_instant_cols() || table->has_row_versions()) {
    dd_import_instant_add_columns(table, table_def);
  }

  /* If the table is stored in a remote tablespace, we need to
  determine that filepath from the link file and system tables.
  Find the space ID in SYS_TABLES since this is an ALTER TABLE. */
  dd_get_and_save_data_dir_path(table, table_def, true);

  if (DICT_TF_HAS_DATA_DIR(table->flags)) {
    ut_a(table->data_dir_path != nullptr);

    const auto dir = table->data_dir_path;

    filepath = Fil_path::make(dir, table->name.m_name, IBD, true);
  } else {
    filepath = Fil_path::make_ibd_from_table_name(table->name.m_name);
  }

  DBUG_EXECUTE_IF("ib_import_OOM_15", ut::free(filepath); filepath = nullptr;);

  if (filepath == nullptr) {
    row_mysql_unlock_data_dictionary(trx);
    return (row_import_cleanup(prebuilt, trx, DB_OUT_OF_MEMORY));
  }

  /* Open the tablespace so that we can access via the buffer pool.
  The tablespace is initially opened as a temporary one, because
  we will not be writing any redo log for it before we have invoked
  fil_space_set_imported() to declare it a persistent tablespace. */

  uint32_t fsp_flags = dict_tf_to_fsp_flags(table->flags);
  if (cfg.m_encryption_metadata.can_encrypt()) {
    fsp_flags_set_encryption(fsp_flags);
  }

  std::string tablespace_name(table->name.m_name);
  dict_name::convert_to_space(tablespace_name);

  err = fil_ibd_open(true, FIL_TYPE_IMPORT, table->space, fsp_flags,
                     tablespace_name.c_str(), filepath, true, false);

  DBUG_EXECUTE_IF("ib_import_open_tablespace_failure",
                  err = DB_TABLESPACE_NOT_FOUND;);

  if (err != DB_SUCCESS) {
    row_mysql_unlock_data_dictionary(trx);

    ib_senderrf(trx->mysql_thd, IB_LOG_LEVEL_ERROR, ER_FILE_NOT_FOUND, filepath,
                err, ut_strerr(err));

    ut::free(filepath);

    return row_import_cleanup(prebuilt, trx, err);
  }

  /* For encrypted tablespace, set encryption information. */
  if (FSP_FLAGS_GET_ENCRYPTION(fsp_flags)) {
    ut_ad(cfg.m_encryption_metadata.can_encrypt());
    err = fil_set_encryption(table->space, cfg.m_encryption_metadata.m_type,
                             cfg.m_encryption_metadata.m_key,
                             cfg.m_encryption_metadata.m_iv);
  }

  const char *compression_algorithm =
      Compression::to_string(cfg.m_compression_type);

  if (err == DB_SUCCESS && !Compression::is_none(compression_algorithm)) {
    err = dict_set_compression(table, compression_algorithm, true);
  }

  row_mysql_unlock_data_dictionary(trx);

  if (err != DB_SUCCESS) {
    return row_import_cleanup(prebuilt, trx, err);
  }

  /* Set the autoextend_size attribute. */
  {
    auto dc = dd::get_dd_client(trx->mysql_thd);
    dd::cache::Dictionary_client::Auto_releaser releaser(dc);
    uint64_t autoextend_size{};
    if (!dd_get_tablespace_size_option(dc, table->dd_space_id,
                                       &autoextend_size)) {
      ut_d(dberr_t ret =)
          fil_set_autoextend_size(table->space, autoextend_size);
      ut_ad(ret == DB_SUCCESS);
    }
  }

  ut::free(filepath);

  err = ibuf_check_bitmap_on_import(trx, table->space);

  DBUG_EXECUTE_IF("ib_import_check_bitmap_failure", err = DB_CORRUPTION;);

  if (err != DB_SUCCESS) {
    return (row_import_cleanup(prebuilt, trx, err));
  }

  /* The first index must always be the clustered index. */

  dict_index_t *index = table->first_index();

  if (!index->is_clustered()) {
    return (row_import_error(prebuilt, trx, DB_CORRUPTION));
  }

  /* Update the Btree segment headers for index node and
  leaf nodes in the root page. Set the new space id. */

  err = btr_root_adjust_on_import(index);

  DBUG_EXECUTE_IF("ib_import_cluster_root_adjust_failure",
                  err = DB_CORRUPTION;);

  if (err != DB_SUCCESS) {
    return (row_import_error(prebuilt, trx, err));
  }

  DBUG_EXECUTE_IF("ib_import_page_corrupt",
                  row_index_t *i_index = cfg.get_index(index->name);
                  ++i_index->m_stats.m_n_purge_failed;);

  if (err != DB_SUCCESS) {
    return (row_import_error(prebuilt, trx, err));
  } else if (cfg.requires_purge(index->name)) {
    /* Purge any delete-marked records that couldn't be
    purged during the page conversion phase from the
    cluster index. */

    IndexPurge purge(trx, index);

    trx->op_info = "cluster: purging delete marked records";

    err = purge.garbage_collect();

    trx->op_info = "";
  }

  DBUG_EXECUTE_IF("ib_import_cluster_failure", err = DB_CORRUPTION;);

  if (err != DB_SUCCESS) {
    return (row_import_error(prebuilt, trx, err));
  }

  /* For secondary indexes, purge any records that couldn't be purged
  during the page conversion phase. */

  err = row_import_adjust_root_pages_of_secondary_indexes(trx, table, cfg);

  DBUG_EXECUTE_IF("ib_import_sec_root_adjust_failure", err = DB_CORRUPTION;);

  if (err != DB_SUCCESS) {
    return (row_import_error(prebuilt, trx, err));
  }

  /* Ensure that the next available DB_ROW_ID is not smaller than
  any DB_ROW_ID stored in the table. */

  if (prebuilt->clust_index_was_generated) {
    err = row_import_set_sys_max_row_id(prebuilt, table);

    if (err != DB_SUCCESS) {
      return (row_import_error(prebuilt, trx, err));
    }
  }

  fil_space_t *space = fil_space_acquire(table->space);

  /* Update Btree segment headers for SDI Index */
  if (FSP_FLAGS_HAS_SDI(space->flags)) {
    dict_mutex_enter_for_mysql();
    dict_index_t *sdi_index = dict_sdi_get_index(table->space);
    dict_mutex_exit_for_mysql();

    err = btr_root_adjust_on_import(sdi_index);

    if (err != DB_SUCCESS) {
      fil_space_release(space);
      return (row_import_error(prebuilt, trx, err));
    }
  }
  fil_space_release(space);

  ib::info(ER_IB_MSG_950) << "Phase III - Flush changes to disk";

  /* Ensure that all pages dirtied during the IMPORT make it to disk.
  The only dirty pages generated should be from the pessimistic purge
  of delete marked records that couldn't be purged in Phase I. */

  buf_LRU_flush_or_remove_pages(prebuilt->table->space, BUF_REMOVE_FLUSH_WRITE,
                                trx);

  if (trx_is_interrupted(trx)) {
    ib::info(ER_IB_MSG_951) << "Phase III - Flush interrupted";
    return (row_import_error(prebuilt, trx, DB_INTERRUPTED));
  }

  ib::info(ER_IB_MSG_952) << "Phase IV - Flush complete";
  fil_space_set_imported(prebuilt->table->space);

  /* Check if the on-disk .ibd file doesn't have SDI index.
  If it doesn't exist, create SDI Index page now. */
  mtr_t mtr;
  mtr.start();
  buf_block_t *block =
      buf_page_get(page_id_t(table->space, 0), dict_table_page_size(table),
                   RW_SX_LATCH, UT_LOCATION_HERE, &mtr);

  buf_block_dbg_add_level(block, SYNC_FSP_PAGE);

  page_t *page = buf_block_get_frame(block);

  ulint space_flags_from_disk = fsp_header_get_field(page, FSP_SPACE_FLAGS);
  mtr.commit();

  if (!FSP_FLAGS_HAS_SDI(space_flags_from_disk)) {
    /* This is IMPORT from 5.7 .ibd file or pre 8.0.1 */
    dict_mutex_enter_for_mysql();
    dict_sdi_remove_from_cache(table->space, nullptr, true);
    btr_sdi_create_index(table->space, true);
    dict_mutex_exit_for_mysql();
    /* Update server and space version number in the page 0 of tablespace */
    if (upgrade_space_version(table->space, false)) {
      return (row_import_error(prebuilt, trx, DB_TABLESPACE_NOT_FOUND));
    }
  } else {
    ut_ad(space->flags == space_flags_from_disk);
  }

  if (dd_is_table_in_encrypted_tablespace(table)) {
    mtr_t mtr;
    byte encrypt_info[Encryption::INFO_SIZE];

    fil_space_t *space = fil_space_get(table->space);

    mtr_start(&mtr);

    mtr_x_lock_space(space, &mtr);

    memset(encrypt_info, 0, Encryption::INFO_SIZE);

    if (!fsp_header_rotate_encryption(space, encrypt_info, &mtr)) {
      mtr_commit(&mtr);
      return (row_import_cleanup(prebuilt, trx, DB_ERROR));
    }

    mtr_commit(&mtr);
  }

  /* The dictionary latches will be released in in row_import_cleanup()
  after the transaction commit, for both success and error. */

  row_mysql_lock_data_dictionary(trx, UT_LOCATION_HERE);

  DBUG_EXECUTE_IF("ib_import_internal_error", trx->error_state = DB_ERROR;
                  err = DB_ERROR;
                  ib_errf(trx->mysql_thd, IB_LOG_LEVEL_ERROR, ER_INTERNAL_ERROR,
                          "While importing table %s", table->name.m_name);
                  return (row_import_error(prebuilt, trx, err)););

  table->ibd_file_missing = false;
  table->flags2 &= ~DICT_TF2_DISCARDED;

  /* Set autoinc value read from cfg file. The value is set to zero
  if the cfg file is missing and is initialized later from table
  column value. */
  ib::info(ER_IB_MSG_953) << table->name << " autoinc value set to " << autoinc;

  dict_table_autoinc_lock(table);
  dict_table_autoinc_initialize(table, autoinc);
  dict_table_autoinc_unlock(table);
  /* This should be set later in handler level, where we know the
  autoinc counter field index */
  table->autoinc_field_no = ULINT_UNDEFINED;

  ut_a(err == DB_SUCCESS);

  /* After discard, sdi_table->ibd_file_missing is set to true.
  This is avoid to purge on SDI tables after discard.
  At the end of successful import, set sdi_table->ibd_file_missing to
  false, indicating that .ibd of SDI table is available */
  dict_table_t *sdi_table = dict_sdi_get_table(space->id, true, false);
  sdi_table->ibd_file_missing = false;
  dict_sdi_close_table(sdi_table);

  row_mysql_unlock_data_dictionary(trx);

  err = row_import_check_corruption(table, trx->mysql_thd, cfg.m_missing);

  row_mysql_lock_data_dictionary(trx, UT_LOCATION_HERE);

  return (row_import_cleanup(prebuilt, trx, err));
}
