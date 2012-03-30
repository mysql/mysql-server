/*****************************************************************************

Copyright (c) 2000, 2010, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file include/row0mysql.h
Interface between Innobase row operations and MySQL.
Contains also create table and other data dictionary operations.

Created 9/17/2000 Heikki Tuuri
*******************************************************/

#ifndef row0mysql_h
#define row0mysql_h

#include "univ.i"
#include "data0data.h"
#include "que0types.h"
#include "dict0types.h"
#include "trx0types.h"
#include "row0types.h"
#include "btr0pcur.h"
#include "trx0types.h"

extern ibool row_rollback_on_timeout;

typedef struct row_prebuilt_struct row_prebuilt_t;

/*******************************************************************//**
Frees the blob heap in prebuilt when no longer needed. */
UNIV_INTERN
void
row_mysql_prebuilt_free_blob_heap(
/*==============================*/
	row_prebuilt_t*	prebuilt);	/*!< in: prebuilt struct of a
					ha_innobase:: table handle */
/*******************************************************************//**
Stores a >= 5.0.3 format true VARCHAR length to dest, in the MySQL row
format.
@return pointer to the data, we skip the 1 or 2 bytes at the start
that are used to store the len */
UNIV_INTERN
byte*
row_mysql_store_true_var_len(
/*=========================*/
	byte*	dest,	/*!< in: where to store */
	ulint	len,	/*!< in: length, must fit in two bytes */
	ulint	lenlen);/*!< in: storage length of len: either 1 or 2 bytes */
/*******************************************************************//**
Reads a >= 5.0.3 format true VARCHAR length, in the MySQL row format, and
returns a pointer to the data.
@return pointer to the data, we skip the 1 or 2 bytes at the start
that are used to store the len */
UNIV_INTERN
const byte*
row_mysql_read_true_varchar(
/*========================*/
	ulint*		len,	/*!< out: variable-length field length */
	const byte*	field,	/*!< in: field in the MySQL format */
	ulint		lenlen);/*!< in: storage length of len: either 1
				or 2 bytes */
/*******************************************************************//**
Stores a reference to a BLOB in the MySQL format. */
UNIV_INTERN
void
row_mysql_store_blob_ref(
/*=====================*/
	byte*		dest,	/*!< in: where to store */
	ulint		col_len,/*!< in: dest buffer size: determines into
				how many bytes the BLOB length is stored,
				the space for the length may vary from 1
				to 4 bytes */
	const void*	data,	/*!< in: BLOB data; if the value to store
				is SQL NULL this should be NULL pointer */
	ulint		len);	/*!< in: BLOB length; if the value to store
				is SQL NULL this should be 0; remember
				also to set the NULL bit in the MySQL record
				header! */
/*******************************************************************//**
Reads a reference to a BLOB in the MySQL format.
@return	pointer to BLOB data */
UNIV_INTERN
const byte*
row_mysql_read_blob_ref(
/*====================*/
	ulint*		len,		/*!< out: BLOB length */
	const byte*	ref,		/*!< in: BLOB reference in the
					MySQL format */
	ulint		col_len);	/*!< in: BLOB reference length
					(not BLOB length) */
/**************************************************************//**
Pad a column with spaces. */
UNIV_INTERN
void
row_mysql_pad_col(
/*==============*/
	ulint	mbminlen,	/*!< in: minimum size of a character,
				in bytes */
	byte*	pad,		/*!< out: padded buffer */
	ulint	len);		/*!< in: number of bytes to pad */

