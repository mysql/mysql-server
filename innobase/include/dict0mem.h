/******************************************************
Data dictionary memory object creation

(c) 1996 Innobase Oy

Created 1/8/1996 Heikki Tuuri
*******************************************************/

#ifndef dict0mem_h
#define dict0mem_h

#include "univ.i"
#include "dict0types.h"
#include "data0type.h"
#include "data0data.h"
#include "mem0mem.h"
#include "rem0types.h"
#include "btr0types.h"
#include "ut0mem.h"
#include "ut0lst.h"
#include "ut0rnd.h"
#include "ut0byte.h"
#include "sync0rw.h"
#include "lock0types.h"
#include "hash0hash.h"
#include "que0types.h"

/* Type flags of an index: OR'ing of the flags is allowed to define a
combination of types */
#define DICT_CLUSTERED	1	/* clustered index */
#define DICT_UNIQUE	2	/* unique index */
#define	DICT_UNIVERSAL 	4	/* index which can contain records from any
				other index */
#define	DICT_IBUF 	8	/* insert buffer tree */
				
/* Flags for ordering an index field: OR'ing of the flags allowed */
#define	DICT_DESCEND	1	/* in descending order (default ascending) */

/* Types for a table object */
#define DICT_TABLE_ORDINARY		1
#define	DICT_TABLE_CLUSTER_MEMBER	2
#define	DICT_TABLE_CLUSTER		3 /* this means that the table is
					  really a cluster definition */

/**************************************************************************
Creates a table memory object. */

dict_table_t*
dict_mem_table_create(
/*==================*/
				/* out, own: table object */
	char*	name,		/* in: table name */
	ulint	space,		/* in: space where the clustered index of
				the table is placed; this parameter is
				ignored if the table is made a member of
				a cluster */
	ulint	n_cols);	/* in: number of columns */
/**************************************************************************
Creates a cluster memory object. */

dict_cluster_t*
dict_mem_cluster_create(
/*====================*/
				/* out, own: cluster object (where the type
				dict_cluster_t == dict_table_t) */
	char*	name,		/* in: cluster name */
	ulint	space,		/* in: space where the clustered indexes
				of the member tables are placed */
	ulint	n_cols,		/* in: number of columns */
	ulint	mix_len);	/* in: length of the common key prefix in the
				cluster */
/**************************************************************************
Declares a non-published table as a member in a cluster. */

void
dict_mem_table_make_cluster_member(
/*===============================*/
	dict_table_t*	table,		/* in: non-published table */
	char*		cluster_name);	/* in: cluster name */
/**************************************************************************
Adds a column definition to a table. */

void
dict_mem_table_add_col(
/*===================*/
	dict_table_t*	table,	/* in: table */
	char*		name,	/* in: column name */
	ulint		mtype,	/* in: main datatype */
	ulint		prtype,	/* in: precise type */
	ulint		len,	/* in: length */
	ulint		prec);	/* in: precision */
/**************************************************************************
Creates an index memory object. */

dict_index_t*
dict_mem_index_create(
/*==================*/
				/* out, own: index object */
	char*	table_name,	/* in: table name */
	char*	index_name,	/* in: index name */
	ulint	space,		/* in: space where the index tree is placed,
				ignored if the index is of the clustered
				type */
	ulint	type,		/* in: DICT_UNIQUE, DICT_CLUSTERED, ... ORed */
	ulint	n_fields);	/* in: number of fields */
/**************************************************************************
Adds a field definition to an index. NOTE: does not take a copy
of the column name if the field is a column. The memory occupied
by the column name may be released only after publishing the index. */

void
dict_mem_index_add_field(
/*=====================*/
	dict_index_t*	index,	/* in: index */
	char*		name,	/* in: column name */
	ulint		order);	/* in: order criterion; 0 means an ascending
				order */
/**************************************************************************
Frees an index memory object. */

void
dict_mem_index_free(
/*================*/
	dict_index_t*	index);	/* in: index */
