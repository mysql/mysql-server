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
#include "srv0start.h"
#include "srv0srv.h"

/************************************************************************
Finds the first table name in the given database. */

char*
dict_get_first_table_name_in_db(
/*============================*/
				/* out, own: table name, NULL if
				does not exist; the caller must
				free the memory in the string! */
	const char*	name)	/* in: database name which ends in '/' */
{
	dict_table_t*	sys_tables;
	btr_pcur_t	pcur;
	dict_index_t*	sys_index;
	dtuple_t*	tuple;
	mem_heap_t*	heap;
	dfield_t*	dfield;
	rec_t*		rec;
	byte*		field;
	ulint		len;
	mtr_t		mtr;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

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
loop:
	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur, &mtr)) {
		/* Not found */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap);
		
		return(NULL);
	}	

	field = rec_get_nth_field(rec, 0, &len);

	if (len < strlen(name)
	    || ut_memcmp(name, field, strlen(name)) != 0) {
		/* Not found */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap);
		
		return(NULL);
	}

	if (!rec_get_deleted_flag(rec)) {

		/* We found one */

                char*	table_name = mem_strdupl((char*) field, len);
		
		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap);
		
		return(table_name);
	}
	
	btr_pcur_move_to_next_user_rec(&pcur, &mtr);

	goto loop;
}

/************************************************************************
Prints to the standard output information on all tables found in the data
dictionary system table. */

void
dict_print(void)
/*============*/
{
	dict_table_t*	sys_tables;
	dict_index_t*	sys_index;
	dict_table_t*	table;
	btr_pcur_t	pcur;
	rec_t*		rec;
	byte*		field;
	ulint		len;
	mtr_t		mtr;
	
	/* Enlarge the fatal semaphore wait timeout during the InnoDB table
	monitor printout */

	mutex_enter(&kernel_mutex);
	srv_fatal_semaphore_wait_threshold += 7200; /* 2 hours */
	mutex_exit(&kernel_mutex);

	mutex_enter(&(dict_sys->mutex));

	mtr_start(&mtr);

	sys_tables = dict_table_get_low("SYS_TABLES");
	sys_index = UT_LIST_GET_FIRST(sys_tables->indexes);

	btr_pcur_open_at_index_side(TRUE, sys_index, BTR_SEARCH_LEAF, &pcur,
								TRUE, &mtr);
loop:
	btr_pcur_move_to_next_user_rec(&pcur, &mtr);

	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur, &mtr)) {
		/* end of index */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		
		mutex_exit(&(dict_sys->mutex));

		/* Restore the fatal semaphore wait timeout */

		mutex_enter(&kernel_mutex);
		srv_fatal_semaphore_wait_threshold -= 7200; /* 2 hours */
		mutex_exit(&kernel_mutex);

		return;
	}	

	field = rec_get_nth_field(rec, 0, &len);

	if (!rec_get_deleted_flag(rec)) {

		/* We found one */

                char*	table_name = mem_strdupl((char*) field, len);

		btr_pcur_store_position(&pcur, &mtr);

		mtr_commit(&mtr);

		table = dict_table_get_low(table_name);
		mem_free(table_name);

		if (table == NULL) {
			fputs("InnoDB: Failed to load table ", stderr);
			ut_print_namel(stderr, NULL, field, len);
			putc('\n', stderr);
		} else {
			/* The table definition was corrupt if there
			is no index */

			if (dict_table_get_first_index(table)) {
				dict_update_statistics_low(table, TRUE);
			}

			dict_table_print_low(table);
		}

		mtr_start(&mtr);

		btr_pcur_restore_position(BTR_SEARCH_LEAF, &pcur, &mtr);
	}

	goto loop;
}

/************************************************************************
In a crash recovery we already have all the tablespace objects created.
This function compares the space id information in the InnoDB data dictionary
to what we already read with fil_load_single_table_tablespaces().
In a normal startup we just scan the biggest space id, and store it to
fil_system. */

