/******************************************************
Database object creation

(c) 1996 Innobase Oy

Created 1/8/1996 Heikki Tuuri
*******************************************************/

#include "dict0crea.h"

#ifdef UNIV_NONINL
#include "dict0crea.ic"
#endif

#include "btr0pcur.h"
#include "btr0btr.h"
#include "page0page.h"
#include "mach0data.h"
#include "dict0boot.h"
#include "dict0dict.h"
#include "que0que.h"
#include "row0ins.h"
#include "row0mysql.h"
#include "pars0pars.h"
#include "trx0roll.h"
#include "usr0sess.h"

/*********************************************************************
Based on a table object, this function builds the entry to be inserted
in the SYS_TABLES system table. */
static
dtuple_t*
dict_create_sys_tables_tuple(
/*=========================*/
				/* out: the tuple which should be inserted */
	dict_table_t*	table, 	/* in: table */
	mem_heap_t*	heap)	/* in: memory heap from which the memory for
				the built tuple is allocated */
{
	dict_table_t*	sys_tables;
	dtuple_t*	entry;
	dfield_t*	dfield;
	byte*		ptr;

	ut_ad(table && heap);

	sys_tables = dict_sys->sys_tables;
	
	entry = dtuple_create(heap, 8 + DATA_N_SYS_COLS);

	/* 0: NAME -----------------------------*/
	dfield = dtuple_get_nth_field(entry, 0);

	dfield_set_data(dfield, table->name, ut_strlen(table->name));
	/* 3: ID -------------------------------*/
	dfield = dtuple_get_nth_field(entry, 1);

	ptr = mem_heap_alloc(heap, 8);
	mach_write_to_8(ptr, table->id);

	dfield_set_data(dfield, ptr, 8);
	/* 4: N_COLS ---------------------------*/
	dfield = dtuple_get_nth_field(entry, 2);

	ptr = mem_heap_alloc(heap, 4);
	mach_write_to_4(ptr, table->n_def
			| ((ulint) table->comp << 31));
	dfield_set_data(dfield, ptr, 4);
	/* 5: TYPE -----------------------------*/
	dfield = dtuple_get_nth_field(entry, 3);

	ptr = mem_heap_alloc(heap, 4);
	mach_write_to_4(ptr, table->type);

	dfield_set_data(dfield, ptr, 4);
	/* 6: MIX_ID ---------------------------*/
	dfield = dtuple_get_nth_field(entry, 4);

	ptr = mem_heap_alloc(heap, 8);
	mach_write_to_8(ptr, table->mix_id);

	dfield_set_data(dfield, ptr, 8);
	/* 7: MIX_LEN --------------------------*/

	dfield = dtuple_get_nth_field(entry, 5);

	ptr = mem_heap_alloc(heap, 4);
	mach_write_to_4(ptr, table->mix_len);

	dfield_set_data(dfield, ptr, 4);
	/* 8: CLUSTER_NAME ---------------------*/
	dfield = dtuple_get_nth_field(entry, 6);

 	if (table->type == DICT_TABLE_CLUSTER_MEMBER) {
		dfield_set_data(dfield, table->cluster_name,
				ut_strlen(table->cluster_name));
		ut_error; /* Oracle-style clusters are not supported yet */
	} else {
		dfield_set_data(dfield, NULL, UNIV_SQL_NULL);
	}
	/* 9: SPACE ----------------------------*/
	dfield = dtuple_get_nth_field(entry, 7);

	ptr = mem_heap_alloc(heap, 4);
	mach_write_to_4(ptr, table->space);

	dfield_set_data(dfield, ptr, 4);
	/*----------------------------------*/

	dict_table_copy_types(entry, sys_tables);
				
	return(entry);
}	

/*********************************************************************
Based on a table object, this function builds the entry to be inserted
in the SYS_COLUMNS system table. */
static
dtuple_t*
dict_create_sys_columns_tuple(
/*==========================*/
				/* out: the tuple which should be inserted */
	dict_table_t*	table, 	/* in: table */
	ulint		i,	/* in: column number */
	mem_heap_t*	heap)	/* in: memory heap from which the memory for
				the built tuple is allocated */
{
	dict_table_t*	sys_columns;
	dtuple_t*	entry;
	dict_col_t*	column;
	dfield_t*	dfield;
	byte*		ptr;

	ut_ad(table && heap);

	column = dict_table_get_nth_col(table, i);

	sys_columns = dict_sys->sys_columns;
	
	entry = dtuple_create(heap, 7 + DATA_N_SYS_COLS);

	/* 0: TABLE_ID -----------------------*/
	dfield = dtuple_get_nth_field(entry, 0);

	ptr = mem_heap_alloc(heap, 8);
	mach_write_to_8(ptr, table->id);

	dfield_set_data(dfield, ptr, 8);
	/* 1: POS ----------------------------*/
	dfield = dtuple_get_nth_field(entry, 1);

	ptr = mem_heap_alloc(heap, 4);
	mach_write_to_4(ptr, i);

	dfield_set_data(dfield, ptr, 4);
	/* 4: NAME ---------------------------*/
	dfield = dtuple_get_nth_field(entry, 2);

	dfield_set_data(dfield, column->name, ut_strlen(column->name));
	/* 5: MTYPE --------------------------*/
	dfield = dtuple_get_nth_field(entry, 3);

	ptr = mem_heap_alloc(heap, 4);
	mach_write_to_4(ptr, (column->type).mtype);

	dfield_set_data(dfield, ptr, 4);
	/* 6: PRTYPE -------------------------*/
	dfield = dtuple_get_nth_field(entry, 4);

	ptr = mem_heap_alloc(heap, 4);
	mach_write_to_4(ptr, (column->type).prtype);

	dfield_set_data(dfield, ptr, 4);
	/* 7: LEN ----------------------------*/
	dfield = dtuple_get_nth_field(entry, 5);

	ptr = mem_heap_alloc(heap, 4);
	mach_write_to_4(ptr, (column->type).len);

	dfield_set_data(dfield, ptr, 4);
	/* 8: PREC ---------------------------*/
	dfield = dtuple_get_nth_field(entry, 6);

	ptr = mem_heap_alloc(heap, 4);
	mach_write_to_4(ptr, (column->type).prec);

	dfield_set_data(dfield, ptr, 4);
	/*---------------------------------*/

	dict_table_copy_types(entry, sys_columns);

	return(entry);
}	

