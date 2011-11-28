/*****************************************************************************

Copyright (c) 2005, 2010, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file include/row0merge.h
Index build routines using a merge sort

Created 13/06/2005 Jan Lindstrom
*******************************************************/

#ifndef row0merge_h
#define row0merge_h

#include "univ.i"
#include "data0data.h"
#include "dict0types.h"
#include "trx0types.h"
#include "que0types.h"
#include "mtr0mtr.h"
#include "rem0types.h"
#include "rem0rec.h"
#include "read0types.h"
#include "btr0types.h"
#include "row0mysql.h"
#include "lock0types.h"
#include "srv0srv.h"

/** @brief Block size for I/O operations in merge sort.

The minimum is UNIV_PAGE_SIZE, or page_get_free_space_of_empty()
rounded to a power of 2.

When not creating a PRIMARY KEY that contains column prefixes, this
can be set as small as UNIV_PAGE_SIZE / 2.  See the comment above
ut_ad(data_size < sizeof(row_merge_block_t)). */
typedef byte   row_merge_block_t;

/** @brief Secondary buffer for I/O operations of merge records.

This buffer is used for writing or reading a record that spans two
row_merge_block_t.  Thus, it must be able to hold one merge record,
whose maximum size is the same as the minimum size of
row_merge_block_t. */
typedef byte	mrec_buf_t[UNIV_PAGE_SIZE_MAX];

/** @brief Merge record in row_merge_block_t.

The format is the same as a record in ROW_FORMAT=COMPACT with the
exception that the REC_N_NEW_EXTRA_BYTES are omitted. */
typedef byte	mrec_t;

/** Buffer for sorting in main memory. */
struct row_merge_buf_struct {
	mem_heap_t*	heap;		/*!< memory heap where allocated */
	dict_index_t*	index;		/*!< the index the tuples belong to */
	ulint		total_size;	/*!< total amount of data bytes */
	ulint		n_tuples;	/*!< number of data tuples */
	ulint		max_tuples;	/*!< maximum number of data tuples */
	const dfield_t**tuples;		/*!< array of pointers to
					arrays of fields that form
					the data tuples */
	const dfield_t**tmp_tuples;	/*!< temporary copy of tuples,
					for sorting */
};

/** Buffer for sorting in main memory. */
typedef struct row_merge_buf_struct	row_merge_buf_t;

/** Information about temporary files used in merge sort */
struct merge_file_struct {
	int		fd;		/*!< file descriptor */
	ulint		offset;		/*!< file offset (end of file) */
	ib_uint64_t	n_rec;		/*!< number of records in the file */
};

/** Information about temporary files used in merge sort */
typedef struct merge_file_struct	merge_file_t;

/** Index field definition */
struct merge_index_field_struct {
	ulint		prefix_len;	/*!< column prefix length, or 0
					if indexing the whole column */
	const char*	field_name;	/*!< field name */
};

/** Index field definition */
typedef struct merge_index_field_struct	merge_index_field_t;

/** Definition of an index being created */
struct merge_index_def_struct {
	const char*		name;		/*!< index name */
	ulint			ind_type;	/*!< 0, DICT_UNIQUE,
						or DICT_CLUSTERED */
	ulint			n_fields;	/*!< number of fields
						in index */
	merge_index_field_t*	fields;		/*!< field definitions */
};

/** Definition of an index being created */
typedef struct merge_index_def_struct	merge_index_def_t;

/** Structure for reporting duplicate records. */
struct row_merge_dup_struct {
	const dict_index_t*	index;		/*!< index being sorted */
	struct TABLE*		table;		/*!< MySQL table object */
	ulint			n_dup;		/*!< number of duplicates */
};

/** Structure for reporting duplicate records. */
typedef struct row_merge_dup_struct row_merge_dup_t;

/*********************************************************************//**
Sets an exclusive lock on a table, for the duration of creating indexes.
@return	error code or DB_SUCCESS */
UNIV_INTERN
ulint
row_merge_lock_table(
/*=================*/
	trx_t*		trx,		/*!< in/out: transaction */
	dict_table_t*	table,		/*!< in: table to lock */
	enum lock_mode	mode);		/*!< in: LOCK_X or LOCK_S */
