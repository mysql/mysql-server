/*****************************************************************************

Copyright (c) 1996, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/row0row.h
 General row routines

 Created 4/20/1996 Heikki Tuuri
 *******************************************************/

#ifndef row0row_h
#define row0row_h

#include "btr0types.h"
#include "data0data.h"
#include "dict0types.h"
#include "mtr0mtr.h"
#include "que0types.h"
#include "rem0types.h"
#include "row0types.h"
#include "trx0types.h"
#include "univ.i"

/** Gets the offset of the DB_TRX_ID field, in bytes relative to the origin of
 a clustered index record.
 @return offset of DATA_TRX_ID */
UNIV_INLINE
ulint row_get_trx_id_offset(
    const dict_index_t *index, /*!< in: clustered index */
    const ulint *offsets)      /*!< in: record offsets */
    MY_ATTRIBUTE((warn_unused_result));
/** Reads the trx id field from a clustered index record.
 @return value of the field */
UNIV_INLINE
trx_id_t row_get_rec_trx_id(
    const rec_t *rec,          /*!< in: record */
    const dict_index_t *index, /*!< in: clustered index */
    const ulint *offsets)      /*!< in: rec_get_offsets(rec, index) */
    MY_ATTRIBUTE((warn_unused_result));
/** Reads the roll pointer field from a clustered index record.
 @return value of the field */
UNIV_INLINE
roll_ptr_t row_get_rec_roll_ptr(
    const rec_t *rec,          /*!< in: record */
    const dict_index_t *index, /*!< in: clustered index */
    const ulint *offsets)      /*!< in: rec_get_offsets(rec, index) */
    MY_ATTRIBUTE((warn_unused_result));

/* Flags for row build type. */
#define ROW_BUILD_NORMAL 0     /*!< build index row */
#define ROW_BUILD_FOR_PURGE 1  /*!< build row for purge. */
#define ROW_BUILD_FOR_UNDO 2   /*!< build row for undo. */
#define ROW_BUILD_FOR_INSERT 3 /*!< build row for insert. */
/** When an insert or purge to a table is performed, this function builds
 the entry to be inserted into or purged from an index on the table.
 @return index entry which should be inserted or purged
 @retval NULL if the externally stored columns in the clustered index record
 are unavailable and ext != NULL, or row is missing some needed columns. */
dtuple_t *row_build_index_entry_low(
    const dtuple_t *row,       /*!< in: row which should be
                               inserted or purged */
    const row_ext_t *ext,      /*!< in: externally stored column
                               prefixes, or NULL */
    const dict_index_t *index, /*!< in: index on the table */
    mem_heap_t *heap,          /*!< in: memory heap from which
                               the memory for the index entry
                               is allocated */
    ulint flag)                /*!< in: ROW_BUILD_NORMAL,
                               ROW_BUILD_FOR_PURGE
                               or ROW_BUILD_FOR_UNDO */
    MY_ATTRIBUTE((warn_unused_result));
/** When an insert or purge to a table is performed, this function builds
 the entry to be inserted into or purged from an index on the table.
 @return index entry which should be inserted or purged, or NULL if the
 externally stored columns in the clustered index record are
 unavailable and ext != NULL */
UNIV_INLINE
dtuple_t *row_build_index_entry(
    const dtuple_t *row,       /*!< in: row which should be
                               inserted or purged */
    const row_ext_t *ext,      /*!< in: externally stored column
                               prefixes, or NULL */
    const dict_index_t *index, /*!< in: index on the table */
    mem_heap_t *heap)          /*!< in: memory heap from which
                               the memory for the index entry
                               is allocated */
    MY_ATTRIBUTE((warn_unused_result));
/** An inverse function to row_build_index_entry. Builds a row from a
 record in a clustered index.
 @return own: row built; see the NOTE below! */
dtuple_t *row_build(ulint type,                /*!< in: ROW_COPY_POINTERS or
                                               ROW_COPY_DATA; the latter
                                               copies also the data fields to
                                               heap while the first only
                                               places pointers to data fields
                                               on the index page, and thus is
                                               more efficient */
                    const dict_index_t *index, /*!< in: clustered index */
                    const rec_t *rec,          /*!< in: record in the clustered
                                               index; NOTE: in the case
                                               ROW_COPY_POINTERS the data
                                               fields in the row will point
                                               directly into this record,
                                               therefore, the buffer page of
                                               this record must be at least
                                               s-latched and the latch held
                                               as long as the row dtuple is used! */
                    const ulint *offsets, /*!< in: rec_get_offsets(rec,index)
                                          or NULL, in which case this function
                                          will invoke rec_get_offsets() */
                    const dict_table_t *col_table,
                    /*!< in: table, to check which
                    externally stored columns
                    occur in the ordering columns
                    of an index, or NULL if
                    index->table should be
                    consulted instead; the user
                    columns in this table should be
                    the same columns as in index->table */
                    const dtuple_t *add_cols,
                    /*!< in: default values of
                    added columns, or NULL */
                    const ulint *col_map, /*!< in: mapping of old column
                                          numbers to new ones, or NULL */
                    row_ext_t **ext,      /*!< out, own: cache of
                                          externally stored column
                                          prefixes, or NULL */
                    mem_heap_t *heap);    /*!< in: memory heap from which
                                          the memory needed is allocated */