/*******************************************************************
Builds a table definition to insert. */
static
ulint
dict_build_table_def_step(
/*======================*/
				/* out: DB_SUCCESS or error code */
	que_thr_t*	thr,	/* in: query thread */
	tab_node_t*	node)	/* in: table create node */
{
	dict_table_t*	table;
	dict_table_t*	cluster_table;
	dtuple_t*	row;
	ulint		error;
	const char*	path_or_name;
	ibool		is_path;
	mtr_t		mtr;
	ulint		i;
	ulint		row_len;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	table = node->table;

	table->id = dict_hdr_get_new_id(DICT_HDR_TABLE_ID);

	thr_get_trx(thr)->table_id = table->id;

	row_len = 0;
	for (i = 0; i < table->n_def; i++) {
		row_len += dtype_get_min_size(dict_col_get_type(
						&table->cols[i]));
	}
	if (row_len > BTR_PAGE_MAX_REC_SIZE) {
		return(DB_TOO_BIG_RECORD);
	}

	if (table->type == DICT_TABLE_CLUSTER_MEMBER) {

		cluster_table = dict_table_get_low(table->cluster_name);

		if (cluster_table == NULL) {
			
			return(DB_CLUSTER_NOT_FOUND);
		}

		/* Inherit space and mix len from the cluster */

		table->space = cluster_table->space;
		table->mix_len = cluster_table->mix_len;
		
		table->mix_id = dict_hdr_get_new_id(DICT_HDR_MIX_ID);
	}

	if (srv_file_per_table) {
		/* We create a new single-table tablespace for the table.
		We initially let it be 4 pages:
		- page 0 is the fsp header and an extent descriptor page,
		- page 1 is an ibuf bitmap page,
		- page 2 is the first inode page,
		- page 3 will contain the root of the clustered index of the
		  table we create here. */
	
		table->space = 0;	/* reset to zero for the call below */

		if (table->dir_path_of_temp_table) {
			/* We place tables created with CREATE TEMPORARY
			TABLE in the tmp dir of mysqld server */

			path_or_name = table->dir_path_of_temp_table;
			is_path = TRUE;
		} else {
			path_or_name = table->name;
			is_path = FALSE;
		}

		error = fil_create_new_single_table_tablespace(
					&(table->space), path_or_name, is_path,
					FIL_IBD_FILE_INITIAL_SIZE);
		if (error != DB_SUCCESS) {

			return(error);
		}

		mtr_start(&mtr);

		fsp_header_init(table->space, FIL_IBD_FILE_INITIAL_SIZE, &mtr);
		
		mtr_commit(&mtr);
	}

	row = dict_create_sys_tables_tuple(table, node->heap);

	ins_node_set_new_row(node->tab_def, row);

	return(DB_SUCCESS);
}

/*******************************************************************
Builds a column definition to insert. */
static
ulint
dict_build_col_def_step(
/*====================*/
				/* out: DB_SUCCESS */
	tab_node_t*	node)	/* in: table create node */
{
	dtuple_t*	row;

	row = dict_create_sys_columns_tuple(node->table, node->col_no,
								node->heap);
	ins_node_set_new_row(node->col_def, row);
	
	return(DB_SUCCESS);
}

/*********************************************************************
Based on an index object, this function builds the entry to be inserted
in the SYS_INDEXES system table. */
static
dtuple_t*
dict_create_sys_indexes_tuple(
/*==========================*/
				/* out: the tuple which should be inserted */
	dict_index_t*	index, 	/* in: index */
	mem_heap_t*	heap)	/* in: memory heap from which the memory for
				the built tuple is allocated */
{
	dict_table_t*	sys_indexes;
	dict_table_t*	table;
	dtuple_t*	entry;
	dfield_t*	dfield;
	byte*		ptr;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(index && heap);

	sys_indexes = dict_sys->sys_indexes;

	table = dict_table_get_low(index->table_name);
	
	entry = dtuple_create(heap, 7 + DATA_N_SYS_COLS);

	/* 0: TABLE_ID -----------------------*/
	dfield = dtuple_get_nth_field(entry, 0);

	ptr = mem_heap_alloc(heap, 8);
	mach_write_to_8(ptr, table->id);

	dfield_set_data(dfield, ptr, 8);
	/* 1: ID ----------------------------*/
	dfield = dtuple_get_nth_field(entry, 1);

	ptr = mem_heap_alloc(heap, 8);
	mach_write_to_8(ptr, index->id);

	dfield_set_data(dfield, ptr, 8);
	/* 4: NAME --------------------------*/
	dfield = dtuple_get_nth_field(entry, 2);

	dfield_set_data(dfield, index->name, ut_strlen(index->name));
	/* 5: N_FIELDS ----------------------*/
	dfield = dtuple_get_nth_field(entry, 3);

	ptr = mem_heap_alloc(heap, 4);
	mach_write_to_4(ptr, index->n_fields);

	dfield_set_data(dfield, ptr, 4);
	/* 6: TYPE --------------------------*/
	dfield = dtuple_get_nth_field(entry, 4);

	ptr = mem_heap_alloc(heap, 4);
	mach_write_to_4(ptr, index->type);

	dfield_set_data(dfield, ptr, 4);
	/* 7: SPACE --------------------------*/

#if DICT_SYS_INDEXES_SPACE_NO_FIELD != 7
#error "DICT_SYS_INDEXES_SPACE_NO_FIELD != 7"
#endif

	dfield = dtuple_get_nth_field(entry, 5);

	ptr = mem_heap_alloc(heap, 4);
	mach_write_to_4(ptr, index->space);

	dfield_set_data(dfield, ptr, 4);
	/* 8: PAGE_NO --------------------------*/

#if DICT_SYS_INDEXES_PAGE_NO_FIELD != 8
#error "DICT_SYS_INDEXES_PAGE_NO_FIELD != 8"
#endif

	dfield = dtuple_get_nth_field(entry, 6);

	ptr = mem_heap_alloc(heap, 4);
	mach_write_to_4(ptr, FIL_NULL);

	dfield_set_data(dfield, ptr, 4);
	/*--------------------------------*/

	dict_table_copy_types(entry, sys_indexes);

	return(entry);
}	

