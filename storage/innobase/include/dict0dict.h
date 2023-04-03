/*****************************************************************************

Copyright (c) 1996, 2023, Oracle and/or its affiliates.
Copyright (c) 2012, Facebook Inc.

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

/** @file include/dict0dict.h
 Data dictionary system

 Created 1/8/1996 Heikki Tuuri
 *******************************************************/

#ifndef dict0dict_h
#define dict0dict_h

#include <set>
#include <vector>

#include <deque>
#include "data0data.h"
#include "data0type.h"
#include "dict/dict.h"
#include "dict0mem.h"
#include "dict0types.h"
#include "fsp0fsp.h"
#include "fsp0sysspace.h"
#include "hash0hash.h"
#include "mem0mem.h"
#include "rem0types.h"
#include "row0types.h"
#include "sql/dd/object_id.h"
#include "srv0mon.h" /* for dict0dict.ic */
#include "sync0rw.h"
#include "trx0types.h"
#include "univ.i"
#include "ut0byte.h"
#include "ut0mem.h"
#include "ut0new.h"
#include "ut0rnd.h"

#define dict_sys_mutex_enter() mutex_enter(&dict_sys->mutex)

#define dict_sys_mutex_exit() mutex_exit(&dict_sys->mutex)

#define dict_sys_mutex_own() mutex_own(&dict_sys->mutex)

#define dict_sys_mutex_free() mutex_free(&dict_sys->mutex)
/** initial memory heap size when creating a table or index object */
constexpr uint32_t DICT_HEAP_SIZE = 100;

/** SDI version. Written on Page 1 & 2 at FIL_PAGE_FILE_FLUSH_LSN offset. */
const uint32_t SDI_VERSION = 1;

/** Space id of system tablespace */
const space_id_t SYSTEM_TABLE_SPACE = TRX_SYS_SPACE;

/** Get the database name length in a table name.
 @return database name length */
[[nodiscard]] ulint dict_get_db_name_len(
    const char *name); /*!< in: table name in the form
         dbname '/' tablename */
#ifndef UNIV_HOTBACKUP
/** Frees a foreign key struct. */
void dict_foreign_free(
    dict_foreign_t *foreign); /*!< in, own: foreign key struct */
/** Finds the highest [number] for foreign key constraints of the table. Looks
 only at the >= 4.0.18-format id's, which are of the form
 databasename/tablename_ibfk_[number].
 @return highest number, 0 if table has no new format foreign key constraints */
ulint dict_table_get_highest_foreign_id(
    dict_table_t *table); /*!< in: table in the dictionary
                          memory cache */
#endif                    /* !UNIV_HOTBACKUP */
/** Return the end of table name where we have removed dbname and '/'.
 @return table name */
[[nodiscard]] const char *dict_remove_db_name(
    const char *name); /*!< in: table name in the
         form dbname '/' tablename */

/** Operation to perform when opening a table */
enum dict_table_op_t {
  /** Expect the tablespace to exist. */
  DICT_TABLE_OP_NORMAL = 0,
  /** Drop any orphan indexes after an aborted online index creation */
  DICT_TABLE_OP_DROP_ORPHAN,
  /** Silently load the tablespace if it does not exist,
  and do not load the definitions of incomplete indexes. */
  DICT_TABLE_OP_LOAD_TABLESPACE
};

/** Decrements the count of open handles to a table.
@param[in,out] table Table
@param[in] dict_locked True=data dictionary locked
@param[in] try_drop True=try to drop any orphan indexes after an aborted online
index creation */
void dict_table_close(dict_table_t *table, bool dict_locked, bool try_drop);

/** Closes the only open handle to a table and drops a table while assuring
 that dict_sys->mutex is held the whole time.  This assures that the table
 is not evicted after the close when the count of open handles goes to zero.
 Because dict_sys->mutex is held, we do not need to call
 dict_table_prevent_eviction().  */
void dict_table_close_and_drop(
    trx_t *trx,           /*!< in: data dictionary transaction */
    dict_table_t *table); /*!< in/out: table */
/** Inits the data dictionary module. */
void dict_init(void);

/** Closes the data dictionary module. */
void dict_close(void);

/** Inits the structure for persisting dynamic metadata */
void dict_persist_init(void);

/** Clear the structure */
void dict_persist_close(void);

#ifndef UNIV_HOTBACKUP
/** Write back the dirty persistent dynamic metadata of the table
to DDTableBuffer.
@param[in,out]  table   table object */
void dict_table_persist_to_dd_table_buffer(dict_table_t *table);

/** Read persistent dynamic metadata stored in a buffer
@param[in]      buffer          buffer to read
@param[in]      size            size of data in buffer
@param[in]      metadata        where we store the metadata from buffer */
void dict_table_read_dynamic_metadata(const byte *buffer, ulint size,
                                      PersistentTableMetadata *metadata);

/** Determine bytes of column prefix to be stored in the undo log. Please
note that if !dict_table_has_atomic_blobs(table), no prefix
needs to be stored in the undo log.
@param[in] table Table.
@param[in] col   Column which index prefix is based on.
@return bytes of column prefix to be stored in the undo log */
[[nodiscard]] static inline ulint dict_max_field_len_store_undo(
    dict_table_t *table, const dict_col_t *col);

/** Determine maximum bytes of a virtual column need to be stored
in the undo log.
@param[in]      table           dict_table_t for the table
@param[in]      col_no          virtual column number
@return maximum bytes of virtual column to be stored in the undo log */
static inline ulint dict_max_v_field_len_store_undo(dict_table_t *table,
                                                    ulint col_no);

#endif /* !UNIV_HOTBACKUP */
/** Gets the column number.
 @return col->ind, table column position (starting from 0) */
[[nodiscard]] static inline ulint dict_col_get_no(
    const dict_col_t *col); /*!< in: column */
/** Gets the column position in the clustered index.
@param[in] col         Table column.
@param[in] clust_index Clustered index. */
[[nodiscard]] static inline ulint dict_col_get_clust_pos(
    const dict_col_t *col, const dict_index_t *clust_index);

#ifndef UNIV_HOTBACKUP
/** Gets the column position in the given index.
@param[in]      col     table column
@param[in]      index   index to be searched for column
@return position of column in the given index. */
[[nodiscard]] static inline ulint dict_col_get_index_pos(
    const dict_col_t *col, const dict_index_t *index) MY_ATTRIBUTE((nonnull));

/** If the given column name is reserved for InnoDB system columns, return
 true.
 @return true if name is reserved */
[[nodiscard]] bool dict_col_name_is_reserved(
    const char *name); /*!< in: column name */
/** Acquire the autoinc lock. */
void dict_table_autoinc_lock(dict_table_t *table); /*!< in/out: table */

/** Unconditionally set the autoinc counter.
@param[in,out] table Table
@param[in] value Next value to assign to a row */
void dict_table_autoinc_initialize(dict_table_t *table, uint64_t value);

/** Reads the next autoinc value (== autoinc counter value), 0 if not yet
 initialized.
 @return value for a new row, or 0 */
[[nodiscard]] uint64_t dict_table_autoinc_read(
    const dict_table_t *table); /*!< in: table */

/** Updates the autoinc counter if the value supplied is greater than the
 current value.
@param[in,out] table Table
@param[in] value Value which was assigned to a row */
void dict_table_autoinc_update_if_greater(dict_table_t *table, uint64_t value);

/** Release the autoinc lock. */
void dict_table_autoinc_unlock(dict_table_t *table); /*!< in/out: table */

/** Update the persisted autoinc counter to specified one, we should hold
autoinc_persisted_mutex.
@param[in,out]  table   table
@param[in]      autoinc set autoinc_persisted to this value */
static inline void dict_table_autoinc_persisted_update(dict_table_t *table,
                                                       uint64_t autoinc);

/** Set the column position of autoinc column in clustered index for a table.
@param[in]      table   table
@param[in]      pos     column position in table definition */
static inline void dict_table_autoinc_set_col_pos(dict_table_t *table,
                                                  ulint pos);

/** Write redo logs for autoinc counter that is to be inserted, or to
update some existing smaller one to bigger.
@param[in,out]  table   InnoDB table object
@param[in]      value   AUTOINC counter to log
@param[in,out]  mtr     Mini-transaction
@return true if auto increment needs to be persisted to DD table buffer. */
bool dict_table_autoinc_log(dict_table_t *table, uint64_t value, mtr_t *mtr);

/** Check if a table has an autoinc counter column.
@param[in]      table   table
@return true if there is an autoinc column in the table, otherwise false. */
static inline bool dict_table_has_autoinc_col(const dict_table_t *table);

#endif /* !UNIV_HOTBACKUP */

