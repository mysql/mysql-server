/*****************************************************************************

Copyright (c) 1996, 2015, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2012, Facebook Inc.

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

/******************************************************************//**
@file dict/dict.cc
Data dictionary system

Created 1/8/1996 Heikki Tuuri
***********************************************************************/

/** NOTE: The functions in this file should only use functions from
other files in library. The code in this file is used to make a library for
external tools. */

#include "dict0dict.h"
#include "dict0mem.h"

/*******************************************************************//**
Adds a column to index. */
void
dict_index_add_col(
/*===============*/
	dict_index_t*		index,		/*!< in/out: index */
	const dict_table_t*	table,		/*!< in: table */
	dict_col_t*		col,		/*!< in: column */
	ulint			prefix_len)	/*!< in: column prefix length */
{
	dict_field_t*	field;
	const char*	col_name;

#ifndef UNIV_LIBRARY
	if (dict_col_is_virtual(col)) {
		dict_v_col_t*	v_col = reinterpret_cast<dict_v_col_t*>(col);

		/* When v_col->v_indexes==NULL,
		ha_innobase::commit_inplace_alter_table(commit=true)
		will evict and reload the table definition, and
		v_col->v_indexes will not be NULL for the new table. */
		if (v_col->v_indexes != NULL) {
			/* Register the index with the virtual column index
			list */
			struct dict_v_idx_t	new_idx
				 = {index, index->n_def};

			v_col->v_indexes->push_back(new_idx);

		}

		col_name = dict_table_get_v_col_name_mysql(
			table, dict_col_get_no(col));
	} else
#endif /* !UNIV_LIBRARY */
	{
		col_name = dict_table_get_col_name(table, dict_col_get_no(col));
	}

	dict_mem_index_add_field(index, col_name, prefix_len);

	field = dict_index_get_nth_field(index, index->n_def - 1);

	field->col = col;
	/* DATA_POINT is a special type, whose fixed_len should be:
	1) DATA_MBR_LEN, when it's indexed in R-TREE. In this case,
	it must be the first col to be added.
	2) DATA_POINT_LEN(be equal to fixed size of column), when it's
	indexed in B-TREE,
	3) DATA_POINT_LEN, if a POINT col is the PRIMARY KEY, and we are
	adding the PK col to other B-TREE/R-TREE. */
	/* TODO: We suppose the dimension is 2 now. */
	if (dict_index_is_spatial(index) && DATA_POINT_MTYPE(col->mtype)
	    && index->n_def == 1) {
		field->fixed_len = DATA_MBR_LEN;
	} else {
		field->fixed_len = static_cast<unsigned int>(
					dict_col_get_fixed_size(
					col, dict_table_is_comp(table)));
	}

	if (prefix_len && field->fixed_len > prefix_len) {
		field->fixed_len = (unsigned int) prefix_len;
	}

	/* Long fixed-length fields that need external storage are treated as
	variable-length fields, so that the extern flag can be embedded in
	the length word. */

	if (field->fixed_len > DICT_MAX_FIXED_COL_LEN) {
		field->fixed_len = 0;
	}
#if DICT_MAX_FIXED_COL_LEN != 768
	/* The comparison limit above must be constant.  If it were
	changed, the disk format of some fixed-length columns would
	change, which would be a disaster. */
# error "DICT_MAX_FIXED_COL_LEN != 768"
#endif

	if (!(col->prtype & DATA_NOT_NULL)) {
		index->n_nullable++;
	}
}

/**********************************************************************//**
Returns a column's name.
@return column name. NOTE: not guaranteed to stay valid if table is
modified in any way (columns added, etc.). */
const char*
dict_table_get_col_name(
/*====================*/
	const dict_table_t*	table,	/*!< in: table */
	ulint			col_nr)	/*!< in: column number */
{
	ulint		i;
	const char*	s;

	ut_ad(table);
	ut_ad(col_nr < table->n_def);
	ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);

	s = table->col_names;
	if (s) {
		for (i = 0; i < col_nr; i++) {
			s += strlen(s) + 1;
		}
	}

	return(s);
}