/*********************************************************************//**
Drop an index from the InnoDB system tables.  The data dictionary must
have been locked exclusively by the caller, because the transaction
will not be committed. */
UNIV_INTERN
void
row_merge_drop_index(
/*=================*/
	dict_index_t*	index,	/*!< in: index to be removed */
	dict_table_t*	table,	/*!< in: table */
	trx_t*		trx);	/*!< in: transaction handle */
/*********************************************************************//**
Drop those indexes which were created before an error occurred when
building an index.  The data dictionary must have been locked
exclusively by the caller, because the transaction will not be
committed. */
UNIV_INTERN
void
row_merge_drop_indexes(
/*===================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_table_t*	table,		/*!< in: table containing the indexes */
	dict_index_t**	index,		/*!< in: indexes to drop */
	ulint		num_created);	/*!< in: number of elements in
					index[] */
/*********************************************************************//**
Drop all partially created indexes during crash recovery. */
UNIV_INTERN
void
row_merge_drop_temp_indexes(void);
/*=============================*/
/*********************************************************************//**
Rename the tables in the data dictionary.  The data dictionary must
have been locked exclusively by the caller, because the transaction
will not be committed.
@return	error code or DB_SUCCESS */
UNIV_INTERN
ulint
row_merge_rename_tables(
/*====================*/
	dict_table_t*	old_table,	/*!< in/out: old table, renamed to
					tmp_name */
	dict_table_t*	new_table,	/*!< in/out: new table, renamed to
					old_table->name */
	const char*	tmp_name,	/*!< in: new name for old_table */
	trx_t*		trx);		/*!< in: transaction handle */
/*********************************************************************//**
Create a temporary table for creating a primary key, using the definition
of an existing table.
@return	table, or NULL on error */
UNIV_INTERN
dict_table_t*
row_merge_create_temporary_table(
/*=============================*/
	const char*		table_name,	/*!< in: new table name */
	const merge_index_def_t*index_def,	/*!< in: the index definition
						of the primary key */
	const dict_table_t*	table,		/*!< in: old table definition */
	trx_t*			trx);		/*!< in/out: transaction
						(sets error_state) */
/*********************************************************************//**
Rename the temporary indexes in the dictionary to permanent ones.  The
data dictionary must have been locked exclusively by the caller,
because the transaction will not be committed.
@return	DB_SUCCESS if all OK */
UNIV_INTERN
ulint
row_merge_rename_indexes(
/*=====================*/
	trx_t*		trx,		/*!< in/out: transaction */
	dict_table_t*	table);		/*!< in/out: table with new indexes */
/*********************************************************************//**
Create the index and load in to the dictionary.
@return	index, or NULL on error */
UNIV_INTERN
dict_index_t*
row_merge_create_index(
/*===================*/
	trx_t*			trx,	/*!< in/out: trx (sets error_state) */
	dict_table_t*		table,	/*!< in: the index is on this table */
	const merge_index_def_t*index_def);
					/*!< in: the index definition */
/*********************************************************************//**
Check if a transaction can use an index.
@return	TRUE if index can be used by the transaction else FALSE */
UNIV_INTERN
ibool
row_merge_is_index_usable(
/*======================*/
	const trx_t*		trx,	/*!< in: transaction */
	const dict_index_t*	index);	/*!< in: index to check */
/*********************************************************************//**
If there are views that refer to the old table name then we "attach" to
the new instance of the table else we drop it immediately.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ulint
row_merge_drop_table(
/*=================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_table_t*	table);		/*!< in: table instance to drop */
/*********************************************************************//**
Build indexes on a table by reading a clustered index,
creating a temporary file containing index entries, merge sorting
these index entries and inserting sorted index entries to indexes.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ulint
row_merge_build_indexes(
/*====================*/
	trx_t*		trx,		/*!< in: transaction */
	dict_table_t*	old_table,	/*!< in: table where rows are
					read from */
	dict_table_t*	new_table,	/*!< in: table where indexes are
					created; identical to old_table
					unless creating a PRIMARY KEY */
	dict_index_t**	indexes,	/*!< in: indexes to be created */
	ulint		n_indexes,	/*!< in: size of indexes[] */
	struct TABLE*	table);		/*!< in/out: MySQL table, for
					reporting erroneous key value
					if applicable */
/********************************************************************//**
Write a buffer to a block. */
UNIV_INTERN
void
row_merge_buf_write(
/*================*/
	const row_merge_buf_t*	buf,	/*!< in: sorted buffer */
	const merge_file_t*	of,	/*!< in: output file */
	row_merge_block_t*	block);	/*!< out: buffer for writing to file */
