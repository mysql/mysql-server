/*****************************************************************************

Copyright (c) 1996, 2017, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2012, Facebook Inc.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/******************************************************************//**
@file dict/dict0mem.cc
Data dictionary memory object creation

Created 1/8/1996 Heikki Tuuri
***********************************************************************/

#ifndef UNIV_HOTBACKUP
# include <mysql_com.h>

# include "ha_prototypes.h"

# include "data0type.h"
#endif /* !UNIV_HOTBACKUP */
#include "dict0dict.h"
#include "dict0mem.h"
#ifndef UNIV_HOTBACKUP
# include "fts0priv.h"
# include "mach0data.h"
# include "my_dbug.h"
# include "rem0rec.h"
# include "ut0crc32.h"
# include "lock0lock.h"
#endif /* !UNIV_HOTBACKUP */

#include <iostream>

#include "sync0sync.h"

/** An interger randomly initialized at startup used to make a temporary
table name as unuique as possible. */
static ib_uint32_t	dict_temp_file_num;

/** Display an identifier.
@param[in,out]	s	output stream
@param[in]	id_name	SQL identifier (other than table name)
@return the output stream */
std::ostream&
operator<<(
	std::ostream&		s,
	const id_name_t&	id_name)
{
	const char	q	= '`';
	const char*	c	= id_name;
	s << q;
	for (; *c != 0; c++) {
		if (*c == q) {
			s << *c;
		}
		s << *c;
	}
	s << q;
	return(s);
}

/** Display a table name.
@param[in,out]	s		output stream
@param[in]	table_name	table name
@return the output stream */
std::ostream&
operator<<(
	std::ostream&		s,
	const table_name_t&	table_name)
{
#ifndef UNIV_HOTBACKUP
	return(s << ut_get_name(NULL, table_name.m_name));
#else /* !UNIV_HOTBACKUP */
	return(s << table_name.m_name);
#endif /* !UNIV_HOTBACKUP */
}

#ifndef UNIV_HOTBACKUP
/** Adds a virtual column definition to a table.
@param[in,out]	table		table
@param[in,out]	heap		temporary memory heap, or NULL. It is
				used to store name when we have not finished
				adding all columns. When all columns are
				added, the whole name will copy to memory from
				table->heap
@param[in]	name		column name
@param[in]	mtype		main datatype
@param[in]	prtype		precise type
@param[in]	len		length
@param[in]	pos		position in a table
@param[in]	num_base	number of base columns
@return the virtual column definition */
dict_v_col_t*
dict_mem_table_add_v_col(
	dict_table_t*	table,
	mem_heap_t*	heap,
	const char*	name,
	ulint		mtype,
	ulint		prtype,
	ulint		len,
	ulint		pos,
	ulint		num_base)
{
	dict_v_col_t*	v_col;
	ulint		i;

	ut_ad(table);
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
	ut_ad(!heap == !name);

	ut_ad(prtype & DATA_VIRTUAL);

	i = table->n_v_def++;

	table->n_t_def++;

	if (name != NULL) {
		if (table->n_v_def == table->n_v_cols) {
			heap = table->heap;
		}

		if (i && !table->v_col_names) {
			/* All preceding column names are empty. */
			char* s = static_cast<char*>(
				mem_heap_zalloc(heap, table->n_v_def));

			table->v_col_names = s;
		}

		table->v_col_names = dict_add_col_name(table->v_col_names,
						       i, name, heap);
	}

	v_col = dict_table_get_nth_v_col(table, i);

	dict_mem_fill_column_struct(&v_col->m_col, pos, mtype, prtype, len);
	v_col->v_pos = i;

	if (num_base != 0) {
		v_col->base_col = static_cast<dict_col_t**>(mem_heap_zalloc(
					table->heap, num_base * sizeof(
						*v_col->base_col)));
	} else {
		v_col->base_col = NULL;
	}

	v_col->num_base = num_base;

	/* Initialize the index list for virtual columns */
	v_col->v_indexes = UT_NEW_NOKEY(dict_v_idx_list());

	return(v_col);
}

