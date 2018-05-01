/*****************************************************************************

Copyright (c) 1996, 2018, Oracle and/or its affiliates. All Rights Reserved.
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

/** @file dict/mem.cc
 Data dictionary memory object creation

 Created 1/8/1996 Heikki Tuuri
 ***********************************************************************/

/** NOTE: The functions in this file should only use functions from
other files in library. The code in this file is used to make a library for
external tools. */

#include <new>

#include "dict0dict.h"
#ifndef UNIV_HOTBACKUP
#include "lock0lock.h"
#endif /* !UNIV_HOTBACKUP */
#include "my_inttypes.h"

/** Append 'name' to 'col_names'.  @see dict_table_t::col_names
 @return new column names array */
const char *dict_add_col_name(
    const char *col_names, /*!< in: existing column names, or
                           NULL */
    ulint cols,            /*!< in: number of existing columns */
    const char *name,      /*!< in: new column name */
    mem_heap_t *heap)      /*!< in: heap */
{
  ulint old_len;
  ulint new_len;
  ulint total_len;
  char *res;

  ut_ad(!cols == !col_names);

  /* Find out length of existing array. */
  if (col_names) {
    const char *s = col_names;
    ulint i;

    for (i = 0; i < cols; i++) {
      s += strlen(s) + 1;
    }

    old_len = s - col_names;
  } else {
    old_len = 0;
  }

  new_len = strlen(name) + 1;
  total_len = old_len + new_len;

  res = static_cast<char *>(mem_heap_alloc(heap, total_len));

  if (old_len > 0) {
    memcpy(res, col_names, old_len);
  }

  memcpy(res + old_len, name, new_len);

  return (res);
}

/** Free a table memory object. */
void dict_mem_table_free(dict_table_t *table) /*!< in: table */
{
  ut_ad(table);
  ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
  ut_d(table->cached = FALSE);

#ifndef UNIV_HOTBACKUP
#ifndef UNIV_LIBRARY
  if (dict_table_has_fts_index(table) ||
      DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID) ||
      DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_ADD_DOC_ID)) {
    if (table->fts) {
      fts_optimize_remove_table(table);

      fts_free(table);
    }
  }

  dict_table_mutex_destroy(table);

  dict_table_autoinc_destroy(table);

  dict_table_stats_latch_destroy(table);

  table->foreign_set.~dict_foreign_set();
  table->referenced_set.~dict_foreign_set();
#endif /* !UNIV_LIBRARY */
#endif /* !UNIV_HOTBACKUP */

  ut_free(table->name.m_name);
  table->name.m_name = NULL;

#ifndef UNIV_HOTBACKUP
#ifndef UNIV_LIBRARY
  /* Clean up virtual index info structures that are registered
  with virtual columns */
  for (ulint i = 0; i < table->n_v_def; i++) {
    dict_v_col_t *vcol = dict_table_get_nth_v_col(table, i);

    UT_DELETE(vcol->v_indexes);
  }
#endif /* !UNIV_LIBRARY */
#endif /* !UNIV_HOTBACKUP */

  if (table->s_cols != NULL) {
    UT_DELETE(table->s_cols);
  }

#ifndef UNIV_HOTBACKUP
  if (table->temp_prebuilt != NULL) {
    ut_ad(table->is_intrinsic());
    UT_DELETE(table->temp_prebuilt);
  }
#endif /* !UNIV_HOTBACKUP */

  mem_heap_free(table->heap);
}

/** Creates a table memory object.
 @return own: table object */
dict_table_t *dict_mem_table_create(
    const char *name, /*!< in: table name */
    space_id_t space, /*!< in: space where the clustered index of
                      the table is placed */
    ulint n_cols,     /*!< in: total number of columns including
                      virtual and non-virtual columns */
    ulint n_v_cols,   /*!< in: number of virtual columns */
    ulint flags,      /*!< in: table flags */
    ulint flags2)     /*!< in: table flags2 */
{
  dict_table_t *table;
  mem_heap_t *heap;

  ut_ad(name);
#ifndef UNIV_HOTBACKUP
  ut_a(dict_tf2_is_valid(flags, flags2));
  ut_a(!(flags2 & DICT_TF2_UNUSED_BIT_MASK));
#endif /* !UNIV_HOTBACKUP */

  heap = mem_heap_create(DICT_HEAP_SIZE);

  table = static_cast<dict_table_t *>(mem_heap_zalloc(heap, sizeof(*table)));

#ifndef UNIV_HOTBACKUP
#ifndef UNIV_LIBRARY
  lock_table_lock_list_init(&table->locks);
#endif /* !UNIV_LIBRARY */
#endif /* !UNIV_HOTBACKUP */

  UT_LIST_INIT(table->indexes, &dict_index_t::indexes);

  table->heap = heap;

  ut_d(table->magic_n = DICT_TABLE_MAGIC_N);

  table->flags = (unsigned int)flags;
  table->flags2 = (unsigned int)flags2;
  table->name.m_name = mem_strdup(name);
  table->space = (unsigned int)space;
  table->dd_space_id = dd::INVALID_OBJECT_ID;
  table->n_t_cols = (unsigned int)(n_cols + table->get_n_sys_cols());
  table->n_v_cols = (unsigned int)(n_v_cols);
  table->n_cols = table->n_t_cols - table->n_v_cols;
  table->n_instant_cols = table->n_cols;

  table->cols = static_cast<dict_col_t *>(
      mem_heap_alloc(heap, table->n_cols * sizeof(dict_col_t)));
  table->v_cols = static_cast<dict_v_col_t *>(
      mem_heap_alloc(heap, n_v_cols * sizeof(*table->v_cols)));

#ifndef UNIV_HOTBACKUP
#ifndef UNIV_LIBRARY
  dict_table_mutex_create_lazy(table);

  /* true means that the stats latch will be enabled -
  dict_table_stats_lock() will not be noop. */
  dict_table_stats_latch_create(table, true);

  table->autoinc_lock =
      static_cast<ib_lock_t *>(mem_heap_alloc(heap, lock_get_size()));

  /* lazy creation of table autoinc latch */
  dict_table_autoinc_create_lazy(table);

  table->version = 0;
  table->autoinc = 0;
  table->autoinc_persisted = 0;
  table->autoinc_field_no = ULINT_UNDEFINED;
  table->sess_row_id = 0;
  table->sess_trx_id = 0;

  /* If the table has an FTS index or we are in the process
  of building one, create the table->fts */
  if (dict_table_has_fts_index(table) ||
      DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID) ||
      DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_ADD_DOC_ID)) {
    table->fts = fts_create(table);
    table->fts->cache = fts_cache_create(table);
  } else {
    table->fts = NULL;
  }

  if (DICT_TF_HAS_SHARED_SPACE(table->flags)) {
    dict_get_and_save_space_name(table, true);
  }

  new (&table->foreign_set) dict_foreign_set();
  new (&table->referenced_set) dict_foreign_set();