void
dict_check_tablespaces_or_store_max_id(
/*===================================*/
	ibool	in_crash_recovery)	/* in: are we doing a crash recovery */
{
	dict_table_t*	sys_tables;
	dict_index_t*	sys_index;
	btr_pcur_t	pcur;
	rec_t*		rec;
	byte*		field;
	ulint		len;
	ulint		space_id;
	ulint		max_space_id	= 0;
	mtr_t		mtr;
	
	mutex_enter(&(dict_sys->mutex));

	mtr_start(&mtr);

	sys_tables = dict_table_get_low("SYS_TABLES");
	sys_index = UT_LIST_GET_FIRST(sys_tables->indexes);

	btr_pcur_open_at_index_side(TRUE, sys_index, BTR_SEARCH_LEAF, &pcur,
								TRUE, &mtr);
loop:
	btr_pcur_move_to_next_user_rec(&pcur, &mtr);

	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur, &mtr)) {
		/* end of index */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		
		/* We must make the tablespace cache aware of the biggest
		known space id */

		/* printf("Biggest space id in data dictionary %lu\n",
							    max_space_id); */
		fil_set_max_space_id_if_bigger(max_space_id);

		mutex_exit(&(dict_sys->mutex));

		return;
	}	

	field = rec_get_nth_field(rec, 0, &len);

	if (!rec_get_deleted_flag(rec)) {

		/* We found one */

                char*	name = mem_strdupl((char*) field, len);

		field = rec_get_nth_field(rec, 9, &len);
		ut_a(len == 4);
			
		space_id = mach_read_from_4(field);

		btr_pcur_store_position(&pcur, &mtr);

		mtr_commit(&mtr);
		
		if (space_id != 0 && in_crash_recovery) {
			/* Check that the tablespace (the .ibd file) really
			exists; print a warning to the .err log if not */
			
			fil_space_for_table_exists_in_mem(space_id, name,
							FALSE, TRUE, TRUE);
		}

		mem_free(name);

		if (space_id > max_space_id) {
			max_space_id = space_id;
		}

		mtr_start(&mtr);

		btr_pcur_restore_position(BTR_SEARCH_LEAF, &pcur, &mtr);
	}

	goto loop;
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
	char*		name;
	ulint		mtype;
	ulint		prtype;
	ulint		col_len;
	ulint		prec;
	ulint		i;
	mtr_t		mtr;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

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
		name = mem_heap_strdupl(heap, (char*) field, len);

		field = rec_get_nth_field(rec, 5, &len);
		mtype = mach_read_from_4(field);

		field = rec_get_nth_field(rec, 6, &len);
		prtype = mach_read_from_4(field);

		if (dtype_is_non_binary_string_type(mtype, prtype)
		    && dtype_get_charset_coll(prtype) == 0) {
			/* This is a non-binary string type, and the table
			was created with < 4.1.2. Use the default charset. */

			prtype = dtype_form_prtype(prtype,
					data_mysql_default_charset_coll);
		}

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
Report that an index field or index for a table has been delete marked. */
static
void
dict_load_report_deleted_index(
/*===========================*/
	const char*	name,	/* in: table name */
	ulint		field)	/* in: index field, or ULINT_UNDEFINED */
{
	fprintf(stderr, "InnoDB: Error: data dictionary entry"
		" for table %s is corrupt!\n", name);
	if (field != ULINT_UNDEFINED) {
		fprintf(stderr,
			"InnoDB: Index field %lu is delete marked.\n", field);
	} else {
		fputs("InnoDB: An index is delete marked.\n", stderr);
	}
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
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	ulint		pos_and_prefix_len;
	ulint		prefix_len;
	rec_t*		rec;
	byte*		field;
	ulint		len;
	byte*		buf;
	ulint		i;
	mtr_t		mtr;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

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
		if (rec_get_deleted_flag(rec)) {
			dict_load_report_deleted_index(table->name, i);
		}
		
		field = rec_get_nth_field(rec, 0, &len);
		ut_ad(len == 8);
		ut_a(ut_memcmp(buf, field, len) == 0);

		field = rec_get_nth_field(rec, 1, &len);
		ut_a(len == 4);

		/* The next field stores the field position in the index
		and a possible column prefix length if the index field
		does not contain the whole column. The storage format is
		like this: if there is at least one prefix field in the index,
		then the HIGH 2 bytes contain the field number (== i) and the
		low 2 bytes the prefix length for the field. Otherwise the
		field number (== i) is contained in the 2 LOW bytes. */

		pos_and_prefix_len = mach_read_from_4(field);

		ut_a((pos_and_prefix_len & 0xFFFFUL) == i
		     || (pos_and_prefix_len & 0xFFFF0000UL) == (i << 16));

		if ((i == 0 && pos_and_prefix_len > 0)
		    || (pos_and_prefix_len & 0xFFFF0000UL) > 0) {

		        prefix_len = pos_and_prefix_len & 0xFFFFUL;
		} else {
		        prefix_len = 0;
		}

		ut_a(0 == ut_strcmp("COL_NAME",
			dict_field_get_col(
			dict_index_get_nth_field(
			dict_table_get_first_index(sys_fields), 4))->name));

		field = rec_get_nth_field(rec, 4, &len);

		dict_mem_index_add_field(index,
                                         mem_heap_strdupl(heap, (char*) field, len), 0, prefix_len);

		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	} 

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
}