/** Adds a stored column definition to a table.
@param[in]	table		table
@param[in]	num_base	number of base columns. */
void
dict_mem_table_add_s_col(
	dict_table_t*	table,
	ulint		num_base)
{
	ulint	i = table->n_def - 1;
	dict_col_t*	col = table->get_col(i);
	dict_s_col_t	s_col;

	ut_ad(col != NULL);

	if (table->s_cols == NULL) {
		table->s_cols = UT_NEW_NOKEY(dict_s_col_list());
	}

	s_col.m_col = col;
	s_col.s_pos = i + table->n_v_def;

	if (num_base != 0) {
		s_col.base_col = static_cast<dict_col_t**>(mem_heap_zalloc(
					table->heap, num_base * sizeof(dict_col_t*)));
	} else {
		s_col.base_col = NULL;
	}

	s_col.num_base = num_base;
	table->s_cols->push_back(s_col);
}


/**********************************************************************//**
Renames a column of a table in the data dictionary cache. */
static
void
dict_mem_table_col_rename_low(
/*==========================*/
	dict_table_t*	table,	/*!< in/out: table */
	unsigned	i,	/*!< in: column offset corresponding to s */
	const char*	to,	/*!< in: new column name */
	const char*	s,	/*!< in: pointer to table->col_names */
	bool		is_virtual)
				/*!< in: if this is a virtual column */
{
	char*	t_col_names = const_cast<char*>(
		is_virtual ? table->v_col_names : table->col_names);
	ulint	n_col = is_virtual ? table->n_v_def : table->n_def;

	size_t from_len = strlen(s), to_len = strlen(to);

	ut_ad(i < table->n_def || is_virtual);
	ut_ad(i < table->n_v_def || !is_virtual);

	ut_ad(from_len <= NAME_LEN);
	ut_ad(to_len <= NAME_LEN);

	if (from_len == to_len) {
		/* The easy case: simply replace the column name in
		table->col_names. */
		strcpy(const_cast<char*>(s), to);
	} else {
		/* We need to adjust all affected index->field
		pointers, as in dict_index_add_col(). First, copy
		table->col_names. */
		ulint	prefix_len	= s - t_col_names;

		for (; i < n_col; i++) {
			s += strlen(s) + 1;
		}

		ulint	full_len	= s - t_col_names;
		char*	col_names;

		if (to_len > from_len) {
			ulint table_size_before_rename_col
				= mem_heap_get_size(table->heap);
			col_names = static_cast<char*>(
				mem_heap_alloc(
					table->heap,
					full_len + to_len - from_len));
			ulint table_size_after_rename_col
				= mem_heap_get_size(table->heap);
			if (table_size_before_rename_col
				!= table_size_after_rename_col) {
				dict_sys->size +=
					table_size_after_rename_col
						- table_size_before_rename_col;
			}
			memcpy(col_names, t_col_names, prefix_len);
		} else {
			col_names = const_cast<char*>(t_col_names);
		}

		memcpy(col_names + prefix_len, to, to_len);
		memmove(col_names + prefix_len + to_len,
			t_col_names + (prefix_len + from_len),
			full_len - (prefix_len + from_len));

		/* Replace the field names in every index. */
		for (dict_index_t* index = table->first_index();
		     index != NULL;
		     index = index->next()) {
			ulint	n_fields = dict_index_get_n_fields(index);

			for (ulint i = 0; i < n_fields; i++) {
				dict_field_t*	field = index->get_field(i);

				/* if is_virtual and that in field->col does
				not match, continue */
				if ((!is_virtual) !=
				    (!field->col->is_virtual())) {
					continue;
				}

				ulint		name_ofs
					= field->name - t_col_names;
				if (name_ofs <= prefix_len) {
					field->name = col_names + name_ofs;
				} else {
					ut_a(name_ofs < full_len);
					field->name = col_names
						+ name_ofs + to_len - from_len;
				}
			}
		}

		if (is_virtual) {
			table->v_col_names = col_names;
		} else {
			table->col_names = col_names;
		}
	}

	/* Virtual columns are not allowed for foreign key */
	if (is_virtual) {
		return;
	}

	dict_foreign_t*	foreign;

	/* Replace the field names in every foreign key constraint. */
	for (dict_foreign_set::iterator it = table->foreign_set.begin();
	     it != table->foreign_set.end();
	     ++it) {

		foreign = *it;

		for (unsigned f = 0; f < foreign->n_fields; f++) {
			/* These can point straight to
			table->col_names, because the foreign key
			constraints will be freed at the same time
			when the table object is freed. */
			foreign->foreign_col_names[f]
				= foreign->foreign_index->get_field(f)->name;
		}
	}

	for (dict_foreign_set::iterator it = table->referenced_set.begin();
	     it != table->referenced_set.end();
	     ++it) {

		foreign = *it;

		for (unsigned f = 0; f < foreign->n_fields; f++) {
			/* foreign->referenced_col_names[] need to be
			copies, because the constraint may become
			orphan when foreign_key_checks=0 and the
			parent table is dropped. */

			const char* col_name =
				foreign->referenced_index->get_field(f)->name;

			if (strcmp(foreign->referenced_col_names[f],
				   col_name)) {
				char**	rc = const_cast<char**>(
					foreign->referenced_col_names + f);
				size_t	col_name_len_1 = strlen(col_name) + 1;

				if (col_name_len_1 <= strlen(*rc) + 1) {
					memcpy(*rc, col_name, col_name_len_1);
				} else {
					*rc = static_cast<char*>(
						mem_heap_dup(
							foreign->heap,
							col_name,
							col_name_len_1));
				}
			}
		}
	}
}

