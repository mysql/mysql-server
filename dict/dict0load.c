/*****************************************************************************

Copyright (c) 1996, 2010, Innobase Oy. All Rights Reserved.

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
@file dict/dict0load.c
Loads to the memory cache database object definitions
from dictionary tables

Created 4/24/1996 Heikki Tuuri
*******************************************************/

#include "dict0load.h"
#include "mysql_version.h"

#ifdef UNIV_NONINL
#include "dict0load.ic"
#endif

#include "btr0pcur.h"
#include "btr0btr.h"
#include "page0page.h"
#include "mach0data.h"
#include "dict0dict.h"
#include "dict0boot.h"
#include "rem0cmp.h"
#include "srv0start.h"
#include "srv0srv.h"
#include "trx0sys.h"

/****************************************************************//**
Compare the name of an index column.
@return	TRUE if the i'th column of index is 'name'. */
static
ibool
name_of_col_is(
/*===========*/
	const dict_table_t*	table,	/*!< in: table */
	const dict_index_t*	index,	/*!< in: index */
	ulint			i,	/*!< in: index field offset */
	const char*		name)	/*!< in: name to compare to */
{
	ulint	tmp = dict_col_get_no(dict_field_get_col(
					      dict_index_get_nth_field(
						      index, i)));

	return(strcmp(name, dict_table_get_col_name(table, tmp)) == 0);
}

/********************************************************************//**
Finds the first table name in the given database.
@return own: table name, NULL if does not exist; the caller must free
the memory in the string! */
UNIV_INTERN
char*
dict_get_first_table_name_in_db(
/*============================*/
	const char*	name)	/*!< in: database name which ends in '/' */
{
	dict_table_t*	sys_tables;
	btr_pcur_t	pcur;
	dict_index_t*	sys_index;
	dtuple_t*	tuple;
	mem_heap_t*	heap;
	dfield_t*	dfield;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	mtr_t		mtr;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	heap = mem_heap_create(1000);

	mtr_start(&mtr);

	sys_tables = dict_table_get_low("SYS_TABLES");
	sys_index = UT_LIST_GET_FIRST(sys_tables->indexes);
	ut_a(!dict_table_is_comp(sys_tables));

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(dfield, name, ut_strlen(name));
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
loop:
	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur)) {
		/* Not found */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap);

		return(NULL);
	}

	field = rec_get_nth_field_old(rec, 0, &len);

	if (len < strlen(name)
	    || ut_memcmp(name, field, strlen(name)) != 0) {
		/* Not found */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap);

		return(NULL);
	}

	if (!rec_get_deleted_flag(rec, 0)) {

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

/********************************************************************//**
Prints to the standard output information on all tables found in the data
dictionary system table. */
UNIV_INTERN
void
dict_print(void)
/*============*/
{
	dict_table_t*	sys_tables;
	dict_index_t*	sys_index;
	dict_table_t*	table;
	btr_pcur_t	pcur;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	mtr_t		mtr;

	/* Enlarge the fatal semaphore wait timeout during the InnoDB table
	monitor printout */

	mutex_enter(&kernel_mutex);
	srv_fatal_semaphore_wait_threshold += SRV_SEMAPHORE_WAIT_EXTENSION;
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

	if (!btr_pcur_is_on_user_rec(&pcur)) {
		/* end of index */

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);

		mutex_exit(&(dict_sys->mutex));

		/* Restore the fatal semaphore wait timeout */

		mutex_enter(&kernel_mutex);
		srv_fatal_semaphore_wait_threshold -= SRV_SEMAPHORE_WAIT_EXTENSION;
		mutex_exit(&kernel_mutex);

		return;
	}

	field = rec_get_nth_field_old(rec, 0, &len);

	if (!rec_get_deleted_flag(rec, 0)) {

		/* We found one */

		char*	table_name = mem_strdupl((char*) field, len);

		btr_pcur_store_position(&pcur, &mtr);

		mtr_commit(&mtr);

		table = dict_table_get_low(table_name);
		mem_free(table_name);

		if (table == NULL) {
			fputs("InnoDB: Failed to load table ", stderr);
			ut_print_namel(stderr, NULL, TRUE, (char*) field, len);
			putc('\n', stderr);
		} else {
			/* The table definition was corrupt if there
			is no index */

			if (dict_table_get_first_index(table)) {
				dict_update_statistics(table, FALSE /* update
						       even if initialized */, FALSE);
			}

			dict_table_print_low(table);
		}

		mtr_start(&mtr);

		btr_pcur_restore_position(BTR_SEARCH_LEAF, &pcur, &mtr);
	}

	goto loop;
}

