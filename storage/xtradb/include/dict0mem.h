/*****************************************************************************

Copyright (c) 1996, 2014, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/dict0mem.h
Data dictionary memory object creation

Created 1/8/1996 Heikki Tuuri
*******************************************************/

#ifndef dict0mem_h
#define dict0mem_h

#include "univ.i"
#include "dict0types.h"
#include "data0type.h"
#include "mem0mem.h"
#include "rem0types.h"
#include "btr0types.h"
#ifndef UNIV_HOTBACKUP
# include "lock0types.h"
# include "que0types.h"
# include "sync0rw.h"
#endif /* !UNIV_HOTBACKUP */
#include "ut0mem.h"
#include "ut0lst.h"
#include "ut0rnd.h"
#include "ut0byte.h"
#include "hash0hash.h"
#include "trx0types.h"
#include "ut0rbt.h"

/** Type flags of an index: OR'ing of the flags is allowed to define a
combination of types */
/* @{ */
#define DICT_CLUSTERED	1	/*!< clustered index */
#define DICT_UNIQUE	2	/*!< unique index */
#define	DICT_UNIVERSAL	4	/*!< index which can contain records from any
				other index */
#define	DICT_IBUF	8	/*!< insert buffer tree */
#define	DICT_CORRUPT	16	/*!< bit to store the corrupted flag
				in SYS_INDEXES.TYPE */

#define	DICT_IT_BITS	5	/*!< number of bits used for
				SYS_INDEXES.TYPE */
/* @} */

/** Types for a table object */
#define DICT_TABLE_ORDINARY		1 /*!< ordinary table */
#if 0 /* not implemented */
#define	DICT_TABLE_CLUSTER_MEMBER	2
#define	DICT_TABLE_CLUSTER		3 /* this means that the table is
					  really a cluster definition */
#endif

/** Table flags.  All unused bits must be 0. */
/* @{ */
#define DICT_TF_COMPACT			1	/* Compact page format.
						This must be set for
						new file formats
						(later than
						DICT_TF_FORMAT_51). */

/** Compressed page size (0=uncompressed, up to 15 compressed sizes) */
/* @{ */
#define DICT_TF_ZSSIZE_SHIFT		1
#define DICT_TF_ZSSIZE_MASK		(15 << DICT_TF_ZSSIZE_SHIFT)
#define DICT_TF_ZSSIZE_MAX (UNIV_PAGE_SIZE_SHIFT - PAGE_ZIP_MIN_SIZE_SHIFT + 1)
/* @} */

/** File format */
/* @{ */
#define DICT_TF_FORMAT_SHIFT		5	/* file format */
#define DICT_TF_FORMAT_MASK		\
((~(~0 << (DICT_TF_BITS - DICT_TF_FORMAT_SHIFT))) << DICT_TF_FORMAT_SHIFT)
#define DICT_TF_FORMAT_51		0	/*!< InnoDB/MySQL up to 5.1 */
#define DICT_TF_FORMAT_ZIP		1	/*!< InnoDB plugin for 5.1:
						compressed tables,
						new BLOB treatment */
/** Maximum supported file format */
#define DICT_TF_FORMAT_MAX		DICT_TF_FORMAT_ZIP

/** Minimum supported file format */
#define DICT_TF_FORMAT_MIN		DICT_TF_FORMAT_51

/* @} */
#define DICT_TF_BITS			6	/*!< number of flag bits */
#if (1 << (DICT_TF_BITS - DICT_TF_FORMAT_SHIFT)) <= DICT_TF_FORMAT_MAX
# error "DICT_TF_BITS is insufficient for DICT_TF_FORMAT_MAX"
#endif
/* @} */

/** @brief Additional table flags.

These flags will be stored in SYS_TABLES.MIX_LEN.  All unused flags
will be written as 0.  The column may contain garbage for tables
created with old versions of InnoDB that only implemented
ROW_FORMAT=REDUNDANT. */
/* @{ */
#define DICT_TF2_SHIFT			DICT_TF_BITS
						/*!< Shift value for
						table->flags. */
#define DICT_TF2_TEMPORARY		1	/*!< TRUE for tables from
						CREATE TEMPORARY TABLE. */
