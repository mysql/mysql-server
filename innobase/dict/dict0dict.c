/**********************************************************************
Data dictionary system

(c) 1996 Innobase Oy

Created 1/8/1996 Heikki Tuuri
***********************************************************************/

#include "dict0dict.h"

#ifdef UNIV_NONINL
#include "dict0dict.ic"
#endif

#include "buf0buf.h"
#include "data0type.h"
#include "mach0data.h"
#include "dict0boot.h"
#include "dict0mem.h"
#include "trx0undo.h"
#include "btr0btr.h"
#include "btr0cur.h"
#include "btr0sea.h"
#include "pars0pars.h"
#include "pars0sym.h"
#include "que0que.h"


dict_sys_t*	dict_sys	= NULL;	/* the dictionary system */

#define	DICT_HEAP_SIZE		100	/* initial memory heap size when
					creating a table or index object */
#define DICT_POOL_PER_PROCEDURE_HASH 512 /* buffer pool max size per stored
					procedure hash table fixed size in
					bytes */
#define DICT_POOL_PER_TABLE_HASH 512	/* buffer pool max size per table
					hash table fixed size in bytes */
#define DICT_POOL_PER_COL_HASH	128	/* buffer pool max size per column
					hash table fixed size in bytes */
#define DICT_POOL_PER_VARYING	4	/* buffer pool max size per data
					dictionary varying size in bytes */

/**************************************************************************
Adds a column to the data dictionary hash table. */
static
void
dict_col_add_to_cache(
/*==================*/
	dict_table_t*	table,	/* in: table */
	dict_col_t*	col);	/* in: column */
/**************************************************************************
Repositions a column in the data dictionary hash table when the table name
changes. */
static
void
dict_col_reposition_in_cache(
/*=========================*/
	dict_table_t*	table,		/* in: table */
	dict_col_t*	col,		/* in: column */
	char*		new_name);	/* in: new table name */
/**************************************************************************
Removes a column from the data dictionary hash table. */
static
void
dict_col_remove_from_cache(
/*=======================*/
	dict_table_t*	table,	/* in: table */
	dict_col_t*	col);	/* in: column */
/**************************************************************************
Removes an index from the dictionary cache. */
static
void
dict_index_remove_from_cache(
/*=========================*/
	dict_table_t*	table,	/* in: table */
	dict_index_t*	index);	/* in, own: index */
/***********************************************************************
Adds a column to index. */
UNIV_INLINE
void
dict_index_add_col(
/*===============*/
	dict_index_t*	index,	/* in: index */
	dict_col_t*	col,	/* in: column */
	ulint		order);	/* in: order criterion */
/***********************************************************************
Copies fields contained in index2 to index1. */
static
void
dict_index_copy(
/*============*/
	dict_index_t*	index1,	/* in: index to copy to */
	dict_index_t*	index2,	/* in: index to copy from */
	ulint		start,	/* in: first position to copy */
	ulint		end);	/* in: last position to copy */
/***********************************************************************
Tries to find column names for the index in the column hash table and
sets the col field of the index. */
static
ibool
dict_index_find_cols(
/*=================*/
				/* out: TRUE if success */
	dict_table_t*	table,	/* in: table */
	dict_index_t*	index);	/* in: index */	
/***********************************************************************
Builds the internal dictionary cache representation for a clustered
index, containing also system fields not defined by the user. */
static
dict_index_t*
dict_index_build_internal_clust(
/*============================*/
				/* out, own: the internal representation
				of the clustered index */
	dict_table_t*	table,	/* in: table */
	dict_index_t*	index);	/* in: user representation of a clustered
				index */	
/***********************************************************************
Builds the internal dictionary cache representation for a non-clustered
index, containing also system fields not defined by the user. */
static
dict_index_t*
dict_index_build_internal_non_clust(
/*================================*/
				/* out, own: the internal representation
				of the non-clustered index */
	dict_table_t*	table,	/* in: table */
	dict_index_t*	index);	/* in: user representation of a non-clustered
				index */	
/**************************************************************************
In an index tree, finds the index corresponding to a record in the tree. */
UNIV_INLINE
dict_index_t*
dict_tree_find_index_low(
/*=====================*/
				/* out: index */
	dict_tree_t*	tree,	/* in: index tree */
	rec_t*		rec);	/* in: record for which to find correct index */
/**************************************************************************
Prints a table data. */
static
void
dict_table_print_low(
/*=================*/
	dict_table_t*	table);	/* in: table */
/**************************************************************************
Prints a column data. */
static
void
dict_col_print_low(
/*===============*/
	dict_col_t*	col);	/* in: column */
/**************************************************************************
Prints an index data. */
static
void
dict_index_print_low(
/*=================*/
	dict_index_t*	index);	/* in: index */
/**************************************************************************
Prints a field data. */
static
void
dict_field_print_low(
/*=================*/
	dict_field_t*	field);	/* in: field */

/************************************************************************
Reserves the dictionary system mutex for MySQL. */

void
dict_mutex_enter_for_mysql(void)
/*============================*/
{
	mutex_enter(&(dict_sys->mutex));
}
	
/************************************************************************
Releases the dictionary system mutex for MySQL. */

void
dict_mutex_exit_for_mysql(void)
/*===========================*/
{
	mutex_exit(&(dict_sys->mutex));
}
	
/************************************************************************
Gets the nth column of a table. */

dict_col_t*
dict_table_get_nth_col_noninline(
/*=============================*/
				/* out: pointer to column object */
	dict_table_t*	table,	/* in: table */
	ulint		pos)	/* in: position of column */
{
	return(dict_table_get_nth_col(table, pos));
}

/************************************************************************
Gets the first index on the table (the clustered index). */

dict_index_t*
dict_table_get_first_index_noninline(
/*=================================*/
				/* out: index, NULL if none exists */
	dict_table_t*	table)	/* in: table */
{
	return(dict_table_get_first_index(table));
}

/************************************************************************
Gets the next index on the table. */

dict_index_t*
dict_table_get_next_index_noninline(
/*================================*/
				/* out: index, NULL if none left */
	dict_index_t*	index)	/* in: index */
{
	return(dict_table_get_next_index(index));
}

/**************************************************************************
Returns an index object. */

dict_index_t*
dict_table_get_index_noninline(
/*===========================*/
				/* out: index, NULL if does not exist */
	dict_table_t*	table,	/* in: table */
	char*		name)	/* in: index name */
{
	return(dict_table_get_index(table, name));
}
	
/************************************************************************
Initializes the autoinc counter. It is not an error to initialize already
initialized counter. */

void
dict_table_autoinc_initialize(
/*==========================*/
	dict_table_t*	table,	/* in: table */
	ib_longlong	value)	/* in: value which was assigned to a row */
{
	mutex_enter(&(table->autoinc_mutex));

	table->autoinc_inited = TRUE;
	table->autoinc = value;

	mutex_exit(&(table->autoinc_mutex));
}