/**********************************************************************//**
Renames a column of a table in the data dictionary cache. */
void
dict_mem_table_col_rename(
/*======================*/
	dict_table_t*	table,	/*!< in/out: table */
	ulint		nth_col,/*!< in: column index */
	const char*	from,	/*!< in: old column name */
	const char*	to,	/*!< in: new column name */
	bool		is_virtual)
				/*!< in: if this is a virtual column */
{
	const char*	s = is_virtual ? table->v_col_names : table->col_names;

	ut_ad((!is_virtual && nth_col < table->n_def)
	       || (is_virtual && nth_col < table->n_v_def));

	for (ulint i = 0; i < nth_col; i++) {
		size_t	len = strlen(s);
		ut_ad(len > 0);
		s += len + 1;
	}

	/* This could fail if the data dictionaries are out of sync.
	Proceed with the renaming anyway. */
	ut_ad(!strcmp(from, s));

	dict_mem_table_col_rename_low(table, static_cast<unsigned>(nth_col),
				      to, s, is_virtual);
}

/**********************************************************************//**
Creates and initializes a foreign constraint memory object.
@return own: foreign constraint struct */
dict_foreign_t*
dict_mem_foreign_create(void)
/*=========================*/
{
	dict_foreign_t*	foreign;
	mem_heap_t*	heap;
	DBUG_ENTER("dict_mem_foreign_create");

	heap = mem_heap_create(100);

	foreign = static_cast<dict_foreign_t*>(
		mem_heap_zalloc(heap, sizeof(dict_foreign_t)));

	foreign->heap = heap;

	foreign->v_cols = NULL;

	DBUG_PRINT("dict_mem_foreign_create", ("heap: %p", heap));

	DBUG_RETURN(foreign);
}

/**********************************************************************//**
Sets the foreign_table_name_lookup pointer based on the value of
lower_case_table_names.  If that is 0 or 1, foreign_table_name_lookup
will point to foreign_table_name.  If 2, then another string is
allocated from foreign->heap and set to lower case. */
void
dict_mem_foreign_table_name_lookup_set(
/*===================================*/
	dict_foreign_t*	foreign,	/*!< in/out: foreign struct */
	ibool		do_alloc)	/*!< in: is an alloc needed */
{
	if (innobase_get_lower_case_table_names() == 2) {
		if (do_alloc) {
			ulint	len;

			len = strlen(foreign->foreign_table_name) + 1;

			foreign->foreign_table_name_lookup =
				static_cast<char*>(
					mem_heap_alloc(foreign->heap, len));
		}
		strcpy(foreign->foreign_table_name_lookup,
		       foreign->foreign_table_name);
		innobase_casedn_str(foreign->foreign_table_name_lookup);
	} else {
		foreign->foreign_table_name_lookup
			= foreign->foreign_table_name;
	}
}