/** Adds system columns to a table object.
@param[in,out] table Table
@param[in] heap Temporary heap */
void dict_table_add_system_columns(dict_table_t *table, mem_heap_t *heap);

#ifndef UNIV_HOTBACKUP
/** Mark if table has big rows.
@param[in,out]  table   table handler */
void dict_table_set_big_rows(dict_table_t *table) MY_ATTRIBUTE((nonnull));

/** Adds a table object to the dictionary cache.
@param[in,out]  table           table
@param[in]      can_be_evicted  true if can be evicted */
void dict_table_add_to_cache(dict_table_t *table, bool can_be_evicted);

/** Removes a table object from the dictionary cache. */
void dict_table_remove_from_cache(dict_table_t *table); /*!< in, own: table */

/** Try to invalidate an entry from the dict cache, for a partitioned table,
if any table found.
@param[in]      name    Table name */
void dict_partitioned_table_remove_from_cache(const char *name);

#ifdef UNIV_DEBUG
/** Removes a table object from the dictionary cache, for debug purpose
@param[in,out]  table           table object
@param[in]      lru_evict       true if table being evicted to make room
                                in the table LRU list */
void dict_table_remove_from_cache_debug(dict_table_t *table, bool lru_evict);
#endif /* UNIV_DEBUG */

/** Renames a table object.
 @return true if success */
[[nodiscard]] dberr_t dict_table_rename_in_cache(
    dict_table_t *table,  /*!< in/out: table */
    const char *new_name, /*!< in: new name */
    bool rename_also_foreigns);
/*!< in: in ALTER TABLE we want
to preserve the original table name
in constraints which reference it */

/** Removes an index from the dictionary cache.
@param[in,out]  table   table whose index to remove
@param[in,out]  index   index to remove, this object is destroyed and must not
be accessed by the caller afterwards */
void dict_index_remove_from_cache(dict_table_t *table, dict_index_t *index);

/** Change the id of a table object in the dictionary cache. This is used in
 DISCARD TABLESPACE. */
void dict_table_change_id_in_cache(
    dict_table_t *table, /*!< in/out: table object already in cache */
    table_id_t new_id);  /*!< in: new id to set */
/** Removes a foreign constraint struct from the dictionary cache. */
void dict_foreign_remove_from_cache(
    dict_foreign_t *foreign); /*!< in, own: foreign constraint */
/** Adds a foreign key constraint object to the dictionary cache. May free
 the object if there already is an object with the same identifier in.
 At least one of foreign table or referenced table must already be in
 the dictionary cache!
 @return DB_SUCCESS or error code */
[[nodiscard]] dberr_t dict_foreign_add_to_cache(
    dict_foreign_t *foreign,
    /*!< in, own: foreign key constraint */
    const char **col_names,
    /*!< in: column names, or NULL to use
    foreign->foreign_table->col_names */
    bool check_charsets,
    /*!< in: whether to check charset
    compatibility */
    bool can_free_fk,
    /*!< in: whether free existing FK */
    dict_err_ignore_t ignore_err);
/*!< in: error to be ignored */
/** Checks if a table is referenced by foreign keys.
 @return true if table is referenced by a foreign key */
[[nodiscard]] bool dict_table_is_referenced_by_foreign_key(
    const dict_table_t *table); /*!< in: InnoDB table */
/** Replace the index passed in with another equivalent index in the
 foreign key lists of the table.
 @return whether all replacements were found */
[[nodiscard]] bool dict_foreign_replace_index(
    dict_table_t *table, /*!< in/out: table */
    const char **col_names,
    /*!< in: column names, or NULL
    to use table->col_names */
    const dict_index_t *index); /*!< in: index to be replaced */
#endif                          /* !UNIV_HOTBACKUP */
/** Returns a table object and increments its open handle count.
 NOTE! This is a high-level function to be used mainly from outside the
 'dict' directory. Inside this directory dict_table_get_low
 is usually the appropriate function.
 @param[in] table_name Table name
 @param[in] dict_locked true=data dictionary locked
 @param[in] try_drop true=try to drop any orphan indexes after
                                 an aborted online index creation
 @param[in] ignore_err error to be ignored when loading the table
 @return table, NULL if does not exist */
[[nodiscard]] dict_table_t *dict_table_open_on_name(
    const char *table_name, bool dict_locked, bool try_drop,
    dict_err_ignore_t ignore_err);

/** Tries to find an index whose first fields are the columns in the array,
 in the same order and is not marked for deletion and is not the same
 as types_idx.
 @return matching index, NULL if not found */
[[nodiscard]] dict_index_t *dict_foreign_find_index(
    const dict_table_t *table, /*!< in: table */
    const char **col_names,
    /*!< in: column names, or NULL
    to use table->col_names */
    const char **columns, /*!< in: array of column names */
    ulint n_cols,         /*!< in: number of columns */
    const dict_index_t *types_idx,
    /*!< in: NULL or an index
    whose types the column types
    must match */
    bool check_charsets,
    /*!< in: whether to check
    charsets.  only has an effect
    if types_idx != NULL */
    ulint check_null);
/*!< in: nonzero if none of
the columns must be declared
NOT NULL */

/** Returns a virtual column's name.
@param[in]      table   target table
@param[in]      col_nr  virtual column number (nth virtual column)
@return column name or NULL if column number out of range. */
const char *dict_table_get_v_col_name(const dict_table_t *table, ulint col_nr);

/** Check if the table has a given (non_virtual) column.
@param[in]      table           table object
@param[in]      col_name        column name
@param[in]      col_nr          column number guessed, 0 as default
@return column number if the table has the specified column,
otherwise table->n_def */
ulint dict_table_has_column(const dict_table_t *table, const char *col_name,
                            ulint col_nr = 0);

/** Outputs info on a foreign key of a table in a format suitable for
 CREATE TABLE.
@param[in] file File where to print
@param[in] trx Transaction
@param[in] foreign Foreign key constraint
@param[in] add_newline Whether to add a newline */
void dict_print_info_on_foreign_key_in_create_format(FILE *file, trx_t *trx,
                                                     dict_foreign_t *foreign,
                                                     bool add_newline);

/** Tries to find an index whose first fields are the columns in the array,
 in the same order and is not marked for deletion and is not the same
 as types_idx.
 @return matching index, NULL if not found */
[[nodiscard]] bool dict_foreign_qualify_index(
    const dict_table_t *table, /*!< in: table */
    const char **col_names,
    /*!< in: column names, or NULL
    to use table->col_names */
    const char **columns,      /*!< in: array of column names */
    ulint n_cols,              /*!< in: number of columns */
    const dict_index_t *index, /*!< in: index to check */
    const dict_index_t *types_idx,
    /*!< in: NULL or an index
    whose types the column types
    must match */
    bool check_charsets,
    /*!< in: whether to check
    charsets.  only has an effect
    if types_idx != NULL */
    ulint check_null);
/*!< in: nonzero if none of
the columns must be declared
NOT NULL */

/* Skip corrupted index */
static inline void dict_table_skip_corrupt_index(dict_index_t *&index) {
  while (index && index->is_corrupted()) {
    index = index->next();
  }
}

/* Get the next non-corrupt index */
static inline void dict_table_next_uncorrupted_index(dict_index_t *&index) {
  index = index->next();
  dict_table_skip_corrupt_index(index);
}

/** Check if index is auto-generated clustered index.
@param[in]      index   index

@return true if index is auto-generated clustered index. */
static inline bool dict_index_is_auto_gen_clust(const dict_index_t *index);

/** Check whether the index is unique.
 @return nonzero for unique index, zero for other indexes */
[[nodiscard]] static inline ulint dict_index_is_unique(
    const dict_index_t *index); /*!< in: index */
/** Check whether the index is a Spatial Index.
 @return        nonzero for Spatial Index, zero for other indexes */
[[nodiscard]] static inline ulint dict_index_is_spatial(
    const dict_index_t *index); /*!< in: index */
/** Check whether the index contains a virtual column.
@param[in]      index   index
@return nonzero for index on virtual column, zero for other indexes */
static inline ulint dict_index_has_virtual(const dict_index_t *index);
/** Check whether the index is the insert buffer tree.
 @return nonzero for insert buffer, zero for other indexes */
[[nodiscard]] static inline ulint dict_index_is_ibuf(
    const dict_index_t *index); /*!< in: index */

/** Check whether the index consists of descending columns only.
@param[in]      index  index tree
@retval true if index has any descending column
@retval false if index has only ascending columns */
[[nodiscard]] static inline bool dict_index_has_desc(const dict_index_t *index);
/** Check whether the index is a secondary index or the insert buffer tree.
 @return nonzero for insert buffer, zero for other indexes */