/********************************************************************//**
Determine the flags of a table described in SYS_TABLES.
@return compressed page size in kilobytes; or 0 if the tablespace is
uncompressed, ULINT_UNDEFINED on error */
static
ulint
dict_sys_tables_get_flags(
/*======================*/
	const rec_t*	rec)	/*!< in: a record of SYS_TABLES */
{
	const byte*	field;
	ulint		len;
	ulint		n_cols;
	ulint		flags;

	field = rec_get_nth_field_old(rec, 5, &len);
	ut_a(len == 4);

	flags = mach_read_from_4(field);

	if (UNIV_LIKELY(flags == DICT_TABLE_ORDINARY)) {
		return(0);
	}

	field = rec_get_nth_field_old(rec, 4/*N_COLS*/, &len);
	n_cols = mach_read_from_4(field);

	if (UNIV_UNLIKELY(!(n_cols & 0x80000000UL))) {
		/* New file formats require ROW_FORMAT=COMPACT. */
		return(ULINT_UNDEFINED);
	}

	switch (flags & (DICT_TF_FORMAT_MASK | DICT_TF_COMPACT)) {
	default:
	case DICT_TF_FORMAT_51 << DICT_TF_FORMAT_SHIFT:
	case DICT_TF_FORMAT_51 << DICT_TF_FORMAT_SHIFT | DICT_TF_COMPACT:
		/* flags should be DICT_TABLE_ORDINARY,
		or DICT_TF_FORMAT_MASK should be nonzero. */
		return(ULINT_UNDEFINED);

	case DICT_TF_FORMAT_ZIP << DICT_TF_FORMAT_SHIFT | DICT_TF_COMPACT:
#if DICT_TF_FORMAT_MAX > DICT_TF_FORMAT_ZIP
# error "missing case labels for DICT_TF_FORMAT_ZIP .. DICT_TF_FORMAT_MAX"
#endif
		/* We support this format. */
		break;
	}

	if (UNIV_UNLIKELY((flags & DICT_TF_ZSSIZE_MASK)
			  > (DICT_TF_ZSSIZE_MAX << DICT_TF_ZSSIZE_SHIFT))) {
		/* Unsupported compressed page size. */
		return(ULINT_UNDEFINED);
	}

	if (UNIV_UNLIKELY(flags & (~0 << DICT_TF_BITS))) {
		/* Some unused bits are set. */
		return(ULINT_UNDEFINED);
	}

	return(flags);
}