/**********************************************************************//**
Sets the referenced_table_name_lookup pointer based on the value of
lower_case_table_names.  If that is 0 or 1, referenced_table_name_lookup
will point to referenced_table_name.  If 2, then another string is
allocated from foreign->heap and set to lower case. */
void
dict_mem_referenced_table_name_lookup_set(
/*======================================*/
	dict_foreign_t*	foreign,	/*!< in/out: foreign struct */
	ibool		do_alloc)	/*!< in: is an alloc needed */
{
	if (innobase_get_lower_case_table_names() == 2) {
		if (do_alloc) {
			ulint	len;

			len = strlen(foreign->referenced_table_name) + 1;

			foreign->referenced_table_name_lookup =
				static_cast<char*>(
					mem_heap_alloc(foreign->heap, len));
		}
		strcpy(foreign->referenced_table_name_lookup,
		       foreign->referenced_table_name);
		innobase_casedn_str(foreign->referenced_table_name_lookup);
	} else {
		foreign->referenced_table_name_lookup
			= foreign->referenced_table_name;
	}
}

/** Fill the virtual column set with virtual column information
present in the given virtual index.
@param[in]	index	virtual index
@param[out]	v_cols	virtual column set. */
static
void
dict_mem_fill_vcol_has_index(
	const dict_index_t*	index,
	dict_vcol_set**		v_cols)
{
	for (ulint i = 0; i < index->table->n_v_cols; i++) {
		dict_v_col_t*	v_col = dict_table_get_nth_v_col(
					index->table, i);
		if (!v_col->m_col.ord_part) {
			continue;
		}

		for (auto it = v_col->v_indexes->begin();
		     it != v_col->v_indexes->end(); ++it) {
			dict_v_idx_t	v_idx = *it;

			if (v_idx.index == index) {
				if (*v_cols == NULL) {
					*v_cols = UT_NEW_NOKEY(dict_vcol_set());
				}

				(*v_cols)->insert(v_col);
			}
		}
	}
}

/** Fill the virtual column set with virtual column of the index
if the index contains the given column name.
@param[in]	col_name	column name
@param[in]	table		innodb table object
@param[out]	v_cols		set of virtual column information. */
static
void
dict_mem_fill_vcol_from_v_indexes(
	const char*		col_name,
	const dict_table_t*	table,
	dict_vcol_set**		v_cols)
{
	/* virtual column can't be Primary Key, so start with secondary index */
	for (const dict_index_t* index = table->first_index()->next();
	     index != NULL;
	     index = index->next()) {

		/* Skip if the index have newly added
		virtual column because field name is NULL.
		Later virtual column set will be
		refreshed during loading of table. */
		if (!dict_index_has_virtual(index)
		    || index->has_new_v_col) {
			continue;
		}

		for (ulint i = 0; i < index->n_fields; i++) {
			dict_field_t* field = index->get_field(i);

			if (strcmp(field->name, col_name) == 0) {
				dict_mem_fill_vcol_has_index(
					index, v_cols);
			}
		}
	}
}

/** Fill the virtual column set with virtual columns which have base columns
as the given col_name
@param[in]	col_name	column name
@param[in]	table		table object
@param[out]	v_cols		set of virtual columns. */
static
void
dict_mem_fill_vcol_set_for_base_col(
	const char*		col_name,
	const dict_table_t*	table,
	dict_vcol_set**		v_cols)
{
	for (ulint i = 0; i < table->n_v_cols; i++) {
		dict_v_col_t*	v_col = dict_table_get_nth_v_col(table, i);

		if (!v_col->m_col.ord_part) {
			continue;
		}

		for (ulint j = 0; j < v_col->num_base; j++) {
			if (strcmp(col_name,
				   table->get_col_name(
					   v_col->base_col[j]->ind)) == 0) {

				if (*v_cols == NULL) {
					*v_cols = UT_NEW_NOKEY(dict_vcol_set());
				}

				(*v_cols)->insert(v_col);
			}
		}
	}
}

/** Fills the dependent virtual columns in a set.
Reason for being dependent are
1) FK can be present on base column of virtual columns
2) FK can be present on column which is a part of virtual index
@param[in,out]  foreign foreign key information. */
void
dict_mem_foreign_fill_vcol_set(
	dict_foreign_t* foreign)
{
	ulint	type = foreign->type;

	if (type == 0) {
		return;
	}

	for (ulint i = 0; i < foreign->n_fields; i++) {
		/** FK can be present on base columns
		of virtual columns. */
		dict_mem_fill_vcol_set_for_base_col(
			foreign->foreign_col_names[i],
			foreign->foreign_table,
			&foreign->v_cols);

		/** FK can be present on the columns
		which can be a part of virtual index. */
		dict_mem_fill_vcol_from_v_indexes(
			foreign->foreign_col_names[i],
			foreign->foreign_table,
			&foreign->v_cols);
	}
}