/** An inverse function to row_build_index_entry. Builds a row from a
record in a clustered index, with possible indexing on ongoing
addition of new virtual columns.
@param[in]	type		ROW_COPY_POINTERS or ROW_COPY_DATA;
@param[in]	index		clustered index
@param[in]	rec		record in the clustered index
@param[in]	offsets		rec_get_offsets(rec,index) or NULL
@param[in]	col_table	table, to check which
                                externally stored columns
                                occur in the ordering columns
                                of an index, or NULL if
                                index->table should be
                                consulted instead
@param[in]	add_cols	default values of added columns, or NULL
@param[in]	add_v		new virtual columns added
                                along with new indexes
@param[in]	col_map		mapping of old column
                                numbers to new ones, or NULL
@param[in]	ext		cache of externally stored column
                                prefixes, or NULL
@param[in]	heap		memory heap from which
                                the memory needed is allocated
@return own: row built */
dtuple_t *row_build_w_add_vcol(ulint type, const dict_index_t *index,
                               const rec_t *rec, const ulint *offsets,
                               const dict_table_t *col_table,
                               const dtuple_t *add_cols,
                               const dict_add_v_col_t *add_v,
                               const ulint *col_map, row_ext_t **ext,
                               mem_heap_t *heap);

/** Converts an index record to a typed data tuple.
 @return index entry built; does not set info_bits, and the data fields
 in the entry will point directly to rec */
dtuple_t *row_rec_to_index_entry_low(
    const rec_t *rec,          /*!< in: record in the index */
    const dict_index_t *index, /*!< in: index */
    const ulint *offsets,      /*!< in: rec_get_offsets(rec, index) */
    ulint *n_ext,              /*!< out: number of externally
                               stored columns */
    mem_heap_t *heap)          /*!< in: memory heap from which
                               the memory needed is allocated */
    MY_ATTRIBUTE((warn_unused_result));
/** Converts an index record to a typed data tuple. NOTE that externally
 stored (often big) fields are NOT copied to heap.
 @return own: index entry built */
dtuple_t *row_rec_to_index_entry(
    const rec_t *rec,          /*!< in: record in the index */
    const dict_index_t *index, /*!< in: index */
    const ulint *offsets,      /*!< in/out: rec_get_offsets(rec) */
    ulint *n_ext,              /*!< out: number of externally
                               stored columns */
    mem_heap_t *heap)          /*!< in: memory heap from which
                               the memory needed is allocated */
    MY_ATTRIBUTE((warn_unused_result));
/** Builds from a secondary index record a row reference with which we can
 search the clustered index record.
 @return own: row reference built; see the NOTE below! */
dtuple_t *row_build_row_ref(
    ulint type,                /*!< in: ROW_COPY_DATA, or ROW_COPY_POINTERS:
                               the former copies also the data fields to
                               heap, whereas the latter only places pointers
                               to data fields on the index page */
    const dict_index_t *index, /*!< in: secondary index */
    const rec_t *rec,          /*!< in: record in the index;
                               NOTE: in the case ROW_COPY_POINTERS
                               the data fields in the row will point
                               directly into this record, therefore,
                               the buffer page of this record must be
                               at least s-latched and the latch held
                               as long as the row reference is used! */
    mem_heap_t *heap)          /*!< in: memory heap from which the memory
                               needed is allocated */
    MY_ATTRIBUTE((warn_unused_result));
/** Builds from a secondary index record a row reference with which we can
 search the clustered index record. */
void row_build_row_ref_in_tuple(
    dtuple_t *ref,             /*!< in/out: row reference built;
                               see the NOTE below! */
    const rec_t *rec,          /*!< in: record in the index;
                               NOTE: the data fields in ref
                               will point directly into this
                               record, therefore, the buffer
                               page of this record must be at
                               least s-latched and the latch
                               held as long as the row
                               reference is used! */
    const dict_index_t *index, /*!< in: secondary index */
    ulint *offsets,            /*!< in: rec_get_offsets(rec, index)
                               or NULL */
    trx_t *trx);               /*!< in: transaction or NULL */