/************************************************************************
Gets the next autoinc value, 0 if not yet initialized. */

ib_longlong
dict_table_autoinc_get(
/*===================*/
				/* out: value for a new row, or 0 */
	dict_table_t*	table)	/* in: table */
{
	ib_longlong	value;

	mutex_enter(&(table->autoinc_mutex));

	if (!table->autoinc_inited) {

		value = 0;
	} else {
		table->autoinc = table->autoinc + 1;
		value = table->autoinc;
	}
	
	mutex_exit(&(table->autoinc_mutex));

	return(value);
}

/************************************************************************
Updates the autoinc counter if the value supplied is bigger than the
current value. If not inited, does nothing. */

void
dict_table_autoinc_update(
/*======================*/
	dict_table_t*	table,	/* in: table */
	ib_longlong	value)	/* in: value which was assigned to a row */
{
	mutex_enter(&(table->autoinc_mutex));

	if (table->autoinc_inited) {
		if (value > table->autoinc) {
			table->autoinc = value;
		}
	}	

	mutex_exit(&(table->autoinc_mutex));
}

/************************************************************************
Looks for column n in an index. */

ulint
dict_index_get_nth_col_pos(
/*=======================*/
				/* out: position in internal representation
				of the index; if not contained, returns
				ULINT_UNDEFINED */
	dict_index_t*	index,	/* in: index */
	ulint		n)	/* in: column number */
{
	dict_field_t*	field;
	dict_col_t*	col;
	ulint		pos;
	ulint		n_fields;
	
	ut_ad(index);
	ut_ad(index->magic_n == DICT_INDEX_MAGIC_N);

	if (index->type & DICT_CLUSTERED) {
		col = dict_table_get_nth_col(index->table, n);

		return(col->clust_pos);
	}

	n_fields = dict_index_get_n_fields(index);
	
	for (pos = 0; pos < n_fields; pos++) {
		field = dict_index_get_nth_field(index, pos);
		col = field->col;

		if (dict_col_get_no(col) == n) {

			return(pos);
		}
	}

	return(ULINT_UNDEFINED);
}
	
/**************************************************************************
Returns a table object, based on table id, and memoryfixes it. */

dict_table_t*
dict_table_get_on_id(
/*=================*/
				/* out: table, NULL if does not exist */
	dulint	table_id,	/* in: table id */
	trx_t*	trx)		/* in: transaction handle */
{
	dict_table_t*	table;
	
	if (ut_dulint_cmp(table_id, DICT_FIELDS_ID) <= 0) {
		/* It is a system table which will always exist in the table
		cache: we avoid acquiring the dictionary mutex, because
		if we are doing a rollback to handle an error in TABLE
		CREATE, for example, we already have the mutex! */

		ut_ad(mutex_own(&(dict_sys->mutex)));

		return(dict_table_get_on_id_low(table_id, trx));
	}

	mutex_enter(&(dict_sys->mutex));

	table = dict_table_get_on_id_low(table_id, trx);
	
	mutex_exit(&(dict_sys->mutex));

	return(table);
}

/************************************************************************
Looks for column n postion in the clustered index. */

ulint
dict_table_get_nth_col_pos(
/*=======================*/
				/* out: position in internal representation
				of the clustered index */
	dict_table_t*	table,	/* in: table */
	ulint		n)	/* in: column number */
{
	return(dict_index_get_nth_col_pos(dict_table_get_first_index(table),
								n));
}

/**************************************************************************
Inits the data dictionary module. */

void
dict_init(void)
/*===========*/
{
	dict_sys = mem_alloc(sizeof(dict_sys_t));

	mutex_create(&(dict_sys->mutex));
	mutex_set_level(&(dict_sys->mutex), SYNC_DICT);

	dict_sys->table_hash = hash_create(buf_pool_get_max_size() /
					(DICT_POOL_PER_TABLE_HASH *
					UNIV_WORD_SIZE));
	dict_sys->table_id_hash = hash_create(buf_pool_get_max_size() /
					(DICT_POOL_PER_TABLE_HASH *
					UNIV_WORD_SIZE));
	dict_sys->col_hash = hash_create(buf_pool_get_max_size() /
					(DICT_POOL_PER_COL_HASH *
					UNIV_WORD_SIZE));
	dict_sys->procedure_hash = hash_create(buf_pool_get_max_size() /
					(DICT_POOL_PER_PROCEDURE_HASH *
					UNIV_WORD_SIZE));
	dict_sys->size = 0;

	UT_LIST_INIT(dict_sys->table_LRU);
}

/**************************************************************************
Returns a table object and memoryfixes it. NOTE! This is a high-level
function to be used mainly from outside the 'dict' directory. Inside this
directory dict_table_get_low is usually the appropriate function. */

dict_table_t*
dict_table_get(
/*===========*/
				/* out: table, NULL if does not exist */
	char*	table_name,	/* in: table name */
	trx_t*	trx)		/* in: transaction handle or NULL */
{
	dict_table_t*	table;

	UT_NOT_USED(trx);

	mutex_enter(&(dict_sys->mutex));
	
	table = dict_table_get_low(table_name);

	mutex_exit(&(dict_sys->mutex));

	if (table != NULL) {
		if (table->stat_last_estimate_counter == (ulint)(-1)) {
			dict_update_statistics(table);
		}
	}
	
	return(table);
}

/**************************************************************************
Adds a table object to the dictionary cache. */