/**************************************************************//**
Stores a non-SQL-NULL field given in the MySQL format in the InnoDB format.
The counterpart of this function is row_sel_field_store_in_mysql_format() in
row0sel.c.
@return	up to which byte we used buf in the conversion */
UNIV_INTERN
byte*
row_mysql_store_col_in_innobase_format(
/*===================================*/
	dfield_t*	dfield,		/*!< in/out: dfield where dtype
					information must be already set when
					this function is called! */
	byte*		buf,		/*!< in/out: buffer for a converted
					integer value; this must be at least
					col_len long then! */
	ibool		row_format_col,	/*!< TRUE if the mysql_data is from
					a MySQL row, FALSE if from a MySQL
					key value;
					in MySQL, a true VARCHAR storage
					format differs in a row and in a
					key value: in a key value the length
					is always stored in 2 bytes! */
	const byte*	mysql_data,	/*!< in: MySQL column value, not
					SQL NULL; NOTE that dfield may also
					get a pointer to mysql_data,
					therefore do not discard this as long
					as dfield is used! */
	ulint		col_len,	/*!< in: MySQL column length; NOTE that
					this is the storage length of the
					column in the MySQL format row, not
					necessarily the length of the actual
					payload data; if the column is a true
					VARCHAR then this is irrelevant */
	ulint		comp);		/*!< in: nonzero=compact format */
/****************************************************************//**
Handles user errors and lock waits detected by the database engine.
@return TRUE if it was a lock wait and we should continue running the
query thread */
UNIV_INTERN
ibool
row_mysql_handle_errors(
/*====================*/
	ulint*		new_err,/*!< out: possible new error encountered in
				rollback, or the old error which was
				during the function entry */
	trx_t*		trx,	/*!< in: transaction */
	que_thr_t*	thr,	/*!< in: query thread */
	trx_savept_t*	savept);/*!< in: savepoint */
/********************************************************************//**
Create a prebuilt struct for a MySQL table handle.
@return	own: a prebuilt struct */
UNIV_INTERN
row_prebuilt_t*
row_create_prebuilt(
/*================*/
	dict_table_t*	table,		/*!< in: Innobase table handle */
	ulint		mysql_row_len);	/*!< in: length in bytes of a row in
					the MySQL format */
/********************************************************************//**
Free a prebuilt struct for a MySQL table handle. */
UNIV_INTERN
void
row_prebuilt_free(
/*==============*/
	row_prebuilt_t*	prebuilt,	/*!< in, own: prebuilt struct */
	ibool		dict_locked);	/*!< in: TRUE=data dictionary locked */
/*********************************************************************//**
Updates the transaction pointers in query graphs stored in the prebuilt
struct. */
UNIV_INTERN
void
row_update_prebuilt_trx(
/*====================*/
	row_prebuilt_t*	prebuilt,	/*!< in/out: prebuilt struct
					in MySQL handle */
	trx_t*		trx);		/*!< in: transaction handle */
/*********************************************************************//**
Unlocks AUTO_INC type locks that were possibly reserved by a trx. This
function should be called at the the end of an SQL statement, by the
connection thread that owns the transaction (trx->mysql_thd). */
UNIV_INTERN
void
row_unlock_table_autoinc_for_mysql(
/*===============================*/
	trx_t*	trx);			/*!< in/out: transaction */
/*********************************************************************//**
Sets an AUTO_INC type lock on the table mentioned in prebuilt. The
AUTO_INC lock gives exclusive access to the auto-inc counter of the
table. The lock is reserved only for the duration of an SQL statement.
It is not compatible with another AUTO_INC or exclusive lock on the
table.
@return	error code or DB_SUCCESS */
UNIV_INTERN
int
row_lock_table_autoinc_for_mysql(
/*=============================*/
	row_prebuilt_t*	prebuilt);	/*!< in: prebuilt struct in the MySQL
					table handle */
/*********************************************************************//**
Sets a table lock on the table mentioned in prebuilt.
@return	error code or DB_SUCCESS */
UNIV_INTERN
int
row_lock_table_for_mysql(
/*=====================*/
	row_prebuilt_t*	prebuilt,	/*!< in: prebuilt struct in the MySQL
					table handle */
	dict_table_t*	table,		/*!< in: table to lock, or NULL
					if prebuilt->table should be
					locked as
					prebuilt->select_lock_type */
	ulint		mode);		/*!< in: lock mode of table
					(ignored if table==NULL) */