[[nodiscard]] static inline ulint dict_index_is_sec_or_ibuf(
    const dict_index_t *index); /*!< in: index */

/** Get all the FTS indexes on a table.
@param[in]      table   table
@param[out]     indexes all FTS indexes on this table
@return number of FTS indexes */
ulint dict_table_get_all_fts_indexes(dict_table_t *table, ib_vector_t *indexes);

static inline ulint dict_table_get_n_tot_u_cols(const dict_table_t *table);

/** Gets the number of virtual columns in a table in the dictionary cache.
@param[in]      table   the table to check
@return number of virtual columns of a table */
static inline ulint dict_table_get_n_v_cols(const dict_table_t *table);

/** Check if a table has indexed virtual columns
@param[in]      table   the table to check
@return true is the table has indexed virtual columns */
static inline bool dict_table_has_indexed_v_cols(const dict_table_t *table);

#ifndef UNIV_HOTBACKUP
/** Gets the approximately estimated number of rows in the table.
 @return estimated number of rows */
[[nodiscard]] static inline uint64_t dict_table_get_n_rows(
    const dict_table_t *table); /*!< in: table */
/** Increment the number of rows in the table by one.
 Notice that this operation is not protected by any latch, the number is
 approximate. */
static inline void dict_table_n_rows_inc(
    dict_table_t *table); /*!< in/out: table */
/** Decrement the number of rows in the table by one.
 Notice that this operation is not protected by any latch, the number is
 approximate. */
static inline void dict_table_n_rows_dec(
    dict_table_t *table); /*!< in/out: table */
#endif                    /* !UNIV_HOTBACKUP */

/** Get nth virtual column according to its original MySQL table position
@param[in]      table   target table
@param[in]      col_nr  column number in MySQL Table definition
@return dict_v_col_t ptr */
dict_v_col_t *dict_table_get_nth_v_col_mysql(const dict_table_t *table,
                                             ulint col_nr);

#ifdef UNIV_DEBUG
/** Gets the nth virtual column of a table.
@param[in]      table   table
@param[in]      pos     position of virtual column
@return pointer to virtual column object */
static inline dict_v_col_t *dict_table_get_nth_v_col(const dict_table_t *table,
                                                     ulint pos);

#else /* UNIV_DEBUG */
/* Get nth virtual columns */
#define dict_table_get_nth_v_col(table, pos) ((table)->v_cols + (pos))
#endif /* UNIV_DEBUG */
/** Gets the given system column number of a table.
@param[in] table Table.
@param[in] sys   DATA_ROW_ID, ...
@return column number */
[[nodiscard]] static inline ulint dict_table_get_sys_col_no(
    const dict_table_t *table, ulint sys);
/** Check whether the table uses the compact page format.
 @return true if table uses the compact page format */
[[nodiscard]] static inline bool dict_table_is_comp(
    const dict_table_t *table); /*!< in: table */

/** Determine if a table uses atomic BLOBs (no locally stored prefix).
@param[in]      table   InnoDB table
@return whether BLOBs are atomic */
[[nodiscard]] static inline bool dict_table_has_atomic_blobs(
    const dict_table_t *table);

#ifndef UNIV_HOTBACKUP
/** Set the various values in a dict_table_t::flags pointer.
@param[in,out]  flags           Pointer to a 4 byte Table Flags
@param[in]      format          File Format
@param[in]      zip_ssize       Zip Shift Size
@param[in]      use_data_dir    Table uses DATA DIRECTORY
@param[in]      shared_space    Table uses a General Shared Tablespace */
static inline void dict_tf_set(uint32_t *flags, rec_format_t format,
                               uint32_t zip_ssize, bool use_data_dir,
                               bool shared_space);

/** Initialize a dict_table_t::flags pointer.
@param[in]      compact         Table uses Compact or greater
@param[in]      zip_ssize       Zip Shift Size (log 2 minus 9)
@param[in]      atomic_blobs    Table uses Compressed or Dynamic
@param[in]      data_dir        Table uses DATA DIRECTORY
@param[in]      shared_space    Table uses a General Shared Tablespace */
static inline uint32_t dict_tf_init(bool compact, ulint zip_ssize,
                                    bool atomic_blobs, bool data_dir,
                                    bool shared_space);

/** Convert a 32 bit integer table flags to the 32 bit FSP Flags.
Fsp Flags are written into the tablespace header at the offset
FSP_SPACE_FLAGS and are also stored in the fil_space_t::flags field.
The following chart shows the translation of the low order bit.
Other bits are the same.
========================= Low order bit ==========================
                    | REDUNDANT | COMPACT | COMPRESSED | DYNAMIC
dict_table_t::flags |     0     |    1    |     1      |    1
fil_space_t::flags  |     0     |    0    |     1      |    1
==================================================================
@param[in]      table_flags     dict_table_t::flags
@return tablespace flags (fil_space_t::flags) */
uint32_t dict_tf_to_fsp_flags(uint32_t table_flags);

/** Extract the page size info from table flags.
@param[in]      flags   flags
@return a structure containing the compressed and uncompressed
page sizes and a boolean indicating if the page is compressed. */
static inline const page_size_t dict_tf_get_page_size(uint32_t flags);

#endif /* !UNIV_HOTBACKUP */

/** Determine the extent size (in pages) for the given table
@param[in]      table   the table whose extent size is being
                        calculated.
@return extent size in pages (256, 128 or 64) */
page_no_t dict_table_extent_size(const dict_table_t *table);

/** Get the table page size.
@param[in]      table   table
@return compressed page size, or 0 if not compressed */
[[nodiscard]] static inline const page_size_t dict_table_page_size(
    const dict_table_t *table);

#ifndef UNIV_HOTBACKUP
/** Obtain exclusive locks on all index trees of the table. This is to prevent
 accessing index trees while InnoDB is updating internal metadata for
 operations such as FLUSH TABLES. */
static inline void dict_table_x_lock_indexes(
    dict_table_t *table); /*!< in: table */
/** Release the exclusive locks on all index tree. */
static inline void dict_table_x_unlock_indexes(
    dict_table_t *table); /*!< in: table */
/** Check if the table has an FTS index.
 @return true if table has an FTS index */
[[nodiscard]] static inline bool dict_table_has_fts_index(
    dict_table_t *table); /*!< in: table */
#ifdef UNIV_DEBUG
/** Validate no active background threads to cause purge or rollback
 operations. */
void dict_validate_no_purge_rollback_threads();
#endif /* UNIV_DEBUG */
#endif /* !UNIV_HOTBACKUP */
/** Checks if a column is in the ordering columns of the clustered index of a
 table. Column prefixes are treated like whole columns.
 @return true if the column, or its prefix, is in the clustered key */
[[nodiscard]] bool dict_table_col_in_clustered_key(
    const dict_table_t *table, /*!< in: table */
    ulint n);                  /*!< in: column number */
/** Copies types of virtual columns contained in table to tuple and sets all
fields of the tuple to the SQL NULL value.  This function should
be called right after dtuple_create().
@param[in,out]  tuple   data tuple
@param[in]      table   table */
void dict_table_copy_v_types(dtuple_t *tuple, const dict_table_t *table);
/** Copies types of columns contained in table to tuple and sets all
 fields of the tuple to the SQL NULL value.  This function should
 be called right after dtuple_create(). */
void dict_table_copy_types(dtuple_t *tuple, /*!< in/out: data tuple */
                           const dict_table_t *table); /*!< in: table */
#ifndef UNIV_HOTBACKUP
/********************************************************************
Wait until all the background threads of the given table have exited, i.e.,
bg_threads == 0. Note: bg_threads_mutex must be reserved when
calling this. */
void dict_table_wait_for_bg_threads_to_exit(
    dict_table_t *table,              /* in: table */
    std::chrono::microseconds delay); /* in: time to wait between
                         checks of bg_threads. */

/** Look up an index among already opened tables. Does not attempt to open
tables that are not available in the dictionary cache.  This behaviour is fine
for information schema's scenarios - use dd_table_open_on_id() if you need
to access index meta-data reliably.
@param[in]      id      index identifier
@return index or NULL if not found */
[[nodiscard]] const dict_index_t *dict_index_find(const index_id_t &id);

/** Make room in the table cache by evicting an unused table. The unused table
 should not be part of FK relationship and currently not used in any user
 transaction. There is no guarantee that it will remove a table.
 @return number of tables evicted. */
ulint dict_make_room_in_cache(
    ulint max_tables, /*!< in: max tables allowed in cache */
    ulint pct_check); /*!< in: max percent to check */

constexpr uint32_t BIG_ROW_SIZE = 1024;