void
dict_table_add_to_cache(
/*====================*/
	dict_table_t*	table)	/* in: table */
{
	ulint	fold;
	ulint	id_fold;
	ulint	i;
	
	ut_ad(table);
	ut_ad(mutex_own(&(dict_sys->mutex)));
	ut_ad(table->n_def == table->n_cols - DATA_N_SYS_COLS);
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
	ut_ad(table->cached == FALSE);
	
	fold = ut_fold_string(table->name);
	id_fold = ut_fold_dulint(table->id);
	
	table->cached = TRUE;
	
	/* NOTE: the system columns MUST be added in the following order
	(so that they can be indexed by the numerical value of DATA_ROW_ID,
	etc.) and as the last columns of the table memory object.
	The clustered index will not always physically contain all
	system columns. */

	dict_mem_table_add_col(table, "DB_ROW_ID", DATA_SYS, DATA_ROW_ID, 0, 0);
	ut_ad(DATA_ROW_ID == 0);
	dict_mem_table_add_col(table, "DB_TRX_ID", DATA_SYS, DATA_TRX_ID, 0, 0);
	ut_ad(DATA_TRX_ID == 1);
	dict_mem_table_add_col(table, "DB_ROLL_PTR", DATA_SYS, DATA_ROLL_PTR,
									0, 0);
	ut_ad(DATA_ROLL_PTR == 2);

	dict_mem_table_add_col(table, "DB_MIX_ID", DATA_SYS, DATA_MIX_ID, 0, 0);
	ut_ad(DATA_MIX_ID == 3);
	ut_ad(DATA_N_SYS_COLS == 4); /* This assert reminds that if a new
					system column is added to the program,
					it should be dealt with here */ 

	/* Look for a table with the same name: error if such exists */
	{
		dict_table_t*	table2;
		HASH_SEARCH(name_hash, dict_sys->table_hash, fold, table2,
				(ut_strcmp(table2->name, table->name) == 0));
		ut_a(table2 == NULL);
	}

	/* Look for a table with the same id: error if such exists */
	{
		dict_table_t*	table2;
		HASH_SEARCH(id_hash, dict_sys->table_id_hash, id_fold, table2,
				(ut_dulint_cmp(table2->id, table->id) == 0));
		ut_a(table2 == NULL);
	}

	if (table->type == DICT_TABLE_CLUSTER_MEMBER) {

		table->mix_id_len = mach_dulint_get_compressed_size(
								table->mix_id);
		mach_dulint_write_compressed(table->mix_id_buf, table->mix_id);
	}

	/* Add the columns to the column hash table */
	for (i = 0; i < table->n_cols; i++) {
		dict_col_add_to_cache(table, dict_table_get_nth_col(table, i));
	}

	/* Add table to hash table of tables */
	HASH_INSERT(dict_table_t, name_hash, dict_sys->table_hash, fold, table);

	/* Add table to hash table of tables based on table id */
	HASH_INSERT(dict_table_t, id_hash, dict_sys->table_id_hash, id_fold,
								table);
	/* Add table to LRU list of tables */
	UT_LIST_ADD_FIRST(table_LRU, dict_sys->table_LRU, table);

	/* If the dictionary cache grows too big, trim the table LRU list */

	dict_sys->size += mem_heap_get_size(table->heap);
	/* dict_table_LRU_trim(); */
}

/**************************************************************************
Renames a table object. */

ibool
dict_table_rename_in_cache(
/*=======================*/
					/* out: TRUE if success */
	dict_table_t*	table,		/* in: table */
	char*		new_name)	/* in: new name */
{
	ulint	fold;
	ulint	old_size;
	char*	name_buf;
	ulint	i;
	
	ut_ad(table);
	ut_ad(mutex_own(&(dict_sys->mutex)));
	
	old_size = mem_heap_get_size(table->heap);
	
	fold = ut_fold_string(new_name);
	
	/* Look for a table with the same name: error if such exists */
	{
		dict_table_t*	table2;
		HASH_SEARCH(name_hash, dict_sys->table_hash, fold, table2,
				(ut_strcmp(table2->name, new_name) == 0));
		if (table2) {
			return(FALSE);
		}
	}

	/* Reposition the columns in the column hash table; they are hashed
	according to the pair (table name, column name) */

	for (i = 0; i < table->n_cols; i++) {
		dict_col_reposition_in_cache(table,
				dict_table_get_nth_col(table, i), new_name);
	}

	/* Remove table from the hash tables of tables */
	HASH_DELETE(dict_table_t, name_hash, dict_sys->table_hash,
					ut_fold_string(table->name), table);	

	name_buf = mem_heap_alloc(table->heap, ut_strlen(new_name) + 1);
					
	ut_memcpy(name_buf, new_name, ut_strlen(new_name) + 1);

	table->name = name_buf;
					
	/* Add table to hash table of tables */
	HASH_INSERT(dict_table_t, name_hash, dict_sys->table_hash, fold, table);
	
	dict_sys->size += (mem_heap_get_size(table->heap) - old_size);

	return(TRUE);
}

/**************************************************************************
Removes a table object from the dictionary cache. */

void
dict_table_remove_from_cache(
/*=========================*/
	dict_table_t*	table)	/* in, own: table */
{
	dict_index_t*	index;
	ulint		size;
	ulint		i;
	
	ut_ad(table);
	ut_ad(mutex_own(&(dict_sys->mutex)));
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

	/* printf("Removing table %s from dictionary cache\n", table->name); */

	/* Remove the indexes from the cache */
	index = UT_LIST_GET_LAST(table->indexes);

	while (index != NULL) {
		dict_index_remove_from_cache(table, index);
		index = UT_LIST_GET_LAST(table->indexes);
	}

	/* Remove the columns of the table from the cache */
	for (i = 0; i < table->n_cols; i++) {
		dict_col_remove_from_cache(table,
					   dict_table_get_nth_col(table, i));
	}

	/* Remove table from the hash tables of tables */
	HASH_DELETE(dict_table_t, name_hash, dict_sys->table_hash,
					ut_fold_string(table->name), table);
	HASH_DELETE(dict_table_t, id_hash, dict_sys->table_id_hash,
					ut_fold_dulint(table->id), table);

	/* Remove table from LRU list of tables */
	UT_LIST_REMOVE(table_LRU, dict_sys->table_LRU, table);

	mutex_free(&(table->autoinc_mutex));

	size = mem_heap_get_size(table->heap);

	ut_ad(dict_sys->size >= size);

	dict_sys->size -= size;

	mem_heap_free(table->heap);
}

/**************************************************************************
Frees tables from the end of table_LRU if the dictionary cache occupies
too much space. Currently not used! */

void
dict_table_LRU_trim(void)
/*=====================*/
{
	dict_table_t*	table;
	dict_table_t*	prev_table;

	ut_a(0);

	ut_ad(mutex_own(&(dict_sys->mutex)));

	table = UT_LIST_GET_LAST(dict_sys->table_LRU);

	while (table && (dict_sys->size >
			 buf_pool_get_max_size() / DICT_POOL_PER_VARYING)) {

		prev_table = UT_LIST_GET_PREV(table_LRU, table);

		if (table->mem_fix == 0) {
			dict_table_remove_from_cache(table);
		}

		table = prev_table;
	}
}

/**************************************************************************
Adds a column to the data dictionary hash table. */
static
void
dict_col_add_to_cache(
/*==================*/
	dict_table_t*	table,	/* in: table */
	dict_col_t*	col)	/* in: column */
{
	ulint	fold;

	ut_ad(table && col);
	ut_ad(mutex_own(&(dict_sys->mutex)));
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
	
	fold = ut_fold_ulint_pair(ut_fold_string(table->name),
				  ut_fold_string(col->name));

	/* Look for a column with same table name and column name: error */
	{
		dict_col_t*	col2;
		HASH_SEARCH(hash, dict_sys->col_hash, fold, col2,
			(ut_strcmp(col->name, col2->name) == 0)
			&& (ut_strcmp((col2->table)->name, table->name)
							== 0));  
		ut_a(col2 == NULL);
	}

	HASH_INSERT(dict_col_t, hash, dict_sys->col_hash, fold, col);
}