/*********************************************************************//**
Does an insert for MySQL.
@return	error code or DB_SUCCESS */
UNIV_INTERN
int
row_insert_for_mysql(
/*=================*/
	byte*		mysql_rec,	/*!< in: row in the MySQL format */
	row_prebuilt_t*	prebuilt);	/*!< in: prebuilt struct in MySQL
					handle */
/*********************************************************************//**
Builds a dummy query graph used in selects. */
UNIV_INTERN
void
row_prebuild_sel_graph(
/*===================*/
	row_prebuilt_t*	prebuilt);	/*!< in: prebuilt struct in MySQL
					handle */
/*********************************************************************//**
Gets pointer to a prebuilt update vector used in updates. If the update
graph has not yet been built in the prebuilt struct, then this function
first builds it.
@return	prebuilt update vector */
UNIV_INTERN
upd_t*
row_get_prebuilt_update_vector(
/*===========================*/
	row_prebuilt_t*	prebuilt);	/*!< in: prebuilt struct in MySQL
					handle */
/*********************************************************************//**
Checks if a table is such that we automatically created a clustered
index on it (on row id).
@return	TRUE if the clustered index was generated automatically */
UNIV_INTERN
ibool
row_table_got_default_clust_index(
/*==============================*/
	const dict_table_t*	table);	/*!< in: table */
/*********************************************************************//**
Does an update or delete of a row for MySQL.
@return	error code or DB_SUCCESS */
UNIV_INTERN
int
row_update_for_mysql(
/*=================*/
	byte*		mysql_rec,	/*!< in: the row to be updated, in
					the MySQL format */
	row_prebuilt_t*	prebuilt);	/*!< in: prebuilt struct in MySQL
					handle */
/*********************************************************************//**
This can only be used when srv_locks_unsafe_for_binlog is TRUE or this
session is using a READ COMMITTED or READ UNCOMMITTED isolation level.
Before calling this function row_search_for_mysql() must have
initialized prebuilt->new_rec_locks to store the information which new
record locks really were set. This function removes a newly set
clustered index record lock under prebuilt->pcur or
prebuilt->clust_pcur.  Thus, this implements a 'mini-rollback' that
releases the latest clustered index record lock we set.
@return error code or DB_SUCCESS */
UNIV_INTERN
int
row_unlock_for_mysql(
/*=================*/
	row_prebuilt_t*	prebuilt,	/*!< in/out: prebuilt struct in MySQL
					handle */
	ibool		has_latches_on_recs);/*!< in: TRUE if called
					so that we have the latches on
					the records under pcur and
					clust_pcur, and we do not need
					to reposition the cursors. */
/*********************************************************************//**
Creates an query graph node of 'update' type to be used in the MySQL
interface.
@return	own: update node */
UNIV_INTERN
upd_node_t*
row_create_update_node_for_mysql(
/*=============================*/
	dict_table_t*	table,	/*!< in: table to update */
	mem_heap_t*	heap);	/*!< in: mem heap from which allocated */
/**********************************************************************//**
Does a cascaded delete or set null in a foreign key operation.
@return	error code or DB_SUCCESS */
UNIV_INTERN
ulint
row_update_cascade_for_mysql(
/*=========================*/
	que_thr_t*	thr,	/*!< in: query thread */
	upd_node_t*	node,	/*!< in: update node used in the cascade
				or set null operation */
	dict_table_t*	table);	/*!< in: table where we do the operation */
/*********************************************************************//**
Locks the data dictionary exclusively for performing a table create or other
data dictionary modification operation. */
UNIV_INTERN
void
row_mysql_lock_data_dictionary_func(
/*================================*/
	trx_t*		trx,	/*!< in/out: transaction */
	const char*	file,	/*!< in: file name */
	ulint		line);	/*!< in: line number */
