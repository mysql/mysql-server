/**********************************************************************
Data dictionary memory object creation

(c) 1996 Innobase Oy

Created 1/8/1996 Heikki Tuuri
***********************************************************************/

#include "dict0mem.h"

#ifdef UNIV_NONINL
#include "dict0mem.ic"
#endif

#include "rem0rec.h"
#include "data0type.h"
#include "mach0data.h"
#include "dict0dict.h"
#include "que0que.h"
#include "pars0pars.h"

#define	DICT_HEAP_SIZE		100	/* initial memory heap size when
					creating a table or index object */

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
	ulint	n_cols)		/* in: number of columns */
{
	dict_table_t*	table;
	char*		str;
	mem_heap_t*	heap;
	
	ut_ad(name);

	heap = mem_heap_create(DICT_HEAP_SIZE);

	table = mem_heap_alloc(heap, sizeof(dict_table_t));

	table->heap = heap;
	
	str = mem_heap_alloc(heap, 1 + ut_strlen(name));

	ut_strcpy(str, name);

	table->type = DICT_TABLE_ORDINARY;
	table->name = str;
	table->space = space;
	table->n_def = 0;
	table->n_cols = n_cols + DATA_N_SYS_COLS;
	table->mem_fix = 0;
	table->cached = FALSE;
	
	table->cols = mem_heap_alloc(heap, (n_cols + DATA_N_SYS_COLS)
							* sizeof(dict_col_t));
	UT_LIST_INIT(table->indexes);
	UT_LIST_INIT(table->locks);

	table->does_not_fit_in_memory = FALSE;

	table->stat_last_estimate_counter = (ulint)(-1);

	table->stat_modif_counter = 0;
	
	mutex_create(&(table->autoinc_mutex));
	mutex_set_level(&(table->autoinc_mutex), SYNC_DICT_AUTOINC_MUTEX);

	table->autoinc_inited = FALSE;

	table->magic_n = DICT_TABLE_MAGIC_N;
	
	return(table);
}

/**************************************************************************
Creates a cluster memory object. */

dict_table_t*
dict_mem_cluster_create(
/*====================*/
				/* out, own: cluster object */
	char*	name,		/* in: cluster name */
	ulint	space,		/* in: space where the clustered indexes
				of the member tables are placed */
	ulint	n_cols,		/* in: number of columns */
	ulint	mix_len)	/* in: length of the common key prefix in the
				cluster */
{
	dict_table_t*		cluster;

	cluster = dict_mem_table_create(name, space, n_cols);

	cluster->type = DICT_TABLE_CLUSTER;
	cluster->mix_len = mix_len;

	return(cluster);
}

/**************************************************************************
Declares a non-published table as a member in a cluster. */

void
dict_mem_table_make_cluster_member(
/*===============================*/
	dict_table_t*	table,		/* in: non-published table */
	char*		cluster_name)	/* in: cluster name */
{
	table->type = DICT_TABLE_CLUSTER_MEMBER;
	table->cluster_name = cluster_name;
}

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
	ulint		prec)	/* in: precision */
{
	char*		str;
	dict_col_t*	col;
	dtype_t*	type;
	
	ut_ad(table && name);
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
	
	table->n_def++;
	
	col = dict_table_get_nth_col(table, table->n_def - 1);	

	str = mem_heap_alloc(table->heap, 1 + ut_strlen(name));

	ut_strcpy(str, name);

	col->ind = table->n_def - 1;
	col->name = str;
	col->table = table;
	col->ord_part = 0;

	col->clust_pos = ULINT_UNDEFINED;
	
	type = dict_col_get_type(col);

	dtype_set(type, mtype, prtype, len, prec);
}

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
	ulint	n_fields)	/* in: number of fields */
{
	char*		str;
	dict_index_t*	index;
	mem_heap_t*	heap;
	
	ut_ad(table_name && index_name);

	heap = mem_heap_create(DICT_HEAP_SIZE);
	index = mem_heap_alloc(heap, sizeof(dict_index_t));

	index->heap = heap;
	
	str = mem_heap_alloc(heap, 1 + ut_strlen(index_name));

	ut_strcpy(str, index_name);

	index->type = type;
	index->space = space;
	index->name = str;
	index->table_name = table_name;
	index->table = NULL;
	index->n_def = 0;
	index->n_fields = n_fields;
	index->fields = mem_heap_alloc(heap, 1 + n_fields
						* sizeof(dict_field_t));
					/* The '1 +' above prevents allocation
					of an empty mem block */
	index->cached = FALSE;
	index->magic_n = DICT_INDEX_MAGIC_N;

	return(index);
}

/**************************************************************************
Adds a field definition to an index. NOTE: does not take a copy
of the column name if the field is a column. The memory occupied
by the column name may be released only after publishing the index. */

void
dict_mem_index_add_field(
/*=====================*/
	dict_index_t*	index,	/* in: index */
	char*		name,	/* in: column name */
	ulint		order)	/* in: order criterion; 0 means an ascending
				order */
{
	dict_field_t*	field;
	
	ut_ad(index && name);
	ut_ad(index->magic_n == DICT_INDEX_MAGIC_N);
	
	index->n_def++;

	field = dict_index_get_nth_field(index, index->n_def - 1);	

	field->name = name;
	field->order = order;
}

/**************************************************************************
Frees an index memory object. */

void
dict_mem_index_free(
/*================*/
	dict_index_t*	index)	/* in: index */
{
	mem_heap_free(index->heap);
}

/**************************************************************************
Creates a procedure memory object. */

dict_proc_t*
dict_mem_procedure_create(
/*======================*/
					/* out, own: procedure object */
	char*		name,		/* in: procedure name */
	char*		sql_string,	/* in: procedure definition as an SQL
					string */
	que_fork_t*	graph)		/* in: parsed procedure graph */
{
	dict_proc_t*	proc;
	proc_node_t*	proc_node;
	mem_heap_t*	heap;
	char*		str;
	
	ut_ad(name);

	heap = mem_heap_create(128);

	proc = mem_heap_alloc(heap, sizeof(dict_proc_t));

	proc->heap = heap;
	
	str = mem_heap_alloc(heap, 1 + ut_strlen(name));

	ut_strcpy(str, name);

	proc->name = str;

	str = mem_heap_alloc(heap, 1 + ut_strlen(sql_string));

	ut_strcpy(str, sql_string);

	proc->sql_string = str;

	UT_LIST_INIT(proc->graphs);

/*	UT_LIST_ADD_LAST(graphs, proc->graphs, graph); */

#ifdef UNIV_DEBUG
	UT_LIST_VALIDATE(graphs, que_t, proc->graphs);
#endif
	proc->mem_fix = 0;

	proc_node = que_fork_get_child(graph);

	proc_node->dict_proc = proc;

	return(proc);
}
