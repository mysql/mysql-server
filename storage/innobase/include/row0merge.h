/*****************************************************************************

Copyright (c) 2005, 2019, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/row0merge.h
 Index build routines using a merge sort

 Created 13/06/2005 Jan Lindstrom
 *******************************************************/

#ifndef row0merge_h
#define row0merge_h

#include "btr0types.h"
#include "data0data.h"
#include "dict0types.h"
#include "lock0types.h"
#include "mtr0mtr.h"
#include "que0types.h"
#include "rem0rec.h"
#include "rem0types.h"
#include "row0mysql.h"
#include "srv0srv.h"
#include "trx0types.h"
#include "univ.i"
#include "ut0stage.h"

// Forward declaration
struct ib_sequence_t;

/** @brief Block size for I/O operations in merge sort.

The minimum is UNIV_PAGE_SIZE, or page_get_free_space_of_empty()
rounded to a power of 2.

When not creating a PRIMARY KEY that contains column prefixes, this
can be set as small as UNIV_PAGE_SIZE / 2. */
typedef byte row_merge_block_t;

/** @brief Secondary buffer for I/O operations of merge records.

This buffer is used for writing or reading a record that spans two
row_merge_block_t.  Thus, it must be able to hold one merge record,
whose maximum size is the same as the minimum size of
row_merge_block_t. */
typedef byte mrec_buf_t[UNIV_PAGE_SIZE_MAX];

/** @brief Merge record in row_merge_block_t.

The format is the same as a record in ROW_FORMAT=COMPACT with the
exception that the REC_N_NEW_EXTRA_BYTES are omitted. */
typedef byte mrec_t;

/** Merge record in row_merge_buf_t */
struct mtuple_t {
  dfield_t *fields; /*!< data fields */
};

/** Buffer for sorting in main memory. */
struct row_merge_buf_t {
  mem_heap_t *heap;     /*!< memory heap where allocated */
  dict_index_t *index;  /*!< the index the tuples belong to */
  ulint total_size;     /*!< total amount of data bytes */
  ulint n_tuples;       /*!< number of data tuples */
  ulint max_tuples;     /*!< maximum number of data tuples */
  mtuple_t *tuples;     /*!< array of data tuples */
  mtuple_t *tmp_tuples; /*!< temporary copy of tuples,
                        for sorting */
};

/** Information about temporary files used in merge sort */
struct merge_file_t {
  int fd;            /*!< file descriptor */
  ulint offset;      /*!< file offset (end of file) */
  ib_uint64_t n_rec; /*!< number of records in the file */
};

/** Index field definition */
struct index_field_t {
  ulint col_no;        /*!< column offset */
  ulint prefix_len;    /*!< column prefix length, or 0
                       if indexing the whole column */
  bool is_v_col;       /*!< whether this is a virtual column */
  bool is_multi_value; /*!< whether it has multi-value */
  bool is_ascending;   /*!< true=ASC, false=DESC */
};

/** Definition of an index being created */
struct index_def_t {
  const char *name;          /*!< index name */
  bool rebuild;              /*!< whether the table is rebuilt */
  ulint ind_type;            /*!< 0, DICT_UNIQUE,
                             or DICT_CLUSTERED */
  ulint key_number;          /*!< MySQL key number,
                             or ULINT_UNDEFINED if none */
  ulint n_fields;            /*!< number of fields in index */
  index_field_t *fields;     /*!< field definitions */
  st_mysql_ftparser *parser; /*!< fulltext parser plugin */
  bool is_ngram;             /*!< true if it's ngram parser */
  bool srid_is_valid;        /*!< true if we want to check SRID
                             while inserting to index */
  uint32_t srid;             /*!< SRID obtained from dd column */
};

/** Structure for reporting duplicate records. */
struct row_merge_dup_t {
  dict_index_t *index;  /*!< index being sorted */
  struct TABLE *table;  /*!< MySQL table object */
  const ulint *col_map; /*!< mapping of column numbers
                        in table to the rebuilt table
                        (index->table), or NULL if not
                        rebuilding table */
  ulint n_dup;          /*!< number of duplicates */
};

/** Report a duplicate key. */
void row_merge_dup_report(
    row_merge_dup_t *dup,   /*!< in/out: for reporting duplicates */
    const dfield_t *entry); /*!< in: duplicate index entry */