#define row_mysql_lock_data_dictionary(trx)				\
	row_mysql_lock_data_dictionary_func(trx, __FILE__, __LINE__)
/*********************************************************************//**
Unlocks the data dictionary exclusive lock. */
UNIV_INTERN
void
row_mysql_unlock_data_dictionary(
/*=============================*/
	trx_t*	trx);	/*!< in/out: transaction */
/*********************************************************************//**
Locks the data dictionary in shared mode from modifications, for performing
foreign key check, rollback, or other operation invisible to MySQL. */
UNIV_INTERN
void
row_mysql_freeze_data_dictionary_func(
/*==================================*/
	trx_t*		trx,	/*!< in/out: transaction */
	const char*	file,	/*!< in: file name */
	ulint		line);	/*!< in: line number */
#define row_mysql_freeze_data_dictionary(trx)				\
	row_mysql_freeze_data_dictionary_func(trx, __FILE__, __LINE__)
/*********************************************************************//**
Unlocks the data dictionary shared lock. */
UNIV_INTERN
void
row_mysql_unfreeze_data_dictionary(
/*===============================*/
	trx_t*	trx);	/*!< in/out: transaction */
/*********************************************************************//**
Creates a table for MySQL. If the name of the table ends in
one of "innodb_monitor", "innodb_lock_monitor", "innodb_tablespace_monitor",
"innodb_table_monitor", then this will also start the printing of monitor
output by the master thread. If the table name ends in "innodb_mem_validate",
InnoDB will try to invoke mem_validate().
@return	error code or DB_SUCCESS */
UNIV_INTERN
int
row_create_table_for_mysql(
/*=======================*/
	dict_table_t*	table,		/*!< in, own: table definition
					(will be freed) */
	trx_t*		trx);		/*!< in: transaction handle */
/*********************************************************************//**
Does an index creation operation for MySQL. TODO: currently failure
to create an index results in dropping the whole table! This is no problem
currently as all indexes must be created at the same time as the table.
@return	error number or DB_SUCCESS */
UNIV_INTERN
int
row_create_index_for_mysql(
/*=======================*/
	dict_index_t*	index,		/*!< in, own: index definition
					(will be freed) */
	trx_t*		trx,		/*!< in: transaction handle */
	const ulint*	field_lengths); /*!< in: if not NULL, must contain
					dict_index_get_n_fields(index)
					actual field lengths for the
					index columns, which are
					then checked for not being too
					large. */
/*********************************************************************//**
Scans a table create SQL string and adds to the data dictionary
the foreign key constraints declared in the string. This function
should be called after the indexes for a table have been created.
Each foreign key constraint must be accompanied with indexes in
bot participating tables. The indexes are allowed to contain more
fields than mentioned in the constraint.
@return	error code or DB_SUCCESS */
UNIV_INTERN
int
row_table_add_foreign_constraints(
/*==============================*/
	trx_t*		trx,		/*!< in: transaction */
	const char*	sql_string,	/*!< in: table create statement where
					foreign keys are declared like:
				FOREIGN KEY (a, b) REFERENCES table2(c, d),
					table2 can be written also with the
					database name before it: test.table2 */
	size_t		sql_length,	/*!< in: length of sql_string */
	const char*	name,		/*!< in: table full name in the
					normalized form
					database_name/table_name */
	ibool		reject_fks);	/*!< in: if TRUE, fail with error
					code DB_CANNOT_ADD_CONSTRAINT if
					any foreign keys are found. */