/**************************************************************************
Removes a column from the data dictionary hash table. */
static
void
dict_col_remove_from_cache(
/*=======================*/
	dict_table_t*	table,	/* in: table */
	dict_col_t*	col)	/* in: column */
{
	ulint		fold;

	ut_ad(table && col);
	ut_ad(mutex_own(&(dict_sys->mutex)));
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
	
	fold = ut_fold_ulint_pair(ut_fold_string(table->name),
				  ut_fold_string(col->name));

	HASH_DELETE(dict_col_t, hash, dict_sys->col_hash, fold, col);
}

/**************************************************************************
Repositions a column in the data dictionary hash table when the table name
changes. */
static
void
dict_col_reposition_in_cache(
/*=========================*/
	dict_table_t*	table,		/* in: table */
	dict_col_t*	col,		/* in: column */
	char*		new_name)	/* in: new table name */
{
	ulint		fold;

	ut_ad(table && col);
	ut_ad(mutex_own(&(dict_sys->mutex)));
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
	
	fold = ut_fold_ulint_pair(ut_fold_string(table->name),
				  ut_fold_string(col->name));

	HASH_DELETE(dict_col_t, hash, dict_sys->col_hash, fold, col);

	fold = ut_fold_ulint_pair(ut_fold_string(new_name),
				  ut_fold_string(col->name));
				  
	HASH_INSERT(dict_col_t, hash, dict_sys->col_hash, fold, col);
}

/**************************************************************************
Adds an index to the dictionary cache. */

ibool
dict_index_add_to_cache(
/*====================*/
				/* out: TRUE if success */
	dict_table_t*	table,	/* in: table on which the index is */
	dict_index_t*	index)	/* in, own: index; NOTE! The index memory
				object is freed in this function! */
{
	dict_index_t*	new_index;
	dict_tree_t*	tree;
	dict_table_t*	cluster;
	dict_field_t*	field;
	ulint		n_ord;
	ibool		success;
	ulint		i;
	
	ut_ad(index);
	ut_ad(mutex_own(&(dict_sys->mutex)));
	ut_ad(index->n_def == index->n_fields);
	ut_ad(index->magic_n == DICT_INDEX_MAGIC_N);
	
	ut_ad(mem_heap_validate(index->heap));

	{
		dict_index_t*	index2;
		index2 = UT_LIST_GET_FIRST(table->indexes);

		while (index2 != NULL) {
			ut_ad(ut_strcmp(index->name, index2->name) != 0);

			index2 = UT_LIST_GET_NEXT(indexes, index2);
		}

		ut_a(UT_LIST_GET_LEN(table->indexes) == 0
	      			|| (index->type & DICT_CLUSTERED) == 0);
	}

	success = dict_index_find_cols(table, index);

	if (!success) {
		dict_mem_index_free(index);

		return(FALSE);
	}

	/* Build the cache internal representation of the index,
	containing also the added system fields */

	if (index->type & DICT_CLUSTERED) {
		new_index = dict_index_build_internal_clust(table, index);
	} else {
		new_index = dict_index_build_internal_non_clust(table, index);
	}

	new_index->search_info = btr_search_info_create(new_index->heap);
	
	/* Set the n_fields value in new_index to the actual defined
	number of fields in the cache internal representation */

	new_index->n_fields = new_index->n_def;
	
	/* Add the new index as the last index for the table */

	UT_LIST_ADD_LAST(indexes, table->indexes, new_index);	
	new_index->table = table;
	new_index->table_name = table->name;

	/* Increment the ord_part counts in columns which are ordering */

	if (index->type & DICT_UNIVERSAL) {
		n_ord = new_index->n_fields;
	} else {
		n_ord = dict_index_get_n_unique(new_index);
	}

	for (i = 0; i < n_ord; i++) {

		field = dict_index_get_nth_field(new_index, i);

		dict_field_get_col(field)->ord_part++;
	}

	if (table->type == DICT_TABLE_CLUSTER_MEMBER) {
		/* The index tree is found from the cluster object */
	    
		cluster = dict_table_get_low(table->cluster_name);

		tree = dict_index_get_tree(UT_LIST_GET_FIRST(cluster->indexes));

		new_index->tree = tree;
		new_index->page_no = tree->page;
	} else {
		/* Create an index tree memory object for the index */
		tree = dict_tree_create(new_index);
		ut_ad(tree);

		new_index->tree = tree;
	}

	/* Add the index to the list of indexes stored in the tree */
	UT_LIST_ADD_LAST(tree_indexes, tree->tree_indexes, new_index); 
	
	/* If the dictionary cache grows too big, trim the table LRU list */

	dict_sys->size += mem_heap_get_size(new_index->heap);
	/* dict_table_LRU_trim(); */

	dict_mem_index_free(index);

	return(TRUE);
}

/**************************************************************************
Removes an index from the dictionary cache. */
static
void
dict_index_remove_from_cache(
/*=========================*/
	dict_table_t*	table,	/* in: table */
	dict_index_t*	index)	/* in, own: index */
{
	dict_field_t*	field;
	ulint		size;
	ulint		i;

	ut_ad(table && index);
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
	ut_ad(index->magic_n == DICT_INDEX_MAGIC_N);
	ut_ad(mutex_own(&(dict_sys->mutex)));

	ut_ad(UT_LIST_GET_LEN((index->tree)->tree_indexes) == 1);
	dict_tree_free(index->tree);

	/* Decrement the ord_part counts in columns which are ordering */
	for (i = 0; i < dict_index_get_n_unique(index); i++) {

		field = dict_index_get_nth_field(index, i);

		ut_ad(dict_field_get_col(field)->ord_part > 0);
		(dict_field_get_col(field)->ord_part)--;
	}

	/* Remove the index from the list of indexes of the table */
	UT_LIST_REMOVE(indexes, table->indexes, index);

	size = mem_heap_get_size(index->heap);

	ut_ad(dict_sys->size >= size);

	dict_sys->size -= size;

	mem_heap_free(index->heap);
}

/***********************************************************************
Tries to find column names for the index in the column hash table and
sets the col field of the index. */
static
ibool
dict_index_find_cols(
/*=================*/
				/* out: TRUE if success */
	dict_table_t*	table,	/* in: table */
	dict_index_t*	index)	/* in: index */	
{
	dict_col_t*	col;
	dict_field_t*	field;
	ulint		fold;
	ulint		i;
	
	ut_ad(table && index);
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
	ut_ad(mutex_own(&(dict_sys->mutex)));

	for (i = 0; i < index->n_fields; i++) {
		field = dict_index_get_nth_field(index, i);

		fold = ut_fold_ulint_pair(ut_fold_string(table->name),
				  	       ut_fold_string(field->name));
			
		HASH_SEARCH(hash, dict_sys->col_hash, fold, col,
				(ut_strcmp(col->name, field->name) == 0)
				&& (ut_strcmp((col->table)->name, table->name)
								== 0));  
		if (col == NULL) {

 			return(FALSE);
		} else {
			field->col = col;
		}
	}

	return(TRUE);
}
	