/*********************************************************************
Based on an index object, this function builds the entry to be inserted
in the SYS_FIELDS system table. */
static
dtuple_t*
dict_create_sys_fields_tuple(
/*=========================*/
				/* out: the tuple which should be inserted */
	dict_index_t*	index, 	/* in: index */
	ulint		i,	/* in: field number */
	mem_heap_t*	heap)	/* in: memory heap from which the memory for
				the built tuple is allocated */
{
	dict_table_t*	sys_fields;
	dtuple_t*	entry;
	dict_field_t*	field;
	dfield_t*	dfield;
	byte*		ptr;
	ibool		index_contains_column_prefix_field	= FALSE;
	ulint		j;

	ut_ad(index && heap);

	for (j = 0; j < index->n_fields; j++) {
	        if (dict_index_get_nth_field(index, j)->prefix_len > 0) {
	                index_contains_column_prefix_field = TRUE;	   
		}
	}

	field = dict_index_get_nth_field(index, i);

	sys_fields = dict_sys->sys_fields;
	
	entry = dtuple_create(heap, 3 + DATA_N_SYS_COLS);

	/* 0: INDEX_ID -----------------------*/
	dfield = dtuple_get_nth_field(entry, 0);

	ptr = mem_heap_alloc(heap, 8);
	mach_write_to_8(ptr, index->id);

	dfield_set_data(dfield, ptr, 8);
	/* 1: POS + PREFIX LENGTH ----------------------------*/

	dfield = dtuple_get_nth_field(entry, 1);

	ptr = mem_heap_alloc(heap, 4);
	
	if (index_contains_column_prefix_field) {
		/* If there are column prefix fields in the index, then
		we store the number of the field to the 2 HIGH bytes
		and the prefix length to the 2 low bytes, */

	        mach_write_to_4(ptr, (i << 16) + field->prefix_len);
	} else {
	        /* Else we store the number of the field to the 2 LOW bytes.
		This is to keep the storage format compatible with
		InnoDB versions < 4.0.14. */
	  
	        mach_write_to_4(ptr, i);
	}

	dfield_set_data(dfield, ptr, 4);
	/* 4: COL_NAME -------------------------*/
	dfield = dtuple_get_nth_field(entry, 2);

	dfield_set_data(dfield, field->name,
				ut_strlen(field->name));
	/*---------------------------------*/

	dict_table_copy_types(entry, sys_fields);
	
	return(entry);
}	

/*********************************************************************
Creates the tuple with which the index entry is searched for writing the index
tree root page number, if such a tree is created. */
static
dtuple_t*
dict_create_search_tuple(
/*=====================*/
				/* out: the tuple for search */
	dtuple_t*	tuple,	/* in: the tuple inserted in the SYS_INDEXES
				table */
	mem_heap_t*	heap)	/* in: memory heap from which the memory for
				the built tuple is allocated */
{
	dtuple_t*	search_tuple;
	dfield_t*	field1;
	dfield_t*	field2;

	ut_ad(tuple && heap);

	search_tuple = dtuple_create(heap, 2);

	field1 = dtuple_get_nth_field(tuple, 0);	
	field2 = dtuple_get_nth_field(search_tuple, 0);	

	dfield_copy(field2, field1);

	field1 = dtuple_get_nth_field(tuple, 1);	
	field2 = dtuple_get_nth_field(search_tuple, 1);	

	dfield_copy(field2, field1);

	ut_ad(dtuple_validate(search_tuple));

	return(search_tuple);
}

/*******************************************************************
Builds an index definition row to insert. */
static
ulint
dict_build_index_def_step(
/*======================*/
				/* out: DB_SUCCESS or error code */
	que_thr_t*	thr,	/* in: query thread */
	ind_node_t*	node)	/* in: index create node */
{
	dict_table_t*	table;
	dict_index_t*	index;
	dtuple_t*	row;
	trx_t*		trx;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	trx = thr_get_trx(thr);

	index = node->index;

	table = dict_table_get_low(index->table_name);

	if (table == NULL) {
		return(DB_TABLE_NOT_FOUND);
	}

	trx->table_id = table->id;

	node->table = table;

	ut_ad((UT_LIST_GET_LEN(table->indexes) > 0)
	      || (index->type & DICT_CLUSTERED));
	
	index->id = dict_hdr_get_new_id(DICT_HDR_INDEX_ID);

	/* Inherit the space id from the table; we store all indexes of a
	table in the same tablespace */

	index->space = table->space;
	node->page_no = FIL_NULL;
	row = dict_create_sys_indexes_tuple(index, node->heap);
	node->ind_row = row;

	ins_node_set_new_row(node->ind_def, row);

	return(DB_SUCCESS);
}