/********************************************************************//**
Sort a buffer. */
UNIV_INTERN
void
row_merge_buf_sort(
/*===============*/
        row_merge_buf_t*        buf,    /*!< in/out: sort buffer */
        row_merge_dup_t*        dup);	/*!< in/out: for reporting duplicates */
/********************************************************************//**
Write a merge block to the file system.
@return TRUE if request was successful, FALSE if fail */
UNIV_INTERN
ibool
row_merge_write(
/*============*/
	int		fd,	/*!< in: file descriptor */
	ulint		offset,	/*!< in: offset where to write,
				in number of row_merge_block_t elements */
	const void*	buf);	/*!< in: data */
/********************************************************************//**
Empty a sort buffer.
@return sort buffer */
UNIV_INTERN
row_merge_buf_t*
row_merge_buf_empty(
/*================*/
        row_merge_buf_t*        buf);    /*!< in,own: sort buffer */
/*********************************************************************//**
Create a merge file. */
UNIV_INTERN
void
row_merge_file_create(
/*==================*/
        merge_file_t*   merge_file);     /*!< out: merge file structure */
/*********************************************************************//**
Merge disk files.
@return DB_SUCCESS or error code */
UNIV_INTERN
ulint
row_merge_sort(
/*===========*/
	trx_t*			trx,	/*!< in: transaction */
	const dict_index_t*	index,	/*!< in: index being created */
	merge_file_t*		file,	/*!< in/out: file containing
					index entries */
	row_merge_block_t*	block,	/*!< in/out: 3 buffers */
	int*			tmpfd,	/*!< in/out: temporary file handle */
	struct TABLE*		table);	/*!< in/out: MySQL table, for
					reporting erroneous key value
					if applicable */
/*********************************************************************//**
Allocate a sort buffer.
@return own: sort buffer */
UNIV_INTERN
row_merge_buf_t*
row_merge_buf_create(
/*=================*/
        dict_index_t*   index);  /*!< in: secondary index */
/*********************************************************************//**
Deallocate a sort buffer. */
UNIV_INTERN
void
row_merge_buf_free(
/*===============*/
	row_merge_buf_t*	buf);    /*!< in,own: sort buffer, to be freed */
/*********************************************************************//**
Destroy a merge file. */
UNIV_INTERN
void
row_merge_file_destroy(
/*===================*/
	merge_file_t*	merge_file);	/*!< out: merge file structure */
/*********************************************************************//**
Compare two merge records.
@return 1, 0, -1 if mrec1 is greater, equal, less, respectively, than mrec2 */
UNIV_INTERN
int
row_merge_cmp(
/*==========*/
	const mrec_t*		mrec1,		/*!< in: first merge
						record to be compared */
	const mrec_t*		mrec2,		/*!< in: second merge
						record to be compared */
	const ulint*		offsets1,	/*!< in: first record offsets */
	const ulint*		offsets2,	/*!< in: second record offsets */
	const dict_index_t*	index,		/*!< in: index */
	ibool*			null_eq);	/*!< out: set to TRUE if
						found matching null values */
/********************************************************************//**
Read a merge block from the file system.
@return TRUE if request was successful, FALSE if fail */
UNIV_INTERN
ibool
row_merge_read(
/*===========*/
	int			fd,	/*!< in: file descriptor */
	ulint			offset,	/*!< in: offset where to read
					in number of row_merge_block_t
					elements */
	row_merge_block_t*	buf);	/*!< out: data */
/********************************************************************//**
Read a merge record.
@return pointer to next record, or NULL on I/O error or end of list */
UNIV_INTERN __attribute__((nonnull))
const byte*
row_merge_read_rec(
/*===============*/
	row_merge_block_t*	block,	/*!< in/out: file buffer */
	mrec_buf_t*		buf,	/*!< in/out: secondary buffer */
	const byte*		b,	/*!< in: pointer to record */
	const dict_index_t*	index,	/*!< in: index of the record */
	int			fd,	/*!< in: file descriptor */
	ulint*			foffs,	/*!< in/out: file offset */
	const mrec_t**		mrec,	/*!< out: pointer to merge record,
					or NULL on end of list
					(non-NULL on I/O error) */
	ulint*			offsets);/*!< out: offsets of mrec */
#endif /* row0merge.h */