#endif /* !UNIV_LIBRARY */
#endif /* !UNIV_HOTBACKUP */
  table->is_dd_table = false;
  table->explicitly_non_lru = false;

  return (table);
}

/** Creates an index memory object.
 @return own: index object */
dict_index_t *dict_mem_index_create(
    const char *table_name, /*!< in: table name */
    const char *index_name, /*!< in: index name */
    ulint space,            /*!< in: space where the index tree is
                            placed, ignored if the index is of
                            the clustered type */
    ulint type,             /*!< in: DICT_UNIQUE,
                            DICT_CLUSTERED, ... ORed */
    ulint n_fields)         /*!< in: number of fields */
{
  dict_index_t *index;
  mem_heap_t *heap;

  ut_ad(table_name && index_name);

  heap = mem_heap_create(DICT_HEAP_SIZE);

  index = static_cast<dict_index_t *>(mem_heap_zalloc(heap, sizeof(*index)));

  dict_mem_fill_index_struct(index, heap, table_name, index_name, space, type,
                             n_fields);

#ifndef UNIV_HOTBACKUP
#ifndef UNIV_LIBRARY
  dict_index_zip_pad_mutex_create_lazy(index);

  if (type & DICT_SPATIAL) {
    mutex_create(LATCH_ID_RTR_SSN_MUTEX, &index->rtr_ssn.mutex);
    index->rtr_track = static_cast<rtr_info_track_t *>(
        mem_heap_alloc(heap, sizeof(*index->rtr_track)));
    mutex_create(LATCH_ID_RTR_ACTIVE_MUTEX,
                 &index->rtr_track->rtr_active_mutex);
    index->rtr_track->rtr_active = UT_NEW_NOKEY(rtr_info_active());
  }
#endif /* !UNIV_LIBRARY */
#endif /* !UNIV_HOTBACKUP */

  return (index);
}

/** Adds a column definition to a table. */
void dict_mem_table_add_col(
    dict_table_t *table, /*!< in: table */
    mem_heap_t *heap,    /*!< in: temporary memory heap, or NULL */
    const char *name,    /*!< in: column name, or NULL */
    ulint mtype,         /*!< in: main datatype */
    ulint prtype,        /*!< in: precise type */
    ulint len)           /*!< in: precision */
{
  dict_col_t *col;
  ulint i;

  ut_ad(table);
  ut_ad(table->magic_n == DICT_TABLE_MAGIC_N);
  ut_ad(!heap == !name);

  ut_ad(!(prtype & DATA_VIRTUAL));

  i = table->n_def++;

  table->n_t_def++;

  if (name) {
    if (table->n_def == table->n_cols) {
      heap = table->heap;
    }
    if (i && !table->col_names) {
      /* All preceding column names are empty. */
      char *s = static_cast<char *>(mem_heap_zalloc(heap, table->n_def));

      table->col_names = s;
    }

    table->col_names = dict_add_col_name(table->col_names, i, name, heap);
  }

  col = table->get_col(i);

  dict_mem_fill_column_struct(col, i, mtype, prtype, len);
}

/** This function populates a dict_col_t memory structure with
 supplied information. */
void dict_mem_fill_column_struct(
    dict_col_t *column, /*!< out: column struct to be
                        filled */
    ulint col_pos,      /*!< in: column position */
    ulint mtype,        /*!< in: main data type */
    ulint prtype,       /*!< in: precise type */
    ulint col_len)      /*!< in: column length */
{
  column->ind = (unsigned int)col_pos;
  column->ord_part = 0;
  column->max_prefix = 0;
  column->mtype = (unsigned int)mtype;
  column->prtype = (unsigned int)prtype;
  column->len = (unsigned int)col_len;
  column->instant_default = nullptr;
#ifndef UNIV_HOTBACKUP
#ifndef UNIV_LIBRARY
  ulint mbminlen;
  ulint mbmaxlen;
  dtype_get_mblen(mtype, prtype, &mbminlen, &mbmaxlen);
  column->set_mbminmaxlen(mbminlen, mbmaxlen);
#endif /* !UNIV_LIBRARY */
#endif /* !UNIV_HOTBACKUP */
}