/** Adds an index to the dictionary cache.
@param[in,out]  table   table on which the index is
@param[in,out]  index   index; NOTE! The index memory
                        object is freed in this function!
@param[in]      page_no root page number of the index
@param[in]      strict  true=refuse to create the index
                        if records could be too big to fit in
                        an B-tree page
@return DB_SUCCESS, DB_TOO_BIG_RECORD, or DB_CORRUPTION */
[[nodiscard]] dberr_t dict_index_add_to_cache(dict_table_t *table,
                                              dict_index_t *index,
                                              page_no_t page_no, bool strict);

/** Clears the virtual column's index list before index is being freed.
@param[in]  index   Index being freed */
void dict_index_remove_from_v_col_list(dict_index_t *index);

/** Adds an index to the dictionary cache, with possible indexing newly
added column.
@param[in,out]  table   table on which the index is
@param[in,out]  index   index; NOTE! The index memory
                        object is freed in this function!
@param[in]      add_v   new virtual column that being added along with
                        an add index call
@param[in]      page_no root page number of the index
@param[in]      strict  true=refuse to create the index
                        if records could be too big to fit in
                        an B-tree page
@return DB_SUCCESS, DB_TOO_BIG_RECORD, or DB_CORRUPTION */
[[nodiscard]] dberr_t dict_index_add_to_cache_w_vcol(
    dict_table_t *table, dict_index_t *index, const dict_add_v_col_t *add_v,
    page_no_t page_no, bool strict);
#endif /* !UNIV_HOTBACKUP */
/** Gets the number of fields in the internal representation of an index,
 including fields added by the dictionary system.
 @return number of fields */
[[nodiscard]] static inline ulint dict_index_get_n_fields(
    const dict_index_t *index); /*!< in: an internal
                               representation of index (in
                               the dictionary cache) */
/** Gets the number of fields in the internal representation of an index
 that uniquely determine the position of an index entry in the index, if
 we do not take multiversioning into account: in the B-tree use the value
 returned by dict_index_get_n_unique_in_tree.
 @return number of fields */
[[nodiscard]] static inline ulint dict_index_get_n_unique(
    const dict_index_t *index); /*!< in: an internal representation
                               of index (in the dictionary cache) */
/** Gets the number of fields in the internal representation of an index
 which uniquely determine the position of an index entry in the index, if
 we also take multiversioning into account.
 @return number of fields */
[[nodiscard]] static inline ulint dict_index_get_n_unique_in_tree(
    const dict_index_t *index); /*!< in: an internal representation
                               of index (in the dictionary cache) */

/** The number of fields in the nonleaf page of spatial index, except
the page no field. */
constexpr uint32_t DICT_INDEX_SPATIAL_NODEPTR_SIZE = 1;
/**
Gets the number of fields on nonleaf page level in the internal representation
of an index which uniquely determine the position of an index entry in the
index, if we also take multiversioning into account. Note, it doesn't
include page no field.
@param[in]      index   index
@return number of fields */
[[nodiscard]] static inline uint16_t dict_index_get_n_unique_in_tree_nonleaf(
    const dict_index_t *index);
/** Gets the number of user-defined ordering fields in the index. In the
 internal representation we add the row id to the ordering fields to make all
 indexes unique, but this function returns the number of fields the user defined
 in the index as ordering fields.
 @return number of fields */
[[nodiscard]] static inline ulint dict_index_get_n_ordering_defined_by_user(
    const dict_index_t *index); /*!< in: an internal representation
                               of index (in the dictionary cache) */
/** Returns true if the index contains a column or a prefix of that column.
@param[in]      index           index
@param[in]      n               column number
@param[in]      is_virtual      whether it is a virtual col
@return true if contains the column or its prefix */
[[nodiscard]] bool dict_index_contains_col_or_prefix(const dict_index_t *index,
                                                     ulint n, bool is_virtual);
/** Looks for a matching field in an index. The column has to be the same. The
 column in index must be complete, or must contain a prefix longer than the
 column in index2. That is, we must be able to construct the prefix in index2
 from the prefix in index.
 @return position in internal representation of the index;
 ULINT_UNDEFINED if not contained */
[[nodiscard]] ulint dict_index_get_nth_field_pos(
    const dict_index_t *index,  /*!< in: index from which to search */
    const dict_index_t *index2, /*!< in: index */
    ulint n);                   /*!< in: field number in index2 */
/** Looks for non-virtual column n position in the clustered index.
 @return position in internal representation of the clustered index */
[[nodiscard]] ulint dict_table_get_nth_col_pos(
    const dict_table_t *table, /*!< in: table */
    ulint n);                  /*!< in: column number */

/** Get the innodb column position for a non-virtual column according to
its original MySQL table position n
@param[in]      table   table
@param[in]      n       MySQL column position
@return column position in InnoDB */
ulint dict_table_mysql_pos_to_innodb(const dict_table_t *table, ulint n);

/** Copies types of fields contained in index to tuple.
@param[in,out] tuple Data tuple
@param[in] index Index
@param[in] n_fields Number of field types to copy */
void dict_index_copy_types(dtuple_t *tuple, const dict_index_t *index,
                           ulint n_fields);

#ifdef UNIV_DEBUG
/** Checks that a tuple has n_fields_cmp value in a sensible range, so that
 no comparison can occur with the page number field in a node pointer.
 @return true if ok */
[[nodiscard]] bool dict_index_check_search_tuple(
    const dict_index_t *index, /*!< in: index tree */
    const dtuple_t *tuple);    /*!< in: tuple used in a search */
/** Whether and when to allow temporary index names */
enum check_name {
  /** Require all indexes to be complete. */
  CHECK_ALL_COMPLETE,
  /** Allow aborted online index creation. */
  CHECK_ABORTED_OK,
  /** Allow partial indexes to exist. */
  CHECK_PARTIAL_OK
};

/** Check for duplicate index entries in a table [using the index name]
@param[in] table Check for dup indexes in this table
@param[in] check Whether and when to allow temporary index names */
void dict_table_check_for_dup_indexes(const dict_table_t *table,
                                      enum check_name check);

/** Check if a table is a temporary table with compressed row format,
we should always expect false.
@param[in]      table   table
@return true if it's a compressed temporary table, false otherwise */
inline bool dict_table_is_compressed_temporary(const dict_table_t *table);
#endif /* UNIV_DEBUG */
/** Builds a node pointer out of a physical record and a page number.
 @return own: node pointer */
[[nodiscard]] dtuple_t *dict_index_build_node_ptr(
    const dict_index_t *index, /*!< in: index */
    const rec_t *rec,          /*!< in: record for which
                               to build node  pointer */
    page_no_t page_no,         /*!< in: page number to
                               put in node pointer */
    mem_heap_t *heap,          /*!< in: memory heap where
                               pointer created */
    ulint level);              /*!< in: level of rec in tree:
                              0 means leaf level */
/** Copies an initial segment of a physical record, long enough to specify an
index entry uniquely. In case buf is nullptr or the buf_size is not enough to
store the prefix, the new buffer will be allocated.
@param[in]      index   index
@param[in]      rec     record for which to copy prefix
@param[out]     n_fields        number of fields copied
@param[in,out]  buf      memory buffer for the copied prefix, or nullptr
@param[in,out]  buf_size        buffer size, size of allocated buffer
@return pointer to the prefix record */
[[nodiscard]] rec_t *dict_index_copy_rec_order_prefix(const dict_index_t *index,
                                                      const rec_t *rec,
                                                      ulint *n_fields,
                                                      byte **buf,
                                                      size_t *buf_size);
/** Builds a typed data tuple out of a physical record.
 @return own: data tuple */
[[nodiscard]] dtuple_t *dict_index_build_data_tuple(
    dict_index_t *index, /*!< in: index */
    rec_t *rec,          /*!< in: record for which to build data tuple */
    ulint n_fields,      /*!< in: number of data fields */
    mem_heap_t *heap);   /*!< in: memory heap where tuple created */
/** Gets the space id of the root of the index tree.
 @return space id */
[[nodiscard]] static inline space_id_t dict_index_get_space(
    const dict_index_t *index); /*!< in: index */

/** Sets the space id of the root of the index tree.
@param[in,out]  index   index
@param[in]      space   space id */
static inline void dict_index_set_space(dict_index_t *index, space_id_t space);

/** Gets the page number of the root of the index tree.
 @return page number */
[[nodiscard]] static inline page_no_t dict_index_get_page(
    const dict_index_t *tree); /*!< in: index */
/** Gets the read-write lock of the index tree.
 @return read-write lock */
[[nodiscard]] static inline rw_lock_t *dict_index_get_lock(
    dict_index_t *index); /*!< in: index */