/***********************************************************************
Adds a column to index. */
UNIV_INLINE
void
dict_index_add_col(
/*===============*/
	dict_index_t*	index,	/* in: index */
	dict_col_t*	col,	/* in: column */
	ulint		order)	/* in: order criterion */
{
	dict_field_t*	field;

	dict_mem_index_add_field(index, col->name, order);

	field = dict_index_get_nth_field(index, index->n_def - 1);

	field->col = col;
}

/***********************************************************************
Copies fields contained in index2 to index1. */
static
void
dict_index_copy(
/*============*/
	dict_index_t*	index1,	/* in: index to copy to */
	dict_index_t*	index2,	/* in: index to copy from */
	ulint		start,	/* in: first position to copy */
	ulint		end)	/* in: last position to copy */
{
	dict_field_t*	field;
	ulint		i;
	
	/* Copy fields contained in index2 */

	for (i = start; i < end; i++) {

		field = dict_index_get_nth_field(index2, i);
		dict_index_add_col(index1, field->col, field->order);
	}
}

/***********************************************************************
Copies types of fields contained in index to tuple. */

void
dict_index_copy_types(
/*==================*/
	dtuple_t*	tuple,		/* in: data tuple */
	dict_index_t*	index,		/* in: index */
	ulint		n_fields)	/* in: number of field types to copy */
{
	dtype_t*	dfield_type;
	dtype_t*	type;
	ulint		i;

	if (index->type & DICT_UNIVERSAL) {
		dtuple_set_types_binary(tuple, n_fields);

		return;
	}

	for (i = 0; i < n_fields; i++) {
		dfield_type = dfield_get_type(dtuple_get_nth_field(tuple, i));
		type = dict_col_get_type(dict_field_get_col(
				dict_index_get_nth_field(index, i)));
		*dfield_type = *type;
	}
}

/***********************************************************************
Copies types of columns contained in table to tuple. */

void
dict_table_copy_types(
/*==================*/
	dtuple_t*	tuple,	/* in: data tuple */
	dict_table_t*	table)	/* in: index */
{
	dtype_t*	dfield_type;
	dtype_t*	type;
	ulint		i;

	ut_ad(!(table->type & DICT_UNIVERSAL));

	for (i = 0; i < dtuple_get_n_fields(tuple); i++) {

		dfield_type = dfield_get_type(dtuple_get_nth_field(tuple, i));
		type = dict_col_get_type(dict_table_get_nth_col(table, i));

		*dfield_type = *type;
	}
}

/***********************************************************************
Builds the internal dictionary cache representation for a clustered
index, containing also system fields not defined by the user. */
static
dict_index_t*
dict_index_build_internal_clust(
/*============================*/
				/* out, own: the internal representation
				of the clustered index */
	dict_table_t*	table,	/* in: table */
	dict_index_t*	index)	/* in: user representation of a clustered
				index */	
{
	dict_index_t*	new_index;
	dict_field_t*	field;
	dict_col_t*	col;
	ulint		fixed_size;
	ulint		trx_id_pos;
	ulint		i;

	ut_ad(table && index);
	ut_ad(index->type & DICT_CLUSTERED);
	ut_ad(mutex_own(&(dict_sys->mutex)));
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

	/* Create a new index object with certainly enough fields */
	new_index = dict_mem_index_create(table->name,
				     index->name,
				     table->space,
				     index->type,
				     index->n_fields + table->n_cols);

	/* Copy other relevant data from the old index struct to the new
	struct: it inherits the values */

	new_index->n_user_defined_cols = index->n_fields;
	
	new_index->id = index->id;
	new_index->page_no = index->page_no;

	if (table->type != DICT_TABLE_ORDINARY) {
		/* The index is mixed: copy common key prefix fields */
		
		dict_index_copy(new_index, index, 0, table->mix_len);

		/* Add the mix id column */
		dict_index_add_col(new_index,
			        dict_table_get_sys_col(table, DATA_MIX_ID), 0);

		/* Copy the rest of fields */
		dict_index_copy(new_index, index, table->mix_len,
							index->n_fields);
	} else {
		/* Copy the fields of index */
		dict_index_copy(new_index, index, 0, index->n_fields);
	}

	if (index->type & DICT_UNIVERSAL) {
		/* No fixed number of fields determines an entry uniquely */

		new_index->n_uniq = ULINT_MAX;
		
	} else if (index->type & DICT_UNIQUE) {
		/* Only the fields defined so far are needed to identify
		the index entry uniquely */

		new_index->n_uniq = new_index->n_def;
	} else {
		/* Also the row id is needed to identify the entry */
		new_index->n_uniq = 1 + new_index->n_def;
	}

	new_index->trx_id_offset = 0;

	if (!(index->type & DICT_IBUF)) {
		/* Add system columns, trx id first */

		trx_id_pos = new_index->n_def;

		ut_ad(DATA_ROW_ID == 0);
		ut_ad(DATA_TRX_ID == 1);
		ut_ad(DATA_ROLL_PTR == 2);

		if (!(index->type & DICT_UNIQUE)) {
			dict_index_add_col(new_index,
			   dict_table_get_sys_col(table, DATA_ROW_ID), 0);
			trx_id_pos++;
		}

		dict_index_add_col(new_index,
			   dict_table_get_sys_col(table, DATA_TRX_ID), 0);	
		dict_index_add_col(new_index,
			   dict_table_get_sys_col(table, DATA_ROLL_PTR), 0);

		for (i = 0; i < trx_id_pos; i++) {

			fixed_size = dtype_get_fixed_size(
				dict_index_get_nth_type(new_index, i));

			if (fixed_size == 0) {
				new_index->trx_id_offset = 0;

				break;
			}

			new_index->trx_id_offset += fixed_size;
		}

	}

	/* Set auxiliary variables in table columns as undefined */
	for (i = 0; i < table->n_cols; i++) {

		col = dict_table_get_nth_col(table, i);
		col->aux = ULINT_UNDEFINED;
	}

	/* Mark with 0 the table columns already contained in new_index */
	for (i = 0; i < new_index->n_def; i++) {

		field = dict_index_get_nth_field(new_index, i);
		(field->col)->aux = 0;
	}
	
	/* Add to new_index non-system columns of table not yet included
	there */
	for (i = 0; i < table->n_cols - DATA_N_SYS_COLS; i++) {

		col = dict_table_get_nth_col(table, i);
		ut_ad(col->type.mtype != DATA_SYS);

		if (col->aux == ULINT_UNDEFINED) {
			dict_index_add_col(new_index, col, 0);
		}
	}

	ut_ad((index->type & DICT_IBUF)
				|| (UT_LIST_GET_LEN(table->indexes) == 0));

	/* Store to the column structs the position of the table columns
	in the clustered index */

	for (i = 0; i < new_index->n_def; i++) {
		field = dict_index_get_nth_field(new_index, i);
		(field->col)->clust_pos = i;
	}
	
	new_index->cached = TRUE;

	return(new_index);
}	