/*********************************************************************//**
The master thread in srv0srv.c calls this regularly to drop tables which
we must drop in background after queries to them have ended. Such lazy
dropping of tables is needed in ALTER TABLE on Unix.
@return	how many tables dropped + remaining tables in list */
UNIV_INTERN
ulint
row_drop_tables_for_mysql_in_background(void);
/*=========================================*/
/*********************************************************************//**
Get the background drop list length. NOTE: the caller must own the kernel
mutex!
@return	how many tables in list */
UNIV_INTERN
ulint
row_get_background_drop_list_len_low(void);
/*======================================*/
/*********************************************************************//**
Truncates a table for MySQL.
@return	error code or DB_SUCCESS */
UNIV_INTERN
int
row_truncate_table_for_mysql(
/*=========================*/
	dict_table_t*	table,	/*!< in: table handle */
	trx_t*		trx);	/*!< in: transaction handle */
/*********************************************************************//**
Drops a table for MySQL.  If the name of the dropped table ends in
one of "innodb_monitor", "innodb_lock_monitor", "innodb_tablespace_monitor",
"innodb_table_monitor", then this will also stop the printing of monitor
output by the master thread.  If the data dictionary was not already locked
by the transaction, the transaction will be committed.  Otherwise, the
data dictionary will remain locked.
@return	error code or DB_SUCCESS */
UNIV_INTERN
int
row_drop_table_for_mysql(
/*=====================*/
	const char*	name,	/*!< in: table name */
	trx_t*		trx,	/*!< in: transaction handle */
	ibool		drop_db);/*!< in: TRUE=dropping whole database */
/*********************************************************************//**
Drop all temporary tables during crash recovery. */
UNIV_INTERN
void
row_mysql_drop_temp_tables(void);
/*============================*/

/*********************************************************************//**
Discards the tablespace of a table which stored in an .ibd file. Discarding
means that this function deletes the .ibd file and assigns a new table id for
the table. Also the flag table->ibd_file_missing is set TRUE.
@return	error code or DB_SUCCESS */
UNIV_INTERN
int
row_discard_tablespace_for_mysql(
/*=============================*/
	const char*	name,	/*!< in: table name */
	trx_t*		trx);	/*!< in: transaction handle */
/*****************************************************************//**
Imports a tablespace. The space id in the .ibd file must match the space id
of the table in the data dictionary.
@return	error code or DB_SUCCESS */
UNIV_INTERN
int
row_import_tablespace_for_mysql(
/*============================*/
	const char*	name,	/*!< in: table name */
	trx_t*		trx);	/*!< in: transaction handle */
/*********************************************************************//**
Drops a database for MySQL.
@return	error code or DB_SUCCESS */
UNIV_INTERN
int
row_drop_database_for_mysql(
/*========================*/
	const char*	name,	/*!< in: database name which ends to '/' */
	trx_t*		trx);	/*!< in: transaction handle */
/*********************************************************************//**
Renames a table for MySQL.
@return	error code or DB_SUCCESS */
UNIV_INTERN
ulint
row_rename_table_for_mysql(
/*=======================*/
	const char*	old_name,	/*!< in: old table name */
	const char*	new_name,	/*!< in: new table name */
	trx_t*		trx,		/*!< in: transaction handle */
	ibool		commit);	/*!< in: if TRUE then commit trx */
/*********************************************************************//**
Checks that the index contains entries in an ascending order, unique
constraint is not broken, and calculates the number of index entries
in the read view of the current transaction.
@return	DB_SUCCESS if ok */
UNIV_INTERN
ulint
row_check_index_for_mysql(
/*======================*/
	row_prebuilt_t*		prebuilt,	/*!< in: prebuilt struct
						in MySQL handle */
	const dict_index_t*	index,		/*!< in: index */
	ulint*			n_rows);	/*!< out: number of entries
						seen in the consistent read */

/*********************************************************************//**
Determines if a table is a magic monitor table.
@return	TRUE if monitor table */
UNIV_INTERN
ibool
row_is_magic_monitor_table(
/*=======================*/
	const char*	table_name);	/*!< in: name of the table, in the
					form database/table_name */

/* A struct describing a place for an individual column in the MySQL
row format which is presented to the table handler in ha_innobase.
This template struct is used to speed up row transformations between
Innobase and MySQL. */