/** Returns free space reserved for future updates of records. This is
 relevant only in the case of many consecutive inserts, as updates
 which make the records bigger might fragment the index.
 @return number of free bytes on page, reserved for updates */
static inline ulint dict_index_get_space_reserve(void);

/* Online index creation @{ */
/** Gets the status of online index creation.
 @return the status */
[[nodiscard]] static inline enum online_index_status
dict_index_get_online_status(
    const dict_index_t *index); /*!< in: secondary index */

/** Sets the status of online index creation.
@param[in,out]  index   index
@param[in]      status  status */
static inline void dict_index_set_online_status(
    dict_index_t *index, enum online_index_status status);

/** Determines if a secondary index is being or has been created online,
 or if the table is being rebuilt online, allowing concurrent modifications
 to the table.
 @retval true if the index is being or has been built online, or
 if this is a clustered index and the table is being or has been rebuilt online
 @retval false if the index has been created or the table has been
 rebuilt completely */
[[nodiscard]] static inline bool dict_index_is_online_ddl(
    const dict_index_t *index); /*!< in: index */
/** Calculates the minimum record length in an index. */
[[nodiscard]] ulint dict_index_calc_min_rec_len(
    const dict_index_t *index); /*!< in: index */
/** Reserves the dictionary system mutex for MySQL. */
void dict_mutex_enter_for_mysql(void);
/** Releases the dictionary system mutex for MySQL. */
void dict_mutex_exit_for_mysql(void);

#ifndef UNIV_HOTBACKUP
/** Create a dict_table_t's stats latch or delay for lazy creation.
This function is only called from either single threaded environment
or from a thread that has not shared the table object with other threads.
@param[in,out]  table   table whose stats latch to create
@param[in]      enabled if false then the latch is disabled
and dict_table_stats_lock()/unlock() become noop on this table. */
void dict_table_stats_latch_create(dict_table_t *table, bool enabled);

/** Destroy a dict_table_t's stats latch.
This function is only called from either single threaded environment
or from a thread that has not shared the table object with other threads.
@param[in,out]  table   table whose stats latch to destroy */
void dict_table_stats_latch_destroy(dict_table_t *table);

/** Lock the appropriate latch to protect a given table's statistics.
@param[in]      table           table whose stats to lock
@param[in]      latch_mode      RW_S_LATCH or RW_X_LATCH */
void dict_table_stats_lock(dict_table_t *table, ulint latch_mode);

/** Unlock the latch that has been locked by dict_table_stats_lock().
@param[in]      table           table whose stats to unlock
@param[in]      latch_mode      RW_S_LATCH or RW_X_LATCH */
void dict_table_stats_unlock(dict_table_t *table, ulint latch_mode);

/** Checks if the database name in two table names is the same.
 @return true if same db name */
[[nodiscard]] bool dict_tables_have_same_db(
    const char *name1,  /*!< in: table name in the
           form dbname '/' tablename */
    const char *name2); /*!< in: table name in the
                       form dbname '/' tablename */
/** Get an index by name.
@param[in]      table           the table where to look for the index
@param[in]      name            the index name to look for
@param[in]      committed       true=search for committed,
false=search for uncommitted
@return index, NULL if does not exist */
[[nodiscard]] dict_index_t *dict_table_get_index_on_name(dict_table_t *table,
                                                         const char *name,
                                                         bool committed = true);
/** Get an index by name.
@param[in]      table           the table where to look for the index
@param[in]      name            the index name to look for
@param[in]      committed       true=search for committed,
false=search for uncommitted
@return index, NULL if does not exist */
inline const dict_index_t *dict_table_get_index_on_name(
    const dict_table_t *table, const char *name, bool committed = true) {
  return (dict_table_get_index_on_name(const_cast<dict_table_t *>(table), name,
                                       committed));
}

/** Check whether a column exists in an FTS index.
@param[in] indexes    Vector containing only FTS indexes
@param[in] col_no     Column number to search for.
@param[in] is_virtual Whether it is a virtual column.
@return ULINT_UNDEFINED if no match else the offset within the vector. */
[[nodiscard]] static inline ulint dict_table_is_fts_column(ib_vector_t *indexes,
                                                           ulint col_no,
                                                           bool is_virtual);
/** Prevent table eviction by moving a table to the non-LRU list from the
 LRU list if it is not already there. */
static inline void dict_table_prevent_eviction(
    dict_table_t *table); /*!< in: table to prevent eviction */

/** Allow the table to be evicted by moving a table to the LRU list from
the non-LRU list if it is not already there.
@param[in]      table   InnoDB table object can be evicted */
static inline void dict_table_allow_eviction(dict_table_t *table);

/** Move this table to non-LRU list for DDL operations if it's
currently not there. This also prevents later opening table via DD objects,
when the table name in InnoDB doesn't match with DD object.
@param[in,out]  table   Table to put in non-LRU list */
static inline void dict_table_ddl_acquire(dict_table_t *table);

/** Move this table to LRU list after DDL operations if it was moved
to non-LRU list
@param[in,out]  table   Table to put in LRU list */
static inline void dict_table_ddl_release(dict_table_t *table);

/** Move a table to the non LRU end of the LRU list. */
void dict_table_move_from_lru_to_non_lru(
    dict_table_t *table); /*!< in: table to move from LRU to non-LRU */

/** Move a table to the LRU end from the non LRU list.
@param[in]      table   InnoDB table object */
void dict_table_move_from_non_lru_to_lru(dict_table_t *table);

/** Move to the most recently used segment of the LRU list. */
void dict_move_to_mru(dict_table_t *table); /*!< in: table to move to MRU */

/** Maximum number of columns in a foreign key constraint. Please Note MySQL
has a much lower limit on the number of columns allowed in a foreign key
constraint */
constexpr uint32_t MAX_NUM_FK_COLUMNS = 500;

/* Buffers for storing detailed information about the latest foreign key
and unique key errors */
extern FILE *dict_foreign_err_file;
extern ib_mutex_t dict_foreign_err_mutex; /* mutex protecting the
                                          foreign key error messages */
#endif                                    /* !UNIV_HOTBACKUP */

/** the dictionary system */
extern dict_sys_t *dict_sys;
#ifndef UNIV_HOTBACKUP
/** the data dictionary rw-latch protecting dict_sys */
extern rw_lock_t *dict_operation_lock;

/** Forward declaration */
class DDTableBuffer;
#endif /* !UNIV_HOTBACKUP */
struct dict_persist_t;

/** the dictionary persisting structure */
extern dict_persist_t *dict_persist;

/* Dictionary system struct */
struct dict_sys_t {
#ifndef UNIV_HOTBACKUP
  DictSysMutex mutex;          /*!< mutex protecting the data
                               dictionary; protects also the
                               disk-based dictionary system tables;
                               this mutex serializes CREATE TABLE
                               and DROP TABLE, as well as reading
                               the dictionary data for a table from
                               system tables */
#endif                         /* !UNIV_HOTBACKUP */
  row_id_t row_id;             /*!< the next row id to assign;
                               NOTE that at a checkpoint this
                               must be written to the dict system
                               header and flushed to a file; in
                               recovery this must be derived from
                               the log records */
  hash_table_t *table_hash;    /*!< hash table of the tables, based
                               on name */
  hash_table_t *table_id_hash; /*!< hash table of the tables, based
                               on id */
  size_t size;                 /*!< varying space in bytes occupied
                               by the data dictionary table and
                               index objects */
  /** Handler to sys_* tables, they're only for upgrade */
  dict_table_t *sys_tables;  /*!< SYS_TABLES table */
  dict_table_t *sys_columns; /*!< SYS_COLUMNS table */
  dict_table_t *sys_indexes; /*!< SYS_INDEXES table */
  dict_table_t *sys_fields;  /*!< SYS_FIELDS table */
  dict_table_t *sys_virtual; /*!< SYS_VIRTUAL table */

  /** Permanent handle to mysql.innodb_table_stats */
  dict_table_t *table_stats;
  /** Permanent handle to mysql.innodb_index_stats */
  dict_table_t *index_stats;
  /** Permanent handle to mysql.innodb_ddl_log */
  dict_table_t *ddl_log;
  /** Permanent handle to mysql.innodb_dynamic_metadata */
  dict_table_t *dynamic_metadata;
  using Table_LRU_list_base = UT_LIST_BASE_NODE_T(dict_table_t, table_LRU);

  /** List of tables that can be evicted from the cache */
  Table_LRU_list_base table_LRU;
  /** List of tables that can't be evicted from the cache */
  Table_LRU_list_base table_non_LRU;