#define DICT_TF2_BITS			(DICT_TF2_SHIFT + 1)
						/*!< Total number of bits
						in table->flags. */
/* @} */

/** Tables could be chained together with Foreign key constraint. When
first load the parent table, we would load all of its descedents.
This could result in rescursive calls and out of stack error eventually.
DICT_FK_MAX_RECURSIVE_LOAD defines the maximum number of recursive loads,
when exceeded, the child table will not be loaded. It will be loaded when
the foreign constraint check needs to be run. */
#define DICT_FK_MAX_RECURSIVE_LOAD	20

/** Similarly, when tables are chained together with foreign key constraints
with on cascading delete/update clause, delete from parent table could
result in recursive cascading calls. This defines the maximum number of
such cascading deletes/updates allowed. When exceeded, the delete from
parent table will fail, and user has to drop excessive foreign constraint
before proceeds. */
#define FK_MAX_CASCADE_DEL		255

/**********************************************************************//**
Creates a table memory object.
@return	own: table object */
UNIV_INTERN
dict_table_t*
dict_mem_table_create(
/*==================*/
	const char*	name,		/*!< in: table name */
	ulint		space,		/*!< in: space where the clustered index
					of the table is placed; this parameter
					is ignored if the table is made
					a member of a cluster */
	ulint		n_cols,		/*!< in: number of columns */
	ulint		flags);		/*!< in: table flags */
/****************************************************************//**
Free a table memory object. */
UNIV_INTERN
void
dict_mem_table_free(
/*================*/
	dict_table_t*	table);		/*!< in: table */
/**********************************************************************//**
Adds a column definition to a table. */
UNIV_INTERN
void
dict_mem_table_add_col(
/*===================*/
	dict_table_t*	table,	/*!< in: table */
	mem_heap_t*	heap,	/*!< in: temporary memory heap, or NULL */
	const char*	name,	/*!< in: column name, or NULL */
	ulint		mtype,	/*!< in: main datatype */
	ulint		prtype,	/*!< in: precise type */
	ulint		len);	/*!< in: precision */
/**********************************************************************//**
This function populates a dict_col_t memory structure with
supplied information. */
UNIV_INTERN
void
dict_mem_fill_column_struct(
/*========================*/
	dict_col_t*	column,		/*!< out: column struct to be
					filled */
	ulint		col_pos,	/*!< in: column position */
	ulint		mtype,		/*!< in: main data type */
	ulint		prtype,		/*!< in: precise type */
	ulint		col_len);	/*!< in: column length */
/**********************************************************************//**
This function poplulates a dict_index_t index memory structure with
supplied information. */
UNIV_INLINE
void
dict_mem_fill_index_struct(
/*=======================*/
	dict_index_t*	index,		/*!< out: index to be filled */
	mem_heap_t*	heap,		/*!< in: memory heap */
	const char*	table_name,	/*!< in: table name */
	const char*	index_name,	/*!< in: index name */
	ulint		space,		/*!< in: space where the index tree is
					placed, ignored if the index is of
					the clustered type */
	ulint		type,		/*!< in: DICT_UNIQUE,
					DICT_CLUSTERED, ... ORed */
	ulint		n_fields);	/*!< in: number of fields */
/**********************************************************************//**
Creates an index memory object.
@return	own: index object */
UNIV_INTERN
dict_index_t*
dict_mem_index_create(
/*==================*/
	const char*	table_name,	/*!< in: table name */
	const char*	index_name,	/*!< in: index name */
	ulint		space,		/*!< in: space where the index tree is
					placed, ignored if the index is of
					the clustered type */
	ulint		type,		/*!< in: DICT_UNIQUE,
					DICT_CLUSTERED, ... ORed */
	ulint		n_fields);	/*!< in: number of fields */
/**********************************************************************//**
Adds a field definition to an index. NOTE: does not take a copy
of the column name if the field is a column. The memory occupied
by the column name may be released only after publishing the index. */
UNIV_INTERN
void
dict_mem_index_add_field(
/*=====================*/
	dict_index_t*	index,		/*!< in: index */
	const char*	name,		/*!< in: column name */
	ulint		prefix_len);	/*!< in: 0 or the column prefix length
					in a MySQL index like
					INDEX (textcol(25)) */