/**************************************************************************
Creates a procedure memory object. */

dict_proc_t*
dict_mem_procedure_create(
/*======================*/
					/* out, own: procedure object */
	char*		name,		/* in: procedure name */
	char*		sql_string,	/* in: procedure definition as an SQL
					string */
	que_fork_t*	graph);		/* in: parsed procedure graph */
					

/* Data structure for a column in a table */
struct dict_col_struct{
	hash_node_t	hash;	/* hash chain node */
	ulint		ind;	/* table column position (they are numbered
				starting from 0) */
	ulint		clust_pos;/* position of the column in the
				clustered index */
	ulint		ord_part;/* count of how many times this column
				appears in an ordering fields of an index */
	char*		name;	/* name */
	dtype_t		type;	/* data type */
	dict_table_t*	table;	/* back pointer to table of this column */
	ulint		aux;	/* this is used as an auxiliary variable 
				in some of the functions below */
};

/* Data structure for a field in an index */
struct dict_field_struct{
	dict_col_t*	col;	/* pointer to the table column */
	char*		name;	/* name of the column */
	ulint		order;	/* flags for ordering this field:
				DICT_DESCEND, ... */
};

/* Data structure for an index tree */
struct dict_tree_struct{
	ulint		type;	/* tree type */
	dulint		id;	/* id of the index stored in the tree, in the
				case of a mixed index, the id of the clustered
				index of the cluster table */
	ulint		space;	/* space of index tree */
	ulint		page;	/* index tree root page number */
	byte		pad[64];/* Padding to prevent other memory hotspots on
				the same memory cache line */
	rw_lock_t	lock;	/* read-write lock protecting the upper levels
				of the index tree */
	ulint		mem_fix;/* count of how many times this tree
				struct has been memoryfixed (by mini-
				transactions wanting to access the index
				tree) */
	UT_LIST_BASE_NODE_T(dict_index_t)
			tree_indexes; /* list of indexes stored in the
				index tree: if the tree is not of the
				mixed type there is only one index in
				the list; if the tree is of the mixed
				type, the first index in the list is the
				index of the cluster which owns the tree */
	ulint		magic_n;/* magic number */
};

#define	DICT_TREE_MAGIC_N	7545676

/* Data structure for an index */
struct dict_index_struct{
	dulint		id;	/* id of the index */
	mem_heap_t*	heap;	/* memory heap */
	ulint		type;	/* index type */
	char*		name;	/* index name */
	char*		table_name; /* table name */
	dict_table_t*	table;	/* back pointer to table */
	ulint		space;	/* space where the index tree is placed */
	ulint		page_no;/* page number of the index tree root */
	ulint		trx_id_offset;/* position of the the trx id column
				in a clustered index record, if the fields
				before it are known to be of a fixed size,
				0 otherwise */
	ulint		n_user_defined_cols;
				/* number of columns the user defined to
				be in the index: in the internal
				representation we add more columns */
	ulint		n_uniq;	/* number of fields from the beginning
				which are enough to determine an index
				entry uniquely */
	ulint		n_def;	/* number of fields defined so far */
	ulint		n_fields;/* number of fields in the index */
	dict_field_t*	fields;	/* array of field descriptions */
	UT_LIST_NODE_T(dict_index_t)
			indexes;/* list of indexes of the table */
	dict_tree_t*	tree;	/* index tree struct */
	UT_LIST_NODE_T(dict_index_t)
			tree_indexes; /* list of indexes of the same index
				tree */
	ibool		cached;	/* TRUE if the index object is in the
				dictionary cache */
	btr_search_t*	search_info; /* info used in optimistic searches */
	/*----------------------*/
	ulint		stat_n_diff_key_vals;
				/* approximate number of different key values
				for this index; we periodically calculate
				new estimates */
	ulint		stat_index_size;
				/* approximate index size in database pages */
	ulint		magic_n;/* magic number */
};

