/******************************************************
Loads to the memory cache database object definitions
from dictionary tables

(c) 1996 Innobase Oy

Created 4/24/1996 Heikki Tuuri
*******************************************************/

#include "dict0load.h"

#ifdef UNIV_NONINL
#include "dict0load.ic"
#endif

#include "btr0pcur.h"
#include "btr0btr.h"
#include "page0page.h"
#include "mach0data.h"
#include "dict0dict.h"
#include "dict0boot.h"

/************************************************************************
Loads definitions for table columns. */
static
void
dict_load_columns(
/*==============*/
	dict_table_t*	table,	/* in: table */
	mem_heap_t*	heap);	/* in: memory heap for temporary storage */
/************************************************************************
Loads definitions for table indexes. */
static
void
dict_load_indexes(
/*==============*/
	dict_table_t*	table,	/* in: table */
	mem_heap_t*	heap);	/* in: memory heap for temporary storage */
/************************************************************************
Loads definitions for index fields. */
static
void
dict_load_fields(
/*=============*/
	dict_table_t*	table,	/* in: table */
	dict_index_t*	index,	/* in: index whose fields to load */
	mem_heap_t*	heap);	/* in: memory heap for temporary storage */


/************************************************************************
Loads a table definition and also all its index definitions, and also
the cluster definition if the table is a member in a cluster. */

dict_table_t*
dict_load_table(
/*============*/
			/* out: table, NULL if does not exist */
	char*	name)	/* in: table name */
{
	dict_table_t*	table;
	dict_table_t*	sys_tables;
	mtr_t		mtr;
	btr_pcur_t	pcur;
	dict_index_t*	sys_index;
	dtuple_t*	tuple;
	mem_heap_t*	heap;
	dfield_t*	dfield;
	rec_t*		rec;
	byte*		field;
	ulint		len;
	char*		buf;
	ulint		space;
	ulint		n_cols;
	
	ut_ad(mutex_own(&(dict_sys->mutex)));

	heap = mem_heap_create(1000);
	
	mtr_start(&mtr);

	sys_tables = dict_table_get_low("SYS_TABLES");
	sys_index = UT_LIST_GET_FIRST(sys_tables->indexes);

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(dfield, name, ut_strlen(name));
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
					BTR_SEARCH_LEAF, &pcur, &mtr);
	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur, &mtr)
					|| rec_get_deleted_flag(rec)) {
		/* Not found */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap);
		
		return(NULL);
	}	

	field = rec_get_nth_field(rec, 0, &len);

	/* Check if the table name in record is the searched one */
	if (len != ut_strlen(name) || ut_memcmp(name, field, len) != 0) {

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap);
		
		return(NULL);
	}

	ut_a(0 == ut_strcmp("SPACE",
		dict_field_get_col(
		dict_index_get_nth_field(
			dict_table_get_first_index(sys_tables), 9))->name));
	
	field = rec_get_nth_field(rec, 9, &len);
	space = mach_read_from_4(field);

	ut_a(0 == ut_strcmp("N_COLS",
		dict_field_get_col(
		dict_index_get_nth_field(
			dict_table_get_first_index(sys_tables), 4))->name));

	field = rec_get_nth_field(rec, 4, &len);
	n_cols = mach_read_from_4(field);

	table = dict_mem_table_create(name, space, n_cols);

	ut_a(0 == ut_strcmp("ID",
		dict_field_get_col(
		dict_index_get_nth_field(
			dict_table_get_first_index(sys_tables), 3))->name));

	field = rec_get_nth_field(rec, 3, &len);
	table->id = mach_read_from_8(field);

	field = rec_get_nth_field(rec, 5, &len);
	table->type = mach_read_from_4(field);

	if (table->type == DICT_TABLE_CLUSTER_MEMBER) {
		ut_a(0);
	
		field = rec_get_nth_field(rec, 6, &len);
		table->mix_id = mach_read_from_8(field);

		field = rec_get_nth_field(rec, 8, &len);
		buf = mem_heap_alloc(heap, len);
		ut_memcpy(buf, field, len);

		table->cluster_name = buf;
	}

	if ((table->type == DICT_TABLE_CLUSTER)
	    || (table->type == DICT_TABLE_CLUSTER_MEMBER)) {
		
		field = rec_get_nth_field(rec, 7, &len);
		table->mix_len = mach_read_from_4(field);
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	if (table->type == DICT_TABLE_CLUSTER_MEMBER) {
		/* Load the cluster table definition if not yet in
		memory cache */
		dict_table_get_low(table->cluster_name);
	}

	dict_load_columns(table, heap);

	dict_table_add_to_cache(table);

	dict_load_indexes(table, heap);
	
	mem_heap_free(heap);

	return(table);
}

