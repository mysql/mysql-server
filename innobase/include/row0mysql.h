/******************************************************
Interface between Innobase row operations and MySQL.
Contains also create table and other data dictionary operations.

(c) 2000 Innobase Oy

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

typedef struct row_prebuilt_struct row_prebuilt_t;

/***********************************************************************
Stores a variable-length field (like VARCHAR) length to dest, in the
MySQL format. */
UNIV_INLINE
byte*
row_mysql_store_var_len(
/*====================*/
			/* out: dest + 2 */
	byte*	dest,	/* in: where to store */
	ulint	len);	/* in: length, must fit in two bytes */
/***********************************************************************
Reads a MySQL format variable-length field (like VARCHAR) length and
returns pointer to the field data. */
UNIV_INLINE
byte*
row_mysql_read_var_ref(
/*===================*/
			/* out: field + 2 */
	ulint*	len,	/* out: variable-length field length */
	byte*	field);	/* in: field */
/***********************************************************************
Reads a MySQL format variable-length field (like VARCHAR) length and
returns pointer to the field data. */

byte*
row_mysql_read_var_ref_noninline(
/*=============================*/
			/* out: field + 2 */
	ulint*	len,	/* out: variable-length field length */
	byte*	field);	/* in: field */
/***********************************************************************
Stores a reference to a BLOB in the MySQL format. */

void
row_mysql_store_blob_ref(
/*=====================*/
	byte*	dest,		/* in: where to store */
	ulint	col_len,	/* in: dest buffer size: determines into
				how many bytes the BLOB length is stored,
				this may vary from 1 to 4 bytes */
	byte*	data,		/* in: BLOB data */
	ulint	len);		/* in: BLOB length */
/***********************************************************************
Reads a reference to a BLOB in the MySQL format. */

byte*
row_mysql_read_blob_ref(
/*====================*/
				/* out: pointer to BLOB data */
	ulint*	len,		/* out: BLOB length */
	byte*	ref,		/* in: BLOB reference in the MySQL format */
	ulint	col_len);	/* in: BLOB reference length (not BLOB
				length) */
/******************************************************************
Stores a non-SQL-NULL field given in the MySQL format in the Innobase
format. */
UNIV_INLINE
void
row_mysql_store_col_in_innobase_format(
/*===================================*/
	dfield_t*	dfield,		/* in/out: dfield */
	byte*		buf,		/* in/out: buffer for the converted
					value */
	byte*		mysql_data,	/* in: MySQL column value, not
					SQL NULL; NOTE that dfield may also
					get a pointer to mysql_data,
					therefore do not discard this as long
					as dfield is used! */
	ulint		col_len,	/* in: MySQL column length */
	ulint		type,		/* in: data type */
	ulint		is_unsigned);	/* in: != 0 if unsigned integer type */
/********************************************************************
Handles user errors and lock waits detected by the database engine. */

ibool
row_mysql_handle_errors(
/*====================*/
				/* out: TRUE if it was a lock wait and
				we should continue running the query thread */
	ulint*		new_err,/* out: possible new error encountered in
				rollback, or the old error which was
				during the function entry */
	trx_t*		trx,	/* in: transaction */
	que_thr_t*	thr,	/* in: query thread */
	trx_savept_t*	savept);/* in: savepoint */
/************************************************************************
Create a prebuilt struct for a MySQL table handle. */

row_prebuilt_t*
row_create_prebuilt(
/*================*/
				/* out, own: a prebuilt struct */
	dict_table_t*	table);	/* in: Innobase table handle */
/************************************************************************
Free a prebuilt struct for a MySQL table handle. */

void
row_prebuilt_free(
/*==============*/
	row_prebuilt_t*	prebuilt);	/* in, own: prebuilt struct */
/*************************************************************************
Updates the transaction pointers in query graphs stored in the prebuilt
struct. */

void
row_update_prebuilt_trx(
/*====================*/
					/* out: prebuilt dtuple */
	row_prebuilt_t*	prebuilt,	/* in: prebuilt struct in MySQL
					handle */
	trx_t*		trx);		/* in: transaction handle */
/*************************************************************************
Unlocks an AUTO_INC type lock possibly reserved by trx. */

void		  	
row_unlock_table_autoinc_for_mysql(
/*===============================*/
	trx_t*	trx);	/* in: transaction */
/*************************************************************************
Sets an AUTO_INC type lock on the table mentioned in prebuilt. The
AUTO_INC lock gives exclusive access to the auto-inc counter of the
table. The lock is reserved only for the duration of an SQL statement.
It is not compatible with another AUTO_INC or exclusive lock on the
table. */

int
row_lock_table_autoinc_for_mysql(
/*=============================*/
					/* out: error code or DB_SUCCESS */
	row_prebuilt_t*	prebuilt);	/* in: prebuilt struct in the MySQL
					table handle */
/*************************************************************************
Does an insert for MySQL. */

int
row_insert_for_mysql(
/*=================*/
					/* out: error code or DB_SUCCESS */
	byte*		mysql_rec,	/* in: row in the MySQL format */
	row_prebuilt_t*	prebuilt);	/* in: prebuilt struct in MySQL
					handle */