  /** Iterate each table.
  @tparam Functor visitor
  @param[in,out]  functor to be invoked on each table */
  template <typename Functor>
  void for_each_table(Functor &functor) {
    mutex_enter(&mutex);

    hash_table_t *hash = table_id_hash;
    const auto n_cells = hash->get_n_cells();
    for (ulint i = 0; i < n_cells; i++) {
      for (dict_table_t *table =
               static_cast<dict_table_t *>(hash_get_first(hash, i));
           table;
           table = static_cast<dict_table_t *>(HASH_GET_NEXT(id_hash, table))) {
        functor(table);
      }
    }

    mutex_exit(&mutex);
  }

  /** Check if a tablespace id is a reserved tablespace ID
  @param[in]  space  tablespace id to check
  @return true if a reserved tablespace id, otherwise false */
  static bool is_reserved(space_id_t space) {
    return (space >= dict_sys_t::s_reserved_space_id);
  }

  /** Set of ids of DD tables */
  static std::set<dd::Object_id> s_dd_table_ids;

  /** Check if a table is hardcoded. it only includes the dd tables
  @param[in]    id      table ID
  @retval true  if the table is a persistent hard-coded table
                  (dict_table_t::is_temporary() will not hold)
  @retval false if the table is not hard-coded
                  (it can be persistent or temporary) */
  static bool is_dd_table_id(table_id_t id) {
    return (s_dd_table_ids.find(id) != s_dd_table_ids.end());
  }

  /** The first ID of the redo log pseudo-tablespace */
  static constexpr space_id_t s_log_space_id = 0xFFFFFFF0UL;

  /** Use maximum UINT value to indicate invalid space ID. */
  static constexpr space_id_t s_invalid_space_id = 0xFFFFFFFF;

  /** The data dictionary tablespace ID. */
  static constexpr space_id_t s_dict_space_id = 0xFFFFFFFE;

  /** The innodb_temporary tablespace ID. */
  static constexpr space_id_t s_temp_space_id = 0xFFFFFFFD;

  /** The number of space IDs dedicated to each undo tablespace */
  static constexpr space_id_t s_undo_space_id_range = 400000;

  /** The lowest undo tablespace ID. */
  static constexpr space_id_t s_min_undo_space_id =
      s_log_space_id - (FSP_MAX_UNDO_TABLESPACES * s_undo_space_id_range);

  /** The highest undo tablespace ID. */
  static constexpr space_id_t s_max_undo_space_id = s_log_space_id - 1;

  /** Start space_ids for temporary tablespaces. */
  static constexpr space_id_t s_max_temp_space_id = s_min_undo_space_id - 1;

  /** The number of space IDs dedicated to temporary tablespaces */
  static constexpr space_id_t s_temp_space_id_range = 400000;

  /** Lowest temporary general space id */
  static constexpr space_id_t s_min_temp_space_id =
      s_min_undo_space_id - s_temp_space_id_range;

  /** The first reserved tablespace ID */
  static constexpr space_id_t s_reserved_space_id = s_min_temp_space_id;

  /** The dd::Tablespace::id of the dictionary tablespace. */
  static constexpr dd::Object_id s_dd_dict_space_id = 1;

  /** The dd::Tablespace::id of innodb_system. */
  static constexpr dd::Object_id s_dd_sys_space_id = 2;

  /** The dd::Tablespace::id of innodb_temporary. */
  static constexpr dd::Object_id s_dd_temp_space_id = 3;

  /** The name of the data dictionary tablespace. */
  static const char *s_dd_space_name;

  /** The file name of the data dictionary tablespace. */
  static const char *s_dd_space_file_name;

  /** The name of the hard-coded system tablespace. */
  static const char *s_sys_space_name;

  /** The name of the predefined temporary tablespace. */
  static const char *s_temp_space_name;

  /** The file name of the predefined temporary tablespace. */
  static const char *s_temp_space_file_name;

  /** The hard-coded tablespace name innodb_file_per_table. */
  static const char *s_file_per_table_name;

  /** These two undo tablespaces cannot be dropped. */
  static const char *s_default_undo_space_name_1;
  static const char *s_default_undo_space_name_2;

  /** The table ID of mysql.innodb_dynamic_metadata */
  static constexpr table_id_t s_dynamic_meta_table_id = 2;

  /** The clustered index ID of mysql.innodb_dynamic_metadata */
  static constexpr space_index_t s_dynamic_meta_index_id = 2;
};

/** Structure for persisting dynamic metadata of data dictionary */
struct dict_persist_t {
#ifndef UNIV_HOTBACKUP
  /** Write dynamic metadata to DD buffer table immediately when such data is
  generated. By default, the metadata is first written to redo log and then to
  the DD buffer table during checkpoint. This is going to hurt auto increment
  performance and Currently enabled by clone for short time to eliminate
  dependency with dynamic metadata recovered from redo log. */
  class Enable_immediate {
   public:
    /** Constructor to immediate persisting mode.
    @param[in,out]      persister       dictionary persister */
    Enable_immediate(dict_persist_t *persister);

    /** Destructor to switch back to default mode. */
    ~Enable_immediate();

    /** Disable copy construction */
    Enable_immediate(Enable_immediate const &) = delete;

    /** Disable assignment */
    Enable_immediate &operator=(Enable_immediate const &) = delete;

   private:
    /** Dictionary persister */
    dict_persist_t *m_persister;
  };

  /** @return true if need to write dynamic metadata to DD buffer table
  immediately after logging. */
  bool check_persist_immediately() const {
    return m_persist_immediately.load();
  }

  /** List of tables whose dirty_status are marked as METADATA_DIRTY,
  or METADATA_BUFFERED. It's protected by the mutex */
  UT_LIST_BASE_NODE_T(dict_table_t, dirty_dict_tables)
  dirty_dict_tables;
#endif

  /** Mutex to protect data in this structure, also the
  dict_table_t::dirty_status and
  dict_table_t::in_dirty_dict_tables_list
  This mutex should be low-level one so that it can be used widely
  when necessary, so its level had to be above SYNC_LOG. However,
  after this mutex, persister may have to access B-tree and require
  tree latch, the latch level of this mutex then has to be right
  before the SYNC_INDEX_TREE. */
  ib_mutex_t mutex;

  /** Number of the tables which are of status METADATA_DIRTY.
  It's protected by the mutex */
  std::atomic<uint32_t> num_dirty_tables;

  /** If set, dynamic metadata is saved to DD buffer table immediately.
  Currently we consider only auto increment PM_TABLE_AUTO_INC. Corrupt
  index PM_INDEX_CORRUPTED is not needed to be saved immediately. */
  std::atomic<bool> m_persist_immediately;

#ifndef UNIV_HOTBACKUP
  /** DDTableBuffer table for persistent dynamic metadata */
  DDTableBuffer *table_buffer;
#endif /* !UNIV_HOTBACKUP */

  /** Collection of instances to persist dynamic metadata */
  Persisters *persisters;
};

#ifndef UNIV_HOTBACKUP
/** dummy index for ROW_FORMAT=REDUNDANT supremum and infimum records */
extern dict_index_t *dict_ind_redundant;

/** Inits dict_ind_redundant. */
void dict_ind_init(void);

/** Converts a database and table name from filesystem encoding (e.g.
"@code d@i1b/a@q1b@1Kc @endcode", same format as used in  dict_table_t::name)
in two strings in UTF8MB3 encoding (e.g. dцb and aюbØc). The output buffers must
be at least MAX_DB_UTF8MB3_LEN and MAX_TABLE_UTF8MB3_LEN bytes.
@param[in]      db_and_table    database and table names,
                                e.g. "@code d@i1b/a@q1b@1Kc @endcode"
@param[out]     db_utf8mb3         database name, e.g. dцb
@param[in]      db_utf8mb3_size    db_utf8mb3 size
@param[out]     table_utf8mb3      table name, e.g. aюbØc
@param[in]      table_utf8mb3_size table_utf8mb3 size */
void dict_fs2utf8(const char *db_and_table, char *db_utf8mb3,
                  size_t db_utf8mb3_size, char *table_utf8mb3,
                  size_t table_utf8mb3_size);

/** Resize the hash tables based on the current buffer pool size. */
void dict_resize();

/** Wrapper for the mysql.innodb_dynamic_metadata used to buffer the persistent
dynamic metadata.
This should be a table with only clustered index, no delete-marked records,
no locking, no undo logging, no purge, no adaptive hash index.
We should always use low level btr functions to access and modify the table.
Accessing this table should be protected by dict_sys->mutex */
class DDTableBuffer {
 public:
  /** Default constructor */
  DDTableBuffer();

  /** Destructor */
  ~DDTableBuffer();