/***********************************************************************
Builds the internal dictionary cache representation for a non-clustered
index, containing also system fields not defined by the user. */
static
dict_index_t*
dict_index_build_internal_non_clust(
/*================================*/
				/* out, own: the internal representation
				of the non-clustered index */
	dict_table_t*	table,	/* in: table */
	dict_index_t*	index)	/* in: user representation of a non-clustered
				index */	
{
	dict_field_t*	field;
	dict_index_t*	new_index;
	dict_index_t*	clust_index;
	ulint		i;

	ut_ad(table && index);
	ut_ad(0 == (index->type & DICT_CLUSTERED));
	ut_ad(mutex_own(&(dict_sys->mutex)));
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

	/* The clustered index should be the first in the list of indexes */
	clust_index = UT_LIST_GET_FIRST(table->indexes);
	
	ut_ad(clust_index);
	ut_ad(clust_index->type & DICT_CLUSTERED);
	ut_ad(!(clust_index->type & DICT_UNIVERSAL));

	/* Create a new index */
	new_index = dict_mem_index_create(table->name,
				     index->name,
				     index->space,
				     index->type,
				     index->n_fields
				     + 1 + clust_index->n_uniq);

	/* Copy other relevant data from the old index
	struct to the new struct: it inherits the values */

	new_index->n_user_defined_cols = index->n_fields;
	
	new_index->id = index->id;
	new_index->page_no = index->page_no;

	/* Copy fields from index to new_index */
	dict_index_copy(new_index, index, 0, index->n_fields);

	/* Set the auxiliary variables in the clust_index unique columns
	as undefined */
	for (i = 0; i < clust_index->n_uniq; i++) {

		field = dict_index_get_nth_field(clust_index, i);
		(field->col)->aux = ULINT_UNDEFINED;
	}

	/* Mark with 0 table columns already contained in new_index */
	for (i = 0; i < new_index->n_def; i++) {

		field = dict_index_get_nth_field(new_index, i);
		(field->col)->aux = 0;
	}

	/* Add to new_index columns necessary to determine the clustered
	index entry uniquely */

	for (i = 0; i < clust_index->n_uniq; i++) {

		field = dict_index_get_nth_field(clust_index, i);

		if ((field->col)->aux == ULINT_UNDEFINED) {
			dict_index_add_col(new_index, field->col, 0);
		}
	}

	if ((index->type) & DICT_UNIQUE) {
		new_index->n_uniq = index->n_fields;
	} else {
		new_index->n_uniq = new_index->n_def;
	}

	/* Set the n_fields value in new_index to the actual defined
	number of fields */

	new_index->n_fields = new_index->n_def;

	new_index->cached = TRUE;

	return(new_index);
}	

/**************************************************************************
Adds a stored procedure object to the dictionary cache. */

void
dict_procedure_add_to_cache(
/*========================*/
	dict_proc_t*	proc)	/* in: procedure */
{
	ulint	fold;
	
	mutex_enter(&(dict_sys->mutex));
	
	fold = ut_fold_string(proc->name);

	/* Look for a procedure with the same name: error if such exists */
	{
		dict_proc_t*	proc2;
		
		HASH_SEARCH(name_hash, dict_sys->procedure_hash, fold, proc2,
				(ut_strcmp(proc2->name, proc->name) == 0));
		ut_a(proc2 == NULL);
	}

	/* Add the procedure to the hash table */

	HASH_INSERT(dict_proc_t, name_hash, dict_sys->procedure_hash, fold,
									proc);
	mutex_exit(&(dict_sys->mutex));
}

/**************************************************************************
Reserves a parsed copy of a stored procedure to execute. If there are no
free parsed copies left at the moment, parses a new copy. Takes the copy off
the list of copies: the copy must be returned there with
dict_procedure_release_parsed_copy. */

que_t*
dict_procedure_reserve_parsed_copy(
/*===============================*/
				/* out: the query graph */
	dict_proc_t*	proc)	/* in: dictionary procedure node */
{
	que_t*		graph;
	proc_node_t*	proc_node;
	
	ut_ad(!mutex_own(&kernel_mutex));

	mutex_enter(&(dict_sys->mutex));

#ifdef UNIV_DEBUG
	UT_LIST_VALIDATE(graphs, que_t, proc->graphs);
#endif
	graph = UT_LIST_GET_FIRST(proc->graphs);

	if (graph) {
		UT_LIST_REMOVE(graphs, proc->graphs, graph);

/* 		printf("Graph removed, list length %lu\n",
					UT_LIST_GET_LEN(proc->graphs)); */
#ifdef UNIV_DEBUG
		UT_LIST_VALIDATE(graphs, que_t, proc->graphs);
#endif
	}

	mutex_exit(&(dict_sys->mutex));

	if (graph == NULL) {	
		graph = pars_sql(proc->sql_string);

		proc_node = que_fork_get_child(graph);

		proc_node->dict_proc = proc;

		printf("Parsed a new copy of graph %s\n",
						proc_node->proc_id->name);
	}

/*	printf("Returning graph %lu\n", (ulint)graph); */

	return(graph);
}

/**************************************************************************
Releases a parsed copy of an executed stored procedure. Puts the copy to the
list of copies. */

void
dict_procedure_release_parsed_copy(
/*===============================*/
	que_t*	graph)	/* in: query graph of a stored procedure */
{
	proc_node_t*	proc_node;

	ut_ad(!mutex_own(&kernel_mutex));

	mutex_enter(&(dict_sys->mutex));

	proc_node = que_fork_get_child(graph);

	UT_LIST_ADD_FIRST(graphs, (proc_node->dict_proc)->graphs, graph);

	mutex_exit(&(dict_sys->mutex));
}

/**************************************************************************
Returns an index object if it is found in the dictionary cache. */