/********************************************************************//**
In a crash recovery we already have all the tablespace objects created.
This function compares the space id information in the InnoDB data dictionary
to what we already read with fil_load_single_table_tablespaces().

In a normal startup, we create the tablespace objects for every table in
InnoDB's data dictionary, if the corresponding .ibd file exists.
We also scan the biggest space id, and store it to fil_system. */
UNIV_INTERN
void
dict_check_tablespaces_and_store_max_id(
/*====================================*/
	ibool	in_crash_recovery)	/*!< in: are we doing a crash recovery */
{
	dict_table_t*	sys_tables;
	dict_index_t*	sys_index;
	btr_pcur_t	pcur;
	const rec_t*	rec;
	ulint		max_space_id;
	mtr_t		mtr;

	mutex_enter(&(dict_sys->mutex));

	mtr_start(&mtr);

	sys_tables = dict_table_get_low("SYS_TABLES");
	sys_index = UT_LIST_GET_FIRST(sys_tables->indexes);
	ut_a(!dict_table_is_comp(sys_tables));

	max_space_id = mtr_read_ulint(dict_hdr_get(&mtr)
				      + DICT_HDR_MAX_SPACE_ID,
				      MLOG_4BYTES, &mtr);
	fil_set_max_space_id_if_bigger(max_space_id);

	btr_pcur_open_at_index_side(TRUE, sys_index, BTR_SEARCH_LEAF, &pcur,
				    TRUE, &mtr);
loop:
	btr_pcur_move_to_next_user_rec(&pcur, &mtr);

	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur)) {
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

	if (!rec_get_deleted_flag(rec, 0)) {

		/* We found one */
		const byte*	field;
		ulint		len;
		ulint		space_id;
		ulint		flags;
		char*		name;

		field = rec_get_nth_field_old(rec, 0, &len);
		name = mem_strdupl((char*) field, len);

		flags = dict_sys_tables_get_flags(rec);
		if (UNIV_UNLIKELY(flags == ULINT_UNDEFINED)) {

			field = rec_get_nth_field_old(rec, 5, &len);
			flags = mach_read_from_4(field);

			ut_print_timestamp(stderr);
			fputs("  InnoDB: Error: table ", stderr);
			ut_print_filename(stderr, name);
			fprintf(stderr, "\n"
				"InnoDB: in InnoDB data dictionary"
				" has unknown type %lx.\n",
				(ulong) flags);

			goto loop;
		}

		field = rec_get_nth_field_old(rec, 9, &len);
		ut_a(len == 4);

		space_id = mach_read_from_4(field);

		btr_pcur_store_position(&pcur, &mtr);

		mtr_commit(&mtr);

		if (trx_sys_sys_space(space_id)) {
			/* The system tablespace always exists. */
		} else if (in_crash_recovery) {
			/* Check that the tablespace (the .ibd file) really
			exists; print a warning to the .err log if not.
			Do not print warnings for temporary tables. */
			ibool	is_temp;

			field = rec_get_nth_field_old(rec, 4, &len);
			if (0x80000000UL &  mach_read_from_4(field)) {
				/* ROW_FORMAT=COMPACT: read the is_temp
				flag from SYS_TABLES.MIX_LEN. */
				field = rec_get_nth_field_old(rec, 7, &len);
				is_temp = mach_read_from_4(field)
					& DICT_TF2_TEMPORARY;
			} else {
				/* For tables created with old versions
				of InnoDB, SYS_TABLES.MIX_LEN may contain
				garbage.  Such tables would always be
				in ROW_FORMAT=REDUNDANT.  Pretend that
				all such tables are non-temporary.  That is,
				do not suppress error printouts about
				temporary tables not being found. */
				is_temp = FALSE;
			}

			fil_space_for_table_exists_in_mem(
				space_id, name, is_temp, TRUE, !is_temp);
		} else {
			/* It is a normal database startup: create the space
			object and check that the .ibd file exists. */

			fil_open_single_table_tablespace(FALSE, space_id,
							 flags, name, NULL);
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

/********************************************************************//**
Loads definitions for table columns. */
static
void
dict_load_columns(
/*==============*/
	dict_table_t*	table,	/*!< in: table */
	mem_heap_t*	heap)	/*!< in: memory heap for temporary storage */
{
	dict_table_t*	sys_columns;
	dict_index_t*	sys_index;
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	byte*		buf;
	char*		name;
	ulint		mtype;
	ulint		prtype;
	ulint		col_len;
	ulint		i;
	mtr_t		mtr;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	mtr_start(&mtr);

	sys_columns = dict_table_get_low("SYS_COLUMNS");
	sys_index = UT_LIST_GET_FIRST(sys_columns->indexes);
	ut_a(!dict_table_is_comp(sys_columns));

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	buf = mem_heap_alloc(heap, 8);
	mach_write_to_8(buf, table->id);

	dfield_set_data(dfield, buf, 8);
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
	for (i = 0; i + DATA_N_SYS_COLS < (ulint) table->n_cols; i++) {

		rec = btr_pcur_get_rec(&pcur);

		ut_a(btr_pcur_is_on_user_rec(&pcur));

		ut_a(!rec_get_deleted_flag(rec, 0));

		field = rec_get_nth_field_old(rec, 0, &len);
		ut_ad(len == 8);
		ut_a(ut_dulint_cmp(table->id, mach_read_from_8(field)) == 0);

		field = rec_get_nth_field_old(rec, 1, &len);
		ut_ad(len == 4);
		ut_a(i == mach_read_from_4(field));

		ut_a(name_of_col_is(sys_columns, sys_index, 4, "NAME"));

		field = rec_get_nth_field_old(rec, 4, &len);
		name = mem_heap_strdupl(heap, (char*) field, len);

		field = rec_get_nth_field_old(rec, 5, &len);
		mtype = mach_read_from_4(field);

		field = rec_get_nth_field_old(rec, 6, &len);
		prtype = mach_read_from_4(field);

		if (dtype_get_charset_coll(prtype) == 0
		    && dtype_is_string_type(mtype)) {
			/* The table was created with < 4.1.2. */

			if (dtype_is_binary_string_type(mtype, prtype)) {
				/* Use the binary collation for
				string columns of binary type. */

				prtype = dtype_form_prtype(
					prtype,
					DATA_MYSQL_BINARY_CHARSET_COLL);
			} else {
				/* Use the default charset for
				other than binary columns. */

				prtype = dtype_form_prtype(
					prtype,
					data_mysql_default_charset_coll);
			}
		}

		field = rec_get_nth_field_old(rec, 7, &len);
		col_len = mach_read_from_4(field);

		ut_a(name_of_col_is(sys_columns, sys_index, 8, "PREC"));

		dict_mem_table_add_col(table, heap, name,
				       mtype, prtype, col_len);
		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
}

/********************************************************************//**
Loads definitions for index fields.
@return DB_SUCCESS if ok, DB_CORRUPTION if failed */
static
ulint
dict_load_fields(
/*=============*/
	dict_index_t*	index,	/*!< in: index whose fields to load */
	mem_heap_t*	heap)	/*!< in: memory heap for temporary storage */
{
	dict_table_t*	sys_fields;
	dict_index_t*	sys_index;
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	ulint		pos_and_prefix_len;
	ulint		prefix_len;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	byte*		buf;
	ulint		i;
	mtr_t		mtr;
	ulint		error = DB_SUCCESS;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	mtr_start(&mtr);

	sys_fields = dict_table_get_low("SYS_FIELDS");
	sys_index = UT_LIST_GET_FIRST(sys_fields->indexes);
	ut_a(!dict_table_is_comp(sys_fields));

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

		ut_a(btr_pcur_is_on_user_rec(&pcur));

		/* There could be delete marked records in SYS_FIELDS
		because SYS_FIELDS.INDEX_ID can be updated
		by ALTER TABLE ADD INDEX. */

		if (rec_get_deleted_flag(rec, 0)) {

			goto next_rec;
		}

		field = rec_get_nth_field_old(rec, 0, &len);
		ut_ad(len == 8);

		field = rec_get_nth_field_old(rec, 1, &len);
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

		ut_a(name_of_col_is(sys_fields, sys_index, 4, "COL_NAME"));

		field = rec_get_nth_field_old(rec, 4, &len);

		if (prefix_len >= DICT_MAX_INDEX_COL_LEN) {
			fprintf(stderr, "InnoDB: Error: load index"
					" '%s' failed.\n"
					"InnoDB: index field '%s' has a prefix"
					" length of %lu bytes,\n"
					"InnoDB: which exceeds the"
					" maximum limit of %lu bytes.\n"
					"InnoDB: Please use server that"
					" supports long index prefix\n"
					"InnoDB: or turn on"
					" innodb_force_recovery to load"
					" the table\n",
				index->name, mem_heap_strdupl(
						heap, (char*) field, len),
		    		(ulong) prefix_len,
				(ulong) (DICT_MAX_INDEX_COL_LEN - 1));
			error = DB_CORRUPTION;
			goto func_exit;
		}

		dict_mem_index_add_field(index,
					 mem_heap_strdupl(heap,
							  (char*) field, len),
					 prefix_len);

next_rec:
		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	}

func_exit:
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	return(error);
}

/********************************************************************//**
Loads definitions for table indexes. Adds them to the data dictionary
cache.
@return DB_SUCCESS if ok, DB_CORRUPTION if corruption of dictionary
table or DB_UNSUPPORTED if table has unknown index type */
static
ulint
dict_load_indexes(
/*==============*/
	dict_table_t*	table,	/*!< in: table */
	mem_heap_t*	heap)	/*!< in: memory heap for temporary storage */
{
	dict_table_t*	sys_indexes;
	dict_index_t*	sys_index;
	dict_index_t*	index;
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	const rec_t*	rec;
	const byte*	field;
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
	ulint		error = DB_SUCCESS;

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
	ut_a(!dict_table_is_comp(sys_indexes));

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	buf = mem_heap_alloc(heap, 8);
	mach_write_to_8(buf, table->id);

	dfield_set_data(dfield, buf, 8);
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
	for (;;) {
		if (!btr_pcur_is_on_user_rec(&pcur)) {

			break;
		}

		rec = btr_pcur_get_rec(&pcur);

		field = rec_get_nth_field_old(rec, 0, &len);
		ut_ad(len == 8);

		if (ut_memcmp(buf, field, len) != 0) {
			break;
		} else if (rec_get_deleted_flag(rec, 0)) {
			/* Skip delete marked records */
			goto next_rec;
		}

		field = rec_get_nth_field_old(rec, 1, &len);
		ut_ad(len == 8);
		id = mach_read_from_8(field);

		ut_a(name_of_col_is(sys_indexes, sys_index, 4, "NAME"));

		field = rec_get_nth_field_old(rec, 4, &name_len);
		name_buf = mem_heap_strdupl(heap, (char*) field, name_len);

		field = rec_get_nth_field_old(rec, 5, &len);
		n_fields = mach_read_from_4(field);

		field = rec_get_nth_field_old(rec, 6, &len);
		type = mach_read_from_4(field);

		field = rec_get_nth_field_old(rec, 7, &len);
		space = mach_read_from_4(field);

		ut_a(name_of_col_is(sys_indexes, sys_index, 8, "PAGE_NO"));

		field = rec_get_nth_field_old(rec, 8, &len);
		page_no = mach_read_from_4(field);

		/* We check for unsupported types first, so that the
		subsequent checks are relevant for the supported types. */
		if (type & ~(DICT_CLUSTERED | DICT_UNIQUE)) {

			fprintf(stderr,
				"InnoDB: Error: unknown type %lu"
				" of index %s of table %s\n",
				(ulong) type, name_buf, table->name);

			error = DB_UNSUPPORTED;
			goto func_exit;
		} else if (page_no == FIL_NULL) {

			fprintf(stderr,
				"InnoDB: Error: trying to load index %s"
				" for table %s\n"
				"InnoDB: but the index tree has been freed!\n",
				name_buf, table->name);

			error = DB_CORRUPTION;
			goto func_exit;
		} else if ((type & DICT_CLUSTERED) == 0
			    && NULL == dict_table_get_first_index(table)) {

			fputs("InnoDB: Error: trying to load index ",
			      stderr);
			ut_print_name(stderr, NULL, FALSE, name_buf);
			fputs(" for table ", stderr);
			ut_print_name(stderr, NULL, TRUE, table->name);
			fputs("\nInnoDB: but the first index"
			      " is not clustered!\n", stderr);

			error = DB_CORRUPTION;
			goto func_exit;
		} else if (is_sys_table
			   && ((type & DICT_CLUSTERED)
			       || ((table == dict_sys->sys_tables)
				   && (name_len == (sizeof "ID_IND") - 1)
				   && (0 == ut_memcmp(name_buf,
						      "ID_IND", name_len))))) {

			/* The index was created in memory already at booting
			of the database server */
		} else {
			index = dict_mem_index_create(table->name, name_buf,
						      space, type, n_fields);
			index->id = id;

			error = dict_load_fields(index, heap);

			if (error != DB_SUCCESS) {
				fprintf(stderr, "InnoDB: Error: load index '%s'"
					" for table '%s' failed\n",
					index->name, table->name);

				/* If the force recovery flag is set, and
				if the failed index is not the primary index, we
				will continue and open other indexes */
				if (srv_force_recovery
				    && !(index->type & DICT_CLUSTERED)) {
					error = DB_SUCCESS;
					goto next_rec;
				} else {
					goto func_exit;
				}
			}

			error = dict_index_add_to_cache(table, index, page_no,
							FALSE);
			/* The data dictionary tables should never contain
			invalid index definitions.  If we ignored this error
			and simply did not load this index definition, the
			.frm file would disagree with the index definitions
			inside InnoDB. */
			if (UNIV_UNLIKELY(error != DB_SUCCESS)) {

				goto func_exit;
			}
		}

next_rec:
		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	}

func_exit:
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	return(error);
}

/********************************************************************//**
Loads a table definition and also all its index definitions, and also
the cluster definition if the table is a member in a cluster. Also loads
all foreign key constraints where the foreign key is in the table or where
a foreign key references columns in this table. Adds all these to the data
dictionary cache.
@return table, NULL if does not exist; if the table is stored in an
.ibd file, but the file does not exist, then we set the
ibd_file_missing flag TRUE in the table object we return */
UNIV_INTERN
dict_table_t*
dict_load_table(
/*============*/
	const char*	name)	/*!< in: table name in the
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
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	ulint		space;
	ulint		n_cols;
	ulint		flags;
	ulint		err;
	mtr_t		mtr;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	heap = mem_heap_create(32000);

	mtr_start(&mtr);

	sys_tables = dict_table_get_low("SYS_TABLES");
	sys_index = UT_LIST_GET_FIRST(sys_tables->indexes);
	ut_a(!dict_table_is_comp(sys_tables));

	tuple = dtuple_create(heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(dfield, name, ut_strlen(name));
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur)
	    || rec_get_deleted_flag(rec, 0)) {
		/* Not found */
err_exit:
		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap);

		return(NULL);
	}

	field = rec_get_nth_field_old(rec, 0, &len);

	/* Check if the table name in record is the searched one */
	if (len != ut_strlen(name) || ut_memcmp(name, field, len) != 0) {

		goto err_exit;
	}

	ut_a(name_of_col_is(sys_tables, sys_index, 9, "SPACE"));

	field = rec_get_nth_field_old(rec, 9, &len);
	space = mach_read_from_4(field);

	/* Check if the tablespace exists and has the right name */
	if (!trx_sys_sys_space(space)) {
		flags = dict_sys_tables_get_flags(rec);

		if (UNIV_UNLIKELY(flags == ULINT_UNDEFINED)) {
			field = rec_get_nth_field_old(rec, 5, &len);
			flags = mach_read_from_4(field);

			ut_print_timestamp(stderr);
			fputs("  InnoDB: Error: table ", stderr);
			ut_print_filename(stderr, name);
			fprintf(stderr, "\n"
				"InnoDB: in InnoDB data dictionary"
				" has unknown type %lx.\n",
				(ulong) flags);
			goto err_exit;
		}
	} else {
		flags = 0;
	}

	ut_a(name_of_col_is(sys_tables, sys_index, 4, "N_COLS"));

	field = rec_get_nth_field_old(rec, 4, &len);
	n_cols = mach_read_from_4(field);

	/* The high-order bit of N_COLS is the "compact format" flag.
	For tables in that format, MIX_LEN may hold additional flags. */
	if (n_cols & 0x80000000UL) {
		ulint	flags2;

		flags |= DICT_TF_COMPACT;

		ut_a(name_of_col_is(sys_tables, sys_index, 7, "MIX_LEN"));
		field = rec_get_nth_field_old(rec, 7, &len);

		flags2 = mach_read_from_4(field);

		if (flags2 & (~0 << (DICT_TF2_BITS - DICT_TF2_SHIFT))) {
			ut_print_timestamp(stderr);
			fputs("  InnoDB: Warning: table ", stderr);
			ut_print_filename(stderr, name);
			fprintf(stderr, "\n"
				"InnoDB: in InnoDB data dictionary"
				" has unknown flags %lx.\n",
				(ulong) flags2);

			flags2 &= ~(~0 << (DICT_TF2_BITS - DICT_TF2_SHIFT));
		}

		flags |= flags2 << DICT_TF2_SHIFT;
	}

	/* See if the tablespace is available. */
	if (trx_sys_sys_space(space)) {
		/* The system tablespace is always available. */
	} else if (!fil_space_for_table_exists_in_mem(
			   space, name,
			   (flags >> DICT_TF2_SHIFT) & DICT_TF2_TEMPORARY,
			   FALSE, FALSE)) {

		if ((flags >> DICT_TF2_SHIFT) & DICT_TF2_TEMPORARY) {
			/* Do not bother to retry opening temporary tables. */
			ibd_file_missing = TRUE;
		} else {
			ut_print_timestamp(stderr);
			fprintf(stderr,
				"  InnoDB: error: space object of table");
			ut_print_filename(stderr, name);
			fprintf(stderr, ",\n"
				"InnoDB: space id %lu did not exist in memory."
				" Retrying an open.\n",
				(ulong) space);
			/* Try to open the tablespace */
			if (!fil_open_single_table_tablespace(
				    TRUE, space,
				    flags == DICT_TF_COMPACT ? 0 :
				    flags & ~(~0 << DICT_TF_BITS), name, NULL)) {
				/* We failed to find a sensible
				tablespace file */

				ibd_file_missing = TRUE;
			}
		}
	}

	table = dict_mem_table_create(name, space, n_cols & ~0x80000000UL,
				      flags);

	table->ibd_file_missing = (unsigned int) ibd_file_missing;

	ut_a(name_of_col_is(sys_tables, sys_index, 3, "ID"));

	field = rec_get_nth_field_old(rec, 3, &len);
	table->id = mach_read_from_8(field);

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	dict_load_columns(table, heap);

	dict_table_add_to_cache(table, heap);

	mem_heap_empty(heap);

	err = dict_load_indexes(table, heap);

	/* Initialize table foreign_child value. Its value could be
	changed when dict_load_foreigns() is called below */
	table->fk_max_recusive_level = 0;

	/* If the force recovery flag is set, we open the table irrespective
	of the error condition, since the user may want to dump data from the
	clustered index. However we load the foreign key information only if
	all indexes were loaded. */
	if (err == DB_SUCCESS) {
		err = dict_load_foreigns(table->name, TRUE, TRUE);

		if (err != DB_SUCCESS) {
			dict_table_remove_from_cache(table);
			table = NULL;
		} else {
			table->fk_max_recusive_level = 0;
		}
	} else {
		dict_index_t*	index;

		/* Make sure that at least the clustered index was loaded.
		Otherwise refuse to load the table */
		index = dict_table_get_first_index(table);

		if (!srv_force_recovery || !index
		     || !(index->type & DICT_CLUSTERED)) {
			dict_table_remove_from_cache(table);
			table = NULL;
		}
	}
#if 0
	if (err != DB_SUCCESS && table != NULL) {

		mutex_enter(&dict_foreign_err_mutex);

		ut_print_timestamp(stderr);

		fprintf(stderr,
			"  InnoDB: Error: could not make a foreign key"
			" definition to match\n"
			"InnoDB: the foreign key table"
			" or the referenced table!\n"
			"InnoDB: The data dictionary of InnoDB is corrupt."
			" You may need to drop\n"
			"InnoDB: and recreate the foreign key table"
			" or the referenced table.\n"
			"InnoDB: Submit a detailed bug report"
			" to http://bugs.mysql.com\n"
			"InnoDB: Latest foreign key error printout:\n%s\n",
			dict_foreign_err_buf);

		mutex_exit(&dict_foreign_err_mutex);
	}
#endif /* 0 */
	mem_heap_free(heap);

	return(table);
}