/** Sets an exclusive lock on a table, for the duration of creating indexes.
 @return error code or DB_SUCCESS */
dberr_t row_merge_lock_table(trx_t *trx,          /*!< in/out: transaction */
                             dict_table_t *table, /*!< in: table to lock */
                             enum lock_mode mode) /*!< in: LOCK_X or LOCK_S */
    MY_ATTRIBUTE((warn_unused_result));
/** Drop those indexes which were created before an error occurred.
 The data dictionary must have been locked exclusively by the caller,
 because the transaction will not be committed. */
void row_merge_drop_indexes(
    trx_t *trx,          /*!< in/out: transaction */
    dict_table_t *table, /*!< in/out: table containing the indexes */
    ibool locked);       /*!< in: TRUE=table locked,
                 FALSE=may need to do a lazy drop */

/**Create temporary merge files in the given paramater path, and if
UNIV_PFS_IO defined, register the file descriptor with Performance Schema.
@param[in]	path	location for creating temporary merge files.
@return File descriptor */
int row_merge_file_create_low(const char *path)
    MY_ATTRIBUTE((warn_unused_result));

/** Destroy a merge file. And de-register the file from Performance Schema
 if UNIV_PFS_IO is defined. */
void row_merge_file_destroy_low(int fd); /*!< in: merge file descriptor */

/** Provide a new pathname for a table that is being renamed if it belongs to
 a file-per-table tablespace.  The caller is responsible for freeing the
 memory allocated for the return value.
 @return new pathname of tablespace file, or NULL if space = 0 */
char *row_make_new_pathname(dict_table_t *table, /*!< in: table to be renamed */
                            const char *new_name); /*!< in: new name */
/** Rename the tables in the data dictionary.  The data dictionary must
 have been locked exclusively by the caller, because the transaction
 will not be committed.
 @return error code or DB_SUCCESS */
dberr_t row_merge_rename_tables_dict(
    dict_table_t *old_table, /*!< in/out: old table, renamed to
                             tmp_name */
    dict_table_t *new_table, /*!< in/out: new table, renamed to
                             old_table->name */
    const char *tmp_name,    /*!< in: new name for old_table */
    trx_t *trx)              /*!< in/out: dictionary transaction */
    MY_ATTRIBUTE((warn_unused_result));

/** Create the index and load in to the dictionary.
@param[in,out]	trx		trx (sets error_state)
@param[in,out]	table		the index is on this table
@param[in]	index_def	the index definition
@param[in]	add_v		new virtual columns added along with add
                                index call
@return index, or NULL on error */
dict_index_t *row_merge_create_index(trx_t *trx, dict_table_t *table,
                                     const index_def_t *index_def,
                                     const dict_add_v_col_t *add_v);

/** Drop a table. The caller must have ensured that the background stats
 thread is not processing the table. This can be done by calling
 dict_stats_wait_bg_to_stop_using_table() after locking the dictionary and
 before calling this function.
 @return DB_SUCCESS or error code */
dberr_t row_merge_drop_table(
    trx_t *trx,           /*!< in: transaction */
    dict_table_t *table); /*!< in: table instance to drop */

/** Build indexes on a table by reading a clustered index, creating a temporary
file containing index entries, merge sorting these index entries and inserting
sorted index entries to indexes.
@param[in]	trx		transaction
@param[in]	old_table	table where rows are read from
@param[in]	new_table	table where indexes are created; identical to
old_table unless creating a PRIMARY KEY
@param[in]	online		true if creating indexes online
@param[in]	indexes		indexes to be created
@param[in]	key_numbers	MySQL key numbers
@param[in]	n_indexes	size of indexes[]
@param[in,out]	table		MySQL table, for reporting erroneous key value
if applicable
@param[in]	add_cols	default values of added columns, or NULL
@param[in]	col_map		mapping of old column numbers to new ones, or
NULL if old_table == new_table
@param[in]	add_autoinc	number of added AUTO_INCREMENT columns, or
ULINT_UNDEFINED if none is added
@param[in,out]	sequence	autoinc sequence
@param[in]	skip_pk_sort	whether the new PRIMARY KEY will follow
existing order
@param[in,out]	stage		performance schema accounting object, used by
ALTER TABLE. stage->begin_phase_read_pk() will be called at the beginning of
this function and it will be passed to other functions for further accounting.
@param[in]	add_v		new virtual columns added along with indexes
@param[in]	eval_table	mysql table used to evaluate virtual column
                                value, see innobase_get_computed_value().
@return DB_SUCCESS or error code */
dberr_t row_merge_build_indexes(
    trx_t *trx, dict_table_t *old_table, dict_table_t *new_table, bool online,
    dict_index_t **indexes, const ulint *key_numbers, ulint n_indexes,
    struct TABLE *table, const dtuple_t *add_cols, const ulint *col_map,
    ulint add_autoinc, ib_sequence_t &sequence, bool skip_pk_sort,
    ut_stage_alter_t *stage, const dict_add_v_col_t *add_v,
    struct TABLE *eval_table) MY_ATTRIBUTE((warn_unused_result));