  /** Replace the dynamic metadata for a specific table
  @param[in]    id              table id
  @param[in]    version         table dynamic metadata version
  @param[in]    metadata        the metadata we want to replace
  @param[in]    len             the metadata length
  @return DB_SUCCESS or error code */
  dberr_t replace(table_id_t id, uint64_t version, const byte *metadata,
                  size_t len);

  /** Remove the whole row for a specific table
  @param[in]    id      table id
  @return DB_SUCCESS or error code */
  dberr_t remove(table_id_t id);

  /** Truncate the table. We can call it after all the dynamic
  metadata has been written back to DD table */
  void truncate(void);

  /** Get the buffered metadata for a specific table, the caller
  has to delete the returned std::string object by ut::delete_
  @param[in]    id      table id
  @param[out]   version table dynamic metadata version
  @return the metadata saved in a vector object, if nothing, the
  vector would be empty */
  std::vector<byte> get(table_id_t id, uint64_t *version);

 private:
  /** Initialize m_index, the in-memory clustered index of the table
  and two tuples used in this class */
  void init();

  /** Open the mysql.innodb_dynamic_metadata when DD is not fully up */
  void open();

  /** Create the search and replace tuples */
  void create_tuples();

  /** Initialize the id field of tuple
  @param[out]   tuple   the tuple to be initialized
  @param[in]    id      table id */
  void init_tuple_with_id(dtuple_t *tuple, table_id_t id);

  /** Free the things initialized in init() */
  void close();

  /** Prepare for a update on METADATA field
  @param[in]    entry   clustered index entry to replace rec
  @param[in]    rec     clustered index record
  @return update vector of differing fields without system columns,
  or NULL if there isn't any different field */
  upd_t *update_set_metadata(const dtuple_t *entry, const rec_t *rec);

 private:
  /** The clustered index of this system table */
  dict_index_t *m_index;

  /** The heap used for dynamic allocations, which should always
  be freed before return */
  mem_heap_t *m_dynamic_heap;

  /** The heap used during replace() operation, which should always
  be freed before return */
  mem_heap_t *m_replace_heap;

  /** The heap used to create the search tuple and replace tuple */
  mem_heap_t *m_heap;

  /** The tuple used to search for specified table, it's protected
  by dict_persist->mutex */
  dtuple_t *m_search_tuple;

  /** The tuple used to replace for specified table, it's protected
  by dict_persist->mutex */
  dtuple_t *m_replace_tuple;

 private:
  /** Column number of mysql.innodb_dynamic_metadata.table_id */
  static constexpr unsigned TABLE_ID_COL_NO = 0;

  /** Column number of mysql.innodb_dynamic_metadata.version */
  static constexpr unsigned VERSION_COL_NO = 1;

  /** Column number of mysql.innodb_dynamic_metadata.metadata */
  static constexpr unsigned METADATA_COL_NO = 2;

  /** Number of user columns */
  static constexpr unsigned N_USER_COLS = METADATA_COL_NO + 1;

  /** Number of columns */
  static constexpr unsigned N_COLS = N_USER_COLS + DATA_N_SYS_COLS;

  /** Clustered index field number of
  mysql.innodb_dynamic_metadata.table_id */
  static constexpr unsigned TABLE_ID_FIELD_NO = TABLE_ID_COL_NO;

  /** Clustered index field number of
  mysql.innodb_dynamic_metadata.version */
  static constexpr unsigned VERSION_FIELD_NO = VERSION_COL_NO + 2;

  /** Clustered index field number of
  mysql.innodb_dynamic_metadata.metadata
  Plusing 2 here skips the DATA_TRX_ID and DATA_ROLL_PTR fields */
  static constexpr unsigned METADATA_FIELD_NO = METADATA_COL_NO + 2;

  /** Number of fields in the clustered index */
  static constexpr unsigned N_FIELDS = METADATA_FIELD_NO + 1;
};

/** Mark the dirty_status of a table as METADATA_DIRTY, and add it to the
dirty_dict_tables list if necessary.
@param[in,out]  table           table */
void dict_table_mark_dirty(dict_table_t *table);
#endif /* !UNIV_HOTBACKUP */

/** Flags an index corrupted in the data dictionary cache only. This
is used to mark a corrupted index when index's own dictionary
is corrupted, and we would force to load such index for repair purpose.
Besides, we have to write a redo log.
We don't want to hold dict_sys->mutex here, so that we can set index as
corrupted in some low-level functions. We would only set the flags from
not corrupted to corrupted when server is running, so it should be safe
to set it directly.
@param[in,out]  index           index, must not be NULL */
void dict_set_corrupted(dict_index_t *index) UNIV_COLD;

#ifndef UNIV_HOTBACKUP

/** Check if there is any latest persistent dynamic metadata recorded
in DDTableBuffer table of the specific table. If so, read the metadata and
update the table object accordingly. It's used when loading table.
@param[in]      table           table object */
void dict_table_load_dynamic_metadata(dict_table_t *table);

/** Check if any table has any dirty persistent data, if so
write dirty persistent data of table to mysql.innodb_dynamic_metadata
accordingly. */
void dict_persist_to_dd_table_buffer();

/** Sets merge_threshold in the SYS_INDEXES
@param[in,out]  index           index
@param[in]      merge_threshold value to set */
void dict_index_set_merge_threshold(dict_index_t *index, ulint merge_threshold);

#ifdef UNIV_DEBUG
/** Sets merge_threshold for all indexes in dictionary cache for debug.
@param[in]      merge_threshold_all     value to set for all indexes */
void dict_set_merge_threshold_all_debug(uint merge_threshold_all);
#endif /* UNIV_DEBUG */

/** Validate the table flags.
@param[in]      flags   Table flags
@return true if valid. */
static inline bool dict_tf_is_valid(uint32_t flags);

/** Validate both table flags and table flags2 and make sure they
are compatible.
@param[in]      flags   Table flags
@param[in]      flags2  Table flags2
@return true if valid. */
static inline bool dict_tf2_is_valid(uint32_t flags, uint32_t flags2);

/** Check if the tablespace for the table has been discarded.
 @return true if the tablespace has been discarded. */
[[nodiscard]] static inline bool dict_table_is_discarded(
    const dict_table_t *table); /*!< in: table to check */

/** Check whether the table is DDTableBuffer. See class DDTableBuffer
@param[in]      table   table to check
@return true if this is a DDTableBuffer table. */
static inline bool dict_table_is_table_buffer(const dict_table_t *table);

/** Check if the table is in a shared tablespace (System or General).
@param[in]      table   table to check
@return true if table is a shared tablespace, false if not. */
[[nodiscard]] static inline bool dict_table_in_shared_tablespace(
    const dict_table_t *table);

/** Check whether locking is disabled for this table.
Currently this is done for intrinsic table as their visibility is limited
to the connection and the DDTableBuffer as it's protected by
dict_persist->mutex.

@param[in]      table   table to check
@return true if locking is disabled. */
[[nodiscard]] static inline bool dict_table_is_locking_disabled(
    const dict_table_t *table);

/** Turn-off redo-logging if temporary table.
@param[in]      table   Table to check
@param[out]     mtr     Mini-transaction */
static inline void dict_disable_redo_if_temporary(const dict_table_t *table,
                                                  mtr_t *mtr);

/** Get table session row-id and increment the row-id counter for next use.
@param[in,out]  table   table handler
@return next table local row-id. */
static inline row_id_t dict_table_get_next_table_sess_row_id(
    dict_table_t *table);

/** Get table session trx-id and increment the trx-id counter for next use.
@param[in,out]  table   table handler
@return next table local trx-id. */
static inline trx_id_t dict_table_get_next_table_sess_trx_id(
    dict_table_t *table);

/** Get current session trx-id.
@param[in]      table   table handler
@return table local trx-id. */
static inline trx_id_t dict_table_get_curr_table_sess_trx_id(
    const dict_table_t *table);

/** This function should be called whenever a page is successfully
 compressed. Updates the compression padding information. */
void dict_index_zip_success(
    dict_index_t *index); /*!< in/out: index to be updated. */
/** This function should be called whenever a page compression attempt
 fails. Updates the compression padding information. */
void dict_index_zip_failure(
    dict_index_t *index); /*!< in/out: index to be updated. */
/** Return the optimal page size, for which page will likely compress.
 @return page size beyond which page may not compress*/
[[nodiscard]] ulint dict_index_zip_pad_optimal_page_size(
    dict_index_t *index); /*!< in: index for which page size
                         is requested */
/** Convert table flag to row format string.
 @return row format name */
const char *dict_tf_to_row_format_string(
    uint32_t table_flag); /*!< in: row format setting */
/** Return maximum size of the node pointer record.
 @return maximum size of the record in bytes */
[[nodiscard]] ulint dict_index_node_ptr_max_size(
    const dict_index_t *index); /*!< in: index */