/************************************************************************
Loads definitions for table indexes. Adds them to the data dictionary
cache. */
static
ibool
dict_load_indexes(
/*==============*/
				/* out: TRUE if ok, FALSE if corruption
				of dictionary table */
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
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

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

		if (rec_get_deleted_flag(rec)) {
			dict_load_report_deleted_index(table->name,
				ULINT_UNDEFINED);

			btr_pcur_close(&pcur);
			mtr_commit(&mtr);

			return(FALSE);
		}

		field = rec_get_nth_field(rec, 1, &len);
		ut_ad(len == 8);
		id = mach_read_from_8(field);

		ut_a(0 == ut_strcmp("NAME",
			dict_field_get_col(
			dict_index_get_nth_field(
			dict_table_get_first_index(sys_indexes), 4))->name));
		
		field = rec_get_nth_field(rec, 4, &name_len);
		name_buf = mem_heap_strdupl(heap, (char*) field, name_len);

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

		if (page_no == FIL_NULL) {

			fprintf(stderr,
		"InnoDB: Error: trying to load index %s for table %s\n"
		"InnoDB: but the index tree has been freed!\n",
				name_buf, table->name);

			btr_pcur_close(&pcur);
			mtr_commit(&mtr);

			return(FALSE);
		}

		if ((type & DICT_CLUSTERED) == 0
			    && NULL == dict_table_get_first_index(table)) {

			fprintf(stderr,
		"InnoDB: Error: trying to load index %s for table %s\n"
		"InnoDB: but the first index is not clustered!\n",
				name_buf, table->name);

			btr_pcur_close(&pcur);
			mtr_commit(&mtr);

			return(FALSE);
		}
		
		if (is_sys_table
		    && ((type & DICT_CLUSTERED)
		        || ((table == dict_sys->sys_tables)
		            && (name_len == (sizeof "ID_IND") - 1)
			    && (0 == ut_memcmp(name_buf, "ID_IND",
							name_len))))) {

			/* The index was created in memory already at booting
			of the database server */
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

	return(TRUE);
}

/************************************************************************
Loads a table definition and also all its index definitions, and also
the cluster definition if the table is a member in a cluster. Also loads
all foreign key constraints where the foreign key is in the table or where
a foreign key references columns in this table. Adds all these to the data
dictionary cache. */

dict_table_t*
dict_load_table(
/*============*/
				/* out: table, NULL if does not exist;
				if the table is stored in an .ibd file,
				but the file does not exist,
				then we set the ibd_file_missing flag TRUE
				in the table object we return */
	const char*	name)	/* in: table name in the
				databasename/tablename format */
{
	ibool		ibd_file_missing	= FALSE;
	dict_table_t*	table;
	dict_table_t*	sys_tables;
	btr_pcur_t	pcur;
	dict_index_t*	sys_index;
	dtuple_t*	tuple;
	mem_heap_t*	heap;
	dfield_t*	dfield;
	rec_t*		rec;
	byte*		field;
	ulint		len;
	ulint		space;
	ulint		n_cols;
	ulint		err;
	mtr_t		mtr;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

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

	/* Check if the tablespace exists and has the right name */
	if (space != 0) {
		if (fil_space_for_table_exists_in_mem(space, name, FALSE,
							FALSE, FALSE)) {
			/* Ok; (if we did a crash recovery then the tablespace
			can already be in the memory cache) */
		} else {
			/* Try to open the tablespace */
			if (!fil_open_single_table_tablespace(space, name)) {
				/* We failed to find a sensible tablespace
				file */

				ibd_file_missing = TRUE;
			}
		}
	}

	ut_a(0 == ut_strcmp("N_COLS",
		dict_field_get_col(
		dict_index_get_nth_field(
			dict_table_get_first_index(sys_tables), 4))->name));

	field = rec_get_nth_field(rec, 4, &len);
	n_cols = mach_read_from_4(field);

	table = dict_mem_table_create(name, space, n_cols);

	table->ibd_file_missing = ibd_file_missing;

	ut_a(0 == ut_strcmp("ID",
		dict_field_get_col(
		dict_index_get_nth_field(
			dict_table_get_first_index(sys_tables), 3))->name));

	field = rec_get_nth_field(rec, 3, &len);
	table->id = mach_read_from_8(field);

	field = rec_get_nth_field(rec, 5, &len);
	table->type = mach_read_from_4(field);

	if (table->type == DICT_TABLE_CLUSTER_MEMBER) {
		ut_error;
#if 0 /* clustered tables have not been implemented yet */
		field = rec_get_nth_field(rec, 6, &len);
		table->mix_id = mach_read_from_8(field);

		field = rec_get_nth_field(rec, 8, &len);
		table->cluster_name = mem_heap_strdupl(heap, (char*) field, len);
#endif
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
	
	err = dict_load_foreigns(table->name);
/*
	if (err != DB_SUCCESS) {
	
 		mutex_enter(&dict_foreign_err_mutex);

 		ut_print_timestamp(stderr);
 		
		fprintf(stderr,
"  InnoDB: Error: could not make a foreign key definition to match\n"
"InnoDB: the foreign key table or the referenced table!\n"
"InnoDB: The data dictionary of InnoDB is corrupt. You may need to drop\n"
"InnoDB: and recreate the foreign key table or the referenced table.\n"
"InnoDB: Submit a detailed bug report to http://bugs.mysql.com\n"
"InnoDB: Latest foreign key error printout:\n%s\n", dict_foreign_err_buf);
				
		mutex_exit(&dict_foreign_err_mutex);
	}
*/
	mem_heap_free(heap);

	return(table);
}

/***************************************************************************
Loads a table object based on the table id. */

dict_table_t*
dict_load_table_on_id(
/*==================*/
				/* out: table; NULL if table does not exist */
	dulint	table_id)	/* in: table id */	
{
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
	mtr_t		mtr;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

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
	/* Load the table definition to memory */
	table = dict_load_table(mem_heap_strdupl(heap, (char*) field, len));
	
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
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

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	heap = mem_heap_create(1000);

	dict_load_indexes(table, heap);
	
	mem_heap_free(heap);
}

/************************************************************************
Loads foreign key constraint col names (also for the referenced table). */
static
void
dict_load_foreign_cols(
/*===================*/
	const char*	id,	/* in: foreign constraint id as a null-
				terminated string */
	dict_foreign_t*	foreign)/* in: foreign constraint object */
{
	dict_table_t*	sys_foreign_cols;
	dict_index_t*	sys_index;
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	rec_t*		rec;
	byte*		field;
	ulint		len;
	ulint		i;
	mtr_t		mtr;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	foreign->foreign_col_names = mem_heap_alloc(foreign->heap,
					foreign->n_fields * sizeof(void*));

	foreign->referenced_col_names = mem_heap_alloc(foreign->heap,
					foreign->n_fields * sizeof(void*));
	mtr_start(&mtr);

	sys_foreign_cols = dict_table_get_low("SYS_FOREIGN_COLS");
	sys_index = UT_LIST_GET_FIRST(sys_foreign_cols->indexes);

	tuple = dtuple_create(foreign->heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(dfield, id, ut_strlen(id));
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
						BTR_SEARCH_LEAF, &pcur, &mtr);
   	for (i = 0; i < foreign->n_fields; i++) {

		rec = btr_pcur_get_rec(&pcur);

		ut_a(btr_pcur_is_on_user_rec(&pcur, &mtr));
		ut_a(!rec_get_deleted_flag(rec));
		
		field = rec_get_nth_field(rec, 0, &len);
		ut_a(len == ut_strlen(id));
		ut_a(ut_memcmp(id, field, len) == 0);

		field = rec_get_nth_field(rec, 1, &len);
		ut_a(len == 4);
		ut_a(i == mach_read_from_4(field));

		field = rec_get_nth_field(rec, 4, &len);
		foreign->foreign_col_names[i] =
                        mem_heap_strdupl(foreign->heap, (char*) field, len);

		field = rec_get_nth_field(rec, 5, &len);
		foreign->referenced_col_names[i] =
                  mem_heap_strdupl(foreign->heap, (char*) field, len);

		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	} 

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
}

/***************************************************************************
Loads a foreign key constraint to the dictionary cache. */
static
ulint
dict_load_foreign(
/*==============*/
				/* out: DB_SUCCESS or error code */
	const char*	id)	/* in: foreign constraint id as a
				null-terminated string */
{	
	dict_foreign_t*	foreign;
	dict_table_t*	sys_foreign;
	btr_pcur_t	pcur;
	dict_index_t*	sys_index;
	dtuple_t*	tuple;
	mem_heap_t*	heap2;
	dfield_t*	dfield;
	rec_t*		rec;
	byte*		field;
	ulint		len;
	ulint		err;
	mtr_t		mtr;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	heap2 = mem_heap_create(1000);
	
	mtr_start(&mtr);

	sys_foreign = dict_table_get_low("SYS_FOREIGN");
	sys_index = UT_LIST_GET_FIRST(sys_foreign->indexes);

	tuple = dtuple_create(heap2, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(dfield, id, ut_strlen(id));
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
					BTR_SEARCH_LEAF, &pcur, &mtr);
	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur, &mtr)
					|| rec_get_deleted_flag(rec)) {
		/* Not found */

		fprintf(stderr,
			"InnoDB: Error A: cannot load foreign constraint %s\n",
			id);

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap2);
		
		return(DB_ERROR);
	}	

	field = rec_get_nth_field(rec, 0, &len);

	/* Check if the id in record is the searched one */
	if (len != ut_strlen(id) || ut_memcmp(id, field, len) != 0) {

		fprintf(stderr,
			"InnoDB: Error B: cannot load foreign constraint %s\n",
			id);

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap2);
		
		return(DB_ERROR);
	}

	/* Read the table names and the number of columns associated
	with the constraint */

	mem_heap_free(heap2);
	
	foreign = dict_mem_foreign_create();

	foreign->n_fields = mach_read_from_4(rec_get_nth_field(rec, 5, &len));

	ut_a(len == 4);

	/* We store the type to the bits 24-31 of n_fields */
	
	foreign->type = foreign->n_fields >> 24;
	foreign->n_fields = foreign->n_fields & 0xFFFFFFUL;
	
	foreign->id = mem_heap_strdup(foreign->heap, id);

	field = rec_get_nth_field(rec, 3, &len);
	foreign->foreign_table_name =
                mem_heap_strdupl(foreign->heap, (char*) field, len);
	
	field = rec_get_nth_field(rec, 4, &len);
	foreign->referenced_table_name =
                mem_heap_strdupl(foreign->heap, (char*) field, len);

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	dict_load_foreign_cols(id, foreign);

	/* If the foreign table is not yet in the dictionary cache, we
	have to load it so that we are able to make type comparisons
	in the next function call. */

	dict_table_get_low(foreign->foreign_table_name);

	/* Note that there may already be a foreign constraint object in
	the dictionary cache for this constraint: then the following
	call only sets the pointers in it to point to the appropriate table
	and index objects and frees the newly created object foreign.
	Adding to the cache should always succeed since we are not creating
	a new foreign key constraint but loading one from the data
	dictionary. */

	err = dict_foreign_add_to_cache(foreign);

	return(err); 
}