/**********************************************************************//**
Frees an index memory object. */
UNIV_INTERN
void
dict_mem_index_free(
/*================*/
	dict_index_t*	index);	/*!< in: index */
/**********************************************************************//**
Creates and initializes a foreign constraint memory object.
@return	own: foreign constraint struct */
UNIV_INTERN
dict_foreign_t*
dict_mem_foreign_create(void);
/*=========================*/

/**********************************************************************//**
Sets the foreign_table_name_lookup pointer based on the value of
lower_case_table_names.  If that is 0 or 1, foreign_table_name_lookup
will point to foreign_table_name.  If 2, then another string is
allocated from the heap and set to lower case. */
UNIV_INTERN
void
dict_mem_foreign_table_name_lookup_set(
/*===================================*/
	dict_foreign_t*	foreign,	/*!< in/out: foreign struct */
	ibool		do_alloc);	/*!< in: is an alloc needed */

/**********************************************************************//**
Sets the referenced_table_name_lookup pointer based on the value of
lower_case_table_names.  If that is 0 or 1, referenced_table_name_lookup
will point to referenced_table_name.  If 2, then another string is
allocated from the heap and set to lower case. */
UNIV_INTERN
void
dict_mem_referenced_table_name_lookup_set(
/*======================================*/
	dict_foreign_t*	foreign,	/*!< in/out: foreign struct */
	ibool		do_alloc);	/*!< in: is an alloc needed */

/** Data structure for a column in a table */
struct dict_col_struct{
	/*----------------------*/
	/** The following are copied from dtype_t,
	so that all bit-fields can be packed tightly. */
	/* @{ */
	unsigned	mtype:8;	/*!< main data type */
	unsigned	prtype:24;	/*!< precise type; MySQL data
					type, charset code, flags to
					indicate nullability,
					signedness, whether this is a
					binary string, whether this is
					a true VARCHAR where MySQL
					uses 2 bytes to store the length */

	/* the remaining fields do not affect alphabetical ordering: */

	unsigned	len:16;		/*!< length; for MySQL data this
					is field->pack_length(),
					except that for a >= 5.0.3
					type true VARCHAR this is the
					maximum byte length of the
					string data (in addition to
					the string, MySQL uses 1 or 2
					bytes to store the string length) */

	unsigned	mbminmaxlen:5;	/*!< minimum and maximum length of a
					character, in bytes;
					DATA_MBMINMAXLEN(mbminlen,mbmaxlen);
					mbminlen=DATA_MBMINLEN(mbminmaxlen);
					mbmaxlen=DATA_MBMINLEN(mbminmaxlen) */
	/*----------------------*/
	/* End of definitions copied from dtype_t */
	/* @} */

	unsigned	ind:10;		/*!< table column position
					(starting from 0) */
	unsigned	ord_part:1;	/*!< nonzero if this column
					appears in the ordering fields
					of an index */
	unsigned	max_prefix:12;	/*!< maximum index prefix length on
					this column. Our current max limit is
					3072 for Barracuda table */
};

/** @brief DICT_ANTELOPE_MAX_INDEX_COL_LEN is measured in bytes and
is the maximum indexed column length (or indexed prefix length) in
ROW_FORMAT=REDUNDANT and ROW_FORMAT=COMPACT. Also, in any format,
any fixed-length field that is longer than this will be encoded as
a variable-length field.

It is set to 3*256, so that one can create a column prefix index on
256 characters of a TEXT or VARCHAR column also in the UTF-8
charset. In that charset, a character may take at most 3 bytes.  This
constant MUST NOT BE CHANGED, or the compatibility of InnoDB data
files would be at risk! */
#define DICT_ANTELOPE_MAX_INDEX_COL_LEN	REC_ANTELOPE_MAX_INDEX_COL_LEN