typedef struct mysql_row_templ_struct mysql_row_templ_t;
struct mysql_row_templ_struct {
	ulint	col_no;			/*!< column number of the column */
	ulint	rec_field_no;		/*!< field number of the column in an
					Innobase record in the current index;
					not defined if template_type is
					ROW_MYSQL_WHOLE_ROW */
	ulint	clust_rec_field_no;	/*!< field number of the column in an
					Innobase record in the clustered index;
					not defined if template_type is
					ROW_MYSQL_WHOLE_ROW */
	ulint	mysql_col_offset;	/*!< offset of the column in the MySQL
					row format */
	ulint	mysql_col_len;		/*!< length of the column in the MySQL
					row format */
	ulint	mysql_null_byte_offset;	/*!< MySQL NULL bit byte offset in a
					MySQL record */
	ulint	mysql_null_bit_mask;	/*!< bit mask to get the NULL bit,
					zero if column cannot be NULL */
	ulint	type;			/*!< column type in Innobase mtype
					numbers DATA_CHAR... */
	ulint	mysql_type;		/*!< MySQL type code; this is always
					< 256 */
	ulint	mysql_length_bytes;	/*!< if mysql_type
					== DATA_MYSQL_TRUE_VARCHAR, this tells
					whether we should use 1 or 2 bytes to
					store the MySQL true VARCHAR data
					length at the start of row in the MySQL
					format (NOTE that the MySQL key value
					format always uses 2 bytes for the data
					len) */
	ulint	charset;		/*!< MySQL charset-collation code
					of the column, or zero */
	ulint	mbminlen;		/*!< minimum length of a char, in bytes,
					or zero if not a char type */
	ulint	mbmaxlen;		/*!< maximum length of a char, in bytes,
					or zero if not a char type */
	ulint	is_unsigned;		/*!< if a column type is an integer
					type and this field is != 0, then
					it is an unsigned integer type */
};

#define MYSQL_FETCH_CACHE_SIZE		8
/* After fetching this many rows, we start caching them in fetch_cache */
#define MYSQL_FETCH_CACHE_THRESHOLD	4

#define ROW_PREBUILT_ALLOCATED	78540783
#define ROW_PREBUILT_FREED	26423527

/** A struct for (sometimes lazily) prebuilt structures in an Innobase table
handle used within MySQL; these are used to save CPU time. */