/************************************************************************
This function is called when the database is booted. Loads system table
index definitions except for the clustered index which is added to the
dictionary cache at booting before calling this function. */

void
dict_load_sys_table(
/*================*/
	dict_table_t*	table)	/* in: system table */
{
	mem_heap_t*	heap;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	heap = mem_heap_create(1000);

	dict_load_indexes(table, heap);
	
	mem_heap_free(heap);
}

/************************************************************************
Loads definitions for table columns. */
static
void
dict_load_columns(
/*==============*/
	dict_table_t*	table,	/* in: table */
	mem_heap_t*	heap)	/* in: memory heap for temporary storage */
{
	dict_table_t*	sys_columns;
	dict_index_t*	sys_index;
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	rec_t*		rec;
	byte*		field;
	ulint		len;
	byte*		buf;
	char*		name_buf;
	char*		name;
	ulint		mtype;
	ulint		prtype;
	ulint		col_len;
	ulint		prec;
	ulint		i;
	mtr_t		mtr;
	
	ut_ad(mutex_own(&(dict_sys->mutex)));

	mtr_start(&mtr);

	sys_columns = dict_table_get_low("SYS_COLUMNS");
	sys_index = UT_LIST_GET_FIRST(sys_columns->indexes);

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	buf = mem_heap_alloc(heap, 8);
	mach_write_to_8(buf, table->id);

	dfield_set_data(dfield, buf, 8);
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
						BTR_SEARCH_LEAF, &pcur, &mtr);
   	for (i = 0; i < table->n_cols - DATA_N_SYS_COLS; i++) {

		rec = btr_pcur_get_rec(&pcur);

		ut_a(btr_pcur_is_on_user_rec(&pcur, &mtr));

		ut_a(!rec_get_deleted_flag(rec));
		
		field = rec_get_nth_field(rec, 0, &len);
		ut_ad(len == 8);
		ut_a(ut_dulint_cmp(table->id, mach_read_from_8(field)) == 0);

		field = rec_get_nth_field(rec, 1, &len);
		ut_ad(len == 4);
		ut_a(i == mach_read_from_4(field));

		ut_a(0 == ut_strcmp("NAME",
			dict_field_get_col(
			dict_index_get_nth_field(
			dict_table_get_first_index(sys_columns), 4))->name));

		field = rec_get_nth_field(rec, 4, &len);

		name_buf = mem_heap_alloc(heap, len + 1);
		ut_memcpy(name_buf, field, len);
		name_buf[len] = '\0';

		name = name_buf;

		field = rec_get_nth_field(rec, 5, &len);
		mtype = mach_read_from_4(field);

		field = rec_get_nth_field(rec, 6, &len);
		prtype = mach_read_from_4(field);

		field = rec_get_nth_field(rec, 7, &len);
		col_len = mach_read_from_4(field);

		ut_a(0 == ut_strcmp("PREC",
			dict_field_get_col(
			dict_index_get_nth_field(
			dict_table_get_first_index(sys_columns), 8))->name));

		field = rec_get_nth_field(rec, 8, &len);
		prec = mach_read_from_4(field);

		dict_mem_table_add_col(table, name, mtype, prtype, col_len,
									prec);
		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	} 

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
}