/** Find out maximum indexed column length by its table format.
For ROW_FORMAT=REDUNDANT and ROW_FORMAT=COMPACT, the maximum
field length is REC_ANTELOPE_MAX_INDEX_COL_LEN - 1 (767). For new
barracuda format, the length could be REC_VERSION_56_MAX_INDEX_COL_LEN
(3072) bytes */
#define DICT_MAX_FIELD_LEN_BY_FORMAT(table)				\
		((dict_table_get_format(table) < DICT_TF_FORMAT_ZIP)	\
			? (REC_ANTELOPE_MAX_INDEX_COL_LEN - 1)		\
			: REC_VERSION_56_MAX_INDEX_COL_LEN)

#define DICT_MAX_FIELD_LEN_BY_FORMAT_FLAG(flags)			\
		((((flags & DICT_TF_FORMAT_MASK) >> DICT_TF_FORMAT_SHIFT)\
		    < DICT_TF_FORMAT_ZIP)				\
			? (REC_ANTELOPE_MAX_INDEX_COL_LEN - 1)		\
			: REC_VERSION_56_MAX_INDEX_COL_LEN)

/** Defines the maximum fixed length column size */
#define DICT_MAX_FIXED_COL_LEN		DICT_ANTELOPE_MAX_INDEX_COL_LEN

/** Data structure for a field in an index */
struct dict_field_struct{
	dict_col_t*	col;		/*!< pointer to the table column */
	const char*	name;		/*!< name of the column */
	unsigned	prefix_len:12;	/*!< 0 or the length of the column
					prefix in bytes in a MySQL index of
					type, e.g., INDEX (textcol(25));
					must be smaller than
					DICT_MAX_FIELD_LEN_BY_FORMAT;
					NOTE that in the UTF-8 charset, MySQL
					sets this to (mbmaxlen * the prefix len)
					in UTF-8 chars */
	unsigned	fixed_len:10;	/*!< 0 or the fixed length of the
					column if smaller than
					DICT_ANTELOPE_MAX_INDEX_COL_LEN */
};

/** Data structure for an index.  Most fields will be
initialized to 0, NULL or FALSE in dict_mem_index_create(). */
struct dict_index_struct{
	index_id_t	id;	/*!< id of the index */
	rw_lock_t*	search_latch; /*!< latch protecting the AHI partition
				      corresponding to this index */
	hash_table_t*	search_table; /*!< hash table protected by
				      search_latch */
	mem_heap_t*	heap;	/*!< memory heap */
	const char*	name;	/*!< index name */
	const char*	table_name;/*!< table name */
	dict_table_t*	table;	/*!< back pointer to table */
#ifndef UNIV_HOTBACKUP
	unsigned	space:32;
				/*!< space where the index tree is placed */
	unsigned	page:32;/*!< index tree root page number */
#endif /* !UNIV_HOTBACKUP */
	unsigned	type:DICT_IT_BITS;
				/*!< index type (DICT_CLUSTERED, DICT_UNIQUE,
				DICT_UNIVERSAL, DICT_IBUF, DICT_CORRUPT) */
#define MAX_KEY_LENGTH_BITS 12
	unsigned	trx_id_offset:MAX_KEY_LENGTH_BITS;
				/*!< position of the trx id column
				in a clustered index record, if the fields
				before it are known to be of a fixed size,
				0 otherwise */
#if (1<<MAX_KEY_LENGTH_BITS) < MAX_KEY_LENGTH
# error (1<<MAX_KEY_LENGTH_BITS) < MAX_KEY_LENGTH
#endif
	unsigned	n_user_defined_cols:10;
				/*!< number of columns the user defined to
				be in the index: in the internal
				representation we add more columns */
	unsigned	n_uniq:10;/*!< number of fields from the beginning
				which are enough to determine an index
				entry uniquely */
	unsigned	n_def:10;/*!< number of fields defined so far */
	unsigned	n_fields:10;/*!< number of fields in the index */
	unsigned	n_nullable:10;/*!< number of nullable fields */
	unsigned	cached:1;/*!< TRUE if the index object is in the
				dictionary cache */
	unsigned	to_be_dropped:1;
				/*!< TRUE if this index is marked to be
				dropped in ha_innobase::prepare_drop_index(),
				otherwise FALSE. Protected by
				dict_sys->mutex, dict_operation_lock and
				index->lock.*/
	dict_field_t*	fields;	/*!< array of field descriptions */
#ifndef UNIV_HOTBACKUP
	UT_LIST_NODE_T(dict_index_t)
			indexes;/*!< list of indexes of the table */
	btr_search_t*	search_info; /*!< info used in optimistic searches */
	/*----------------------*/
	/** Statistics for query optimization */
	/* @{ */
	ib_int64_t*	stat_n_diff_key_vals;
				/*!< approximate number of different
				key values for this index, for each
				n-column prefix where n <=
				dict_get_n_unique(index); we
				periodically calculate new
				estimates */
	ib_int64_t*	stat_n_non_null_key_vals;
				/* approximate number of non-null key values
				for this index, for each column where
				n < dict_get_n_unique(index); This
				is used when innodb_stats_method is
				"nulls_ignored". */
	ulint		stat_index_size;
				/*!< approximate index size in
				database pages */
	ulint		stat_n_leaf_pages;
				/*!< approximate number of leaf pages in the
				index tree */
	/* @} */
	rw_lock_t	lock;	/*!< read-write lock protecting the
				upper levels of the index tree */
	trx_id_t	trx_id; /*!< id of the transaction that created this
				index, or 0 if the index existed
				when InnoDB was started up */
#endif /* !UNIV_HOTBACKUP */
#ifdef UNIV_BLOB_DEBUG
	mutex_t		blobs_mutex;
				/*!< mutex protecting blobs */
	void*		blobs;	/*!< map of (page_no,heap_no,field_no)
				to first_blob_page_no; protected by
				blobs_mutex; @see btr_blob_dbg_t */
#endif /* UNIV_BLOB_DEBUG */
#ifdef UNIV_DEBUG
	ulint		magic_n;/*!< magic number */
/** Value of dict_index_struct::magic_n */
# define DICT_INDEX_MAGIC_N	76789786
#endif
};