/***********************************************************************//**
Loads a table object based on the table id.
@return	table; NULL if table does not exist */
UNIV_INTERN
dict_table_t*
dict_load_table_on_id(
/*==================*/
	dulint	table_id)	/*!< in: table id */
{
	byte		id_buf[8];
	btr_pcur_t	pcur;
	mem_heap_t*	heap;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	dict_index_t*	sys_table_ids;
	dict_table_t*	sys_tables;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	dict_table_t*	table;
	mtr_t		mtr;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	table = NULL;

	/* NOTE that the operation of this function is protected by
	the dictionary mutex, and therefore no deadlocks can occur
	with other dictionary operations. */

	mtr_start(&mtr);
	/*---------------------------------------------------*/
	/* Get the secondary index based on ID for table SYS_TABLES */
	sys_tables = dict_sys->sys_tables;
	sys_table_ids = dict_table_get_next_index(
		dict_table_get_first_index(sys_tables));
	ut_a(!dict_table_is_comp(sys_tables));
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

	if (!btr_pcur_is_on_user_rec(&pcur)) {
		/* Not found */
		goto func_exit;
	}

	/* Find the first record that is not delete marked */
	while (rec_get_deleted_flag(rec, 0)) {
		if (!btr_pcur_move_to_next_user_rec(&pcur, &mtr)) {
			goto func_exit;
		}
		rec = btr_pcur_get_rec(&pcur);
	}

	/*---------------------------------------------------*/
	/* Now we have the record in the secondary index containing the
	table ID and NAME */

	rec = btr_pcur_get_rec(&pcur);
	field = rec_get_nth_field_old(rec, 0, &len);
	ut_ad(len == 8);

	/* Check if the table id in record is the one searched for */
	if (ut_dulint_cmp(table_id, mach_read_from_8(field)) != 0) {
		goto func_exit;
	}

	/* Now we get the table name from the record */
	field = rec_get_nth_field_old(rec, 1, &len);
	/* Load the table definition to memory */
	table = dict_load_table(mem_heap_strdupl(heap, (char*) field, len));
func_exit:
	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	mem_heap_free(heap);

	return(table);
}