/***************************************************************************
Loads foreign key constraints where the table is either the foreign key
holder or where the table is referenced by a foreign key. Adds these
constraints to the data dictionary. Note that we know that the dictionary
cache already contains all constraints where the other relevant table is
already in the dictionary cache. */

ulint
dict_load_foreigns(
/*===============*/
					/* out: DB_SUCCESS or error code */
	const char*	table_name)	/* in: table name */
{
	btr_pcur_t	pcur;
	mem_heap_t* 	heap;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	dict_index_t*	sec_index;
	dict_table_t*	sys_foreign;
	rec_t*		rec;
	byte*		field;
	ulint		len;	
	char*		id ;
	ulint		err;
	mtr_t		mtr;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(dict_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */

	sys_foreign = dict_table_get_low("SYS_FOREIGN");

	if (sys_foreign == NULL) {
		/* No foreign keys defined yet in this database */

		fprintf(stderr,
	"InnoDB: Error: no foreign key system tables in the database\n");
		
		return(DB_ERROR);
	}

	mtr_start(&mtr);	

	/* Get the secondary index based on FOR_NAME from table
	SYS_FOREIGN */	

	sec_index = dict_table_get_next_index(
				dict_table_get_first_index(sys_foreign));
start_load:
	heap = mem_heap_create(256);

	tuple  = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(dfield, table_name, ut_strlen(table_name));
	dict_index_copy_types(tuple, sec_index, 1);

	btr_pcur_open_on_user_rec(sec_index, tuple, PAGE_CUR_GE,
						BTR_SEARCH_LEAF, &pcur, &mtr);
loop:
	rec = btr_pcur_get_rec(&pcur);
	
	if (!btr_pcur_is_on_user_rec(&pcur, &mtr)) {
		/* End of index */

		goto load_next_index;
	}

	/* Now we have the record in the secondary index containing a table
	name and a foreign constraint ID */

	rec = btr_pcur_get_rec(&pcur);
	field = rec_get_nth_field(rec, 0, &len);

	/* Check if the table name in record is the one searched for */
	if (len != ut_strlen(table_name)
	    || 0 != ut_memcmp(field, table_name, len)) {

		goto load_next_index;
	}
		
	if (rec_get_deleted_flag(rec)) {

		goto next_rec;
	}

	/* Now we get a foreign key constraint id */
	field = rec_get_nth_field(rec, 1, &len);
	id = mem_heap_strdupl(heap, (char*) field, len);
	
	btr_pcur_store_position(&pcur, &mtr);

	mtr_commit(&mtr);

	/* Load the foreign constraint definition to the dictionary cache */
	
	err = dict_load_foreign(id);

	if (err != DB_SUCCESS) {
		btr_pcur_close(&pcur);
		mem_heap_free(heap);

		return(err);
	}

	mtr_start(&mtr);

	btr_pcur_restore_position(BTR_SEARCH_LEAF, &pcur, &mtr);
next_rec:
	btr_pcur_move_to_next_user_rec(&pcur, &mtr);

	goto loop;

load_next_index:
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	mem_heap_free(heap);
	
	sec_index = dict_table_get_next_index(sec_index);

	if (sec_index != NULL) {

		mtr_start(&mtr);	

		goto start_load;
	}

	return(DB_SUCCESS);
}