/** Fill virtual columns set in each fk constraint present in the table.
@param[in,out]	table	innodb table object. */
void
dict_mem_table_fill_foreign_vcol_set(
	dict_table_t*	table)
{
	dict_foreign_set	fk_set = table->foreign_set;
	dict_foreign_t*		foreign;

	for (auto it = fk_set.begin(); it != fk_set.end(); ++it) {
		foreign = *it;

		dict_mem_foreign_fill_vcol_set(foreign);
	}
}

/** Free the vcol_set from all foreign key constraint on the table.
@param[in,out]	table	innodb table object. */
void
dict_mem_table_free_foreign_vcol_set(
	dict_table_t*	table)
{
	dict_foreign_set	fk_set = table->foreign_set;
	dict_foreign_t*		foreign;

	for (auto it = fk_set.begin(); it != fk_set.end(); ++it) {

		foreign = *it;

		if (foreign->v_cols != NULL) {
			UT_DELETE(foreign->v_cols);
			foreign->v_cols = NULL;
		}
	}
}

/** Check whether index can be used by transaction
@param[in] trx		transaction*/
bool dict_index_t::is_usable(const trx_t* trx) const
{
	/* Indexes that are being created are not usable. */
	if (!is_clustered() && dict_index_is_online_ddl(this)) {
		return false;
	}

	/* Cannot use a corrupted index. */
	if (is_corrupted()) {
		return false;
	}

	/* Check if the specified transaction can see this index. */
	return(table->is_temporary()
			|| trx_id == 0
			|| !MVCC::is_view_active(trx->read_view)
			|| trx->read_view->changes_visible(trx_id, table->name));
}
#endif /* !UNIV_HOTBACKUP */

/** Gets pointer to the nth column in an index.
@param[in] pos	position of the field
@return column */
const dict_col_t* dict_index_t::get_col(ulint pos) const
{
	return(get_field(pos)->col);
}

/** Gets the column number the nth field in an index.
@param[in] pos	position of the field
@return column number */
ulint dict_index_t::get_col_no(ulint pos) const
{
	return(dict_col_get_no(get_col(pos)));
}

/** Returns the position of a system column in an index.
@param[in] type		DATA_ROW_ID, ...
@return position, ULINT_UNDEFINED if not contained */
ulint dict_index_t::get_sys_col_pos(ulint type) const
{
	ut_ad(magic_n == DICT_INDEX_MAGIC_N);
	ut_ad(!dict_index_is_ibuf(this));

	if (is_clustered()) {

		return(dict_col_get_clust_pos(
			table->get_sys_col(type),
			this));
	}

	return(get_col_pos(dict_table_get_sys_col_no(table, type)));
}

/** Looks for column n in an index.
@param[in]	n		column number
@param[in]	inc_prefix	true=consider column prefixes too
@param[in]	is_virtual	true==virtual column
@return position in internal representation of the index;
ULINT_UNDEFINED if not contained */
ulint dict_index_t::get_col_pos(
	ulint n, bool inc_prefix, bool is_virtual) const
{
	const dict_field_t*	field;
	const dict_col_t*	col;
	ulint			pos;
	ulint			n_fields;

	ut_ad(magic_n == DICT_INDEX_MAGIC_N);

	if (is_virtual) {
		col = &(dict_table_get_nth_v_col(table, n)->m_col);
	} else {
		col = table->get_col(n);
	}

	if (is_clustered()) {
		return(dict_col_get_clust_pos(col, this));
	}

	n_fields = dict_index_get_n_fields(this);

	for (pos = 0; pos < n_fields; pos++) {
		field = get_field(pos);

		if (col == field->col
			&& (inc_prefix || field->prefix_len == 0)) {

			return(pos);
		}
	}

	return(ULINT_UNDEFINED);
}
/**********************************************************************//**
Frees an index memory object. */
void
dict_mem_index_free(
/*================*/
	dict_index_t*	index)	/*!< in: index */
{
	ut_ad(index);
	ut_ad(index->magic_n == DICT_INDEX_MAGIC_N);

#ifndef UNIV_HOTBACKUP
	dict_index_zip_pad_mutex_destroy(index);

	if (dict_index_is_spatial(index)) {
		rtr_info_active::iterator	it;
		rtr_info_t*			rtr_info;

		for (it = index->rtr_track->rtr_active->begin();
		     it != index->rtr_track->rtr_active->end(); ++it) {
			rtr_info = *it;

			rtr_info->index = NULL;
		}

		mutex_destroy(&index->rtr_ssn.mutex);
		mutex_destroy(&index->rtr_track->rtr_active_mutex);
		UT_DELETE(index->rtr_track->rtr_active);
	}
#endif /* !UNIV_HOTBACKUP */

	index->rtr_srs.reset();

	mem_heap_free(index->heap);
}
#ifndef UNIV_HOTBACKUP