/*************************************************************************
Builds a dummy query graph used in selects. */

void
row_prebuild_sel_graph(
/*===================*/
	row_prebuilt_t*	prebuilt);	/* in: prebuilt struct in MySQL
					handle */
/*************************************************************************
Gets pointer to a prebuilt update vector used in updates. If the update
graph has not yet been built in the prebuilt struct, then this function
first builds it. */

upd_t*
row_get_prebuilt_update_vector(
/*===========================*/
					/* out: prebuilt update vector */
	row_prebuilt_t*	prebuilt);	/* in: prebuilt struct in MySQL
					handle */
/*************************************************************************
Checks if a table is such that we automatically created a clustered
index on it (on row id). */

ibool
row_table_got_default_clust_index(
/*==============================*/
	dict_table_t*	table);
/*************************************************************************
Calculates the key number used inside MySQL for an Innobase index. We have
to take into account if we generated a default clustered index for the table */

ulint
row_get_mysql_key_number_for_index(
/*===============================*/
	dict_index_t*	index);
/*************************************************************************
Does an update or delete of a row for MySQL. */

int
row_update_for_mysql(
/*=================*/
					/* out: error code or DB_SUCCESS */
	byte*		mysql_rec,	/* in: the row to be updated, in
					the MySQL format */
	row_prebuilt_t*	prebuilt);	/* in: prebuilt struct in MySQL
					handle */
/*************************************************************************
Does a table creation operation for MySQL. If the name of the created
table ends to characters INNODB_MONITOR, then this also starts
printing of monitor output by the master thread. */

int
row_create_table_for_mysql(
/*=======================*/
					/* out: error code or DB_SUCCESS */
	dict_table_t*	table,		/* in: table definition */
	trx_t*		trx);		/* in: transaction handle */
/*************************************************************************
Does an index creation operation for MySQL. TODO: currently failure
to create an index results in dropping the whole table! This is no problem
currently as all indexes must be created at the same time as the table. */

int
row_create_index_for_mysql(
/*=======================*/
					/* out: error number or DB_SUCCESS */
	dict_index_t*	index,		/* in: index defintion */
	trx_t*		trx);		/* in: transaction handle */
/*************************************************************************
Scans a table create SQL string and adds to the data dictionary
the foreign key constraints declared in the string. This function
should be called after the indexes for a table have been created.
Each foreign key constraint must be accompanied with indexes in
bot participating tables. The indexes are allowed to contain more
fields than mentioned in the constraint. */

int
row_table_add_foreign_constraints(
/*==============================*/
				/* out: error code or DB_SUCCESS */
	trx_t*	trx,		/* in: transaction */
	char*	sql_string,	/* in: table create statement where
				foreign keys are declared like:
				FOREIGN KEY (a, b) REFERENCES table2(c, d),
				table2 can be written also with the database
				name before it: test.table2 */
	char*	name);		/* in: table full name in the normalized form
				database_name/table_name */
/*************************************************************************
Drops a table for MySQL. If the name of the dropped table ends to
characters INNODB_MONITOR, then this also stops printing of monitor
output by the master thread. */

int
row_drop_table_for_mysql(
/*=====================*/
				/* out: error code or DB_SUCCESS */
	char*	name,		/* in: table name */
	trx_t*	trx,		/* in: transaction handle */
	ibool	has_dict_mutex);/* in: TRUE if the caller already owns the
				dictionary system mutex */
/*************************************************************************
Drops a database for MySQL. */

int
row_drop_database_for_mysql(
/*========================*/
			/* out: error code or DB_SUCCESS */
	char*	name,	/* in: database name which ends to '/' */
	trx_t*	trx);	/* in: transaction handle */
/*************************************************************************
Renames a table for MySQL. */

int
row_rename_table_for_mysql(
/*=======================*/
				/* out: error code or DB_SUCCESS */
	char*	old_name,	/* in: old table name */
	char*	new_name,	/* in: new table name */
	trx_t*	trx);		/* in: transaction handle */
/*************************************************************************
Checks a table for corruption. */

ulint
row_check_table_for_mysql(
/*======================*/
					/* out: DB_ERROR or DB_SUCCESS */
	row_prebuilt_t*	prebuilt);	/* in: prebuilt struct in MySQL
					handle */

/* A struct describing a place for an individual column in the MySQL
row format which is presented to the table handler in ha_innobase.
This template struct is used to speed up row transformations between
Innobase and MySQL. */

typedef struct mysql_row_templ_struct mysql_row_templ_t;
struct mysql_row_templ_struct {
	ulint	col_no;			/* column number of the column */
	ulint	rec_field_no;		/* field number of the column in an
					Innobase record in the current index;
					not defined if template_type is
					ROW_MYSQL_WHOLE_ROW */
	ulint	mysql_col_offset;	/* offset of the column in the MySQL
					row format */
	ulint	mysql_col_len;		/* length of the column in the MySQL
					row format */
	ulint	mysql_null_byte_offset;	/* MySQL NULL bit byte offset in a
					MySQL record */
	ulint	mysql_null_bit_mask;	/* bit mask to get the NULL bit,
					zero if column cannot be NULL */
	ulint	type;			/* column type in Innobase mtype
					numbers DATA_CHAR... */
	ulint	is_unsigned;		/* if a column type is an integer
					type and this field is != 0, then
					it is an unsigned integer type */
};

