/*****************************************************************************

Copyright (c) 1996, 2022, Oracle and/or its affiliates.
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

/** @file dict/dict.cc
 Data dictionary system

 Created 1/8/1996 Heikki Tuuri
 ***********************************************************************/

/** NOTE: The functions in this file should only use functions from
other files in library. The code in this file is used to make a library for
external tools. */

#include <stddef.h>

#include "dict0dict.h"
#include "dict0mem.h"

/** Adds a column to index.
@param[in,out]  index           index
@param[in]      table           table
@param[in]      col             column
@param[in]      prefix_len      column prefix length
@param[in]      is_ascending    true=ASC, false=DESC */
void dict_index_add_col(dict_index_t *index, const dict_table_t *table,
                        dict_col_t *col, ulint prefix_len, bool is_ascending) {
  dict_field_t *field;
  const char *col_name = nullptr;

#ifndef UNIV_LIBRARY
  if (col->is_virtual()) {
#ifndef UNIV_HOTBACKUP
    dict_v_col_t *v_col = reinterpret_cast<dict_v_col_t *>(col);

    /* When v_col->v_indexes==NULL,
    ha_innobase::commit_inplace_alter_table(commit=true)
    will evict and reload the table definition, and
    v_col->v_indexes will not be NULL for the new table. */
    if (v_col->v_indexes != nullptr) {
      /* Register the index with the virtual column index
      list */
      struct dict_v_idx_t new_idx = {index, index->n_def};

      v_col->v_indexes->push_back(new_idx);
    }

    col_name = dict_table_get_v_col_name_mysql(table, dict_col_get_no(col));
#else  /* !UNIV_HOTBACKUP */
    /* PRELIMINARY TEMPORARY WORKAROUND: is this ever used? */
    bool not_hotbackup = false;
    ut_a(not_hotbackup);
#endif /* !UNIV_HOTBACKUP */
  } else
#endif /* !UNIV_LIBRARY */
  {
    col_name = table->get_col_name(dict_col_get_no(col));
  }

  index->add_field(col_name, prefix_len, is_ascending);

  field = index->get_field(index->n_def - 1);

  field->col = col;
  /* DATA_POINT is a special type, whose fixed_len should be:
  1) DATA_MBR_LEN, when it's indexed in R-TREE. In this case,
  it must be the first col to be added.
  2) DATA_POINT_LEN(be equal to fixed size of column), when it's
  indexed in B-TREE,
  3) DATA_POINT_LEN, if a POINT col is the PRIMARY KEY, and we are
  adding the PK col to other B-TREE/R-TREE. */
  /* TODO: We suppose the dimension is 2 now. */
  if (dict_index_is_spatial(index) && DATA_POINT_MTYPE(col->mtype) &&
      index->n_def == 1) {
    field->fixed_len = DATA_MBR_LEN;
  } else {
    field->fixed_len = static_cast<unsigned int>(
        col->get_fixed_size(dict_table_is_comp(table)));
  }

  if (prefix_len && field->fixed_len > prefix_len) {
    field->fixed_len = (unsigned int)prefix_len;
  }

  /* Long fixed-length fields that need external storage are treated as
  variable-length fields, so that the extern flag can be embedded in
  the length word. */

  if (field->fixed_len > DICT_MAX_FIXED_COL_LEN) {
    field->fixed_len = 0;
  }

  /* The comparison limit above must be constant.  If it were
  changed, the disk format of some fixed-length columns would
  change, which would be a disaster. */
  static_assert(DICT_MAX_FIXED_COL_LEN == 768, "DICT_MAX_FIXED_COL_LEN != 768");

  /* Skip INSTANT DROP column */
  if (col->is_nullable() && !col->is_instant_dropped()) {
    index->n_nullable++;
  }
}