dict_index_t*
dict_index_get_if_in_cache(
/*=======================*/
				/* out: index, NULL if not found */
	dulint	index_id)	/* in: index id */
{
	dict_table_t*	table;
	dict_index_t*	index;

	if (dict_sys == NULL) {
		return(NULL);
	}

	mutex_enter(&(dict_sys->mutex));
	
	table = UT_LIST_GET_FIRST(dict_sys->table_LRU);

	while (table) {
		index = UT_LIST_GET_FIRST(table->indexes);

		while (index) {
			if (0 == ut_dulint_cmp(index->id, index_id)) {

				goto found;
			}

			index = UT_LIST_GET_NEXT(indexes, index);
		}

		table = UT_LIST_GET_NEXT(table_LRU, table);
	}

	index = NULL;
found:
	mutex_exit(&(dict_sys->mutex));

	return(index);
}

/**************************************************************************
Creates an index tree struct. */

dict_tree_t*
dict_tree_create(
/*=============*/
				/* out, own: created tree */
	dict_index_t*	index)	/* in: the index for which to create: in the
				case of a mixed tree, this should be the
				index of the cluster object */
{
	dict_tree_t*	tree;

	tree = mem_alloc(sizeof(dict_tree_t));

	/* Inherit info from the index */

	tree->type = index->type;
	tree->space = index->space;
	tree->page = index->page_no;

	tree->id = index->id;
	
	UT_LIST_INIT(tree->tree_indexes);

	tree->magic_n = DICT_TREE_MAGIC_N;

	rw_lock_create(&(tree->lock));

	rw_lock_set_level(&(tree->lock), SYNC_INDEX_TREE);

	return(tree);
}

/**************************************************************************
Frees an index tree struct. */

void
dict_tree_free(
/*===========*/
	dict_tree_t*	tree)	/* in, own: index tree */
{
	ut_ad(tree);
	ut_ad(tree->magic_n == DICT_TREE_MAGIC_N);

	rw_lock_free(&(tree->lock));
	mem_free(tree);
}

/**************************************************************************
In an index tree, finds the index corresponding to a record in the tree. */
UNIV_INLINE
dict_index_t*
dict_tree_find_index_low(
/*=====================*/
				/* out: index */
	dict_tree_t*	tree,	/* in: index tree */
	rec_t*		rec)	/* in: record for which to find correct index */
{
	dict_index_t*	index;
	dict_table_t*	table;
	dulint		mix_id;
	ulint		len;
	
	index = UT_LIST_GET_FIRST(tree->tree_indexes);
	ut_ad(index);
	table = index->table;
	
	if ((index->type & DICT_CLUSTERED)
				&& (table->type != DICT_TABLE_ORDINARY)) {

		/* Get the mix id of the record */

		mix_id = mach_dulint_read_compressed(
				rec_get_nth_field(rec, table->mix_len, &len));

		while (ut_dulint_cmp(table->mix_id, mix_id) != 0) {

			index = UT_LIST_GET_NEXT(tree_indexes, index);
			table = index->table;
			ut_ad(index);
		}
	}

	return(index);
}

/**************************************************************************
In an index tree, finds the index corresponding to a record in the tree. */

dict_index_t*
dict_tree_find_index(
/*=================*/
				/* out: index */
	dict_tree_t*	tree,	/* in: index tree */
	rec_t*		rec)	/* in: record for which to find correct index */
{
	dict_index_t*	index;
	
	index = dict_tree_find_index_low(tree, rec);
	
	return(index);
}

/**************************************************************************
In an index tree, finds the index corresponding to a dtuple which is used
in a search to a tree. */

dict_index_t*
dict_tree_find_index_for_tuple(
/*===========================*/
				/* out: index; NULL if the tuple does not
				contain the mix id field in a mixed tree */
	dict_tree_t*	tree,	/* in: index tree */
	dtuple_t*	tuple)	/* in: tuple for which to find index */
{
	dict_index_t*	index;
	dict_table_t*	table;
	dulint		mix_id;

	ut_ad(dtuple_check_typed(tuple));
	
	if (UT_LIST_GET_LEN(tree->tree_indexes) == 1) {

		return(UT_LIST_GET_FIRST(tree->tree_indexes));
	}

	index = UT_LIST_GET_FIRST(tree->tree_indexes);
	ut_ad(index);
	table = index->table;

	if (dtuple_get_n_fields(tuple) <= table->mix_len) {

		return(NULL);
	}

	/* Get the mix id of the record */

	mix_id = mach_dulint_read_compressed(
			dfield_get_data(
				dtuple_get_nth_field(tuple, table->mix_len)));

	while (ut_dulint_cmp(table->mix_id, mix_id) != 0) {

		index = UT_LIST_GET_NEXT(tree_indexes, index);
		table = index->table;
		ut_ad(index);
	}

	return(index);
}

/**************************************************************************
Checks that a tuple has n_fields_cmp value in a sensible range, so that
no comparison can occur with the page number field in a node pointer. */

ibool
dict_tree_check_search_tuple(
/*=========================*/
				/* out: TRUE if ok */
	dict_tree_t*	tree,	/* in: index tree */
	dtuple_t*	tuple)	/* in: tuple used in a search */
{
	dict_index_t*	index;

	index = dict_tree_find_index_for_tuple(tree, tuple);

	if (index == NULL) {

		return(TRUE);
	}

	ut_a(dtuple_get_n_fields_cmp(tuple)
				<= dict_index_get_n_unique_in_tree(index));
	return(TRUE);
}

/**************************************************************************
Builds a node pointer out of a physical record and a page number. */

dtuple_t*
dict_tree_build_node_ptr(
/*=====================*/
				/* out, own: node pointer */
	dict_tree_t*	tree,	/* in: index tree */
	rec_t*		rec,	/* in: record for which to build node pointer */
	ulint		page_no,/* in: page number to put in node pointer */
	mem_heap_t*	heap)	/* in: memory heap where pointer created */
{
	dtuple_t*	tuple;
	dict_index_t*	ind;
	dfield_t*	field;
	byte*		buf;
	ulint		n_unique;

	ind = dict_tree_find_index_low(tree, rec);
	
	if (tree->type & DICT_UNIVERSAL) {
		/* In a universal index tree, we take the whole record as
		the node pointer */

		n_unique = rec_get_n_fields(rec);
	} else {	
		n_unique = dict_index_get_n_unique_in_tree(ind);
	}

	tuple = dtuple_create(heap, n_unique + 1);

	/* When searching in the tree for the node pointer, we must not do
	comparison on the last field, the page number field, as on upper
	levels in the tree there may be identical node pointers with a
	different page number; therefore, we set the n_fields_cmp to one
	less: */
	
	dtuple_set_n_fields_cmp(tuple, n_unique);

	dict_index_copy_types(tuple, ind, n_unique);
	
	buf = mem_heap_alloc(heap, 4);

	mach_write_to_4(buf, page_no);
	
	field = dtuple_get_nth_field(tuple, n_unique);
	dfield_set_data(field, buf, 4);

	dtype_set(dfield_get_type(field), DATA_SYS_CHILD, 0, 0, 0);

	rec_copy_prefix_to_dtuple(tuple, rec, n_unique, heap);

	ut_ad(dtuple_check_typed(tuple));

	return(tuple);
}	
	