/** Write a buffer to a block.
@param[in]	buf	sorted buffer
@param[in]	of	output file
@param[out]	block	buffer for writing to file */
void row_merge_buf_write(const row_merge_buf_t *buf, const merge_file_t *of,
                         row_merge_block_t *block);

/** Sort a buffer. */
void row_merge_buf_sort(
    row_merge_buf_t *buf,  /*!< in/out: sort buffer */
    row_merge_dup_t *dup); /*!< in/out: reporter of duplicates
                           (NULL if non-unique index) */
/** Write a merge block to the file system.
 @return true if request was successful, false if fail */
ibool row_merge_write(int fd,           /*!< in: file descriptor */
                      ulint offset,     /*!< in: offset where to write,
                                        in number of row_merge_block_t elements */
                      const void *buf); /*!< in: data */
/** Empty a sort buffer.
 @return sort buffer */
row_merge_buf_t *row_merge_buf_empty(
    row_merge_buf_t *buf) /*!< in,own: sort buffer */
    MY_ATTRIBUTE((warn_unused_result));

/** Create a merge file int the given location.
@param[out]	merge_file	merge file structure
@param[in]	path		location for creating temporary file
@return file descriptor, or -1 on failure */
int row_merge_file_create(merge_file_t *merge_file, const char *path);

/** Merge disk files.
@param[in]	trx	transaction
@param[in]	dup	descriptor of index being created
@param[in,out]	file	file containing index entries
@param[in,out]	block	3 buffers
@param[in,out]	tmpfd	temporary file handle
@param[in,out]	stage	performance schema accounting object, used by
ALTER TABLE. If not NULL, stage->begin_phase_sort() will be called initially
and then stage->inc() will be called for each record processed.
@return DB_SUCCESS or error code */
dberr_t row_merge_sort(trx_t *trx, const row_merge_dup_t *dup,
                       merge_file_t *file, row_merge_block_t *block, int *tmpfd,
                       ut_stage_alter_t *stage = NULL);

/** Allocate a sort buffer.
 @return own: sort buffer */
row_merge_buf_t *row_merge_buf_create(
    dict_index_t *index) /*!< in: secondary index */
    MY_ATTRIBUTE((warn_unused_result, malloc));
/** Deallocate a sort buffer. */
void row_merge_buf_free(
    row_merge_buf_t *buf); /*!< in,own: sort buffer to be freed */
/** Destroy a merge file. */
void row_merge_file_destroy(
    merge_file_t *merge_file); /*!< in/out: merge file structure */
/** Read a merge block from the file system.
 @return true if request was successful, false if fail */
ibool row_merge_read(int fd,                  /*!< in: file descriptor */
                     ulint offset,            /*!< in: offset where to read
                                              in number of row_merge_block_t
                                              elements */
                     row_merge_block_t *buf); /*!< out: data */
/** Read a merge record.
 @return pointer to next record, or NULL on I/O error or end of list */
const byte *row_merge_read_rec(
    row_merge_block_t *block,  /*!< in/out: file buffer */
    mrec_buf_t *buf,           /*!< in/out: secondary buffer */
    const byte *b,             /*!< in: pointer to record */
    const dict_index_t *index, /*!< in: index of the record */
    int fd,                    /*!< in: file descriptor */
    ulint *foffs,              /*!< in/out: file offset */
    const mrec_t **mrec,       /*!< out: pointer to merge record,
                               or NULL on end of list
                               (non-NULL on I/O error) */
    ulint *offsets)            /*!< out: offsets of mrec */
    MY_ATTRIBUTE((warn_unused_result));
#endif /* row0merge.h */