#define	DICT_INDEX_MAGIC_N	76789786

/* Data structure for a database table */
struct dict_table_struct{
	dulint		id;	/* id of the table or cluster */
	ulint		type;	/* DICT_TABLE_ORDINARY, ... */
	mem_heap_t*	heap;	/* memory heap */
	char*		name;	/* table name */
	ulint		space;	/* space where the clustered index of the
				table is placed */
	hash_node_t	name_hash; /* hash chain node */
	hash_node_t	id_hash; /* hash chain node */
	ulint		n_def;	/* number of columns defined so far */
	ulint		n_cols;	/* number of columns */
	dict_col_t*	cols;	/* array of column descriptions */
	UT_LIST_BASE_NODE_T(dict_index_t)
			indexes; /* list of indexes of the table */
	UT_LIST_NODE_T(dict_table_t)
			table_LRU; /* node of the LRU list of tables */
	ulint		mem_fix;/* count of how many times the table 
				and its indexes has been fixed in memory;
				currently NOT used */
	ibool		cached;	/* TRUE if the table object has been added
				to the dictionary cache */
	UT_LIST_BASE_NODE_T(lock_t)
			locks; /* list of locks on the table */
	/*----------------------*/
	dulint		mix_id;	/* if the table is a member in a cluster,
				this is its mix id */
	ulint		mix_len;/* if the table is a cluster or a member
				this is the common key prefix lenght */
	ulint		mix_id_len;/* mix id length in a compressed form */
	byte		mix_id_buf[12];
				/* mix id of a mixed table written in
				a compressed form */
	char*		cluster_name; /* if the table is a member in a
				cluster, this is the name of the cluster */
	/*----------------------*/
	ibool		does_not_fit_in_memory;
				/* this field is used to specify in simulations
				tables which are so big that disk should be
				accessed: disk access is simulated by
				putting the thread to sleep for a while;
				NOTE that this flag is not stored to the data
				dictionary on disk, and the database will
				forget about value TRUE if it has to reload
				the table definition from disk */
	/*----------------------*/
	ulint		stat_n_rows;
				/* approximate number of rows in the table;
				we periodically calculate new estimates */
	ulint		stat_clustered_index_size;
				/* approximate clustered index size in
				database pages */
	ulint		stat_sum_of_other_index_sizes;
				/* other indexes in database pages */
	ulint		stat_last_estimate_counter;
				/* when the estimates were last time
				calculated; a value (ulint)-1 denotes that
				they have not yet been calculated for this
				table (or the counter has wrapped over) */
	ulint		stat_modif_counter;
				/* when a row is inserted, updated, or deleted,
				we add the row length to this number; we
				calculate new estimates for the stat_...
				values for the table and the indexes at an
				interval of DICT_STAT_CALCULATE_INTERVAL,
				but for small tables more often, also
				when the estimate operation is called
				for MySQL SHOW TABLE STATUS; this counter
				is not protected by any latch, because this
				is only used for heuristics */
	ulint		magic_n;/* magic number */
};
#define	DICT_TABLE_MAGIC_N	76333786

/* Statistics are calculated at least with this interval; see the struct
above */
#define DICT_STAT_CALCULATE_INTERVAL	(UNIV_PAGE_SIZE * 8)
					
/* Data structure for a stored procedure */
struct dict_proc_struct{
	mem_heap_t*	heap;	/* memory heap */
	char*		name;	/* procedure name */
	char*		sql_string;
				/* procedure definition as an SQL string:
				we can produce more parsed instances of the
				procedure by parsing this string */
	hash_node_t	name_hash;
				/* hash chain node */
	UT_LIST_BASE_NODE_T(que_fork_t) graphs;
				/* list of parsed instances of the procedure:
				there may be many of them, and they are
				recycled */
	ulint		mem_fix;/* count of how many times this struct 
				has been fixed in memory */
};

#ifndef UNIV_NONINL
#include "dict0mem.ic"
#endif

#endif