/** Data structure for a foreign key constraint; an example:
FOREIGN KEY (A, B) REFERENCES TABLE2 (C, D).  Most fields will be
initialized to 0, NULL or FALSE in dict_mem_foreign_create(). */
struct dict_foreign_struct{
	mem_heap_t*	heap;		/*!< this object is allocated from
					this memory heap */
	char*		id;		/*!< id of the constraint as a
					null-terminated string */
	unsigned	n_fields:10;	/*!< number of indexes' first fields
					for which the foreign key
					constraint is defined: we allow the
					indexes to contain more fields than
					mentioned in the constraint, as long
					as the first fields are as mentioned */
	unsigned	type:6;		/*!< 0 or DICT_FOREIGN_ON_DELETE_CASCADE
					or DICT_FOREIGN_ON_DELETE_SET_NULL */
	char*		foreign_table_name;/*!< foreign table name */
	char*		foreign_table_name_lookup;
				/*!< foreign table name used for dict lookup */
	dict_table_t*	foreign_table;	/*!< table where the foreign key is */
	const char**	foreign_col_names;/*!< names of the columns in the
					foreign key */
	char*		referenced_table_name;/*!< referenced table name */
	char*		referenced_table_name_lookup;
				/*!< referenced table name for dict lookup*/
	dict_table_t*	referenced_table;/*!< table where the referenced key
					is */
	const char**	referenced_col_names;/*!< names of the referenced
					columns in the referenced table */
	dict_index_t*	foreign_index;	/*!< foreign index; we require that
					both tables contain explicitly defined
					indexes for the constraint: InnoDB
					does not generate new indexes
					implicitly */
	dict_index_t*	referenced_index;/*!< referenced index */
	UT_LIST_NODE_T(dict_foreign_t)
			foreign_list;	/*!< list node for foreign keys of the
					table */
	UT_LIST_NODE_T(dict_foreign_t)
			referenced_list;/*!< list node for referenced
					keys of the table */
};