/** Builds from a secondary index record a row reference with which we can
search the clustered index record.
@param[in,out]	ref	typed data tuple where the reference is built
@param[in]	map	array of field numbers in rec telling how ref should
                        be built from the fields of rec
@param[in]	rec	record in the index; must be preserved while ref is
                        used, as we do not copy field values to heap
@param[in]	offsets	array returned by rec_get_offsets() */
UNIV_INLINE
void row_build_row_ref_fast(dtuple_t *ref, const ulint *map, const rec_t *rec,
                            const ulint *offsets);

/** Searches the clustered index record for a row, if we have the row
 reference.
 @return true if found */
ibool row_search_on_row_ref(
    btr_pcur_t *pcur,    /*!< out: persistent cursor, which must
                         be closed by the caller */
    ulint mode,          /*!< in: BTR_MODIFY_LEAF, ... */
    dict_table_t *table, /*!< in: table */
    const dtuple_t *ref, /*!< in: row reference */
    mtr_t *mtr)          /*!< in/out: mtr */
    MY_ATTRIBUTE((warn_unused_result));
/** Fetches the clustered index record for a secondary index record. The latches
 on the secondary index record are preserved.
 @return record or NULL, if no record found */
rec_t *row_get_clust_rec(
    ulint mode,                 /*!< in: BTR_MODIFY_LEAF, ... */
    const rec_t *rec,           /*!< in: record in a secondary index */
    const dict_index_t *index,  /*!< in: secondary index */
    dict_index_t **clust_index, /*!< out: clustered index */
    mtr_t *mtr)                 /*!< in: mtr */
    MY_ATTRIBUTE((warn_unused_result));

/** Parse the integer data from specified data, which could be
DATA_INT, DATA_FLOAT or DATA_DOUBLE. If the value is less than 0
and the type is not unsigned then we reset the value to 0
@param[in]	data		data to read
@param[in]	len		length of data
@param[in]	mtype		mtype of data
@param[in]	unsigned_type	if the data is unsigned
@return the integer value from the data */
inline ib_uint64_t row_parse_int(const byte *data, ulint len, ulint mtype,
                                 bool unsigned_type);

/** Parse the integer data from specified field, which could be
DATA_INT, DATA_FLOAT or DATA_DOUBLE. We could return 0 if
1) the value is less than 0 and the type is not unsigned
or 2) the field is null.
@param[in]	field		field to read the int value
@return the integer value read from the field, 0 for negative signed
int or NULL field */
ib_uint64_t row_parse_int_from_field(const dfield_t *field);

/** Read the autoinc counter from the clustered index row.
@param[in]	row	row to read the autoinc counter
@param[in]	n	autoinc counter is in the nth field
@return the autoinc counter read */
ib_uint64_t row_get_autoinc_counter(const dtuple_t *row, ulint n);

/** Result of row_search_index_entry */
enum row_search_result {
  ROW_FOUND = 0,      /*!< the record was found */
  ROW_NOT_FOUND,      /*!< record not found */
  ROW_BUFFERED,       /*!< one of BTR_INSERT, BTR_DELETE, or
                      BTR_DELETE_MARK was specified, the
                      secondary index leaf page was not in
                      the buffer pool, and the operation was
                      enqueued in the insert/delete buffer */
  ROW_NOT_DELETED_REF /*!< BTR_DELETE was specified, and
                      row_purge_poss_sec() failed */
};

/** Searches an index record.
 @return whether the record was found or buffered */
enum row_search_result row_search_index_entry(
    dict_index_t *index,   /*!< in: index */
    const dtuple_t *entry, /*!< in: index entry */
    ulint mode,            /*!< in: BTR_MODIFY_LEAF, ... */
    btr_pcur_t *pcur,      /*!< in/out: persistent cursor, which must
                           be closed by the caller */
    mtr_t *mtr)            /*!< in: mtr */
    MY_ATTRIBUTE((warn_unused_result));

#define ROW_COPY_DATA 1
#define ROW_COPY_POINTERS 2

/* The allowed latching order of index records is the following:
(1) a secondary index record ->
(2) the clustered index record ->
(3) rollback segment data for the clustered index record. */

/** Formats the raw data in "data" (in InnoDB on-disk format) using
 "dict_field" and writes the result to "buf".
 Not more than "buf_size" bytes are written to "buf".
 The result is always NUL-terminated (provided buf_size is positive) and the
 number of bytes that were written to "buf" is returned (including the
 terminating NUL).
 @return number of bytes that were written */
ulint row_raw_format(const char *data,               /*!< in: raw data */
                     ulint data_len,                 /*!< in: raw data length
                                                     in bytes */
                     const dict_field_t *dict_field, /*!< in: index field */
                     char *buf,                      /*!< out: output buffer */
                     ulint buf_size)                 /*!< in: output buffer size
                                                     in bytes */
    MY_ATTRIBUTE((warn_unused_result));

#include "row0row.ic"

#endif