/**************************************************************************
Copies an initial segment of a physical record, long enough to specify an
index entry uniquely. */

rec_t*
dict_tree_copy_rec_order_prefix(
/*============================*/
				/* out: pointer to the prefix record */
	dict_tree_t*	tree,	/* in: index tree */
	rec_t*		rec,	/* in: record for which to copy prefix */
	byte**		buf,	/* in/out: memory buffer for the copied prefix,
				or NULL */
	ulint*		buf_size)/* in/out: buffer size */
{
	dict_index_t*	ind;
	rec_t*		order_rec;
	ulint		n_fields;
	
	ind = dict_tree_find_index_low(tree, rec);

	n_fields = dict_index_get_n_unique_in_tree(ind);
	
	if (tree->type & DICT_UNIVERSAL) {

		n_fields = rec_get_n_fields(rec);
	}

	order_rec = rec_copy_prefix_to_buf(rec, n_fields, buf, buf_size);

	return(order_rec);
}	

/**************************************************************************
Builds a typed data tuple out of a physical record. */

dtuple_t*
dict_tree_build_data_tuple(
/*=======================*/
				/* out, own: data tuple */
	dict_tree_t*	tree,	/* in: index tree */
	rec_t*		rec,	/* in: record for which to build data tuple */
	mem_heap_t*	heap)	/* in: memory heap where tuple created */
{
	dtuple_t*	tuple;
	dict_index_t*	ind;
	ulint		n_fields;

	ind = dict_tree_find_index_low(tree, rec);

	n_fields = rec_get_n_fields(rec);
	
	tuple = dtuple_create(heap, n_fields); 

	dict_index_copy_types(tuple, ind, n_fields);

	rec_copy_prefix_to_dtuple(tuple, rec, n_fields, heap);

	ut_ad(dtuple_check_typed(tuple));

	return(tuple);
}	
	
/*************************************************************************
Calculates new estimates for table and index statistics. The statistics
are used in query optimization. */

void
dict_update_statistics(
/*===================*/
	dict_table_t*	table)	/* in: table */
{
	mem_heap_t*	heap;
	dict_index_t*	index;
	dtuple_t*	start;
	dtuple_t*	end;
	ulint		n_rows;
	ulint		n_vals;
	ulint		size;
	ulint		sum_of_index_sizes	= 0;

	/* Estimate the number of records in the clustered index */
	index = dict_table_get_first_index(table);

	heap = mem_heap_create(500);

	start = dtuple_create(heap, 0);
	end = dtuple_create(heap, 0);

	n_rows = btr_estimate_n_rows_in_range(index, start, PAGE_CUR_G,
							end, PAGE_CUR_L);
	mem_heap_free(heap);

	if (n_rows > 0) {
		/* For small tables our estimate function tends to give
		values 1 too big */
		n_rows--;
	}

	mutex_enter(&(dict_sys->mutex));

	table->stat_last_estimate_counter = table->stat_modif_counter;
	table->stat_n_rows = n_rows;

	mutex_exit(&(dict_sys->mutex));

	/* Find out the sizes of the indexes and how many different values
	for the key they approximately have */
	
	while (index) {
		n_vals = btr_estimate_number_of_different_key_vals(index);
		size = btr_get_size(index, BTR_TOTAL_SIZE);

		sum_of_index_sizes += size;

		mutex_enter(&(dict_sys->mutex));

		index->stat_n_diff_key_vals = n_vals;
		index->stat_index_size = size;

		mutex_exit(&(dict_sys->mutex));

		index = dict_table_get_next_index(index);
	}

	index = dict_table_get_first_index(table);

	table->stat_clustered_index_size = index->stat_index_size;

	table->stat_sum_of_other_index_sizes = sum_of_index_sizes
					-  index->stat_index_size;	

	table->stat_last_estimate_counter = table->stat_modif_counter;
}

/**************************************************************************
Prints a table data. */

void
dict_table_print(
/*=============*/
	dict_table_t*	table)	/* in: table */
{
	mutex_enter(&(dict_sys->mutex));
	dict_table_print_low(table);
	mutex_exit(&(dict_sys->mutex));
}

/**************************************************************************
Prints a table data when we know the table name. */

void
dict_table_print_by_name(
/*=====================*/
	char*	name)
{
	dict_table_t*	table;

	mutex_enter(&(dict_sys->mutex));

	table = dict_table_get_low(name);

	ut_a(table);
	
	dict_table_print_low(table);
	mutex_exit(&(dict_sys->mutex));
}

/**************************************************************************
Prints a table data. */
static
void
dict_table_print_low(
/*=================*/
	dict_table_t*	table)	/* in: table */
{
	ulint		i;
	dict_index_t*	index;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	printf("--------------------------------------\n");
	printf("TABLE INFO: name %s, columns %lu, indexes %lu\n", table->name,
			table->n_cols, UT_LIST_GET_LEN(table->indexes));
	for (i = 0; i < table->n_cols; i++) {
		printf("   ");
		dict_col_print_low(dict_table_get_nth_col(table, i));
	}

	index = UT_LIST_GET_FIRST(table->indexes);

	while (index != NULL) {
		dict_index_print_low(index);
		index = UT_LIST_GET_NEXT(indexes, index);
	}
}

/**************************************************************************
Prints a column data. */
static
void
dict_col_print_low(
/*===============*/
	dict_col_t*	col)	/* in: column */
{
	dtype_t*	type;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	type = dict_col_get_type(col);
	printf("COLUMN: name %s; ", col->name);

	dtype_print(type);
}

/**************************************************************************
Prints an index data. */
static
void
dict_index_print_low(
/*=================*/
	dict_index_t*	index)	/* in: index */
{
	ulint		i;
	dict_tree_t*	tree;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	tree = index->tree;
	
	printf(
	"INDEX INFO: name %s, table name %s, fields %lu, type %lu\n",
			index->name, index->table_name, index->n_fields,
			index->type);
	printf("   root node: space %lu, page number %lu\n",
					tree->space, tree->page);
			
	for (i = 0; i < index->n_fields; i++) {
		printf("   ");
		dict_field_print_low(dict_index_get_nth_field(index, i));
	}

	btr_print_size(tree);

	btr_print_tree(tree, 7);
}

/**************************************************************************
Prints a field data. */
static
void
dict_field_print_low(
/*=================*/
	dict_field_t*	field)	/* in: field */
{
	ut_ad(mutex_own(&(dict_sys->mutex)));

	printf("FIELD: column name %s, order criterion %lu\n", field->name,
								field->order);
}