/** The flags for ON_UPDATE and ON_DELETE can be ORed; the default is that
a foreign key constraint is enforced, therefore RESTRICT just means no flag */
/* @{ */
#define DICT_FOREIGN_ON_DELETE_CASCADE	1	/*!< ON DELETE CASCADE */
#define DICT_FOREIGN_ON_DELETE_SET_NULL	2	/*!< ON UPDATE SET NULL */
#define DICT_FOREIGN_ON_UPDATE_CASCADE	4	/*!< ON DELETE CASCADE */
#define DICT_FOREIGN_ON_UPDATE_SET_NULL	8	/*!< ON UPDATE SET NULL */
#define DICT_FOREIGN_ON_DELETE_NO_ACTION 16	/*!< ON DELETE NO ACTION */
#define DICT_FOREIGN_ON_UPDATE_NO_ACTION 32	/*!< ON UPDATE NO ACTION */
/* @} */

/** Data structure for a database table.  Most fields will be
initialized to 0, NULL or FALSE in dict_mem_table_create(). */
struct dict_table_struct{
	table_id_t	id;	/*!< id of the table */
	mem_heap_t*	heap;	/*!< memory heap */
	char*		name;	/*!< table name */
	const char*	dir_path_of_temp_table;/*!< NULL or the directory path
				where a TEMPORARY table that was explicitly
				created by a user should be placed if
				innodb_file_per_table is defined in my.cnf;
				in Unix this is usually /tmp/..., in Windows
				temp\... */
	unsigned	space:32;
				/*!< space where the clustered index of the
				table is placed */
	unsigned	flags:DICT_TF2_BITS;/*!< DICT_TF_COMPACT, ... */
	unsigned	ibd_file_missing:1;
				/*!< TRUE if this is in a single-table
				tablespace and the .ibd file is missing; then
				we must return in ha_innodb.cc an error if the
				user tries to query such an orphaned table */
	unsigned	tablespace_discarded:1;
				/*!< this flag is set TRUE when the user
				calls DISCARD TABLESPACE on this
				table, and reset to FALSE in IMPORT
				TABLESPACE */
	unsigned	cached:1;/*!< TRUE if the table object has been added
				to the dictionary cache */
	unsigned	n_def:10;/*!< number of columns defined so far */
	unsigned	n_cols:10;/*!< number of columns */
	unsigned	corrupted:1;
				/*!< TRUE if table is corrupted */
	dict_col_t*	cols;	/*!< array of column descriptions */
	const char*	col_names;
				/*!< Column names packed in a character string
				"name1\0name2\0...nameN\0".  Until
				the string contains n_cols, it will be
				allocated from a temporary heap.  The final
				string will be allocated from table->heap. */
#ifndef UNIV_HOTBACKUP
	hash_node_t	name_hash; /*!< hash chain node */
	hash_node_t	id_hash; /*!< hash chain node */
	UT_LIST_BASE_NODE_T(dict_index_t)
			indexes; /*!< list of indexes of the table */
	UT_LIST_BASE_NODE_T(dict_foreign_t)
			foreign_list;/*!< list of foreign key constraints
				in the table; these refer to columns
				in other tables */
	UT_LIST_BASE_NODE_T(dict_foreign_t)
			referenced_list;/*!< list of foreign key constraints
				which refer to this table */

	ib_rbt_t*	foreign_rbt;	/*!< a rb-tree of all foreign keys
					listed in foreign_list, sorted by
					foreign->id */
	ib_rbt_t*	referenced_rbt;	/*!< a rb-tree of all foreign keys
					listed in referenced_list, sorted by
					foreign->id */