/********************************************************************//**
This function is called when the database is booted. Loads system table
index definitions except for the clustered index which is added to the
dictionary cache at booting before calling this function. */
UNIV_INTERN
void
dict_load_sys_table(
/*================*/
	dict_table_t*	table)	/*!< in: system table */
{
	mem_heap_t*	heap;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	heap = mem_heap_create(1000);

	dict_load_indexes(table, heap);

	mem_heap_free(heap);
}

/********************************************************************//**
Loads foreign key constraint col names (also for the referenced table). */
static
void
dict_load_foreign_cols(
/*===================*/
	const char*	id,	/*!< in: foreign constraint id as a
				null-terminated string */
	dict_foreign_t*	foreign)/*!< in: foreign constraint object */
{
	dict_table_t*	sys_foreign_cols;
	dict_index_t*	sys_index;
	btr_pcur_t	pcur;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	ulint		i;
	mtr_t		mtr;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	foreign->foreign_col_names = mem_heap_alloc(
		foreign->heap, foreign->n_fields * sizeof(void*));

	foreign->referenced_col_names = mem_heap_alloc(
		foreign->heap, foreign->n_fields * sizeof(void*));
	mtr_start(&mtr);

	sys_foreign_cols = dict_table_get_low("SYS_FOREIGN_COLS");
	sys_index = UT_LIST_GET_FIRST(sys_foreign_cols->indexes);
	ut_a(!dict_table_is_comp(sys_foreign_cols));

	tuple = dtuple_create(foreign->heap, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(dfield, id, ut_strlen(id));
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
	for (i = 0; i < foreign->n_fields; i++) {

		rec = btr_pcur_get_rec(&pcur);

		ut_a(btr_pcur_is_on_user_rec(&pcur));
		ut_a(!rec_get_deleted_flag(rec, 0));

		field = rec_get_nth_field_old(rec, 0, &len);
		ut_a(len == ut_strlen(id));
		ut_a(ut_memcmp(id, field, len) == 0);

		field = rec_get_nth_field_old(rec, 1, &len);
		ut_a(len == 4);
		ut_a(i == mach_read_from_4(field));

		field = rec_get_nth_field_old(rec, 4, &len);
		foreign->foreign_col_names[i] = mem_heap_strdupl(
			foreign->heap, (char*) field, len);

		field = rec_get_nth_field_old(rec, 5, &len);
		foreign->referenced_col_names[i] = mem_heap_strdupl(
			foreign->heap, (char*) field, len);

		btr_pcur_move_to_next_user_rec(&pcur, &mtr);
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
}

/***********************************************************************//**
Loads a foreign key constraint to the dictionary cache.
@return	DB_SUCCESS or error code */
static
ulint
dict_load_foreign(
/*==============*/
	const char*	id,	/*!< in: foreign constraint id as a
				null-terminated string */
	ibool		check_charsets,
				/*!< in: TRUE=check charset compatibility */
	ibool		check_recursive)
				/*!< in: Whether to record the foreign table
				parent count to avoid unlimited recursive
				load of chained foreign tables */
{
	dict_foreign_t*	foreign;
	dict_table_t*	sys_foreign;
	btr_pcur_t	pcur;
	dict_index_t*	sys_index;
	dtuple_t*	tuple;
	mem_heap_t*	heap2;
	dfield_t*	dfield;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	ulint		n_fields_and_type;
	mtr_t		mtr;
	dict_table_t*	for_table;
	dict_table_t*	ref_table;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	heap2 = mem_heap_create(1000);

	mtr_start(&mtr);

	sys_foreign = dict_table_get_low("SYS_FOREIGN");
	sys_index = UT_LIST_GET_FIRST(sys_foreign->indexes);
	ut_a(!dict_table_is_comp(sys_foreign));

	tuple = dtuple_create(heap2, 1);
	dfield = dtuple_get_nth_field(tuple, 0);

	dfield_set_data(dfield, id, ut_strlen(id));
	dict_index_copy_types(tuple, sys_index, 1);

	btr_pcur_open_on_user_rec(sys_index, tuple, PAGE_CUR_GE,
				  BTR_SEARCH_LEAF, &pcur, &mtr);
	rec = btr_pcur_get_rec(&pcur);

	if (!btr_pcur_is_on_user_rec(&pcur)
	    || rec_get_deleted_flag(rec, 0)) {
		/* Not found */

		fprintf(stderr,
			"InnoDB: Error A: cannot load foreign constraint %s\n",
			id);

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		mem_heap_free(heap2);

		return(DB_ERROR);
	}

	field = rec_get_nth_field_old(rec, 0, &len);

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

	n_fields_and_type = mach_read_from_4(
		rec_get_nth_field_old(rec, 5, &len));

	ut_a(len == 4);

	/* We store the type in the bits 24..29 of n_fields_and_type. */

	foreign->type = (unsigned int) (n_fields_and_type >> 24);
	foreign->n_fields = (unsigned int) (n_fields_and_type & 0x3FFUL);

	foreign->id = mem_heap_strdup(foreign->heap, id);

	field = rec_get_nth_field_old(rec, 3, &len);
	foreign->foreign_table_name = mem_heap_strdupl(
		foreign->heap, (char*) field, len);

	field = rec_get_nth_field_old(rec, 4, &len);
	foreign->referenced_table_name = mem_heap_strdupl(
		foreign->heap, (char*) field, len);

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	dict_load_foreign_cols(id, foreign);

	ref_table = dict_table_check_if_in_cache_low(
			foreign->referenced_table_name);

	/* We could possibly wind up in a deep recursive calls if
	we call dict_table_get_low() again here if there
	is a chain of tables concatenated together with
	foreign constraints. In such case, each table is
	both a parent and child of the other tables, and
	act as a "link" in such table chains.
	To avoid such scenario, we would need to check the
	number of ancesters the current table has. If that
	exceeds DICT_FK_MAX_CHAIN_LEN, we will stop loading
	the child table.
	Foreign constraints are loaded in a Breath First fashion,
	that is, the index on FOR_NAME is scanned first, and then
	index on REF_NAME. So foreign constrains in which
	current table is a child (foreign table) are loaded first,
	and then those constraints where current table is a
	parent (referenced) table.
	Thus we could check the parent (ref_table) table's
	reference count (fk_max_recusive_level) to know how deep the
	recursive call is. If the parent table (ref_table) is already
	loaded, and its fk_max_recusive_level is larger than
	DICT_FK_MAX_CHAIN_LEN, we will stop the recursive loading
	by skipping loading the child table. It will not affect foreign
	constraint check for DMLs since child table will be loaded
	at that time for the constraint check. */
	if (!ref_table
	    || ref_table->fk_max_recusive_level < DICT_FK_MAX_RECURSIVE_LOAD) {

		/* If the foreign table is not yet in the dictionary cache, we
		have to load it so that we are able to make type comparisons
		in the next function call. */

		for_table = dict_table_get_low(foreign->foreign_table_name);

		if (for_table && ref_table && check_recursive) {
			/* This is to record the longest chain of ancesters
			this table has, if the parent has more ancesters
			than this table has, record it after add 1 (for this
			parent */
			if (ref_table->fk_max_recusive_level
			    >= for_table->fk_max_recusive_level) {
				for_table->fk_max_recusive_level =
					 ref_table->fk_max_recusive_level + 1;
			}
		}
	}

	/* Note that there may already be a foreign constraint object in
	the dictionary cache for this constraint: then the following
	call only sets the pointers in it to point to the appropriate table
	and index objects and frees the newly created object foreign.
	Adding to the cache should always succeed since we are not creating
	a new foreign key constraint but loading one from the data
	dictionary. */

	return(dict_foreign_add_to_cache(foreign, check_charsets));
}

/***********************************************************************//**
Loads foreign key constraints where the table is either the foreign key
holder or where the table is referenced by a foreign key. Adds these
constraints to the data dictionary. Note that we know that the dictionary
cache already contains all constraints where the other relevant table is
already in the dictionary cache.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ulint
dict_load_foreigns(
/*===============*/
	const char*	table_name,	/*!< in: table name */
	ibool		check_recursive,/*!< in: Whether to check recursive
					load of tables chained by FK */
	ibool		check_charsets)	/*!< in: TRUE=check charset
					compatibility */
{
	btr_pcur_t	pcur;
	mem_heap_t*	heap;
	dtuple_t*	tuple;
	dfield_t*	dfield;
	dict_index_t*	sec_index;
	dict_table_t*	sys_foreign;
	const rec_t*	rec;
	const byte*	field;
	ulint		len;
	char*		id ;
	ulint		err;
	mtr_t		mtr;

	ut_ad(mutex_own(&(dict_sys->mutex)));

	sys_foreign = dict_table_get_low("SYS_FOREIGN");

	if (sys_foreign == NULL) {
		/* No foreign keys defined yet in this database */

		fprintf(stderr,
			"InnoDB: Error: no foreign key system tables"
			" in the database\n");

		return(DB_ERROR);
	}

	ut_a(!dict_table_is_comp(sys_foreign));
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

	if (!btr_pcur_is_on_user_rec(&pcur)) {
		/* End of index */

		goto load_next_index;
	}

	/* Now we have the record in the secondary index containing a table
	name and a foreign constraint ID */

	rec = btr_pcur_get_rec(&pcur);
	field = rec_get_nth_field_old(rec, 0, &len);

	/* Check if the table name in the record is the one searched for; the
	following call does the comparison in the latin1_swedish_ci
	charset-collation, in a case-insensitive way. */

	if (0 != cmp_data_data(dfield_get_type(dfield)->mtype,
			       dfield_get_type(dfield)->prtype,
			       dfield_get_data(dfield), dfield_get_len(dfield),
			       field, len)) {

		goto load_next_index;
	}

	/* Since table names in SYS_FOREIGN are stored in a case-insensitive
	order, we have to check that the table name matches also in a binary
	string comparison. On Unix, MySQL allows table names that only differ
	in character case. */

	if (0 != ut_memcmp(field, table_name, len)) {

		goto next_rec;
	}

	if (rec_get_deleted_flag(rec, 0)) {

		goto next_rec;
	}

	/* Now we get a foreign key constraint id */
	field = rec_get_nth_field_old(rec, 1, &len);
	id = mem_heap_strdupl(heap, (char*) field, len);

	btr_pcur_store_position(&pcur, &mtr);

	mtr_commit(&mtr);

	/* Load the foreign constraint definition to the dictionary cache */

	err = dict_load_foreign(id, check_charsets, check_recursive);

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

		/* Switch to scan index on REF_NAME, fk_max_recusive_level
		already been updated when scanning FOR_NAME index, no need to
		update again */
		check_recursive = FALSE;

		goto start_load;
	}

	return(DB_SUCCESS);
}