/** Get index by first field of the index.
@param[in]      table           table
@param[in]      col_index       position of column in table
@return index which is having first field matches with the field present in
field_index position of table */
static inline dict_index_t *dict_table_get_index_on_first_col(
    dict_table_t *table, ulint col_index);
#endif /* !UNIV_HOTBACKUP */

/** encode number of columns and number of virtual columns in one
4 bytes value. We could do this because the number of columns in
InnoDB is limited to 1017
@param[in]      n_col   number of non-virtual column
@param[in]      n_v_col number of virtual column
@return encoded value */
static inline ulint dict_table_encode_n_col(ulint n_col, ulint n_v_col);

/** Decode number of virtual and non-virtual columns in one 4 bytes value.
@param[in]      encoded encoded value
@param[in,out]  n_col   number of non-virtual column
@param[in,out]  n_v_col number of virtual column */
static inline void dict_table_decode_n_col(uint32_t encoded, uint32_t *n_col,
                                           uint32_t *n_v_col);

/** Free the virtual column template
@param[in,out]  vc_templ        virtual column template */
static inline void dict_free_vc_templ(dict_vcol_templ_t *vc_templ);

/** Returns a virtual column's name according to its original
MySQL table position.
@param[in]      table   target table
@param[in]      col_nr  column number (nth column in the table)
@return column name. */
const char *dict_table_get_v_col_name_mysql(const dict_table_t *table,
                                            ulint col_nr);

/** Check whether the table have virtual index.
@param[in]      table   InnoDB table
@return true if the table have virtual index, false otherwise. */
static inline bool dict_table_have_virtual_index(dict_table_t *table);

/** Retrieve in-memory index for SDI table.
@param[in]      tablespace_id   innodb tablespace ID
@return dict_index_t structure or NULL*/
dict_index_t *dict_sdi_get_index(space_id_t tablespace_id);

/** Retrieve in-memory table object for SDI table.
@param[in]      tablespace_id   innodb tablespace ID
@param[in]      dict_locked     true if dict_sys mutex is acquired
@param[in]      is_create       true when creating SDI Index
@return dict_table_t structure */
dict_table_t *dict_sdi_get_table(space_id_t tablespace_id, bool dict_locked,
                                 bool is_create);

/** Remove the SDI table from table cache.
@param[in]      space_id        InnoDB tablespace ID
@param[in]      sdi_table       SDI table
@param[in]      dict_locked     true if dict_sys mutex acquired */
void dict_sdi_remove_from_cache(space_id_t space_id, dict_table_t *sdi_table,
                                bool dict_locked);

/** Check if the index is SDI index
@param[in]      index   in-memory index structure
@return true if index is SDI index else false */
static inline bool dict_index_is_sdi(const dict_index_t *index);

/** Check if an table id belongs SDI table
@param[in]      table_id        dict_table_t id
@return true if table_id is SDI table_id else false */
static inline bool dict_table_is_sdi(uint64_t table_id);

/** Close SDI table.
@param[in]      table           the in-memory SDI table object */
void dict_sdi_close_table(dict_table_t *table);

/** Acquire exclusive MDL on SDI tables. This is acquired to
prevent concurrent DROP table/tablespace when there is purge
happening on SDI table records. Purge will acquire shared
MDL on SDI table.

Exclusive MDL is transactional(released on trx commit). So
for successful acquisition, there should be a valid thd associated
with a trx.

Acquisition order of SDI MDL and SDI table has to be in the same
order:

1. dd_sdi_acquire_exclusive_mdl
2. row_drop_table_from_cache()/innodb_drop_tablespace()
   ->dict_sdi_remove_from_cache()->dd_table_open_on_id()

In purge:

1. dd_sdi_acquire_shared_mdl
2. dd_table_open_on_id()

@param[in]      thd             server thread instance
@param[in]      space_id        InnoDB tablespace id
@param[in,out]  sdi_mdl         MDL ticket on SDI table
@retval DB_SUCESS               on success
@retval DB_LOCK_WAIT_TIMEOUT    on error */
dberr_t dd_sdi_acquire_exclusive_mdl(THD *thd, space_id_t space_id,
                                     MDL_ticket **sdi_mdl);

/** Acquire shared MDL on SDI tables. This is acquired by purge to
prevent concurrent DROP table/tablespace.
DROP table/tablespace will acquire exclusive MDL on SDI table

Acquisition order of SDI MDL and SDI table has to be in same
order:

1. dd_sdi_acquire_exclusive_mdl
2. row_drop_table_from_cache()/innodb_drop_tablespace()
   ->dict_sdi_remove_from_cache()->dd_table_open_on_id()

In purge:

1. dd_sdi_acquire_shared_mdl
2. dd_table_open_on_id()

MDL should be released by caller
@param[in]      thd             server thread instance
@param[in]      space_id        InnoDB tablespace id
@param[in,out]  sdi_mdl         MDL ticket on SDI table
@retval DB_SUCESS               on success
@retval DB_LOCK_WAIT_TIMEOUT    on error */
dberr_t dd_sdi_acquire_shared_mdl(THD *thd, space_id_t space_id,
                                  MDL_ticket **sdi_mdl);

/** Check whether the dict_table_t is a partition.
A partitioned table on the SQL level is composed of InnoDB tables,
where each InnoDB table is a [sub]partition including its secondary indexes
which belongs to the partition.
@param[in]      table   Table to check.
@return true if the dict_table_t is a partition else false. */
static inline bool dict_table_is_partition(const dict_table_t *table);

/** Allocate memory for intrinsic cache elements in the index
@param[in]      index   index object */
static inline void dict_allocate_mem_intrinsic_cache(dict_index_t *index);

/** Evict all tables that are loaded for applying purge.
Since we move the offset of all table ids during upgrade,
these tables cannot exist in cache. Also change table_ids
of SYS_* tables if they are upgraded from earlier versions */
void dict_upgrade_evict_tables_cache();

/** @return true if table is InnoDB SYS_* table
@param[in]      table_id        table id  */
bool dict_table_is_system(table_id_t table_id);

/** Build the table_id array of SYS_* tables. This
array is used to determine if a table is InnoDB SYSTEM
table or not.
@return true if successful, false otherwise */
bool dict_sys_table_id_build();

/** Change the table_id of SYS_* tables if they have been created after
an earlier upgrade. This will update the table_id by adding DICT_MAX_DD_TABLES
*/
void dict_table_change_id_sys_tables();

/** Get the tablespace data directory if set, otherwise empty string.
@return the data directory */
[[nodiscard]] std::string dict_table_get_datadir(const dict_table_t *table);

/** Set the compression type for the tablespace of a table
@param[in]  table         The table that should be compressed
@param[in]  algorithm     Text representation of the algorithm
@param[in]  is_import_op  True if it's being set during import operation
@return DB_SUCCESS or error code */
[[nodiscard]] dberr_t dict_set_compression(dict_table_t *table,
                                           const char *algorithm,
                                           bool is_import_op);

/** @return true if all base column of virtual column is foreign key column
@param[in]      vcol    in-memory virtual column
@param[in]      foreign in-memory Foreign key constraint */
uint32_t dict_vcol_base_is_foreign_key(dict_v_col_t *vcol,
                                       dict_foreign_t *foreign);

/** Get maximum possible size needed for a field.
@param[in]   table        innodb table definition cache
@param[in]   index        index
@param[in]   field        field
@param[out]  rec_max_size max record size needed */
void get_field_max_size(const dict_table_t *table, const dict_index_t *index,
                        const dict_field_t *field, size_t &rec_max_size);

/** Get the maximum size of a record permissible on an index page.
@param[in]  table        innodb table definition cache
@param[in]  index        index
@param[out] page_rec_max maximum size of record on a leaf page
@param[out] page_ptr_max maximum size of record on non-leaf page */
void get_permissible_max_size(const dict_table_t *table,
                              const dict_index_t *index, size_t &page_rec_max,
                              size_t &page_ptr_max);

/** validate that maximum possible size of a row is within permissible limit.
@param[in]  table        innodb table definition cache
@param[in]  index        index
@param[in]  strict       true if error is to be reported
@param[in]  page_rec_max maximum size of possible record on leaf page
@param[in]  page_ptr_max maximum size of possible record on non-leaf page
@param[out] rec_max_size maximum size of record on page
@return true if max record size is within limit, false otherwise. */
bool dict_index_validate_max_rec_size(const dict_table_t *table,
                                      const dict_index_t *index, bool strict,
                                      const size_t page_rec_max,
                                      const size_t page_ptr_max,
                                      size_t &rec_max_size);

#include "dict0dict.ic"

#endif