	UT_LIST_NODE_T(dict_table_t)
			table_LRU; /*!< node of the LRU list of tables */
	ulint		n_mysql_handles_opened;
				/*!< count of how many handles MySQL has opened
				to this table; dropping of the table is
				NOT allowed until this count gets to zero;
				MySQL does NOT itself check the number of
				open handles at drop */
	unsigned	fk_max_recusive_level:8;
				/*!< maximum recursive level we support when
				loading tables chained together with FK
				constraints. If exceeds this level, we will
				stop loading child table into memory along with
				its parent table */
	ulint		n_foreign_key_checks_running;
				/*!< count of how many foreign key check
				operations are currently being performed
				on the table: we cannot drop the table while
				there are foreign key checks running on
				it! */
	trx_id_t	query_cache_inv_trx_id;
				/*!< transactions whose trx id is
				smaller than this number are not
				allowed to store to the MySQL query
				cache or retrieve from it; when a trx
				with undo logs commits, it sets this
				to the value of the trx id counter for
				the tables it had an IX lock on */
	UT_LIST_BASE_NODE_T(lock_t)
			locks; /*!< list of locks on the table */
#ifdef UNIV_DEBUG
	/*----------------------*/
	ibool		does_not_fit_in_memory;
				/*!< this field is used to specify in
				simulations tables which are so big
				that disk should be accessed: disk
				access is simulated by putting the
				thread to sleep for a while; NOTE that
				this flag is not stored to the data
				dictionary on disk, and the database
				will forget about value TRUE if it has
				to reload the table definition from
				disk */
#endif /* UNIV_DEBUG */
	/*----------------------*/
	unsigned	big_rows:1;
				/*!< flag: TRUE if the maximum length of
				a single row exceeds BIG_ROW_SIZE;
				initialized in dict_table_add_to_cache() */
				/** Statistics for query optimization.
				The following stat_* members are usually
				protected by dict_table_stats_lock(). In
				some exceptional cases (performance critical
				code paths) we access or modify stat_n_rows
				and stat_modified_counter without any
				protection. */
				/* @{ */
	unsigned	stat_initialized:1; /*!< TRUE if statistics have
				been calculated the first time
				after database startup or table creation */
	ib_int64_t	stat_n_rows;
				/*!< approximate number of rows in the table;
				we periodically calculate new estimates */
	ulint		stat_clustered_index_size;
				/*!< approximate clustered index size in
				database pages */
	ulint		stat_sum_of_other_index_sizes;
				/*!< other indexes in database pages */
	ulint		stat_modified_counter;
				/*!< when a row is inserted, updated,
				or deleted,
				we add 1 to this number; we calculate new
				estimates for the stat_... values for the
				table and the indexes at an interval of 2 GB
				or when about 1 / 16 of table has been
				modified; also when the estimate operation is
				called for MySQL SHOW TABLE STATUS; the
				counter is reset to zero at statistics
				calculation; this counter is not protected by
				any latch, because this is only used for
				heuristics */
				/* @} */
	/*----------------------*/
				/**!< The following fields are used by the
				AUTOINC code.  The actual collection of
				tables locked during AUTOINC read/write is
				kept in trx_t. In order to quickly determine
				whether a transaction has locked the AUTOINC
				lock we keep a pointer to the transaction
				here in the autoinc_trx variable. This is to
				avoid acquiring the kernel mutex and scanning
				the vector in trx_t.

				When an AUTOINC lock has to wait, the
				corresponding lock instance is created on
				the trx lock heap rather than use the
				pre-allocated instance in autoinc_lock below.*/
				/* @{ */
	lock_t*		autoinc_lock;
				/*!< a buffer for an AUTOINC lock
				for this table: we allocate the memory here
				so that individual transactions can get it
				and release it without a need to allocate
				space from the lock heap of the trx:
				otherwise the lock heap would grow rapidly
				if we do a large insert from a select */
	mutex_t		autoinc_mutex;
				/*!< mutex protecting the autoincrement
				counter */
	ib_uint64_t	autoinc;/*!< autoinc counter value to give to the
				next inserted row */
	ulong		n_waiting_or_granted_auto_inc_locks;
				/*!< This counter is used to track the number
				of granted and pending autoinc locks on this
				table. This value is set after acquiring the
				kernel mutex but we peek the contents to
				determine whether other transactions have
				acquired the AUTOINC lock or not. Of course
				only one transaction can be granted the
				lock but there can be multiple waiters. */
	const trx_t*		autoinc_trx;
				/*!< The transaction that currently holds the
				the AUTOINC lock on this table. */
				/* @} */
	/*----------------------*/
	ibool		is_corrupt;
#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_DEBUG
	ulint		magic_n;/*!< magic number */
/** Value of dict_table_struct::magic_n */
# define DICT_TABLE_MAGIC_N	76333786
#endif /* UNIV_DEBUG */
};

#ifndef UNIV_NONINL
#include "dict0mem.ic"
#endif

#endif