/************************************************************************
Loads definitions for table indexes. */
static
void
dict_load_indexes(
/*==============*/
	dict_table_t*	table,	/* in: table */
	mem_heap_t*	heap)	/* in: memory heap for temporary storage */
{
	dict_table_t*	sys_indexes;
	dict_index_t*	sys_index;
	dict_index_t*	index;
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	rec_t*		rec;
	byte*		field;
	ulint		len;
	ulint		name_len;
	char*		name_buf;
	ulint		type;
	ulint		space;
	ulint		page_no;
	ulint		n_fields;
	byte*		buf;
	ibool		is_sys_table;
	dulint		id;
	mtr_t		mtr;
	
	ut_ad(mutex_own(&(dict_sys->mutex)));

	if ((ut_dulint_get_high(table->id) == 0)
	    && (ut_dulint_get_low(table->id) < DICT_HDR_FIRST_ID)) {
		is_sys_table = TRUE;
	} else {
		is_sys_table = FALSE;
	}
	
	mtr_start(&mtr);

	sys_indexes = dict_table_get_low("SYS_INDEXES");
	sys_index = UT_LIST_GET_FIRST(sys_indexes->indexes);

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	buf = mem_heap_alloc(heap, 8);
	mach_write_to_8(buf, table->id);

	dfield_set_data(dfield, buf, 8);
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
						BTR_SEARCH_LEAF, &pcur, &mtr);
   	for (;;) {
		if (!btr_pcur_is_on_user_rec(&pcur, &mtr)) {

			break;
		}

		rec = btr_pcur_get_rec(&pcur);
		
		field = rec_get_nth_field(rec, 0, &len);
		ut_ad(len == 8);

		if (ut_memcmp(buf, field, len) != 0) {
			break;
		}

		ut_a(!rec_get_deleted_flag(rec));

		field = rec_get_nth_field(rec, 1, &len);
		ut_ad(len == 8);
		id = mach_read_from_8(field);

		ut_a(0 == ut_strcmp("NAME",
			dict_field_get_col(
			dict_index_get_nth_field(
			dict_table_get_first_index(sys_indexes), 4))->name));
		
		field = rec_get_nth_field(rec, 4, &name_len);

		name_buf = mem_heap_alloc(heap, name_len + 1);
		ut_memcpy(name_buf, field, name_len);
		name_buf[name_len] = '\0';

		field = rec_get_nth_field(rec, 5, &len);
		n_fields = mach_read_from_4(field);

		field = rec_get_nth_field(rec, 6, &len);
		type = mach_read_from_4(field);

		field = rec_get_nth_field(rec, 7, &len);
		space = mach_read_from_4(field);

		ut_a(0 == ut_strcmp("PAGE_NO",
			dict_field_get_col(
			dict_index_get_nth_field(
			dict_table_get_first_index(sys_indexes), 8))->name));

		field = rec_get_nth_field(rec, 8, &len);
		page_no = mach_read_from_4(field);

		if (is_sys_table
		    && ((type & DICT_CLUSTERED)
		        || ((table == dict_sys->sys_tables)
		            && (name_len == ut_strlen("ID_IND"))
			    && (0 == ut_memcmp(name_buf, "ID_IND",
							name_len))))) {

			/* The index was created in memory already in
			booting */
		} else {
 			index = dict_mem_index_create(table->name, name_buf,
						space, type, n_fields);
			index->page_no = page_no;
			index->id = id;
		
			dict_load_fields(table, index, heap);

			dict_index_add_to_cache(table, index);
		}

		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	} 

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
}