struct row_prebuilt_struct {
	ulint		magic_n;	/*!< this magic number is set to
					ROW_PREBUILT_ALLOCATED when created,
					or ROW_PREBUILT_FREED when the
					struct has been freed */
	dict_table_t*	table;		/*!< Innobase table handle */
	dict_index_t*	index;		/*!< current index for a search, if
					any */
	trx_t*		trx;		/*!< current transaction handle */
	unsigned	sql_stat_start:1;/*!< TRUE when we start processing of
					an SQL statement: we may have to set
					an intention lock on the table,
					create a consistent read view etc. */
	unsigned	mysql_has_locked:1;/*!< this is set TRUE when MySQL
					calls external_lock on this handle
					with a lock flag, and set FALSE when
					with the F_UNLOCK flag */
	unsigned	clust_index_was_generated:1;
					/*!< if the user did not define a
					primary key in MySQL, then Innobase
					automatically generated a clustered
					index where the ordering column is
					the row id: in this case this flag
					is set to TRUE */
	unsigned	index_usable:1;	/*!< caches the value of
					row_merge_is_index_usable(trx,index) */
	unsigned	read_just_key:1;/*!< set to 1 when MySQL calls
					ha_innobase::extra with the
					argument HA_EXTRA_KEYREAD; it is enough
					to read just columns defined in
					the index (i.e., no read of the
					clustered index record necessary) */
	unsigned	used_in_HANDLER:1;/*!< TRUE if we have been using this
					handle in a MySQL HANDLER low level
					index cursor command: then we must
					store the pcur position even in a
					unique search from a clustered index,
					because HANDLER allows NEXT and PREV
					in such a situation */
	unsigned	template_type:2;/*!< ROW_MYSQL_WHOLE_ROW,
					ROW_MYSQL_REC_FIELDS,
					ROW_MYSQL_DUMMY_TEMPLATE, or
					ROW_MYSQL_NO_TEMPLATE */
	unsigned	n_template:10;	/*!< number of elements in the
					template */
	unsigned	null_bitmap_len:10;/*!< number of bytes in the SQL NULL
					bitmap at the start of a row in the
					MySQL format */
	unsigned	need_to_access_clustered:1; /*!< if we are fetching
					columns through a secondary index
					and at least one column is not in
					the secondary index, then this is
					set to TRUE */
	unsigned	templ_contains_blob:1;/*!< TRUE if the template contains
					a column with DATA_BLOB ==
					get_innobase_type_from_mysql_type();
					not to be confused with InnoDB
					externally stored columns
					(VARCHAR can be off-page too) */
	mysql_row_templ_t* mysql_template;/*!< template used to transform
					rows fast between MySQL and Innobase
					formats; memory for this template
					is not allocated from 'heap' */
	mem_heap_t*	heap;		/*!< memory heap from which
					these auxiliary structures are
					allocated when needed */
	ins_node_t*	ins_node;	/*!< Innobase SQL insert node
					used to perform inserts
					to the table */
	byte*		ins_upd_rec_buff;/*!< buffer for storing data converted
					to the Innobase format from the MySQL
					format */
	const byte*	default_rec;	/*!< the default values of all columns
					(a "default row") in MySQL format */
	ulint		hint_need_to_fetch_extra_cols;
					/*!< normally this is set to 0; if this
					is set to ROW_RETRIEVE_PRIMARY_KEY,
					then we should at least retrieve all
					columns in the primary key; if this
					is set to ROW_RETRIEVE_ALL_COLS, then
					we must retrieve all columns in the
					key (if read_just_key == 1), or all
					columns in the table */
	upd_node_t*	upd_node;	/*!< Innobase SQL update node used
					to perform updates and deletes */
	que_fork_t*	ins_graph;	/*!< Innobase SQL query graph used
					in inserts */
	que_fork_t*	upd_graph;	/*!< Innobase SQL query graph used
					in updates or deletes */
	btr_pcur_t	pcur;		/*!< persistent cursor used in selects
					and updates */
	btr_pcur_t	clust_pcur;	/*!< persistent cursor used in
					some selects and updates */
	que_fork_t*	sel_graph;	/*!< dummy query graph used in
					selects */
	dtuple_t*	search_tuple;	/*!< prebuilt dtuple used in selects */
	byte		row_id[DATA_ROW_ID_LEN];
					/*!< if the clustered index was
					generated, the row id of the
					last row fetched is stored
					here */
	dtuple_t*	clust_ref;	/*!< prebuilt dtuple used in
					sel/upd/del */
	ulint		select_lock_type;/*!< LOCK_NONE, LOCK_S, or LOCK_X */
	ulint		stored_select_lock_type;/*!< this field is used to
					remember the original select_lock_type
					that was decided in ha_innodb.cc,
					::store_lock(), ::external_lock(),
					etc. */
	ulint		row_read_type;	/*!< ROW_READ_WITH_LOCKS if row locks
					should be the obtained for records
					under an UPDATE or DELETE cursor.
					If innodb_locks_unsafe_for_binlog
					is TRUE, this can be set to
					ROW_READ_TRY_SEMI_CONSISTENT, so that
					if the row under an UPDATE or DELETE
					cursor was locked by another
					transaction, InnoDB will resort
					to reading the last committed value
					('semi-consistent read').  Then,
					this field will be set to
					ROW_READ_DID_SEMI_CONSISTENT to
					indicate that.	If the row does not
					match the WHERE condition, MySQL will
					invoke handler::unlock_row() to
					clear the flag back to
					ROW_READ_TRY_SEMI_CONSISTENT and
					to simply skip the row.	 If
					the row matches, the next call to
					row_search_for_mysql() will lock
					the row.
					This eliminates lock waits in some
					cases; note that this breaks
					serializability. */
	ulint		new_rec_locks;	/*!< normally 0; if
					srv_locks_unsafe_for_binlog is
					TRUE or session is using READ
					COMMITTED or READ UNCOMMITTED
					isolation level, set in
					row_search_for_mysql() if we set a new
					record lock on the secondary
					or clustered index; this is
					used in row_unlock_for_mysql()
					when releasing the lock under
					the cursor if we determine
					after retrieving the row that
					it does not need to be locked
					('mini-rollback') */
	ulint		mysql_prefix_len;/*!< byte offset of the end of
					the last requested column */
	ulint		mysql_row_len;	/*!< length in bytes of a row in the
					MySQL format */
	ulint		n_rows_fetched;	/*!< number of rows fetched after
					positioning the current cursor */
	ulint		fetch_direction;/*!< ROW_SEL_NEXT or ROW_SEL_PREV */
	byte*		fetch_cache[MYSQL_FETCH_CACHE_SIZE];
					/*!< a cache for fetched rows if we
					fetch many rows from the same cursor:
					it saves CPU time to fetch them in a
					batch; we reserve mysql_row_len
					bytes for each such row; these
					pointers point 4 bytes past the
					allocated mem buf start, because
					there is a 4 byte magic number at the
					start and at the end */
	ibool		keep_other_fields_on_keyread; /*!< when using fetch
					cache with HA_EXTRA_KEYREAD, don't
					overwrite other fields in mysql row
					row buffer.*/
	ulint		fetch_cache_first;/*!< position of the first not yet
					fetched row in fetch_cache */
	ulint		n_fetch_cached;	/*!< number of not yet fetched rows
					in fetch_cache */
	mem_heap_t*	blob_heap;	/*!< in SELECTS BLOB fields are copied
					to this heap */
	mem_heap_t*	old_vers_heap;	/*!< memory heap where a previous
					version is built in consistent read */
	/*----------------------*/
	ulonglong	autoinc_last_value;
					/*!< last value of AUTO-INC interval */
	ulonglong	autoinc_increment;/*!< The increment step of the auto
					increment column. Value must be
					greater than or equal to 1. Required to
					calculate the next value */
	ulonglong	autoinc_offset; /*!< The offset passed to
					get_auto_increment() by MySQL. Required
					to calculate the next value */
	ulint		autoinc_error;	/*!< The actual error code encountered
					while trying to init or read the
					autoinc value from the table. We
					store it here so that we can return
					it to MySQL */
	/*----------------------*/
	ulint		magic_n2;	/*!< this should be the same as
					magic_n */
};

#define ROW_PREBUILT_FETCH_MAGIC_N	465765687

#define ROW_MYSQL_WHOLE_ROW	0
#define ROW_MYSQL_REC_FIELDS	1
#define ROW_MYSQL_NO_TEMPLATE	2
#define ROW_MYSQL_DUMMY_TEMPLATE 3	/* dummy template used in
					row_scan_and_check_index */

/* Values for hint_need_to_fetch_extra_cols */
#define ROW_RETRIEVE_PRIMARY_KEY	1
#define ROW_RETRIEVE_ALL_COLS		2

/* Values for row_read_type */
#define ROW_READ_WITH_LOCKS		0
#define ROW_READ_TRY_SEMI_CONSISTENT	1
#define ROW_READ_DID_SEMI_CONSISTENT	2

#ifndef UNIV_NONINL
#include "row0mysql.ic"
#endif

#endif