/** Create a temporary tablename like "#sql-ibtid-inc" where
  tid = the Table ID
  inc = a randomly initialized number that is incremented for each file
The table ID is a 64 bit integer, can use up to 20 digits, and is initialized
at bootstrap. The second number is 32 bits, can use up to 10 digits, and is
initialized at startup to a randomly distributed number. It is hoped that the
combination of these two numbers will provide a reasonably unique temporary
file name.
@param[in]	heap	A memory heap
@param[in]	dbtab	Table name in the form database/table name
@param[in]	id	Table id
@return A unique temporary tablename suitable for InnoDB use */
char*
dict_mem_create_temporary_tablename(
	mem_heap_t*	heap,
	const char*	dbtab,
	table_id_t	id)
{
	size_t		size;
	char*		name;
	const char*	dbend   = strchr(dbtab, '/');
	ut_ad(dbend);
	size_t		dblen   = dbend - dbtab + 1;

	/* Increment a randomly initialized  number for each temp file. */
	os_atomic_increment_uint32(&dict_temp_file_num, 1);

	size = dblen + (sizeof(TEMP_FILE_PREFIX) + 3 + 20 + 1 + 10);
	name = static_cast<char*>(mem_heap_alloc(heap, size));
	memcpy(name, dbtab, dblen);
	snprintf(name + dblen, size - dblen,
		    TEMP_FILE_PREFIX_INNODB UINT64PF "-" UINT32PF,
		    id, dict_temp_file_num);

	return(name);
}

/** Initialize dict memory variables */
void
dict_mem_init(void)
{
	/* Initialize a randomly distributed temporary file number */
	ib_uint32_t	now = static_cast<ib_uint32_t>(ut_time());

	const byte*	buf = reinterpret_cast<const byte*>(&now);

	dict_temp_file_num = ut_crc32(buf, sizeof(now));

	DBUG_PRINT("dict_mem_init",
		   ("Starting Temporary file number is " UINT32PF,
		   dict_temp_file_num));
}

/** Validate the search order in the foreign key set.
@param[in]	fk_set	the foreign key set to be validated
@return true if search order is fine in the set, false otherwise. */
bool
dict_foreign_set_validate(
	const dict_foreign_set&	fk_set)
{
	dict_foreign_not_exists	not_exists(fk_set);

	dict_foreign_set::iterator it = std::find_if(
		fk_set.begin(), fk_set.end(), not_exists);

	if (it == fk_set.end()) {
		return(true);
	}

	dict_foreign_t*	foreign = *it;
	std::cerr << "Foreign key lookup failed: " << *foreign;
	std::cerr << fk_set;
	ut_ad(0);
	return(false);
}

/** Validate the search order in the foreign key sets of the table
(foreign_set and referenced_set).
@param[in]	table	table whose foreign key sets are to be validated
@return true if foreign key sets are fine, false otherwise. */
bool
dict_foreign_set_validate(
	const dict_table_t&	table)
{
	return(dict_foreign_set_validate(table.foreign_set)
	       && dict_foreign_set_validate(table.referenced_set));
}
#endif /* !UNIV_HOTBACKUP */

std::ostream&
operator<< (std::ostream& out, const dict_foreign_t& foreign)
{
	out << "[dict_foreign_t: id='" << foreign.id << "'";

	if (foreign.foreign_table_name != NULL) {
		out << ",for: '" << foreign.foreign_table_name << "'";
	}

	out << "]";
	return(out);
}

std::ostream&
operator<< (std::ostream& out, const dict_foreign_set& fk_set)
{
	out << "[dict_foreign_set:";
	std::for_each(fk_set.begin(), fk_set.end(), dict_foreign_print(out));
	out << "]" << std::endl;
	return(out);
}