/*******************************************************************
Builds a field definition row to insert. */
static
ulint
dict_build_field_def_step(
/*======================*/
				/* out: DB_SUCCESS */
	ind_node_t*	node)	/* in: index create node */
{
	dict_index_t*	index;
	dtuple_t*	row;

	index = node->index;
	
	row = dict_create_sys_fields_tuple(index, node->field_no, node->heap);

	ins_node_set_new_row(node->field_def, row);

	return(DB_SUCCESS);
}

/*******************************************************************
Creates an index tree for the index if it is not a member of a cluster. */
static
ulint
dict_create_index_tree_step(
/*========================*/
				/* out: DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
	ind_node_t*	node)	/* in: index create node */
{
	dict_index_t*	index;
	dict_table_t*	sys_indexes;
	dict_table_t*	table;
	dtuple_t*	search_tuple;
	btr_pcur_t	pcur;
	mtr_t		mtr;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	index = node->index;	
	table = node->table;

	sys_indexes = dict_sys->sys_indexes;

	if (index->type & DICT_CLUSTERED
			&& table->type == DICT_TABLE_CLUSTER_MEMBER) {

		/* Do not create a new index tree: entries are put to the
		cluster tree */

		return(DB_SUCCESS);
	}

	/* Run a mini-transaction in which the index tree is allocated for
	the index and its root address is written to the index entry in
	sys_indexes */

	mtr_start(&mtr);

	search_tuple = dict_create_search_tuple(node->ind_row, node->heap);
			    
	btr_pcur_open(UT_LIST_GET_FIRST(sys_indexes->indexes),
				search_tuple, PAGE_CUR_L, BTR_MODIFY_LEAF,
				&pcur, &mtr);

	btr_pcur_move_to_next_user_rec(&pcur, &mtr);

	node->page_no = btr_create(index->type, index->space, index->id,
							table->comp, &mtr);
	/* printf("Created a new index tree in space %lu root page %lu\n",
					index->space, index->page_no); */

	page_rec_write_index_page_no(btr_pcur_get_rec(&pcur),
					DICT_SYS_INDEXES_PAGE_NO_FIELD,
					node->page_no, &mtr);
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	if (node->page_no == FIL_NULL) {

		return(DB_OUT_OF_FILE_SPACE);
	}

	return(DB_SUCCESS);
}

/***********************************************************************
Drops the index tree associated with a row in SYS_INDEXES table. */

void
dict_drop_index_tree(
/*=================*/
	rec_t*	rec,	/* in: record in the clustered index of SYS_INDEXES
			table */
	mtr_t*	mtr)	/* in: mtr having the latch on the record page */
{
	ulint	root_page_no;
	ulint	space;
	byte*	ptr;
	ulint	len;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	ut_a(!dict_sys->sys_indexes->comp);
	ptr = rec_get_nth_field_old(rec, DICT_SYS_INDEXES_PAGE_NO_FIELD, &len);

	ut_ad(len == 4);
	
	root_page_no = mtr_read_ulint(ptr, MLOG_4BYTES, mtr);

	if (root_page_no == FIL_NULL) {
		/* The tree has already been freed */

		return;
	}

	ptr = rec_get_nth_field_old(rec,
				DICT_SYS_INDEXES_SPACE_NO_FIELD, &len);

	ut_ad(len == 4);

	space = mtr_read_ulint(ptr, MLOG_4BYTES, mtr);

	if (!fil_tablespace_exists_in_mem(space)) {
		/* It is a single table tablespace and the .ibd file is
		missing: do nothing */

		return;
	}

	/* We free all the pages but the root page first; this operation
	may span several mini-transactions */

	btr_free_but_not_root(space, root_page_no);

	/* Then we free the root page in the same mini-transaction where
	we write FIL_NULL to the appropriate field in the SYS_INDEXES
	record: this mini-transaction marks the B-tree totally freed */
	
	/* printf("Dropping index tree in space %lu root page %lu\n", space,
							 root_page_no); */
	btr_free_root(space, root_page_no, mtr);

	page_rec_write_index_page_no(rec,
				DICT_SYS_INDEXES_PAGE_NO_FIELD, FIL_NULL, mtr);
}

/***********************************************************************
Truncates the index tree associated with a row in SYS_INDEXES table. */

ulint
dict_truncate_index_tree(
/*=====================*/
				/* out: new root page number, or
				FIL_NULL on failure */
	dict_table_t*	table,	/* in: the table the index belongs to */
	rec_t*		rec,	/* in: record in the clustered index of
				SYS_INDEXES table */
	mtr_t*		mtr)	/* in: mtr having the latch
				on the record page. The mtr may be
				committed and restarted in this call. */
{
	ulint		root_page_no;
	ulint		space;
	ulint		type;
	dulint		index_id;
	byte*		ptr;
	ulint		len;
	ulint		comp;
	dict_index_t*	index;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	ut_a(!dict_sys->sys_indexes->comp);
	ptr = rec_get_nth_field_old(rec, DICT_SYS_INDEXES_PAGE_NO_FIELD, &len);

	ut_ad(len == 4);

	root_page_no = mtr_read_ulint(ptr, MLOG_4BYTES, mtr);

	if (root_page_no == FIL_NULL) {
		/* The tree has been freed. */

		ut_print_timestamp(stderr);
		fprintf(stderr, "  InnoDB: Trying to TRUNCATE"
			" a missing index of table %s!\n", table->name);
		return(FIL_NULL);
	}

	ptr = rec_get_nth_field_old(rec,
				DICT_SYS_INDEXES_SPACE_NO_FIELD, &len);

	ut_ad(len == 4);

	space = mtr_read_ulint(ptr, MLOG_4BYTES, mtr);

	if (!fil_tablespace_exists_in_mem(space)) {
		/* It is a single table tablespace and the .ibd file is
		missing: do nothing */

		ut_print_timestamp(stderr);
		fprintf(stderr, "  InnoDB: Trying to TRUNCATE"
			" a missing .ibd file of table %s!\n", table->name);
		return(FIL_NULL);
	}

	ptr = rec_get_nth_field_old(rec,
				DICT_SYS_INDEXES_TYPE_FIELD, &len);
	ut_ad(len == 4);
	type = mach_read_from_4(ptr);

	ptr = rec_get_nth_field_old(rec, 1, &len);
	ut_ad(len == 8);
	index_id = mach_read_from_8(ptr);

	/* We free all the pages but the root page first; this operation
	may span several mini-transactions */

	btr_free_but_not_root(space, root_page_no);

	/* Then we free the root page in the same mini-transaction where
	we create the b-tree and write its new root page number to the
	appropriate field in the SYS_INDEXES record: this mini-transaction
	marks the B-tree totally truncated */

	comp = page_is_comp(btr_page_get(
				space, root_page_no, RW_X_LATCH, mtr));

	btr_free_root(space, root_page_no, mtr);
	/* We will temporarily write FIL_NULL to the PAGE_NO field
	in SYS_INDEXES, so that the database will not get into an
	inconsistent state in case it crashes between the mtr_commit()
	below and the following mtr_commit() call. */
	page_rec_write_index_page_no(rec, DICT_SYS_INDEXES_PAGE_NO_FIELD,
							FIL_NULL, mtr);

	/* We will need to commit the mini-transaction in order to avoid
	deadlocks in the btr_create() call, because otherwise we would
	be freeing and allocating pages in the same mini-transaction. */
	mtr_commit(mtr);
	/* mtr_commit() will invalidate rec. */
	rec = NULL;
	mtr_start(mtr);

	/* Find the index corresponding to this SYS_INDEXES record. */
	for (index = UT_LIST_GET_FIRST(table->indexes);
			index;
			index = UT_LIST_GET_NEXT(indexes, index)) {
		if (!ut_dulint_cmp(index->id, index_id)) {
			break;
		}
	}

	root_page_no = btr_create(type, space, index_id, comp, mtr);
	if (index) {
		index->tree->page = root_page_no;
	} else {
		ut_print_timestamp(stderr);
		fprintf(stderr,
			"  InnoDB: Index %lu %lu of table %s is missing\n"
			"InnoDB: from the data dictionary during TRUNCATE!\n",
			ut_dulint_get_high(index_id),
			ut_dulint_get_low(index_id),
			table->name);
	}

	return(root_page_no);
}

/*************************************************************************
Creates a table create graph. */

tab_node_t*
tab_create_graph_create(
/*====================*/
				/* out, own: table create node */
	dict_table_t*	table,	/* in: table to create, built as a memory data
				structure */
	mem_heap_t*	heap)	/* in: heap where created */
{
	tab_node_t*	node;

	node = mem_heap_alloc(heap, sizeof(tab_node_t));
	
	node->common.type = QUE_NODE_CREATE_TABLE;

	node->table = table;

	node->state = TABLE_BUILD_TABLE_DEF;
	node->heap = mem_heap_create(256);

	node->tab_def = ins_node_create(INS_DIRECT, dict_sys->sys_tables,
									heap); 
	node->tab_def->common.parent = node;
	
	node->col_def = ins_node_create(INS_DIRECT, dict_sys->sys_columns,
									heap);
	node->col_def->common.parent = node;

	node->commit_node = commit_node_create(heap);
	node->commit_node->common.parent = node;

	return(node);
}

/*************************************************************************
Creates an index create graph. */

ind_node_t*
ind_create_graph_create(
/*====================*/
				/* out, own: index create node */
	dict_index_t*	index,	/* in: index to create, built as a memory data
				structure */
	mem_heap_t*	heap)	/* in: heap where created */
{
	ind_node_t*	node;

	node = mem_heap_alloc(heap, sizeof(ind_node_t));

	node->common.type = QUE_NODE_CREATE_INDEX;

	node->index = index;

	node->state = INDEX_BUILD_INDEX_DEF;
	node->page_no = FIL_NULL;
	node->heap = mem_heap_create(256);

	node->ind_def = ins_node_create(INS_DIRECT,
						dict_sys->sys_indexes, heap); 
	node->ind_def->common.parent = node;

	node->field_def = ins_node_create(INS_DIRECT,
						dict_sys->sys_fields, heap);
	node->field_def->common.parent = node;

	node->commit_node = commit_node_create(heap);
	node->commit_node->common.parent = node;

	return(node);
}

/***************************************************************
Creates a table. This is a high-level function used in SQL execution graphs. */

que_thr_t*
dict_create_table_step(
/*===================*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr)	/* in: query thread */
{
	tab_node_t*	node;
	ulint		err	= DB_ERROR;
	trx_t*		trx;

	ut_ad(thr);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	trx = thr_get_trx(thr);
	
	node = thr->run_node;

	ut_ad(que_node_get_type(node) == QUE_NODE_CREATE_TABLE);

	if (thr->prev_node == que_node_get_parent(node)) {
		node->state = TABLE_BUILD_TABLE_DEF;
	}

	if (node->state == TABLE_BUILD_TABLE_DEF) {

		/* DO THE CHECKS OF THE CONSISTENCY CONSTRAINTS HERE */

		err = dict_build_table_def_step(thr, node);

		if (err != DB_SUCCESS) {

			goto function_exit;
		}
		
		node->state = TABLE_BUILD_COL_DEF;
		node->col_no = 0;

		thr->run_node = node->tab_def;

		return(thr);
	}

	if (node->state == TABLE_BUILD_COL_DEF) {

		if (node->col_no < (node->table)->n_def) {

			err = dict_build_col_def_step(node);

			if (err != DB_SUCCESS) {

				goto function_exit;
			}

			node->col_no++;
		
			thr->run_node = node->col_def;

			return(thr);
		} else {
			node->state = TABLE_COMMIT_WORK;
		}
	}

	if (node->state == TABLE_COMMIT_WORK) {

		/* Table was correctly defined: do NOT commit the transaction
		(CREATE TABLE does NOT do an implicit commit of the current
		transaction) */
		
		node->state = TABLE_ADD_TO_CACHE;

		/* thr->run_node = node->commit_node;

		return(thr); */
	}

	if (node->state == TABLE_ADD_TO_CACHE) {

		dict_table_add_to_cache(node->table);

		err = DB_SUCCESS;
	}

function_exit:
	trx->error_state = err;

	if (err == DB_SUCCESS) {
		/* Ok: do nothing */

	} else if (err == DB_LOCK_WAIT) {

		return(NULL);
	} else {
		/* SQL error detected */

		return(NULL);
	}

	thr->run_node = que_node_get_parent(node);

	return(thr);
} 

/***************************************************************
Creates an index. This is a high-level function used in SQL execution
graphs. */

que_thr_t*
dict_create_index_step(
/*===================*/
				/* out: query thread to run next or NULL */
	que_thr_t*	thr)	/* in: query thread */
{
	ind_node_t*	node;
	ibool		success;
	ulint		err	= DB_ERROR;
	trx_t*		trx;

	ut_ad(thr);
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	trx = thr_get_trx(thr);
	
	node = thr->run_node;

	ut_ad(que_node_get_type(node) == QUE_NODE_CREATE_INDEX);

	if (thr->prev_node == que_node_get_parent(node)) {
		node->state = INDEX_BUILD_INDEX_DEF;
	}

	if (node->state == INDEX_BUILD_INDEX_DEF) {
		/* DO THE CHECKS OF THE CONSISTENCY CONSTRAINTS HERE */
		err = dict_build_index_def_step(thr, node);

		if (err != DB_SUCCESS) {

			goto function_exit;
		}
		
		node->state = INDEX_BUILD_FIELD_DEF;
		node->field_no = 0;

		thr->run_node = node->ind_def;

		return(thr);
	}

	if (node->state == INDEX_BUILD_FIELD_DEF) {

		if (node->field_no < (node->index)->n_fields) {

			err = dict_build_field_def_step(node);

			if (err != DB_SUCCESS) {

				goto function_exit;
			}

			node->field_no++;
		
			thr->run_node = node->field_def;

			return(thr);
		} else {
			node->state = INDEX_CREATE_INDEX_TREE;
		}
	}

	if (node->state == INDEX_CREATE_INDEX_TREE) {

		err = dict_create_index_tree_step(node);

		if (err != DB_SUCCESS) {

			goto function_exit;
		}

		node->state = INDEX_COMMIT_WORK;
	}

	if (node->state == INDEX_COMMIT_WORK) {

		/* Index was correctly defined: do NOT commit the transaction
		(CREATE INDEX does NOT currently do an implicit commit of
		the current transaction) */
		
		node->state = INDEX_ADD_TO_CACHE;

		/* thr->run_node = node->commit_node;

		return(thr); */
	}

	if (node->state == INDEX_ADD_TO_CACHE) {

		success = dict_index_add_to_cache(node->table, node->index,
				node->page_no);

		ut_a(success);

		err = DB_SUCCESS;
	}

function_exit:
	trx->error_state = err;

	if (err == DB_SUCCESS) {
		/* Ok: do nothing */

	} else if (err == DB_LOCK_WAIT) {

		return(NULL);
	} else {
		/* SQL error detected */

		return(NULL);
	}

	thr->run_node = que_node_get_parent(node);

	return(thr);
} 

/********************************************************************
Creates the foreign key constraints system tables inside InnoDB
at database creation or database start if they are not found or are
not of the right form. */

ulint
dict_create_or_check_foreign_constraint_tables(void)
/*================================================*/
				/* out: DB_SUCCESS or error code */
{
	dict_table_t*	table1;
	dict_table_t*	table2;
	que_thr_t*	thr;
	que_t*		graph;
	ulint		error;
	trx_t*		trx;
	const char*	str;	

	mutex_enter(&(dict_sys->mutex));

	table1 = dict_table_get_low("SYS_FOREIGN");
	table2 = dict_table_get_low("SYS_FOREIGN_COLS");
	
	if (table1 && table2
            && UT_LIST_GET_LEN(table1->indexes) == 3
            && UT_LIST_GET_LEN(table2->indexes) == 1) {

            	/* Foreign constraint system tables have already been
            	created, and they are ok */

		mutex_exit(&(dict_sys->mutex));

            	return(DB_SUCCESS);
        }

	mutex_exit(&(dict_sys->mutex));

	trx = trx_allocate_for_mysql();
	
	trx->op_info = "creating foreign key sys tables";

	row_mysql_lock_data_dictionary(trx);

	if (table1) {
		fprintf(stderr,
		"InnoDB: dropping incompletely created SYS_FOREIGN table\n");
		row_drop_table_for_mysql("SYS_FOREIGN", trx, TRUE);
	}

	if (table2) {
		fprintf(stderr,
	"InnoDB: dropping incompletely created SYS_FOREIGN_COLS table\n");
		row_drop_table_for_mysql("SYS_FOREIGN_COLS", trx, TRUE);
	}

	fprintf(stderr,
		"InnoDB: Creating foreign key constraint system tables\n");

	/* NOTE: in dict_load_foreigns we use the fact that
	there are 2 secondary indexes on SYS_FOREIGN, and they
	are defined just like below */
	
	/* NOTE: when designing InnoDB's foreign key support in 2001, we made
	an error and made the table names and the foreign key id of type
	'CHAR' (internally, really a VARCHAR). We should have made the type
	VARBINARY, like in other InnoDB system tables, to get a clean
	design. */

	str =
	"PROCEDURE CREATE_FOREIGN_SYS_TABLES_PROC () IS\n"
	"BEGIN\n"
	"CREATE TABLE\n"
	"SYS_FOREIGN(ID CHAR, FOR_NAME CHAR, REF_NAME CHAR, N_COLS INT);\n"
	"CREATE UNIQUE CLUSTERED INDEX ID_IND ON SYS_FOREIGN (ID);\n"
	"CREATE INDEX FOR_IND ON SYS_FOREIGN (FOR_NAME);\n"
	"CREATE INDEX REF_IND ON SYS_FOREIGN (REF_NAME);\n"
	"CREATE TABLE\n"
  "SYS_FOREIGN_COLS(ID CHAR, POS INT, FOR_COL_NAME CHAR, REF_COL_NAME CHAR);\n"
	"CREATE UNIQUE CLUSTERED INDEX ID_IND ON SYS_FOREIGN_COLS (ID, POS);\n"
	"COMMIT WORK;\n"
	"END;\n";

	graph = pars_sql(str);

	ut_a(graph);

	graph->trx = trx;
	trx->graph = NULL;

	graph->fork_type = QUE_FORK_MYSQL_INTERFACE;

	ut_a(thr = que_fork_start_command(graph));

	que_run_threads(thr);

	error = trx->error_state;

	if (error != DB_SUCCESS) {
		fprintf(stderr, "InnoDB: error %lu in creation\n",
			(ulong) error);
		
		ut_a(error == DB_OUT_OF_FILE_SPACE);

		fprintf(stderr, "InnoDB: creation failed\n");
		fprintf(stderr, "InnoDB: tablespace is full\n");
		fprintf(stderr,
		"InnoDB: dropping incompletely created SYS_FOREIGN tables\n");

		row_drop_table_for_mysql("SYS_FOREIGN", trx, TRUE);
		row_drop_table_for_mysql("SYS_FOREIGN_COLS", trx, TRUE);

		error = DB_MUST_GET_MORE_FILE_SPACE;
	}

	que_graph_free(graph);
	
	trx->op_info = "";

	row_mysql_unlock_data_dictionary(trx);

  	trx_free_for_mysql(trx);

  	if (error == DB_SUCCESS) {
		fprintf(stderr,
		"InnoDB: Foreign key constraint system tables created\n");
	}

	return(error);
}

/************************************************************************
Adds foreign key definitions to data dictionary tables in the database. We
look at table->foreign_list, and also generate names to constraints that were
not named by the user. A generated constraint has a name of the format
databasename/tablename_ibfk_<number>, where the numbers start from 1, and are
given locally for this table, that is, the number is not global, as in the
old format constraints < 4.0.18 it used to be. */

ulint
dict_create_add_foreigns_to_dictionary(
/*===================================*/
				/* out: error code or DB_SUCCESS */
	ulint		start_id,/* in: if we are actually doing ALTER TABLE
				ADD CONSTRAINT, we want to generate constraint
				numbers which are bigger than in the table so
				far; we number the constraints from
				start_id + 1 up; start_id should be set to 0 if
				we are creating a new table, or if the table
				so far has no constraints for which the name
				was generated here */
	dict_table_t*	table,	/* in: table */
	trx_t*		trx)	/* in: transaction */
{
	dict_foreign_t*	foreign;
	que_thr_t*	thr;
	que_t*		graph;
	ulint		number	= start_id + 1;
	ulint		len;
	ulint		error;
	FILE*		ef	= dict_foreign_err_file;
	ulint		i;
	char*		sql;
	char*		sqlend;
	/* This procedure builds an InnoDB stored procedure which will insert
	the necessary rows into SYS_FOREIGN and SYS_FOREIGN_COLS. */
	static const char str1[] = "PROCEDURE ADD_FOREIGN_DEFS_PROC () IS\n"
	"BEGIN\n"
	"INSERT INTO SYS_FOREIGN VALUES(";
	static const char str2[] = ");\n";
	static const char str3[] =
	"INSERT INTO SYS_FOREIGN_COLS VALUES(";
	static const char str4[] =
	"COMMIT WORK;\n"
	"END;\n";

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	if (NULL == dict_table_get_low("SYS_FOREIGN")) {
		fprintf(stderr,
     "InnoDB: table SYS_FOREIGN not found from internal data dictionary\n");

		return(DB_ERROR);
	}

	foreign = UT_LIST_GET_FIRST(table->foreign_list);
loop:
	if (foreign == NULL) {

		return(DB_SUCCESS);
	}

	if (foreign->id == NULL) {
		/* Generate a new constraint id */
		ulint	namelen	= strlen(table->name);
		char*	id	= mem_heap_alloc(foreign->heap, namelen + 20);
		/* no overflow if number < 1e13 */
		sprintf(id, "%s_ibfk_%lu", table->name, (ulong) number++);
		foreign->id = id;
	}

	len = (sizeof str1) + (sizeof str2) + (sizeof str4) - 3
		+ 9/* ' and , chars */ + 10/* 32-bit integer */
		+ ut_strlenq(foreign->id, '\'') * (foreign->n_fields + 1)
		+ ut_strlenq(table->name, '\'')
		+ ut_strlenq(foreign->referenced_table_name, '\'');

	for (i = 0; i < foreign->n_fields; i++) {
		len += 9/* ' and , chars */ + 10/* 32-bit integer */
			+ (sizeof str3) + (sizeof str2) - 2
			+ ut_strlenq(foreign->foreign_col_names[i], '\'')
			+ ut_strlenq(foreign->referenced_col_names[i], '\'');
	}

	sql = sqlend = mem_alloc(len + 1);

	/* INSERT INTO SYS_FOREIGN VALUES(...); */
	memcpy(sqlend, str1, (sizeof str1) - 1);
	sqlend += (sizeof str1) - 1;
	*sqlend++ = '\'';
	sqlend = ut_strcpyq(sqlend, '\'', foreign->id);
	*sqlend++ = '\'', *sqlend++ = ',', *sqlend++ = '\'';
	sqlend = ut_strcpyq(sqlend, '\'', table->name);
	*sqlend++ = '\'', *sqlend++ = ',', *sqlend++ = '\'';
	sqlend = ut_strcpyq(sqlend, '\'', foreign->referenced_table_name);
	*sqlend++ = '\'', *sqlend++ = ',';
	sqlend += sprintf(sqlend, "%010lu",
			foreign->n_fields + (foreign->type << 24));
	memcpy(sqlend, str2, (sizeof str2) - 1);
	sqlend += (sizeof str2) - 1;

	for (i = 0; i < foreign->n_fields; i++) {
		/* INSERT INTO SYS_FOREIGN_COLS VALUES(...); */
		memcpy(sqlend, str3, (sizeof str3) - 1);
		sqlend += (sizeof str3) - 1;
		*sqlend++ = '\'';
		sqlend = ut_strcpyq(sqlend, '\'', foreign->id);
		*sqlend++ = '\''; *sqlend++ = ',';
		sqlend += sprintf(sqlend, "%010lu", (ulong) i);
		*sqlend++ = ','; *sqlend++ = '\'';
		sqlend = ut_strcpyq(sqlend, '\'',
			foreign->foreign_col_names[i]);
		*sqlend++ = '\''; *sqlend++ = ','; *sqlend++ = '\'';
		sqlend = ut_strcpyq(sqlend, '\'',
			foreign->referenced_col_names[i]);
		*sqlend++ = '\'';
		memcpy(sqlend, str2, (sizeof str2) - 1);
		sqlend += (sizeof str2) - 1;
	}

	memcpy(sqlend, str4, sizeof str4);
	sqlend += sizeof str4;

	ut_a(sqlend == sql + len + 1);

	graph = pars_sql(sql);

	ut_a(graph);

	mem_free(sql);

	graph->trx = trx;
	trx->graph = NULL;

	graph->fork_type = QUE_FORK_MYSQL_INTERFACE;

	ut_a(thr = que_fork_start_command(graph));

	que_run_threads(thr);

	error = trx->error_state;

	que_graph_free(graph);

	if (error == DB_DUPLICATE_KEY) {
		mutex_enter(&dict_foreign_err_mutex);
		rewind(ef);
		ut_print_timestamp(ef);
		fputs(" Error in foreign key constraint creation for table ",
			ef);
		ut_print_name(ef, trx, table->name);
		fputs(".\nA foreign key constraint of name ", ef);
		ut_print_name(ef, trx, foreign->id);
		fputs("\nalready exists."
			" (Note that internally InnoDB adds 'databasename/'\n"
			"in front of the user-defined constraint name).\n",
			ef);
		fputs("Note that InnoDB's FOREIGN KEY system tables store\n"
		      "constraint names as case-insensitive, with the\n"
		      "MySQL standard latin1_swedish_ci collation. If you\n"
		      "create tables or databases whose names differ only in\n"
		      "the character case, then collisions in constraint\n"
		      "names can occur. Workaround: name your constraints\n"
		      "explicitly with unique names.\n",
			ef);

		mutex_exit(&dict_foreign_err_mutex);

		return(error);
	}

	if (error != DB_SUCCESS) {
	        fprintf(stderr,
			"InnoDB: Foreign key constraint creation failed:\n"
			"InnoDB: internal error number %lu\n", (ulong) error);

		mutex_enter(&dict_foreign_err_mutex);
		ut_print_timestamp(ef);
		fputs(" Internal error in foreign key constraint creation"
			" for table ", ef);
		ut_print_name(ef, trx, table->name);
		fputs(".\n"
	"See the MySQL .err log in the datadir for more information.\n", ef);
		mutex_exit(&dict_foreign_err_mutex);

		return(error);
	}
	
	foreign = UT_LIST_GET_NEXT(foreign_list, foreign);

	goto loop;
}