#define MYSQL_FETCH_CACHE_SIZE		8
/* After fetching this many rows, we start caching them in fetch_cache */
#define MYSQL_FETCH_CACHE_THRESHOLD	4


/* A struct for (sometimes lazily) prebuilt structures in an Innobase table
handle used within MySQL; these are used to save CPU time. */

struct row_prebuilt_struct {
	dict_table_t*	table;		/* Innobase table handle */
	trx_t*		trx;		/* current transaction handle */
	ibool		sql_stat_start;	/* TRUE when we start processing of
					an SQL statement: we may have to set
					an intention lock on the table,
					create a consistent read view etc. */
	ibool		clust_index_was_generated;
					/* if the user did not define a
					primary key in MySQL, then Innobase
					automatically generated a clustered
					index where the ordering column is
					the row id: in this case this flag
					is set to TRUE */
	dict_index_t*	index;		/* current index for a search, if any */
	ulint		template_type;	/* ROW_MYSQL_WHOLE_ROW, 
					ROW_MYSQL_REC_FIELDS,
					ROW_MYSQL_DUMMY_TEMPLATE, or
					ROW_MYSQL_NO_TEMPLATE */
	ulint		n_template;	/* number of elements in the
					template */
	ulint		null_bitmap_len;/* number of bytes in the SQL NULL
					bitmap at the start of a row in the
					MySQL format */
	ibool		need_to_access_clustered; /* if we are fetching
					columns through a secondary index
					and at least one column is not in
					the secondary index, then this is
					set to TRUE */
	ibool		templ_contains_blob;/* TRUE if the template contains
					BLOB column(s) */
	mysql_row_templ_t* mysql_template;/* template used to transform
					rows fast between MySQL and Innobase
					formats; memory for this template
					is not allocated from 'heap' */
	mem_heap_t*	heap;		/* memory heap from which
					these auxiliary structures are
					allocated when needed */
	ins_node_t*	ins_node;	/* Innobase SQL insert node
					used to perform inserts
					to the table */
	byte*		ins_upd_rec_buff;/* buffer for storing data converted
					to the Innobase format from the MySQL
					format */
	ibool		in_update_remember_pos;
					/* if an update is processed, then if
					this flag is set to TRUE, it means
					that the stored cursor position in
					SELECT is the right position also
					for the update: we can just restore
					the cursor and save CPU time */
	upd_node_t*	upd_node;	/* Innobase SQL update node used
					to perform updates and deletes */
	que_fork_t*	ins_graph;	/* Innobase SQL query graph used
					in inserts */
	que_fork_t*	upd_graph;	/* Innobase SQL query graph used
					in updates or deletes */
	btr_pcur_t*	pcur;		/* persistent cursor used in selects
					and updates */
	btr_pcur_t*	clust_pcur;	/* persistent cursor used in
					some selects and updates */
	que_fork_t*	sel_graph;	/* dummy query graph used in
					selects */
	dtuple_t*	search_tuple;	/* prebuilt dtuple used in selects */
	byte		row_id[DATA_ROW_ID_LEN];
					/* if the clustered index was generated,
					the row id of the last row fetched is
					stored here */
	dtuple_t*	clust_ref;	/* prebuilt dtuple used in
					sel/upd/del */
	ulint		select_lock_type;/* LOCK_NONE, LOCK_S, or LOCK_X */
	ulint		mysql_row_len;	/* length in bytes of a row in the
					MySQL format */
	ulint		n_rows_fetched;	/* number of rows fetched after
					positioning the current cursor */
	ulint		fetch_direction;/* ROW_SEL_NEXT or ROW_SEL_PREV */
	byte*		fetch_cache[MYSQL_FETCH_CACHE_SIZE];
					/* a cache for fetched rows if we
					fetch many rows from the same cursor:
					it saves CPU time to fetch them in a
					batch; we reserve mysql_row_len
					bytes for each such row */
	ulint		fetch_cache_first;/* position of the first not yet
					fetched row in fetch_cache */
	ulint		n_fetch_cached;	/* number of not yet fetched rows
					in fetch_cache */
	mem_heap_t*	blob_heap;	/* in SELECTS BLOB fields are copied
					to this heap */
	mem_heap_t*	old_vers_heap;	/* memory heap where a previous
					version is built in consistent read */
};

#define ROW_MYSQL_WHOLE_ROW	0
#define ROW_MYSQL_REC_FIELDS	1
#define ROW_MYSQL_NO_TEMPLATE	2
#define ROW_MYSQL_DUMMY_TEMPLATE 3	/* dummy template used in
					row_scan_and_check_index */

#ifndef UNIV_NONINL
#include "row0mysql.ic"
#endif

#endif 