/************************************************************************
Loads definitions for index fields. */
static
void
dict_load_fields(
/*=============*/
	dict_table_t*	table,	/* in: table */
	dict_index_t*	index,	/* in: index whose fields to load */
	mem_heap_t*	heap)	/* in: memory heap for temporary storage */
{
	dict_table_t*	sys_fields;
	dict_index_t*	sys_index;
	mtr_t		mtr;
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	char*		col_name;
	rec_t*		rec;
	byte*		field;
	ulint		len;
	byte*		buf;
	ulint		i;
	
	ut_ad(mutex_own(&(dict_sys->mutex)));

	UT_NOT_USED(table);

	mtr_start(&mtr);

	sys_fields = dict_table_get_low("SYS_FIELDS");
	sys_index = UT_LIST_GET_FIRST(sys_fields->indexes);

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	buf = mem_heap_alloc(heap, 8);
	mach_write_to_8(buf, index->id);

	dfield_set_data(dfield, buf, 8);
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
						BTR_SEARCH_LEAF, &pcur, &mtr);
   	for (i = 0; i < index->n_fields; i++) {

		rec = btr_pcur_get_rec(&pcur);

		ut_a(btr_pcur_is_on_user_rec(&pcur, &mtr));
		ut_a(!rec_get_deleted_flag(rec));
		
		field = rec_get_nth_field(rec, 0, &len);
		ut_ad(len == 8);
		ut_a(ut_memcmp(buf, field, len) == 0);

		field = rec_get_nth_field(rec, 1, &len);
		ut_ad(len == 4);
		ut_a(i == mach_read_from_4(field));

		ut_a(0 == ut_strcmp("COL_NAME",
			dict_field_get_col(
			dict_index_get_nth_field(
			dict_table_get_first_index(sys_fields), 4))->name));

		field = rec_get_nth_field(rec, 4, &len);

		col_name = mem_heap_alloc(heap, len + 1);
		ut_memcpy(col_name, field, len);
		col_name[len] = '\0';
		
		dict_mem_index_add_field(index, col_name, 0);

		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	} 

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
}

/***************************************************************************
Loads a table object based on the table id. */

dict_table_t*
dict_load_table_on_id(
/*==================*/
				/* out: table; NULL if table does not exist */
	dulint	table_id)	/* in: table id */	
{
	mtr_t		mtr;
	byte		id_buf[8];
	btr_pcur_t	pcur;
	mem_heap_t* 	heap;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	dict_index_t*	sys_table_ids;
	dict_table_t*	sys_tables;
	rec_t*		rec;
	byte*		field;
	ulint		len;	
	dict_table_t*	table;
	char*		name;
	
	ut_ad(mutex_own(&(dict_sys->mutex)));

	/* NOTE that the operation of this function is protected by
	the dictionary mutex, and therefore no deadlocks can occur
	with other dictionary operations. */

	mtr_start(&mtr);	
	/*---------------------------------------------------*/
	/* Get the secondary index based on ID for table SYS_TABLES */	
	sys_tables = dict_sys->sys_tables;
	sys_table_ids = dict_table_get_next_index(
				dict_table_get_first_index(sys_tables));
	heap = mem_heap_create(256);

	tuple  = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	/* Write the table id in byte format to id_buf */
	mach_write_to_8(id_buf, table_id);
	
	dfield_set_data(dfield, id_buf, 8);
	dict_index_copy_types(tuple, sys_table_ids, 1);

	btr_pcur_open_on_user_rec(sys_table_ids, tuple, PAGE_CUR_GE,
						BTR_SEARCH_LEAF, &pcur, &mtr);
	rec = btr_pcur_get_rec(&pcur);
	
	if (!btr_pcur_is_on_user_rec(&pcur, &mtr)
					|| rec_get_deleted_flag(rec)) {
		/* Not found */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap);
		
		return(NULL);
	}

	/*---------------------------------------------------*/
	/* Now we have the record in the secondary index containing the
	table ID and NAME */

	rec = btr_pcur_get_rec(&pcur);
	field = rec_get_nth_field(rec, 0, &len);
	ut_ad(len == 8);

	/* Check if the table id in record is the one searched for */
	if (ut_dulint_cmp(table_id, mach_read_from_8(field)) != 0) {

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap);
		
		return(NULL);
	}
		
	/* Now we get the table name from the record */
	field = rec_get_nth_field(rec, 1, &len);

	name = mem_heap_alloc(heap, len + 1);
	ut_memcpy(name, field, len);
	name[len] = '\0';
	
	/* Load the table definition to memory */
	table = dict_load_table(name);

	ut_a(table);
	
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	mem_heap_free(heap);

	return(table);
}
