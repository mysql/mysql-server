/*****************************************************************************

Copyright (c) 2008, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file api/api0api.cc
 InnoDB Native API

 2008-08-01 Created Sunny Bains
 3/20/2011 Jimmy Yang extracted from Embedded InnoDB
 *******************************************************/

#include <dd/properties.h>
#include <dd/types/tablespace.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>

#include "api0api.h"
#include "api0misc.h"
#include "btr0pcur.h"
#include "dict0crea.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "dict0priv.h"
#include "dict0sdi-decompress.h"
#include "dict0sdi.h"
#include "fsp0fsp.h"
#include "ha_prototypes.h"
#include "lob0lob.h"
#include "lock0lock.h"
#include "lock0types.h"
#include "my_inttypes.h"
#include "pars0pars.h"
#include "rem0cmp.h"
#include "row0ins.h"
#include "row0merge.h"
#include "row0sel.h"
#include "row0upd.h"
#include "row0vers.h"
#include "srv0start.h"
#include "trx0roll.h"

/** configure variable for binlog option with InnoDB APIs */
bool ib_binlog_enabled = FALSE;

/** configure variable for MDL option with InnoDB APIs */
bool ib_mdl_enabled = FALSE;

/** configure variable for disable rowlock with InnoDB APIs */
bool ib_disable_row_lock = FALSE;

/** configure variable for Transaction isolation levels */
ulong ib_trx_level_setting = IB_TRX_READ_UNCOMMITTED;

/** configure variable for background commit interval in seconds */
ulong ib_bk_commit_interval = 0;

/** InnoDB tuple types. */
enum ib_tuple_type_t {
  TPL_TYPE_ROW, /*!< Data row tuple */
  TPL_TYPE_KEY  /*!< Index key tuple */
};

/** Query types supported. */
enum ib_qry_type_t {
  QRY_NON, /*!< None/Sentinel */
  QRY_INS, /*!< Insert operation */
  QRY_UPD, /*!< Update operation */
  QRY_SEL  /*!< Select operation */
};

/** Query graph types. */
struct ib_qry_grph_t {
  que_fork_t *ins; /*!< Innobase SQL query graph used
                   in inserts */
  que_fork_t *upd; /*!< Innobase SQL query graph used
                   in updates or deletes */
  que_fork_t *sel; /*!< dummy query graph used in
                   selects */
};

/** Query node types. */
struct ib_qry_node_t {
  ins_node_t *ins; /*!< Innobase SQL insert node
                   used to perform inserts to the table */
  upd_node_t *upd; /*!< Innobase SQL update node
                   used to perform updates and deletes */
  sel_node_t *sel; /*!< Innobase SQL select node
                   used to perform selects on the table */
};

/** Query processing fields. */
struct ib_qry_proc_t {
  ib_qry_node_t node; /*!< Query node*/

  ib_qry_grph_t grph; /*!< Query graph */
};

/** Cursor instance for traversing tables/indexes. This will eventually
become row_prebuilt_t. */
struct ib_cursor_t {
  mem_heap_t *heap; /*!< Instance heap */

  mem_heap_t *query_heap; /*!< Heap to use for query graphs */

  ib_qry_proc_t q_proc; /*!< Query processing info */

  ib_match_mode_t match_mode; /*!< ib_cursor_moveto match mode */

  MDL_ticket *mdl; /*!< meta-data lock on the table */

  row_prebuilt_t *prebuilt; /*!< For reading rows */

  bool valid_trx; /*!< Valid transaction attached */
};

/** InnoDB table columns used during table and index schema creation. */
struct ib_col_t {
  const char *name; /*!< Name of column */

  ib_col_type_t ib_col_type; /*!< Main type of the column */

  ulint len; /*!< Length of the column */

  ib_col_attr_t ib_col_attr; /*!< Column attributes */
};

/** InnoDB index columns used during index and index schema creation. */
struct ib_key_col_t {
  const char *name; /*!< Name of column */

  ulint prefix_len; /*!< Column index prefix len or 0 */
};

struct ib_table_def_t;

/** InnoDB index schema used during index creation */
struct ib_index_def_t {
  mem_heap_t *heap; /*!< Heap used to build this and all
                    its columns in the list */

  const char *name; /*!< Index name */

  dict_table_t *table; /*!< Parent InnoDB table */

  ib_table_def_t *schema; /*!< Parent table schema that owns
                          this instance */

  ibool clustered; /*!< True if clustered index */

  ibool unique; /*!< True if unique index */

  ib_vector_t *cols; /*!< Vector of columns */

  trx_t *usr_trx; /*!< User transacton covering the
                  DDL operations */
};

/** InnoDB table schema used during table creation */
struct ib_table_def_t {
  mem_heap_t *heap; /*!< Heap used to build this and all
                    its columns in the list */
  const char *name; /*!< Table name */

  ib_tbl_fmt_t ib_tbl_fmt; /*!< Row format */

  ulint page_size; /*!< Page size */

  ib_vector_t *cols; /*!< Vector of columns */

  ib_vector_t *indexes; /*!< Vector of indexes */

  dict_table_t *table; /* Table read from or NULL */
};

/** InnoDB tuple used for key operations. */
struct ib_tuple_t {
  mem_heap_t *heap; /*!< Heap used to build
                    this and for copying
                    the column values. */

  ib_tuple_type_t type; /*!< Tuple discriminitor. */

  const dict_index_t *index; /*!< Index for tuple can be either
                             secondary or cluster index. */

  dtuple_t *ptr; /*!< The internal tuple
                 instance */
};

/** The following counter is used to convey information to InnoDB
about server activity: in case of normal DML ops it is not
sensible to call srv_active_wake_master_thread after each
operation, we only do it every INNOBASE_WAKE_INTERVAL'th step. */

#define INNOBASE_WAKE_INTERVAL 32

/** Check whether the InnoDB persistent cursor is positioned.
 @return IB_true if positioned */
UNIV_INLINE
ib_bool_t ib_btr_cursor_is_positioned(
    btr_pcur_t *pcur) /*!< in: InnoDB persistent cursor */
{
  return (pcur->old_stored && (pcur->pos_state == BTR_PCUR_IS_POSITIONED ||
                               pcur->pos_state == BTR_PCUR_WAS_POSITIONED));
}

/** Find table using table name.
 @return table instance if found */
static dict_table_t *ib_lookup_table_by_name(
    const char *name) /*!< in: table name to lookup */
{
  dict_table_t *table;

  table = dict_table_get_low(name);

  if (table != NULL && table->ibd_file_missing) {
    table = NULL;
  }

  return (table);
}

/** Increments innobase_active_counter and every INNOBASE_WAKE_INTERVALth
 time calls srv_active_wake_master_thread. This function should be used
 when a single database operation may introduce a small need for
 server utility activity, like checkpointing. */
UNIV_INLINE
void ib_wake_master_thread(void) {
  static ulint ib_signal_counter = 0;

  ++ib_signal_counter;

  if ((ib_signal_counter % INNOBASE_WAKE_INTERVAL) == 0) {
    srv_active_wake_master_thread();
  }
}

/** Read the columns from a rec into a tuple. */
static ib_err_t ib_read_tuple(
    const rec_t *rec,      /*!< in: Record to read */
    ib_bool_t page_format, /*!< in: IB_TRUE if compressed format */
    ib_tuple_t *tuple,     /*!< in: tuple to read into */
    ib_tuple_t *cmp_tuple, /*!< in: tuple to compare and stop
                           reading  */
    int mode,              /*!< in: mode determine when to
                           stop read */
    void **rec_buf_list,   /*!< in/out: row buffer */
    ulint *cur_slot,       /*!< in/out: buffer slot being used */
    ulint *used_len)       /*!< in/out: used buf len */
{
  ulint i;
  void *ptr;
  rec_t *copy;
  ulint rec_meta_data;
  ulint n_index_fields;
  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;
  dtuple_t *dtuple = tuple->ptr;
  const dict_index_t *index = tuple->index;
  ulint offset_size;
  byte *next_ptr;
  int cmp;
  ulint match = 0;

  rec_offs_init(offsets_);

  offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED, &tuple->heap);

  rec_meta_data = rec_get_info_bits(rec, page_format);
  dtuple_set_info_bits(dtuple, rec_meta_data);

  offset_size = rec_offs_size(offsets);

  if (cmp_tuple && mode) {
    /* This is a case of "read upto" certain value. Used for
    index scan for "<" or "<=" case */
    cmp =
        cmp_dtuple_rec_with_match(cmp_tuple->ptr, rec, index, offsets, &match);

    if ((mode == IB_CUR_LE && cmp < 0) || (mode == IB_CUR_L && cmp <= 0)) {
      return (DB_END_OF_INDEX);
    }
  }

  if (rec_buf_list && *rec_buf_list) {
    void *rec_buf = rec_buf_list[*cur_slot];

    if ((16384 - *used_len) < offset_size + 8) {
      (*cur_slot) += 1;

      /* Limit the record buffer size to 16 MB */
      if (*cur_slot >= 1024) {
        return (DB_END_OF_INDEX);
      }

      if (rec_buf_list[*cur_slot] == NULL) {
        rec_buf_list[*cur_slot] = malloc(16384);
      }

      rec_buf = rec_buf_list[*cur_slot];

      if (rec_buf == NULL) {
        return (DB_END_OF_INDEX);
      }
      *used_len = 0;
    }

    ptr = ((byte *)rec_buf + *used_len);

    next_ptr = static_cast<byte *>(
        ut_align((byte *)(rec_buf) + *used_len + offset_size + 8, 8));

    *used_len = next_ptr - (byte *)(rec_buf);
  } else {
    /* Make a copy of the rec. */
    ptr = mem_heap_alloc(tuple->heap, offset_size);
  }

  copy = rec_copy(ptr, rec, offsets);

  n_index_fields =
      ut_min(rec_offs_n_fields(offsets), dtuple_get_n_fields(dtuple));

  for (i = 0; i < n_index_fields; ++i) {
    ulint len;
    const byte *data;
    dfield_t *dfield;

    if (tuple->type == TPL_TYPE_ROW) {
      const dict_col_t *col;
      ulint col_no;
      const dict_field_t *index_field;

      index_field = index->get_field(i);
      col = index_field->col;
      col_no = dict_col_get_no(col);

      dfield = dtuple_get_nth_field(dtuple, col_no);
    } else {
      dfield = dtuple_get_nth_field(dtuple, i);
    }

    data = rec_get_nth_field(copy, offsets, i, index, &len);

    /* Fetch and copy any externally stored column. */
    if (rec_offs_nth_extern(offsets, i)) {
      const page_size_t page_size(dict_table_page_size(index->table));

      /** Passing nullptr to the transaction object.  This
      means that partial update of LOB is not supported
      via this interface.*/
      data = lob::btr_rec_copy_externally_stored_field(
          index, copy, offsets, page_size, i, &len, nullptr,
          dict_index_is_sdi(index), tuple->heap);

      ut_a(len != UNIV_SQL_NULL);
    }

    dfield_set_data(dfield, data, len);
  }

  return (DB_SUCCESS);
}

/** Create an InnoDB key tuple.
 @return tuple instance created, or NULL */
static ib_tpl_t ib_key_tuple_new_low(
    const dict_index_t *index, /*!< in: index for which tuple
                               required */
    ulint n_cols,              /*!< in: no. of user defined cols */
    mem_heap_t *heap)          /*!< in: memory heap */
{
  ib_tuple_t *tuple;
  ulint i;
  ulint n_cmp_cols;

  tuple = static_cast<ib_tuple_t *>(mem_heap_alloc(heap, sizeof(*tuple)));

  if (tuple == NULL) {
    mem_heap_free(heap);
    return (NULL);
  }

  tuple->heap = heap;
  tuple->index = index;
  tuple->type = TPL_TYPE_KEY;

  /* Is it a generated clustered index ? */
  if (n_cols == 0) {
    ++n_cols;
  }

  tuple->ptr = dtuple_create(heap, n_cols);

  /* Copy types and set to SQL_NULL. */
  dict_index_copy_types(tuple->ptr, index, n_cols);

  for (i = 0; i < n_cols; i++) {
    dfield_t *dfield;

    dfield = dtuple_get_nth_field(tuple->ptr, i);
    dfield_set_null(dfield);
  }

  n_cmp_cols = dict_index_get_n_ordering_defined_by_user(index);

  dtuple_set_n_fields_cmp(tuple->ptr, n_cmp_cols);

  return ((ib_tpl_t)tuple);
}

/** Create an InnoDB key tuple.
 @return tuple instance created, or NULL */
static ib_tpl_t ib_key_tuple_new(
    const dict_index_t *index, /*!< in: index of tuple */
    ulint n_cols)              /*!< in: no. of user defined cols */
{
  mem_heap_t *heap;

  heap = mem_heap_create(64);

  if (heap == NULL) {
    return (NULL);
  }

  return (ib_key_tuple_new_low(index, n_cols, heap));
}

/** Create an InnoDB row tuple.
 @return tuple instance, or NULL */
static ib_tpl_t ib_row_tuple_new_low(
    const dict_index_t *index, /*!< in: index of tuple */
    ulint n_cols,              /*!< in: no. of cols in tuple */
    mem_heap_t *heap)          /*!< in: memory heap */
{
  ib_tuple_t *tuple;

  tuple = static_cast<ib_tuple_t *>(mem_heap_alloc(heap, sizeof(*tuple)));

  if (tuple == NULL) {
    mem_heap_free(heap);
    return (NULL);
  }

  tuple->heap = heap;
  tuple->index = index;
  tuple->type = TPL_TYPE_ROW;

  tuple->ptr = dtuple_create(heap, n_cols);

  /* Copy types and set to SQL_NULL. */
  dict_table_copy_types(tuple->ptr, index->table);

  return ((ib_tpl_t)tuple);
}

/** Create an InnoDB row tuple.
 @return tuple instance, or NULL */
static ib_tpl_t ib_row_tuple_new(
    const dict_index_t *index, /*!< in: index of tuple */
    ulint n_cols)              /*!< in: no. of cols in tuple */
{
  mem_heap_t *heap;

  heap = mem_heap_create(64);

  if (heap == NULL) {
    return (NULL);
  }

  return (ib_row_tuple_new_low(index, n_cols, heap));
}

/** Begin a transaction.
 @return innobase txn handle */
ib_err_t ib_trx_start(
    ib_trx_t ib_trx,             /*!< in: transaction to restart */
    ib_trx_level_t ib_trx_level, /*!< in: trx isolation level */
    ib_bool_t read_write,        /*!< in: true if read write
                                 transaction */
    ib_bool_t auto_commit,       /*!< in: auto commit after each
                                 single DML */
    void *thd)                   /*!< in: THD */
{
  ib_err_t err = DB_SUCCESS;
  trx_t *trx = (trx_t *)ib_trx;

  ut_a(ib_trx_level <= IB_TRX_SERIALIZABLE);

  trx->api_trx = true;
  trx->api_auto_commit = auto_commit;
  trx->read_write = read_write;

  trx_start_if_not_started(trx, read_write);

  trx->isolation_level = ib_trx_level;

  /* FIXME: This is a place holder, we should add an arg that comes
  from the client. */
  trx->mysql_thd = static_cast<THD *>(thd);

  return (err);
}

/** Begin a transaction. This will allocate a new transaction handle.
 put the transaction in the active state.
 @return innobase txn handle */
ib_trx_t ib_trx_begin(
    ib_trx_level_t ib_trx_level, /*!< in: trx isolation level */
    ib_bool_t read_write,        /*!< in: true if read write
                                 transaction */
    ib_bool_t auto_commit,       /*!< in: auto commit after each
                                 single DML */
    void *thd)                   /*!< in,out: MySQL THD */
{
  trx_t *trx;
  ib_bool_t started;

  trx = trx_allocate_for_mysql();

  started = ib_trx_start(static_cast<ib_trx_t>(trx), ib_trx_level, read_write,
                         auto_commit, thd);
  ut_a(started);

  return (static_cast<ib_trx_t>(trx));
}

/** Check if transaction is read_only
 @return transaction read_only status */
ib_u32_t ib_trx_read_only(ib_trx_t ib_trx) /*!< in: trx handle */
{
  trx_t *trx = (trx_t *)ib_trx;

  return (trx->read_only);
}
/** Get a trx start time.
 @return trx start_time */
ib_u64_t ib_trx_get_start_time(ib_trx_t ib_trx) /*!< in: transaction */
{
  trx_t *trx = (trx_t *)ib_trx;
  return (static_cast<ib_u64_t>(trx->start_time));
}
/** Release the resources of the transaction.
 @return DB_SUCCESS or err code */
ib_err_t ib_trx_release(ib_trx_t ib_trx) /*!< in: trx handle */
{
  trx_t *trx = (trx_t *)ib_trx;

  ut_ad(trx != NULL);
  trx_free_for_mysql(trx);

  return (DB_SUCCESS);
}

/** Commit a transaction. This function will also release the schema
 latches too.
 @return DB_SUCCESS or err code */
ib_err_t ib_trx_commit(ib_trx_t ib_trx) /*!< in: trx handle */
{
  ib_err_t err = DB_SUCCESS;
  trx_t *trx = reinterpret_cast<trx_t *>(ib_trx);

  if (!trx_is_started(trx)) {
    return (err);
  }

  trx_commit(trx);

  return (DB_SUCCESS);
}

/** Rollback a transaction. This function will also release the schema
 latches too.
 @return DB_SUCCESS or err code */
ib_err_t ib_trx_rollback(ib_trx_t ib_trx) /*!< in: trx handle */
{
  ib_err_t err;
  trx_t *trx = (trx_t *)ib_trx;

  err = static_cast<ib_err_t>(trx_rollback_for_mysql(trx));

  /* It should always succeed */
  ut_a(err == DB_SUCCESS);

  return (err);
}

#ifdef _WIN32
/** Convert a string to lower case. */
static void ib_to_lower_case(char *ptr) /*!< string to convert to lower case */
{
  while (*ptr) {
    *ptr = tolower(*ptr);
    ++ptr;
  }
}
#endif /* _WIN32 */

/** Normalizes a table name string. A normalized name consists of the
 database name catenated to '/' and table name. An example:
 test/mytable. On Windows normalization puts both the database name and the
 table name always to lower case. This function can be called for system
 tables and they don't have a database component. For tables that don't have
 a database component, we don't normalize them to lower case on Windows.
 The assumption is that they are system tables that reside in the system
 table space. */
static void ib_normalize_table_name(
    char *norm_name,  /*!< out: normalized name as a
                      null-terminated string */
    const char *name) /*!< in: table name string */
{
  const char *ptr = name;

  /* Scan name from the end */

  ptr += ut_strlen(name) - 1;

  /* Find the start of the table name. */
  while (ptr >= name && *ptr != '\\' && *ptr != '/' && ptr > name) {
    --ptr;
  }

  /* For system tables there is no '/' or dbname. */
  ut_a(ptr >= name);

  if (ptr > name) {
    const char *db_name;
    const char *table_name;

    table_name = ptr + 1;

    --ptr;

    while (ptr >= name && *ptr != '\\' && *ptr != '/') {
      ptr--;
    }

    db_name = ptr + 1;

    memcpy(norm_name, db_name, ut_strlen(name) + 1 - (db_name - name));

    norm_name[table_name - db_name - 1] = '/';
#ifdef _WIN32
    ib_to_lower_case(norm_name);
#endif
  } else {
    ut_strcpy(norm_name, name);
  }
}

/** Get a table id. The caller must have acquired the dictionary mutex.
 @return DB_SUCCESS if found */
static ib_err_t ib_table_get_id_low(
    const char *table_name, /*!< in: table to find */
    ib_id_u64_t *table_id)  /*!< out: table id if found */
{
  dict_table_t *table;
  ib_err_t err = DB_TABLE_NOT_FOUND;

  *table_id = 0;

  table = ib_lookup_table_by_name(table_name);

  if (table != NULL) {
    *table_id = (table->id);

    err = DB_SUCCESS;
  }

  return (err);
}

/** Create an internal cursor instance.
 @return DB_SUCCESS or err code */
static ib_err_t ib_create_cursor(ib_crsr_t *ib_crsr,  /*!< out: InnoDB cursor */
                                 dict_table_t *table, /*!< in: table instance */
                                 dict_index_t *index, /*!< in: index to use */
                                 trx_t *trx)          /*!< in: transaction */
{
  mem_heap_t *heap;
  ib_cursor_t *cursor;
  ib_err_t err = DB_SUCCESS;

  heap = mem_heap_create(sizeof(*cursor) * 2);

  if (heap != NULL) {
    row_prebuilt_t *prebuilt;

    cursor = static_cast<ib_cursor_t *>(mem_heap_zalloc(heap, sizeof(*cursor)));

    cursor->heap = heap;

    cursor->query_heap = mem_heap_create(64);

    if (cursor->query_heap == NULL) {
      mem_heap_free(heap);

      return (DB_OUT_OF_MEMORY);
    }

    cursor->prebuilt = row_create_prebuilt(table, 0);

    prebuilt = cursor->prebuilt;

    prebuilt->trx = trx;

    cursor->valid_trx = TRUE;

    prebuilt->table = table;
    prebuilt->select_lock_type = LOCK_NONE;
    prebuilt->innodb_api = TRUE;

    prebuilt->index = index;

    ut_a(prebuilt->index != NULL);

    if (prebuilt->trx != NULL) {
      ++prebuilt->trx->n_mysql_tables_in_use;

      prebuilt->index_usable = prebuilt->index->is_usable(prebuilt->trx);

      /* Assign a read view if the transaction does
      not have it yet */

      trx_assign_read_view(prebuilt->trx);
    }

    *ib_crsr = (ib_crsr_t)cursor;
  } else {
    err = DB_OUT_OF_MEMORY;
  }

  return (err);
}

/** Create an internal cursor instance on the clustered index.
@param[out]	ib_crsr		InnoDB cursor
@param[in,out]	table		table instance
@param[in,out]	trx		transaction
@return DB_SUCCESS or err code */
static ib_err_t ib_create_cursor_with_clust_index(ib_crsr_t *ib_crsr,
                                                  dict_table_t *table,
                                                  trx_t *trx) {
  dict_index_t *index = table->first_index();

  return (ib_create_cursor(ib_crsr, table, index, trx));
}

/** Open an InnoDB secondary index cursor and return a cursor handle to it.
 @return DB_SUCCESS or err code */
ib_err_t ib_cursor_open_index_using_name(
    ib_crsr_t ib_open_crsr, /*!< in: open/active cursor */
    const char *index_name, /*!< in: secondary index name */
    ib_crsr_t *ib_crsr,     /*!< out,own: InnoDB index cursor */
    int *idx_type,          /*!< out: index is cluster index */
    ib_id_u64_t *idx_id)    /*!< out: index id */
{
  dict_table_t *table;
  dict_index_t *index;
  space_index_t index_id = 0;
  ib_err_t err = DB_TABLE_NOT_FOUND;
  ib_cursor_t *cursor = (ib_cursor_t *)ib_open_crsr;

  *idx_type = 0;
  *idx_id = 0;
  *ib_crsr = NULL;

  table = cursor->prebuilt->table;
  ut_a(table != NULL);

  mutex_enter(&dict_sys->mutex);
  table->acquire();
  mutex_exit(&dict_sys->mutex);

  /* The first index is always the cluster index. */
  index = table->first_index();

  /* Traverse the user defined indexes. */
  while (index != NULL) {
    if (innobase_strcasecmp(index->name, index_name) == 0) {
      index_id = index->id;
      *idx_type = index->type;
      *idx_id = index_id;
      break;
    }
    index = UT_LIST_GET_NEXT(indexes, index);
  }

  if (!index_id) {
    dict_table_close(table, FALSE, FALSE);
    return (DB_ERROR);
  }

  if (index_id > 0) {
    ut_ad(index->id == index_id);
    err = ib_create_cursor(ib_crsr, table, index, cursor->prebuilt->trx);
  }

  if (*ib_crsr != NULL) {
    const ib_cursor_t *cursor;

    cursor = *(ib_cursor_t **)ib_crsr;

    if (cursor->prebuilt->index == NULL) {
      err = ib_cursor_close(*ib_crsr);
      ut_a(err == DB_SUCCESS);
      *ib_crsr = NULL;
    }
  }

  return (err);
}

/** Open an InnoDB table and return a cursor handle to it.
 @return DB_SUCCESS or err code */
ib_err_t ib_cursor_open_table(
    const char *name,   /*!< in: table name */
    ib_trx_t ib_trx,    /*!< in: Current transaction handle
                        can be NULL */
    ib_crsr_t *ib_crsr) /*!< out,own: InnoDB cursor */
{
  ib_err_t err;
  dict_table_t *table;
  char *normalized_name;
  trx_t *trx = static_cast<trx_t *>(ib_trx);
  MDL_ticket *mdl = nullptr;

  normalized_name = static_cast<char *>(ut_malloc_nokey(ut_strlen(name) + 1));
  ib_normalize_table_name(normalized_name, name);

  ut_ad(ib_trx != nullptr);

  if (!ib_schema_lock_is_exclusive(ib_trx)) {
    table = dd_table_open_on_name(trx->mysql_thd, &mdl, normalized_name, false,
                                  DICT_ERR_IGNORE_NONE);
  } else {
    /* NOTE: We do not acquire MySQL metadata lock */
    table = ib_lookup_table_by_name(normalized_name);
  }

  ut_free(normalized_name);
  normalized_name = NULL;

  /* It can happen that another thread has created the table but
  not the cluster index or it's a broken table definition. Refuse to
  open if that's the case. */
  if (table != NULL && table->first_index() == NULL) {
    table = NULL;
  }

  if (table != NULL) {
    err = ib_create_cursor_with_clust_index(ib_crsr, table, (trx_t *)ib_trx);
    if (mdl != nullptr) {
      (*ib_crsr)->mdl = mdl;
    }
  } else {
    err = DB_TABLE_NOT_FOUND;
  }

  return (err);
}

/** Check the table whether it contains virtual columns.
@param[in]	crsr	InnoDB Cursor
@return true if the table contains virtual column else failure. */
ib_bool_t ib_is_virtual_table(ib_crsr_t crsr) {
  return (crsr->prebuilt->table->n_v_cols > 0);
}

/** Free a context struct for a table handle. */
static void ib_qry_proc_free(
    ib_qry_proc_t *q_proc) /*!< in, own: qproc struct */
{
  que_graph_free_recursive(q_proc->grph.ins);
  que_graph_free_recursive(q_proc->grph.upd);
  que_graph_free_recursive(q_proc->grph.sel);

  memset(q_proc, 0x0, sizeof(*q_proc));
}

/** Reset the cursor.
 @return DB_SUCCESS or err code */
ib_err_t ib_cursor_reset(ib_crsr_t ib_crsr) /*!< in/out: InnoDB cursor */
{
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
  row_prebuilt_t *prebuilt = cursor->prebuilt;

  if (cursor->valid_trx && prebuilt->trx != NULL &&
      prebuilt->trx->n_mysql_tables_in_use > 0) {
    --prebuilt->trx->n_mysql_tables_in_use;
  }

  /* The fields in this data structure are allocated from
  the query heap and so need to be reset too. */
  ib_qry_proc_free(&cursor->q_proc);

  mem_heap_empty(cursor->query_heap);

  return (DB_SUCCESS);
}

/** update the cursor with new transactions and also reset the cursor
 @return DB_SUCCESS or err code */
ib_err_t ib_cursor_new_trx(ib_crsr_t ib_crsr, /*!< in/out: InnoDB cursor */
                           ib_trx_t ib_trx)   /*!< in: transaction */
{
  ib_err_t err = DB_SUCCESS;
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
  trx_t *trx = (trx_t *)ib_trx;

  row_prebuilt_t *prebuilt = cursor->prebuilt;

  row_update_prebuilt_trx(prebuilt, trx);

  cursor->valid_trx = TRUE;

  trx_assign_read_view(prebuilt->trx);

  ib_qry_proc_free(&cursor->q_proc);

  mem_heap_empty(cursor->query_heap);

  return (err);
}

/** Commit the transaction in a cursor
 @return DB_SUCCESS or err code */
ib_err_t ib_cursor_commit_trx(ib_crsr_t ib_crsr, /*!< in/out: InnoDB cursor */
                              ib_trx_t ib_trx)   /*!< in: transaction */
{
  ib_err_t err = DB_SUCCESS;
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
#ifdef UNIV_DEBUG
  row_prebuilt_t *prebuilt = cursor->prebuilt;

  ut_ad(prebuilt->trx == (trx_t *)ib_trx);
#endif /* UNIV_DEBUG */
  ib_trx_commit(ib_trx);
  cursor->valid_trx = FALSE;
  return (err);
}

/** Close an InnoDB table and free the cursor.
 @return DB_SUCCESS or err code */
ib_err_t ib_cursor_close(ib_crsr_t ib_crsr) /*!< in,own: InnoDB cursor */
{
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
  row_prebuilt_t *prebuilt;
  trx_t *trx;

  if (!cursor) {
    return (DB_SUCCESS);
  }

  prebuilt = cursor->prebuilt;
  trx = prebuilt->trx;

  ib_qry_proc_free(&cursor->q_proc);

  /* The transaction could have been detached from the cursor. */
  if (cursor->valid_trx && trx != NULL && trx->n_mysql_tables_in_use > 0) {
    --trx->n_mysql_tables_in_use;
  }

  if (cursor->mdl != nullptr) {
    dd_mdl_release(trx->mysql_thd, &cursor->mdl);
  }
  row_prebuilt_free(prebuilt, FALSE);
  cursor->prebuilt = NULL;

  mem_heap_free(cursor->query_heap);
  mem_heap_free(cursor->heap);
  cursor = NULL;

  return (DB_SUCCESS);
}

/** Run the insert query and do error handling.
 @return DB_SUCCESS or error code */
UNIV_INLINE
ib_err_t ib_insert_row_with_lock_retry(
    que_thr_t *thr,       /*!< in: insert query graph */
    ins_node_t *node,     /*!< in: insert node for the query */
    trx_savept_t *savept) /*!< in: savepoint to rollback to
                          in case of an error */
{
  trx_t *trx;
  ib_err_t err;
  ib_bool_t lock_wait;

  bool is_sdi = dict_table_is_sdi(node->table->id);

  trx = thr_get_trx(thr);

  do {
    thr->run_node = node;
    thr->prev_node = node;

    row_ins_step(thr);

    err = trx->error_state;

    if (err != DB_SUCCESS) {
      que_thr_stop_for_mysql(thr);

      thr->lock_state = QUE_THR_LOCK_ROW;
      lock_wait = static_cast<ib_bool_t>(
          ib_handle_errors(&err, trx, thr, savept, is_sdi));
      thr->lock_state = QUE_THR_LOCK_NOLOCK;
    } else {
      lock_wait = FALSE;
    }
  } while (lock_wait);

  return (err);
}

/** Write a row.
 @return DB_SUCCESS or err code */
static ib_err_t ib_execute_insert_query_graph(
    dict_table_t *table,   /*!< in: table where to insert */
    que_fork_t *ins_graph, /*!< in: query graph */
    ins_node_t *node)      /*!< in: insert node */
{
  trx_t *trx;
  que_thr_t *thr;
  trx_savept_t savept;
  ib_err_t err = DB_SUCCESS;

  trx = ins_graph->trx;

  savept = trx_savept_take(trx);

  thr = que_fork_get_first_thr(ins_graph);

  que_thr_move_to_run_state_for_mysql(thr, trx);

  err = ib_insert_row_with_lock_retry(thr, node, &savept);

  if (err == DB_SUCCESS) {
    que_thr_stop_for_mysql_no_error(thr, trx);

    dict_table_n_rows_inc(table);

    srv_stats.n_rows_inserted.inc();
  }

  trx->op_info = "";

  return (err);
}

/** Create an insert query graph node. */
static void ib_insert_query_graph_create(
    ib_cursor_t *cursor) /*!< in: Cursor instance */
{
  ib_qry_proc_t *q_proc = &cursor->q_proc;
  ib_qry_node_t *node = &q_proc->node;
  trx_t *trx = cursor->prebuilt->trx;

  ut_a(trx_is_started(trx));

  if (node->ins == NULL) {
    dtuple_t *row;
    ib_qry_grph_t *grph = &q_proc->grph;
    mem_heap_t *heap = cursor->query_heap;
    dict_table_t *table = cursor->prebuilt->table;

    node->ins = ins_node_create(INS_DIRECT, table, heap);

    node->ins->select = NULL;
    node->ins->values_list = NULL;

    row = dtuple_create(heap, table->get_n_cols());
    dict_table_copy_types(row, table);

    ut_ad(!dict_table_have_virtual_index(table));

    ins_node_set_new_row(node->ins, row);

    grph->ins = static_cast<que_fork_t *>(que_node_get_parent(
        pars_complete_graph_for_exec(node->ins, trx, heap, NULL)));

    grph->ins->state = QUE_FORK_ACTIVE;
  }
}

/** Insert a row to a table.
 @return DB_SUCCESS or err code */
ib_err_t ib_cursor_insert_row(
    ib_crsr_t ib_crsr,     /*!< in/out: InnoDB cursor instance */
    const ib_tpl_t ib_tpl) /*!< in: tuple to insert */
{
  ib_ulint_t i;
  ib_qry_node_t *node;
  ib_qry_proc_t *q_proc;
  ulint n_fields;
  dtuple_t *dst_dtuple;
  ib_err_t err = DB_SUCCESS;
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
  const ib_tuple_t *src_tuple = (const ib_tuple_t *)ib_tpl;

  ib_insert_query_graph_create(cursor);

  ut_ad(src_tuple->type == TPL_TYPE_ROW);

  q_proc = &cursor->q_proc;
  node = &q_proc->node;

  node->ins->state = INS_NODE_ALLOC_ROW_ID;
  dst_dtuple = node->ins->row;

  n_fields = dtuple_get_n_fields(src_tuple->ptr);
  ut_ad(n_fields == dtuple_get_n_fields(dst_dtuple));

  /* Do a shallow copy of the data fields and check for NULL
  constraints on columns. */
  for (i = 0; i < n_fields; i++) {
    ulint mtype;
    dfield_t *src_field;
    dfield_t *dst_field;

    src_field = dtuple_get_nth_field(src_tuple->ptr, i);

    mtype = dtype_get_mtype(dfield_get_type(src_field));

    /* Don't touch the system columns. */
    if (mtype != DATA_SYS) {
      ulint prtype;

      prtype = dtype_get_prtype(dfield_get_type(src_field));

      if ((prtype & DATA_NOT_NULL) && dfield_is_null(src_field)) {
        err = DB_DATA_MISMATCH;
        break;
      }

      dst_field = dtuple_get_nth_field(dst_dtuple, i);
      ut_ad(mtype == dtype_get_mtype(dfield_get_type(dst_field)));

      /* Do a shallow copy. */
      dfield_set_data(dst_field, src_field->data, src_field->len);

      if (dst_field->len != IB_SQL_NULL) {
        UNIV_MEM_ASSERT_RW(dst_field->data, dst_field->len);
      }
    }
  }

  if (err == DB_SUCCESS) {
    err = ib_execute_insert_query_graph(src_tuple->index->table,
                                        q_proc->grph.ins, node->ins);
  }

  ib_wake_master_thread();

  return (err);
}

/** Gets pointer to a prebuilt update vector used in updates.
 @return update vector */
UNIV_INLINE
upd_t *ib_update_vector_create(ib_cursor_t *cursor) /*!< in: current cursor */
{
  trx_t *trx = cursor->prebuilt->trx;
  mem_heap_t *heap = cursor->query_heap;
  dict_table_t *table = cursor->prebuilt->table;
  ib_qry_proc_t *q_proc = &cursor->q_proc;
  ib_qry_grph_t *grph = &q_proc->grph;
  ib_qry_node_t *node = &q_proc->node;

  ut_a(trx_is_started(trx));

  if (node->upd == NULL) {
    node->upd = static_cast<upd_node_t *>(
        row_create_update_node_for_mysql(table, heap));
  }

  ut_ad(!dict_table_have_virtual_index(table));

  grph->upd = static_cast<que_fork_t *>(que_node_get_parent(
      pars_complete_graph_for_exec(node->upd, trx, heap, NULL)));

  grph->upd->state = QUE_FORK_ACTIVE;

  return (node->upd->update);
}

/** Note that a column has changed. */
static void ib_update_col(

    ib_cursor_t *cursor,    /*!< in: current cursor */
    upd_field_t *upd_field, /*!< in/out: update field */
    ulint col_no,           /*!< in: column number */
    dfield_t *dfield)       /*!< in: updated dfield */
{
  ulint data_len;
  dict_table_t *table = cursor->prebuilt->table;
  dict_index_t *index = table->first_index();

  data_len = dfield_get_len(dfield);

  if (data_len == UNIV_SQL_NULL) {
    dfield_set_null(&upd_field->new_val);
  } else {
    dfield_copy_data(&upd_field->new_val, dfield);
  }

  upd_field->exp = NULL;

  upd_field->orig_len = 0;

  upd_field->field_no = dict_col_get_clust_pos(&table->cols[col_no], index);
}

/** Checks which fields have changed in a row and stores the new data
 to an update vector.
 @return DB_SUCCESS or err code */
static ib_err_t ib_calc_diff(
    ib_cursor_t *cursor,         /*!< in: current cursor */
    upd_t *upd,                  /*!< in/out: update vector */
    const ib_tuple_t *old_tuple, /*!< in: Old tuple in table */
    const ib_tuple_t *new_tuple) /*!< in: New tuple to update */
{
  ulint i;
  ulint n_changed = 0;
  ib_err_t err = DB_SUCCESS;
  ulint n_fields = dtuple_get_n_fields(new_tuple->ptr);

  ut_a(old_tuple->type == TPL_TYPE_ROW);
  ut_a(new_tuple->type == TPL_TYPE_ROW);
  ut_a(old_tuple->index->table == new_tuple->index->table);

  for (i = 0; i < n_fields; ++i) {
    ulint mtype;
    ulint prtype;
    upd_field_t *upd_field;
    dfield_t *new_dfield;
    dfield_t *old_dfield;

    new_dfield = dtuple_get_nth_field(new_tuple->ptr, i);
    old_dfield = dtuple_get_nth_field(old_tuple->ptr, i);

    mtype = dtype_get_mtype(dfield_get_type(old_dfield));
    prtype = dtype_get_prtype(dfield_get_type(old_dfield));

    /* Skip the system columns */
    if (mtype == DATA_SYS) {
      continue;

    } else if ((prtype & DATA_NOT_NULL) && dfield_is_null(new_dfield)) {
      err = DB_DATA_MISMATCH;
      break;
    }

    if (dfield_get_len(new_dfield) != dfield_get_len(old_dfield) ||
        (!dfield_is_null(old_dfield) &&
         memcmp(dfield_get_data(new_dfield), dfield_get_data(old_dfield),
                dfield_get_len(old_dfield)) != 0)) {
      upd_field = &upd->fields[n_changed];

      ib_update_col(cursor, upd_field, i, new_dfield);

      ++n_changed;
    }
  }

  if (err == DB_SUCCESS) {
    upd->info_bits = 0;
    upd->n_fields = n_changed;
  }

  return (err);
}

/** Run the update query and do error handling.
 @return DB_SUCCESS or error code */
UNIV_INLINE
ib_err_t ib_update_row_with_lock_retry(
    que_thr_t *thr,       /*!< in: Update query graph */
    upd_node_t *node,     /*!< in: Update node for the query */
    trx_savept_t *savept) /*!< in: savepoint to rollback to
                          in case of an error */

{
  trx_t *trx;
  ib_err_t err;
  ib_bool_t lock_wait;

  bool is_sdi = dict_table_is_sdi(node->table->id);

  trx = thr_get_trx(thr);

  do {
    thr->run_node = node;
    thr->prev_node = node;

    row_upd_step(thr);

    err = trx->error_state;

    if (err != DB_SUCCESS) {
      que_thr_stop_for_mysql(thr);

      if (err != DB_RECORD_NOT_FOUND) {
        thr->lock_state = QUE_THR_LOCK_ROW;

        lock_wait = static_cast<ib_bool_t>(
            ib_handle_errors(&err, trx, thr, savept, is_sdi));

        thr->lock_state = QUE_THR_LOCK_NOLOCK;
      } else {
        lock_wait = FALSE;
      }
    } else {
      lock_wait = FALSE;
    }
  } while (lock_wait);

  return (err);
}

/** Does an update or delete of a row.
 @return DB_SUCCESS or err code */
UNIV_INLINE
ib_err_t ib_execute_update_query_graph(
    ib_cursor_t *cursor, /*!< in: Cursor instance */
    btr_pcur_t *pcur)    /*!< in: Btree persistent cursor */
{
  ib_err_t err;
  que_thr_t *thr;
  upd_node_t *node;
  trx_savept_t savept;
  trx_t *trx = cursor->prebuilt->trx;
  dict_table_t *table = cursor->prebuilt->table;
  ib_qry_proc_t *q_proc = &cursor->q_proc;

  /* The transaction must be running. */
  ut_a(trx_is_started(trx));

  node = q_proc->node.upd;

  ut_a(pcur->btr_cur.index->is_clustered());
  btr_pcur_copy_stored_position(node->pcur, pcur);

  ut_a(node->pcur->rel_pos == BTR_PCUR_ON);

  savept = trx_savept_take(trx);

  thr = que_fork_get_first_thr(q_proc->grph.upd);

  node->state = UPD_NODE_UPDATE_CLUSTERED;

  que_thr_move_to_run_state_for_mysql(thr, trx);

  err = ib_update_row_with_lock_retry(thr, node, &savept);

  if (err == DB_SUCCESS) {
    que_thr_stop_for_mysql_no_error(thr, trx);

    if (node->is_delete) {
      dict_table_n_rows_dec(table);

      srv_stats.n_rows_deleted.inc();
    } else {
      srv_stats.n_rows_updated.inc();
    }

  } else if (err == DB_RECORD_NOT_FOUND) {
    trx->error_state = DB_SUCCESS;
  }

  trx->op_info = "";

  return (err);
}

/** Update a row in a table.
 @return DB_SUCCESS or err code */
ib_err_t ib_cursor_update_row(
    ib_crsr_t ib_crsr,         /*!< in: InnoDB cursor instance */
    const ib_tpl_t ib_old_tpl, /*!< in: Old tuple in table */
    const ib_tpl_t ib_new_tpl) /*!< in: New tuple to update */
{
  upd_t *upd;
  ib_err_t err;
  btr_pcur_t *pcur;
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
  row_prebuilt_t *prebuilt = cursor->prebuilt;
  const ib_tuple_t *old_tuple = (const ib_tuple_t *)ib_old_tpl;
  const ib_tuple_t *new_tuple = (const ib_tuple_t *)ib_new_tpl;

  if (prebuilt->index->is_clustered()) {
    pcur = cursor->prebuilt->pcur;
  } else if (prebuilt->need_to_access_clustered) {
    pcur = cursor->prebuilt->clust_pcur;
  } else {
    return (DB_ERROR);
  }

  ut_a(old_tuple->type == TPL_TYPE_ROW);
  ut_a(new_tuple->type == TPL_TYPE_ROW);

  upd = ib_update_vector_create(cursor);

  err = ib_calc_diff(cursor, upd, old_tuple, new_tuple);

  if (err == DB_SUCCESS) {
    /* Note that this is not a delete. */
    cursor->q_proc.node.upd->is_delete = FALSE;

    err = ib_execute_update_query_graph(cursor, pcur);
  }

  ib_wake_master_thread();

  return (err);
}

/** Build the update query graph to delete a row from an index.
 @return DB_SUCCESS or err code */
static ib_err_t ib_delete_row(
    ib_cursor_t *cursor, /*!< in: current cursor */
    btr_pcur_t *pcur,    /*!< in: Btree persistent cursor */
    const rec_t *rec)    /*!< in: record to delete */
{
  ulint i;
  upd_t *upd;
  ib_err_t err;
  ib_tuple_t *tuple;
  ib_tpl_t ib_tpl;
  ulint n_cols;
  upd_field_t *upd_field;
  ib_bool_t page_format;
  dict_table_t *table = cursor->prebuilt->table;
  dict_index_t *index = table->first_index();

  n_cols = dict_index_get_n_ordering_defined_by_user(index);
  ib_tpl = ib_key_tuple_new(index, n_cols);

  if (!ib_tpl) {
    return (DB_OUT_OF_MEMORY);
  }

  tuple = (ib_tuple_t *)ib_tpl;

  upd = ib_update_vector_create(cursor);

  page_format = static_cast<ib_bool_t>(dict_table_is_comp(index->table));

  ib_read_tuple(rec, page_format, tuple, NULL, 0, NULL, NULL, NULL);

  upd->n_fields = ib_tuple_get_n_cols(ib_tpl);

  for (i = 0; i < upd->n_fields; ++i) {
    dfield_t *dfield;

    upd_field = &upd->fields[i];
    dfield = dtuple_get_nth_field(tuple->ptr, i);

    dfield_copy_data(&upd_field->new_val, dfield);

    upd_field->exp = NULL;

    upd_field->orig_len = 0;

    upd->info_bits = 0;

    upd_field->field_no = dict_col_get_clust_pos(&table->cols[i], index);
  }

  /* Note that this is a delete. */
  cursor->q_proc.node.upd->is_delete = TRUE;

  err = ib_execute_update_query_graph(cursor, pcur);

  ib_tuple_delete(ib_tpl);

  return (err);
}

/** Delete a row in a table.
 @return DB_SUCCESS or err code */
ib_err_t ib_cursor_delete_row(
    ib_crsr_t ib_crsr) /*!< in: InnoDB cursor instance */
{
  ib_err_t err;
  btr_pcur_t *pcur;
  dict_index_t *index;
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
  row_prebuilt_t *prebuilt = cursor->prebuilt;

  index = prebuilt->index->table->first_index();

  /* Check whether this is a secondary index cursor */
  if (index != prebuilt->index) {
    if (prebuilt->need_to_access_clustered) {
      pcur = prebuilt->clust_pcur;
    } else {
      return (DB_ERROR);
    }
  } else {
    pcur = prebuilt->pcur;
  }

  if (ib_btr_cursor_is_positioned(pcur)) {
    const rec_t *rec;
    ib_bool_t page_format;
    mtr_t mtr;
    rec_t *copy = NULL;
    byte ptr[UNIV_PAGE_SIZE_MAX];

    page_format = static_cast<ib_bool_t>(dict_table_is_comp(index->table));

    mtr_start(&mtr);

    if (btr_pcur_restore_position(BTR_SEARCH_LEAF, pcur, &mtr)) {
      mem_heap_t *heap = NULL;
      ulint offsets_[REC_OFFS_NORMAL_SIZE];
      ulint *offsets = offsets_;

      rec_offs_init(offsets_);

      rec = btr_pcur_get_rec(pcur);

      /* Since mtr will be commited, the rec
      will not be protected. Make a copy of
      the rec. */
      offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED, &heap);
      ut_ad(rec_offs_size(offsets) < UNIV_PAGE_SIZE_MAX);
      copy = rec_copy(ptr, rec, offsets);
    }

    mtr_commit(&mtr);

    if (copy && !rec_get_deleted_flag(copy, page_format)) {
      err = ib_delete_row(cursor, pcur, copy);
    } else {
      err = DB_RECORD_NOT_FOUND;
    }
  } else {
    err = DB_RECORD_NOT_FOUND;
  }

  ib_wake_master_thread();

  return (err);
}

/** Read current row.
 @return DB_SUCCESS or err code */
ib_err_t ib_cursor_read_row(
    ib_crsr_t ib_crsr,    /*!< in: InnoDB cursor instance */
    ib_tpl_t ib_tpl,      /*!< out: read cols into this tuple */
    ib_tpl_t cmp_tpl,     /*!< in: tuple to compare and stop
                          reading */
    int mode,             /*!< in: mode determine when to
                          stop read */
    void **row_buf,       /*!< in/out: row buffer */
    ib_ulint_t *slot,     /*!< in/out: slot being used */
    ib_ulint_t *used_len) /*!< in/out: buffer len used */
{
  ib_err_t err;
  ib_tuple_t *tuple = (ib_tuple_t *)ib_tpl;
  ib_tuple_t *cmp_tuple = (ib_tuple_t *)cmp_tpl;
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;

  ut_a(trx_is_started(cursor->prebuilt->trx));

  /* When searching with IB_EXACT_MATCH set, row_search_for_mysql()
  will not position the persistent cursor but will copy the record
  found into the row cache. It should be the only entry. */
  if (!ib_cursor_is_positioned(ib_crsr)) {
    err = DB_RECORD_NOT_FOUND;
  } else {
    mtr_t mtr;
    btr_pcur_t *pcur;
    row_prebuilt_t *prebuilt = cursor->prebuilt;

    if (prebuilt->need_to_access_clustered && tuple->type == TPL_TYPE_ROW) {
      pcur = prebuilt->clust_pcur;
    } else {
      pcur = prebuilt->pcur;
    }

    if (pcur == NULL) {
      return (DB_ERROR);
    }

    mtr_start(&mtr);

    if (btr_pcur_restore_position(BTR_SEARCH_LEAF, pcur, &mtr)) {
      const rec_t *rec;
      ib_bool_t page_format;

      page_format =
          static_cast<ib_bool_t>(dict_table_is_comp(tuple->index->table));
      rec = btr_pcur_get_rec(pcur);

      if (!rec_get_deleted_flag(rec, page_format)) {
        if (prebuilt->innodb_api && prebuilt->innodb_api_rec != NULL) {
          rec = prebuilt->innodb_api_rec;
        }
      }

      if (!rec_get_deleted_flag(rec, page_format)) {
        err = ib_read_tuple(rec, page_format, tuple, cmp_tuple, mode, row_buf,
                            (ulint *)slot, (ulint *)used_len);
      } else {
        err = DB_RECORD_NOT_FOUND;
      }

    } else {
      err = DB_RECORD_NOT_FOUND;
    }

    mtr_commit(&mtr);
  }

  return (err);
}

/** Move cursor to the first record in the table.
 @return DB_SUCCESS or err code */
UNIV_INLINE
ib_err_t ib_cursor_position(
    ib_cursor_t *cursor, /*!< in: InnoDB cursor instance */
    ib_srch_mode_t mode) /*!< in: Search mode */
{
  ib_err_t err;
  row_prebuilt_t *prebuilt = cursor->prebuilt;
  unsigned char *buf;

  if (prebuilt->innodb_api) {
    prebuilt->cursor_heap = cursor->heap;
  }
  buf = static_cast<unsigned char *>(ut_malloc_nokey(UNIV_PAGE_SIZE));
  dtuple_set_n_fields(prebuilt->search_tuple, 0);

  /* We want to position at one of the ends, row_search_for_mysql()
  uses the search_tuple fields to work out what to do. */

  err = static_cast<ib_err_t>(row_search_for_mysql(
      buf, static_cast<page_cur_mode_t>(mode), prebuilt, 0, 0));

  ut_free(buf);

  return (err);
}

/** Move cursor to the first record in the table.
 @return DB_SUCCESS or err code */
ib_err_t ib_cursor_first(ib_crsr_t ib_crsr) /*!< in: InnoDB cursor instance */
{
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;

  return (ib_cursor_position(cursor, IB_CUR_G));
}

/** Move cursor to the next user record in the table.
 @return DB_SUCCESS or err code */
ib_err_t ib_cursor_next(ib_crsr_t ib_crsr) /*!< in: InnoDB cursor instance */
{
  ib_err_t err;
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
  row_prebuilt_t *prebuilt = cursor->prebuilt;
  byte buf[UNIV_PAGE_SIZE_MAX];

  if (prebuilt->innodb_api) {
    prebuilt->cursor_heap = cursor->heap;
  }
  /* We want to move to the next record */
  dtuple_set_n_fields(prebuilt->search_tuple, 0);

  err = static_cast<ib_err_t>(
      row_search_for_mysql(buf, PAGE_CUR_G, prebuilt, 0, ROW_SEL_NEXT));

  return (err);
}

/** Search for key.
 @return DB_SUCCESS or err code */
ib_err_t ib_cursor_moveto(ib_crsr_t ib_crsr, /*!< in: InnoDB cursor instance */
                          ib_tpl_t ib_tpl,   /*!< in: Key to search for */
                          ib_srch_mode_t ib_srch_mode, /*!< in: search mode */
                          ib_ulint_t direction) /*!< in: search direction */
{
  ulint i;
  ulint n_fields;
  ib_err_t err = DB_SUCCESS;
  ib_tuple_t *tuple = (ib_tuple_t *)ib_tpl;
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
  row_prebuilt_t *prebuilt = cursor->prebuilt;
  dtuple_t *search_tuple = prebuilt->search_tuple;
  unsigned char *buf;

  ut_a(tuple->type == TPL_TYPE_KEY);

  n_fields = dict_index_get_n_ordering_defined_by_user(prebuilt->index);

  if (n_fields > dtuple_get_n_fields(tuple->ptr)) {
    n_fields = dtuple_get_n_fields(tuple->ptr);
  }

  dtuple_set_n_fields(search_tuple, n_fields);
  dtuple_set_n_fields_cmp(search_tuple, n_fields);

  /* Do a shallow copy */
  for (i = 0; i < n_fields; ++i) {
    dfield_copy(dtuple_get_nth_field(search_tuple, i),
                dtuple_get_nth_field(tuple->ptr, i));
  }

  ut_a(prebuilt->select_lock_type <= LOCK_NUM);

  prebuilt->innodb_api_rec = NULL;

  buf = static_cast<unsigned char *>(ut_malloc_nokey(UNIV_PAGE_SIZE));

  if (prebuilt->innodb_api) {
    prebuilt->cursor_heap = cursor->heap;
  }
  err = static_cast<ib_err_t>(
      row_search_for_mysql(buf, static_cast<page_cur_mode_t>(ib_srch_mode),
                           prebuilt, cursor->match_mode, direction));

  ut_free(buf);

  return (err);
}

/** Set the cursor search mode. */
void ib_cursor_set_match_mode(
    ib_crsr_t ib_crsr,          /*!< in: Cursor instance */
    ib_match_mode_t match_mode) /*!< in: ib_cursor_moveto match mode */
{
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;

  cursor->match_mode = match_mode;
}

/** Get the dfield instance for the column in the tuple.
 @return dfield instance in tuple */
UNIV_INLINE
dfield_t *ib_col_get_dfield(ib_tuple_t *tuple, /*!< in: tuple instance */
                            ulint col_no)      /*!< in: col no. in tuple */
{
  dfield_t *dfield;

  dfield = dtuple_get_nth_field(tuple->ptr, col_no);

  return (dfield);
}

/** Predicate to check whether a column type contains variable length data.
 @return DB_SUCCESS or error code */
UNIV_INLINE
ib_err_t ib_col_is_capped(const dtype_t *dtype) /*!< in: column type */
{
  return (static_cast<ib_err_t>((dtype_get_mtype(dtype) == DATA_VARCHAR ||
                                 dtype_get_mtype(dtype) == DATA_CHAR ||
                                 dtype_get_mtype(dtype) == DATA_MYSQL ||
                                 dtype_get_mtype(dtype) == DATA_VARMYSQL ||
                                 dtype_get_mtype(dtype) == DATA_FIXBINARY ||
                                 dtype_get_mtype(dtype) == DATA_BINARY ||
                                 dtype_get_mtype(dtype) == DATA_POINT) &&
                                dtype_get_len(dtype) > 0));
}

/** Set a column of the tuple. Make a copy using the tuple's heap.
 @return DB_SUCCESS or error code */
ib_err_t ib_col_set_value(ib_tpl_t ib_tpl,    /*!< in: tuple instance */
                          ib_ulint_t col_no,  /*!< in: column index in tuple */
                          const void *src,    /*!< in: data value */
                          ib_ulint_t len,     /*!< in: data value len */
                          ib_bool_t need_cpy) /*!< in: if need memcpy */
{
  const dtype_t *dtype;
  dfield_t *dfield;
  void *dst = NULL;
  ib_tuple_t *tuple = (ib_tuple_t *)ib_tpl;
  ulint col_len;

  dfield = ib_col_get_dfield(tuple, col_no);

  /* User wants to set the column to NULL. */
  if (len == IB_SQL_NULL) {
    dfield_set_null(dfield);
    return (DB_SUCCESS);
  }

  dtype = dfield_get_type(dfield);
  col_len = dtype_get_len(dtype);

  /* Not allowed to update system columns. */
  if (dtype_get_mtype(dtype) == DATA_SYS) {
    return (DB_DATA_MISMATCH);
  }

  dst = dfield_get_data(dfield);

  /* Since TEXT/CLOB also map to DATA_VARCHAR we need to make an
  exception. Perhaps we need to set the precise type and check
  for that. */
  if (ib_col_is_capped(dtype)) {
    len = ut_min(len, static_cast<ib_ulint_t>(col_len));

    if (dst == NULL || len > dfield_get_len(dfield)) {
      dst = mem_heap_alloc(tuple->heap, col_len);
      ut_a(dst != NULL);
    }
  } else if (dst == NULL || len > dfield_get_len(dfield)) {
    dst = mem_heap_alloc(tuple->heap, len);
  }

  if (dst == NULL) {
    return (DB_OUT_OF_MEMORY);
  }

  switch (dtype_get_mtype(dtype)) {
    case DATA_INT: {
      if (col_len == len) {
        ibool usign;

        usign = dtype_get_prtype(dtype) & DATA_UNSIGNED;
        mach_write_int_type(static_cast<byte *>(dst),
                            static_cast<const byte *>(src), len, usign);

      } else {
        return (DB_DATA_MISMATCH);
      }
      break;
    }

    case DATA_FLOAT:
      if (len == sizeof(float)) {
        mach_float_write(static_cast<byte *>(dst), *(float *)src);
      } else {
        return (DB_DATA_MISMATCH);
      }
      break;

    case DATA_DOUBLE:
      if (len == sizeof(double)) {
        mach_double_write(static_cast<byte *>(dst), *(double *)src);
      } else {
        return (DB_DATA_MISMATCH);
      }
      break;

    case DATA_SYS:
      ut_error;
      break;

    case DATA_CHAR:
      memcpy(dst, src, len);
      memset((byte *)dst + len, 0x20, col_len - len);
      len = col_len;
      break;

    case DATA_POINT:
      memcpy(dst, src, len);
      break;

    case DATA_BLOB:
    case DATA_VAR_POINT:
    case DATA_GEOMETRY:
    case DATA_BINARY:
    case DATA_DECIMAL:
    case DATA_VARCHAR:
    case DATA_FIXBINARY:
      if (need_cpy) {
        memcpy(dst, src, len);
      } else {
        dfield_set_data(dfield, src, len);
        dst = dfield_get_data(dfield);
      }
      break;

    case DATA_MYSQL:
    case DATA_VARMYSQL: {
      ulint cset;
      CHARSET_INFO *cs;
      int error = 0;
      ulint true_len = len;

      /* For multi byte character sets we need to
      calculate the true length of the data. */
      cset = dtype_get_charset_coll(dtype_get_prtype(dtype));
      cs = all_charsets[cset];
      if (cs) {
        uint pos = (uint)(col_len / cs->mbmaxlen);

        if (len > 0 && cs->mbmaxlen > 1) {
          true_len = (ulint)cs->cset->well_formed_len(
              cs, (const char *)src, (const char *)src + len, pos, &error);

          if (true_len < len) {
            len = static_cast<ib_ulint_t>(true_len);
          }
        }
      }

      /* All invalid bytes in data need be truncated.
      If len == 0, means all bytes of the data is invalid.
      In this case, the data will be truncated to empty.*/
      memcpy(dst, src, len);

      /* For DATA_MYSQL, need to pad the unused
      space with spaces. */
      if (dtype_get_mtype(dtype) == DATA_MYSQL) {
        ulint n_chars;

        if (len < col_len) {
          ulint pad_len = col_len - len;

          ut_a(cs != NULL);
          ut_a(!(pad_len % cs->mbminlen));

          cs->cset->fill(cs, (char *)dst + len, pad_len, 0x20 /* space */);
        }

        /* Why we should do below? See function
        row_mysql_store_col_in_innobase_format */

        ut_a(!(dtype_get_len(dtype) % dtype_get_mbmaxlen(dtype)));

        n_chars = dtype_get_len(dtype) / dtype_get_mbmaxlen(dtype);

        /* Strip space padding. */
        while (col_len > n_chars && ((char *)dst)[col_len - 1] == 0x20) {
          col_len--;
        }

        len = static_cast<ib_ulint_t>(col_len);
      }
      break;
    }

    default:
      ut_error;
  }

  if (dst != dfield_get_data(dfield)) {
    dfield_set_data(dfield, dst, len);
  } else {
    dfield_set_len(dfield, len);
  }

  return (DB_SUCCESS);
}

/** Get the size of the data available in a column of the tuple.
 @return bytes avail or IB_SQL_NULL */
ib_ulint_t ib_col_get_len(ib_tpl_t ib_tpl, /*!< in: tuple instance */
                          ib_ulint_t i)    /*!< in: column index in tuple */
{
  const dfield_t *dfield;
  ulint data_len;
  ib_tuple_t *tuple = (ib_tuple_t *)ib_tpl;

  dfield = ib_col_get_dfield(tuple, i);

  data_len = dfield_get_len(dfield);

  return (static_cast<ib_ulint_t>(data_len == UNIV_SQL_NULL ? IB_SQL_NULL
                                                            : data_len));
}

/** Copy a column value from the tuple.
 @return bytes copied or IB_SQL_NULL */
UNIV_INLINE
ib_ulint_t ib_col_copy_value_low(
    ib_tpl_t ib_tpl, /*!< in: tuple instance */
    ib_ulint_t i,    /*!< in: column index in tuple */
    void *dst,       /*!< out: copied data value */
    ib_ulint_t len)  /*!< in: max data value len to copy */
{
  const void *data;
  const dfield_t *dfield;
  ulint data_len;
  ib_tuple_t *tuple = (ib_tuple_t *)ib_tpl;

  dfield = ib_col_get_dfield(tuple, i);

  data = dfield_get_data(dfield);
  data_len = dfield_get_len(dfield);

  if (data_len != UNIV_SQL_NULL) {
    const dtype_t *dtype = dfield_get_type(dfield);

    switch (dtype_get_mtype(dfield_get_type(dfield))) {
      case DATA_INT: {
        ibool usign;
        uintmax_t ret;

        ut_a(data_len == len);

        usign = dtype_get_prtype(dtype) & DATA_UNSIGNED;
        ret = mach_read_int_type(static_cast<const byte *>(data), data_len,
                                 usign);

        if (usign) {
          if (len == 1) {
            *(ib_i8_t *)dst = (ib_i8_t)ret;
          } else if (len == 2) {
            *(ib_i16_t *)dst = (ib_i16_t)ret;
          } else if (len == 4) {
            *(ib_i32_t *)dst = (ib_i32_t)ret;
          } else {
            *(ib_i64_t *)dst = (ib_i64_t)ret;
          }
        } else {
          if (len == 1) {
            *(ib_u8_t *)dst = (ib_i8_t)ret;
          } else if (len == 2) {
            *(ib_u16_t *)dst = (ib_i16_t)ret;
          } else if (len == 4) {
            *(ib_u32_t *)dst = (ib_i32_t)ret;
          } else {
            *(ib_u64_t *)dst = (ib_i64_t)ret;
          }
        }

        break;
      }
      case DATA_FLOAT:
        if (len == data_len) {
          float f;

          ut_a(data_len == sizeof(f));
          f = mach_float_read(static_cast<const byte *>(data));
          memcpy(dst, &f, sizeof(f));
        } else {
          data_len = 0;
        }
        break;
      case DATA_DOUBLE:
        if (len == data_len) {
          double d;

          ut_a(data_len == sizeof(d));
          d = mach_double_read(static_cast<const byte *>(data));
          memcpy(dst, &d, sizeof(d));
        } else {
          data_len = 0;
        }
        break;
      default:
        data_len = ut_min(data_len, len);
        memcpy(dst, data, data_len);
    }
  } else {
    data_len = IB_SQL_NULL;
  }

  return (static_cast<ib_ulint_t>(data_len));
}

/** Copy a column value from the tuple.
 @return bytes copied or IB_SQL_NULL */
ib_ulint_t ib_col_copy_value(
    ib_tpl_t ib_tpl, /*!< in: tuple instance */
    ib_ulint_t i,    /*!< in: column index in tuple */
    void *dst,       /*!< out: copied data value */
    ib_ulint_t len)  /*!< in: max data value len to copy */
{
  return (ib_col_copy_value_low(ib_tpl, i, dst, len));
}

/** Get the InnoDB column attribute from the internal column precise type.
 @return precise type in api format */
UNIV_INLINE
ib_col_attr_t ib_col_get_attr(ulint prtype) /*!< in: column definition */
{
  ib_col_attr_t attr = IB_COL_NONE;

  if (prtype & DATA_UNSIGNED) {
    attr = static_cast<ib_col_attr_t>(attr | IB_COL_UNSIGNED);
  }

  if (prtype & DATA_NOT_NULL) {
    attr = static_cast<ib_col_attr_t>(attr | IB_COL_NOT_NULL);
  }

  return (attr);
}

/** Get a column name from the tuple.
 @return name of the column */
const char *ib_col_get_name(
    ib_crsr_t ib_crsr, /*!< in: InnoDB cursor instance */
    ib_ulint_t i)      /*!< in: column index in tuple */
{
  const char *name;
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
  dict_table_t *table = cursor->prebuilt->table;
  dict_col_t *col = table->get_col(i);
  ulint col_no = dict_col_get_no(col);

  name = table->get_col_name(col_no);

  return (name);
}

/** Get an index field name from the cursor.
 @return name of the field */
const char *ib_get_idx_field_name(
    ib_crsr_t ib_crsr, /*!< in: InnoDB cursor instance */
    ib_ulint_t i)      /*!< in: column index in tuple */
{
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
  dict_index_t *index = cursor->prebuilt->index;
  dict_field_t *field;

  if (index) {
    field = cursor->prebuilt->index->get_field(i);

    if (field) {
      return (field->name);
    }
  }

  return (NULL);
}

/** Get a column type, length and attributes from the tuple.
 @return len of column data */
UNIV_INLINE
ib_ulint_t ib_col_get_meta_low(
    ib_tpl_t ib_tpl,            /*!< in: tuple instance */
    ib_ulint_t i,               /*!< in: column index in tuple */
    ib_col_meta_t *ib_col_meta) /*!< out: column meta data */
{
  ib_u16_t prtype;
  const dfield_t *dfield;
  ulint data_len;
  ib_tuple_t *tuple = (ib_tuple_t *)ib_tpl;

  dfield = ib_col_get_dfield(tuple, i);

  data_len = dfield_get_len(dfield);

  /* We assume 1-1 mapping between the ENUM and internal type codes. */
  ib_col_meta->type =
      static_cast<ib_col_type_t>(dtype_get_mtype(dfield_get_type(dfield)));

  ib_col_meta->type_len =
      static_cast<ib_u32_t>(dtype_get_len(dfield_get_type(dfield)));

  prtype = (ib_u16_t)dtype_get_prtype(dfield_get_type(dfield));

  ib_col_meta->attr = ib_col_get_attr(prtype);
  ib_col_meta->client_type = prtype & DATA_MYSQL_TYPE_MASK;

  return (static_cast<ib_ulint_t>(data_len));
}

/** Read a signed int 8 bit column from an InnoDB tuple. */
UNIV_INLINE
ib_err_t ib_tuple_check_int(ib_tpl_t ib_tpl, /*!< in: InnoDB tuple */
                            ib_ulint_t i,    /*!< in: column number */
                            ib_bool_t usign, /*!< in: true if unsigned */
                            ulint size)      /*!< in: size of integer */
{
  ib_col_meta_t ib_col_meta;

  ib_col_get_meta_low(ib_tpl, i, &ib_col_meta);

  if (ib_col_meta.type != IB_INT) {
    return (DB_DATA_MISMATCH);
  } else if (ib_col_meta.type_len == IB_SQL_NULL) {
    return (DB_UNDERFLOW);
  } else if (ib_col_meta.type_len != size) {
    return (DB_DATA_MISMATCH);
  } else if ((ib_col_meta.attr & IB_COL_UNSIGNED) && !usign) {
    return (DB_DATA_MISMATCH);
  }

  return (DB_SUCCESS);
}

/** Read a signed int 8 bit column from an InnoDB tuple.
 @return DB_SUCCESS or error */
ib_err_t ib_tuple_read_i8(ib_tpl_t ib_tpl, /*!< in: InnoDB tuple */
                          ib_ulint_t i,    /*!< in: column number */
                          ib_i8_t *ival)   /*!< out: integer value */
{
  ib_err_t err;

  err = ib_tuple_check_int(ib_tpl, i, IB_FALSE, sizeof(*ival));

  if (err == DB_SUCCESS) {
    ib_col_copy_value_low(ib_tpl, i, ival, sizeof(*ival));
  }

  return (err);
}

/** Read an unsigned int 8 bit column from an InnoDB tuple.
 @return DB_SUCCESS or error */
ib_err_t ib_tuple_read_u8(ib_tpl_t ib_tpl, /*!< in: InnoDB tuple */
                          ib_ulint_t i,    /*!< in: column number */
                          ib_u8_t *ival)   /*!< out: integer value */
{
  ib_err_t err;

  err = ib_tuple_check_int(ib_tpl, i, IB_TRUE, sizeof(*ival));

  if (err == DB_SUCCESS) {
    ib_col_copy_value_low(ib_tpl, i, ival, sizeof(*ival));
  }

  return (err);
}

/** Read a signed int 16 bit column from an InnoDB tuple.
 @return DB_SUCCESS or error */
ib_err_t ib_tuple_read_i16(ib_tpl_t ib_tpl, /*!< in: InnoDB tuple */
                           ib_ulint_t i,    /*!< in: column number */
                           ib_i16_t *ival)  /*!< out: integer value */
{
  ib_err_t err;

  err = ib_tuple_check_int(ib_tpl, i, FALSE, sizeof(*ival));

  if (err == DB_SUCCESS) {
    ib_col_copy_value_low(ib_tpl, i, ival, sizeof(*ival));
  }

  return (err);
}

/** Read an unsigned int 16 bit column from an InnoDB tuple.
 @return DB_SUCCESS or error */
ib_err_t ib_tuple_read_u16(ib_tpl_t ib_tpl, /*!< in: InnoDB tuple */
                           ib_ulint_t i,    /*!< in: column number */
                           ib_u16_t *ival)  /*!< out: integer value */
{
  ib_err_t err;

  err = ib_tuple_check_int(ib_tpl, i, IB_TRUE, sizeof(*ival));

  if (err == DB_SUCCESS) {
    ib_col_copy_value_low(ib_tpl, i, ival, sizeof(*ival));
  }

  return (err);
}

/** Read a signed int 32 bit column from an InnoDB tuple.
 @return DB_SUCCESS or error */
ib_err_t ib_tuple_read_i32(ib_tpl_t ib_tpl, /*!< in: InnoDB tuple */
                           ib_ulint_t i,    /*!< in: column number */
                           ib_i32_t *ival)  /*!< out: integer value */
{
  ib_err_t err;

  err = ib_tuple_check_int(ib_tpl, i, FALSE, sizeof(*ival));

  if (err == DB_SUCCESS) {
    ib_col_copy_value_low(ib_tpl, i, ival, sizeof(*ival));
  }

  return (err);
}

/** Read an unsigned int 32 bit column from an InnoDB tuple.
 @return DB_SUCCESS or error */
ib_err_t ib_tuple_read_u32(ib_tpl_t ib_tpl, /*!< in: InnoDB tuple */
                           ib_ulint_t i,    /*!< in: column number */
                           ib_u32_t *ival)  /*!< out: integer value */
{
  ib_err_t err;

  err = ib_tuple_check_int(ib_tpl, i, IB_TRUE, sizeof(*ival));

  if (err == DB_SUCCESS) {
    ib_col_copy_value_low(ib_tpl, i, ival, sizeof(*ival));
  }

  return (err);
}

/** Read a signed int 64 bit column from an InnoDB tuple.
 @return DB_SUCCESS or error */
ib_err_t ib_tuple_read_i64(ib_tpl_t ib_tpl, /*!< in: InnoDB tuple */
                           ib_ulint_t i,    /*!< in: column number */
                           ib_i64_t *ival)  /*!< out: integer value */
{
  ib_err_t err;

  err = ib_tuple_check_int(ib_tpl, i, FALSE, sizeof(*ival));

  if (err == DB_SUCCESS) {
    ib_col_copy_value_low(ib_tpl, i, ival, sizeof(*ival));
  }

  return (err);
}

/** Read an unsigned int 64 bit column from an InnoDB tuple.
 @return DB_SUCCESS or error */
ib_err_t ib_tuple_read_u64(ib_tpl_t ib_tpl, /*!< in: InnoDB tuple */
                           ib_ulint_t i,    /*!< in: column number */
                           ib_u64_t *ival)  /*!< out: integer value */
{
  ib_err_t err;

  err = ib_tuple_check_int(ib_tpl, i, IB_TRUE, sizeof(*ival));

  if (err == DB_SUCCESS) {
    ib_col_copy_value_low(ib_tpl, i, ival, sizeof(*ival));
  }

  return (err);
}

/** Get a column value pointer from the tuple.
 @return NULL or pointer to buffer */
const void *ib_col_get_value(ib_tpl_t ib_tpl, /*!< in: tuple instance */
                             ib_ulint_t i)    /*!< in: column index in tuple */
{
  const void *data;
  const dfield_t *dfield;
  ulint data_len;
  ib_tuple_t *tuple = (ib_tuple_t *)ib_tpl;

  dfield = ib_col_get_dfield(tuple, i);

  data = dfield_get_data(dfield);
  data_len = dfield_get_len(dfield);

  return (data_len != UNIV_SQL_NULL ? data : NULL);
}

/** Get a column type, length and attributes from the tuple.
 @return len of column data */
ib_ulint_t ib_col_get_meta(
    ib_tpl_t ib_tpl,            /*!< in: tuple instance */
    ib_ulint_t i,               /*!< in: column index in tuple */
    ib_col_meta_t *ib_col_meta) /*!< out: column meta data */
{
  return (ib_col_get_meta_low(ib_tpl, i, ib_col_meta));
}

/** "Clear" or reset an InnoDB tuple. We free the heap and recreate the tuple.
 @return new tuple, or NULL */
ib_tpl_t ib_tuple_clear(ib_tpl_t ib_tpl) /*!< in,own: tuple (will be freed) */
{
  const dict_index_t *index;
  ulint n_cols;
  ib_tuple_t *tuple = (ib_tuple_t *)ib_tpl;
  ib_tuple_type_t type = tuple->type;
  mem_heap_t *heap = tuple->heap;

  index = tuple->index;
  n_cols = dtuple_get_n_fields(tuple->ptr);

  mem_heap_empty(heap);

  if (type == TPL_TYPE_ROW) {
    return (ib_row_tuple_new_low(index, n_cols, heap));
  } else {
    return (ib_key_tuple_new_low(index, n_cols, heap));
  }
}

/** Create a new cluster key search tuple and copy the contents of  the
 secondary index key tuple columns that refer to the cluster index record
 to the cluster key. It does a deep copy of the column data.
 @return DB_SUCCESS or error code */
ib_err_t ib_tuple_get_cluster_key(
    ib_crsr_t ib_crsr,         /*!< in: secondary index cursor */
    ib_tpl_t *ib_dst_tpl,      /*!< out,own: destination tuple */
    const ib_tpl_t ib_src_tpl) /*!< in: source tuple */
{
  ulint i;
  ulint n_fields;
  ib_err_t err = DB_SUCCESS;
  ib_tuple_t *dst_tuple = NULL;
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
  ib_tuple_t *src_tuple = (ib_tuple_t *)ib_src_tpl;
  dict_index_t *clust_index;

  clust_index = cursor->prebuilt->table->first_index();

  /* We need to ensure that the src tuple belongs to the same table
  as the open cursor and that it's not a tuple for a cluster index. */
  if (src_tuple->type != TPL_TYPE_KEY) {
    return (DB_ERROR);
  } else if (src_tuple->index->table != cursor->prebuilt->table) {
    return (DB_DATA_MISMATCH);
  } else if (src_tuple->index == clust_index) {
    return (DB_ERROR);
  }

  /* Create the cluster index key search tuple. */
  *ib_dst_tpl = ib_clust_search_tuple_create(ib_crsr);

  if (!*ib_dst_tpl) {
    return (DB_OUT_OF_MEMORY);
  }

  dst_tuple = (ib_tuple_t *)*ib_dst_tpl;
  ut_a(dst_tuple->index == clust_index);

  n_fields = dict_index_get_n_unique(dst_tuple->index);

  /* Do a deep copy of the data fields. */
  for (i = 0; i < n_fields; i++) {
    ulint pos;
    dfield_t *src_field;
    dfield_t *dst_field;

    pos = dict_index_get_nth_field_pos(src_tuple->index, dst_tuple->index, i);

    ut_a(pos != ULINT_UNDEFINED);

    src_field = dtuple_get_nth_field(src_tuple->ptr, pos);
    dst_field = dtuple_get_nth_field(dst_tuple->ptr, i);

    if (!dfield_is_null(src_field)) {
      UNIV_MEM_ASSERT_RW(src_field->data, src_field->len);

      dst_field->data =
          mem_heap_dup(dst_tuple->heap, src_field->data, src_field->len);

      dst_field->len = src_field->len;
    } else {
      dfield_set_null(dst_field);
    }
  }

  return (err);
}

/** Create an InnoDB tuple used for index/table search.
 @return own: Tuple for current index */
ib_tpl_t ib_sec_search_tuple_create(
    ib_crsr_t ib_crsr) /*!< in: Cursor instance */
{
  ulint n_cols;
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
  dict_index_t *index = cursor->prebuilt->index;

  n_cols = dict_index_get_n_unique_in_tree(index);
  return (ib_key_tuple_new(index, n_cols));
}

/** Create an InnoDB tuple used for index/table search.
 @return own: Tuple for current index */
ib_tpl_t ib_sec_read_tuple_create(ib_crsr_t ib_crsr) /*!< in: Cursor instance */
{
  ulint n_cols;
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
  dict_index_t *index = cursor->prebuilt->index;

  n_cols = dict_index_get_n_fields(index);
  return (ib_row_tuple_new(index, n_cols));
}

/** Create an InnoDB tuple used for table key operations.
 @return own: Tuple for current table */
ib_tpl_t ib_clust_search_tuple_create(
    ib_crsr_t ib_crsr) /*!< in: Cursor instance */
{
  ulint n_cols;
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
  dict_index_t *index;

  index = cursor->prebuilt->table->first_index();

  n_cols = dict_index_get_n_ordering_defined_by_user(index);
  return (ib_key_tuple_new(index, n_cols));
}

/** Create an InnoDB tuple for table row operations.
 @return own: Tuple for current table */
ib_tpl_t ib_clust_read_tuple_create(
    ib_crsr_t ib_crsr) /*!< in: Cursor instance */
{
  ulint n_cols;
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
  dict_index_t *index;

  index = cursor->prebuilt->table->first_index();

  n_cols = cursor->prebuilt->table->get_n_cols();
  return (ib_row_tuple_new(index, n_cols));
}

/** Return the number of user columns in the tuple definition.
 @return number of user columns */
ib_ulint_t ib_tuple_get_n_user_cols(
    const ib_tpl_t ib_tpl) /*!< in: Tuple for current table */
{
  const ib_tuple_t *tuple = (const ib_tuple_t *)ib_tpl;

  if (tuple->type == TPL_TYPE_ROW) {
    return (static_cast<ib_ulint_t>((tuple->index->table->get_n_user_cols())));
  }

  return (static_cast<ib_ulint_t>(
      dict_index_get_n_ordering_defined_by_user(tuple->index)));
}

/** Return the number of columns in the tuple definition.
 @return number of columns */
ib_ulint_t ib_tuple_get_n_cols(
    const ib_tpl_t ib_tpl) /*!< in: Tuple for table/index */
{
  const ib_tuple_t *tuple = (const ib_tuple_t *)ib_tpl;

  return (static_cast<ib_ulint_t>(dtuple_get_n_fields(tuple->ptr)));
}

/** Destroy an InnoDB tuple. */
void ib_tuple_delete(ib_tpl_t ib_tpl) /*!< in,own: Tuple instance to delete */
{
  ib_tuple_t *tuple = (ib_tuple_t *)ib_tpl;

  if (!ib_tpl) {
    return;
  }

  mem_heap_free(tuple->heap);
}

/** Get a table id. This function will acquire the dictionary mutex.
 @return DB_SUCCESS if found */
ib_err_t ib_table_get_id(const char *table_name, /*!< in: table to find */
                         ib_id_u64_t *table_id)  /*!< out: table id if found */
{
  ib_err_t err;

  dict_mutex_enter_for_mysql();

  err = ib_table_get_id_low(table_name, table_id);

  dict_mutex_exit_for_mysql();

  return (err);
}

/** Check if cursor is positioned.
 @return IB_true if positioned */
ib_bool_t ib_cursor_is_positioned(
    const ib_crsr_t ib_crsr) /*!< in: InnoDB cursor instance */
{
  const ib_cursor_t *cursor = (const ib_cursor_t *)ib_crsr;
  row_prebuilt_t *prebuilt = cursor->prebuilt;

  return (ib_btr_cursor_is_positioned(prebuilt->pcur));
}

/** Checks if the data dictionary is latched in exclusive mode.
 @return true if exclusive latch */
ib_bool_t ib_schema_lock_is_exclusive(
    const ib_trx_t ib_trx) /*!< in: transaction */
{
  const trx_t *trx = (const trx_t *)ib_trx;

  return (trx->dict_operation_lock_mode == RW_X_LATCH);
}

/** Set the Lock an InnoDB cursor/table.
 @return DB_SUCCESS or error code */
ib_err_t ib_cursor_lock(ib_crsr_t ib_crsr,         /*!< in/out: InnoDB cursor */
                        ib_lck_mode_t ib_lck_mode) /*!< in: InnoDB lock mode */
{
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
  row_prebuilt_t *prebuilt = cursor->prebuilt;
  trx_t *trx = prebuilt->trx;
  dict_table_t *table = prebuilt->table;

  return (
      ib_trx_lock_table_with_retry(trx, table, (enum lock_mode)ib_lck_mode));
}

/** Set the Lock mode of the cursor.
 @return DB_SUCCESS or error code */
ib_err_t ib_cursor_set_lock_mode(
    ib_crsr_t ib_crsr,         /*!< in/out: InnoDB cursor */
    ib_lck_mode_t ib_lck_mode) /*!< in: InnoDB lock mode */
{
  ib_err_t err = DB_SUCCESS;
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
  row_prebuilt_t *prebuilt = cursor->prebuilt;

  ut_a(ib_lck_mode <= static_cast<ib_lck_mode_t>(LOCK_NUM));

  if (ib_lck_mode == IB_LOCK_X) {
    err = ib_cursor_lock(ib_crsr, IB_LOCK_IX);
  } else if (ib_lck_mode == IB_LOCK_S) {
    err = ib_cursor_lock(ib_crsr, IB_LOCK_IS);
  }

  if (err == DB_SUCCESS) {
    prebuilt->select_lock_type = (lock_mode)ib_lck_mode;
    ut_a(trx_is_started(prebuilt->trx));
  }

  return (err);
}

/** Set need to access clustered index record. */
void ib_cursor_set_cluster_access(
    ib_crsr_t ib_crsr) /*!< in/out: InnoDB cursor */
{
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;
  row_prebuilt_t *prebuilt = cursor->prebuilt;

  prebuilt->need_to_access_clustered = TRUE;
}

/** Inform the cursor that it's the start of an SQL statement. */
void ib_cursor_stmt_begin(ib_crsr_t ib_crsr) /*!< in: cursor */
{
  ib_cursor_t *cursor = (ib_cursor_t *)ib_crsr;

  cursor->prebuilt->sql_stat_start = TRUE;
}

/** Write a double value to a column.
 @return DB_SUCCESS or error */
ib_err_t ib_tuple_write_double(
    ib_tpl_t ib_tpl, /*!< in/out: tuple to write to */
    int col_no,      /*!< in: column number */
    double val)      /*!< in: value to write */
{
  const dfield_t *dfield;
  ib_tuple_t *tuple = (ib_tuple_t *)ib_tpl;

  dfield = ib_col_get_dfield(tuple, col_no);

  if (dtype_get_mtype(dfield_get_type(dfield)) == DATA_DOUBLE) {
    return (ib_col_set_value(ib_tpl, col_no, &val, sizeof(val), true));
  } else {
    return (DB_DATA_MISMATCH);
  }
}

/** Read a double column value from an InnoDB tuple.
 @return DB_SUCCESS or error */
ib_err_t ib_tuple_read_double(ib_tpl_t ib_tpl,   /*!< in: InnoDB tuple */
                              ib_ulint_t col_no, /*!< in: column number */
                              double *dval)      /*!< out: double value */
{
  ib_err_t err;
  const dfield_t *dfield;
  ib_tuple_t *tuple = (ib_tuple_t *)ib_tpl;

  dfield = ib_col_get_dfield(tuple, col_no);

  if (dtype_get_mtype(dfield_get_type(dfield)) == DATA_DOUBLE) {
    ib_col_copy_value_low(ib_tpl, col_no, dval, sizeof(*dval));
    err = DB_SUCCESS;
  } else {
    err = DB_DATA_MISMATCH;
  }

  return (err);
}

/** Write a float value to a column.
 @return DB_SUCCESS or error */
ib_err_t ib_tuple_write_float(ib_tpl_t ib_tpl, /*!< in/out: tuple to write to */
                              int col_no,      /*!< in: column number */
                              float val)       /*!< in: value to write */
{
  const dfield_t *dfield;
  ib_tuple_t *tuple = (ib_tuple_t *)ib_tpl;

  dfield = ib_col_get_dfield(tuple, col_no);

  if (dtype_get_mtype(dfield_get_type(dfield)) == DATA_FLOAT) {
    return (ib_col_set_value(ib_tpl, col_no, &val, sizeof(val), true));
  } else {
    return (DB_DATA_MISMATCH);
  }
}

/** Read a float value from an InnoDB tuple.
 @return DB_SUCCESS or error */
ib_err_t ib_tuple_read_float(ib_tpl_t ib_tpl,   /*!< in: InnoDB tuple */
                             ib_ulint_t col_no, /*!< in: column number */
                             float *fval)       /*!< out: float value */
{
  ib_err_t err;
  const dfield_t *dfield;
  ib_tuple_t *tuple = (ib_tuple_t *)ib_tpl;

  dfield = ib_col_get_dfield(tuple, col_no);

  if (dtype_get_mtype(dfield_get_type(dfield)) == DATA_FLOAT) {
    ib_col_copy_value_low(ib_tpl, col_no, fval, sizeof(*fval));
    err = DB_SUCCESS;
  } else {
    err = DB_DATA_MISMATCH;
  }

  return (err);
}

/** Return isolation configuration set by "innodb_api_trx_level"
 @return trx isolation level*/
ib_trx_level_t ib_cfg_trx_level() {
  return (static_cast<ib_trx_level_t>(ib_trx_level_setting));
}

/** Return configure value for background commit interval (in seconds)
 @return background commit interval (in seconds) */
ib_ulint_t ib_cfg_bk_commit_interval() {
  return (static_cast<ib_ulint_t>(ib_bk_commit_interval));
}

/** Get generic configure status
 @return configure status*/
int ib_cfg_get_cfg() {
  int cfg_status;

  cfg_status = (ib_binlog_enabled) ? IB_CFG_BINLOG_ENABLED : 0;

  if (ib_mdl_enabled) {
    cfg_status |= IB_CFG_MDL_ENABLED;
  }

  if (ib_disable_row_lock) {
    cfg_status |= IB_CFG_DISABLE_ROWLOCK;
  }

  return (cfg_status);
}

/** Wrapper of ut_strerr() which converts an InnoDB error number to a
 human readable text message.
 @return string, describing the error */
const char *ib_ut_strerr(ib_err_t num) /*!< in: error number */
{
  return (ut_strerr(num));
}

/** Open an InnoDB table and return a cursor handle to it.
 @return DB_SUCCESS or err code */
static ib_err_t ib_cursor_open_table_using_id(
    ib_id_u64_t table_id, /*!< in: table id of table to open */
    ib_trx_t ib_trx,      /*!< in: Current transaction handle
                          can be NULL */
    ib_crsr_t *ib_crsr)   /*!< out,own: InnoDB cursor */
{
  ib_err_t err;
  dict_table_t *table;
  MDL_ticket *mdl = nullptr;

  table = dd_table_open_on_id(table_id, ib_trx->mysql_thd, &mdl, false, true);

  if (table == NULL) {
    return (DB_TABLE_NOT_FOUND);
  }

  err = ib_create_cursor_with_clust_index(ib_crsr, table, (trx_t *)ib_trx);
  (*ib_crsr)->mdl = mdl;

  return (err);
}

/** Create a tuple to search from SDI table
@param[in,out]	ib_crsr		Memcached cursor
@param[in,out]	sdi_key		SDI Key
@return search tuple */
static ib_tpl_t ib_sdi_create_search_tuple(ib_crsr_t ib_crsr,
                                           const dd::sdi_key_t *sdi_key) {
  ut_ad(ib_crsr->prebuilt->index->get_field(0)->fixed_len == dd::SDI_TYPE_LEN);
  ut_ad(ib_crsr->prebuilt->index->get_field(1)->fixed_len == dd::SDI_KEY_LEN);

  ib_tpl_t key_tpl = ib_clust_search_tuple_create(ib_crsr);
  ib_col_set_value(key_tpl, 0, &sdi_key->type, dd::SDI_TYPE_LEN, false);
  ib_col_set_value(key_tpl, 1, &sdi_key->id, dd::SDI_KEY_LEN, false);

  return (key_tpl);
}

/** Create a tuple to insert into  SDI table
@param[in,out]	ib_crsr		Memcached cursor
@param[in]	sdi_key		SDI Key
@param[in]	uncomp_len	uncompressed length of SDI
@param[in]	comp_len	compressed length of SDI
@param[in]	sdi		compressed SDI data
@return insert tuple */
static ib_tpl_t ib_sdi_create_insert_tuple(ib_crsr_t ib_crsr,
                                           const dd::sdi_key_t *sdi_key,
                                           uint32_t uncomp_len,
                                           uint32_t comp_len, const void *sdi) {
  ut_ad(ib_crsr->prebuilt->index->get_field(0)->fixed_len == dd::SDI_TYPE_LEN);
  ut_ad(ib_crsr->prebuilt->index->get_field(1)->fixed_len == dd::SDI_KEY_LEN);

  ib_tpl_t tuple = ib_clust_read_tuple_create(ib_crsr);
  ib_col_set_value(tuple, 0, &sdi_key->type, dd::SDI_TYPE_LEN, false);
  ib_col_set_value(tuple, 1, &sdi_key->id, dd::SDI_KEY_LEN, false);
  ib_col_set_value(tuple, 2, &uncomp_len, 4, false);
  ib_col_set_value(tuple, 3, &comp_len, 4, false);
  ib_col_set_value(tuple, 4, sdi, static_cast<ib_ulint_t>(comp_len), false);
  return (tuple);
}

/** Open SDI table
@param[in]	tablespace_id	tablespace id
@param[in,out]	trx		innodb transaction
@param[in,out]	ib_crsr		memcached cursor
@return DB_SUCCESS if SDI table is opened, else error */
static ib_err_t ib_sdi_open_table(uint32_t tablespace_id, trx_t *trx,
                                  ib_crsr_t *ib_crsr) {
  if (fsp_has_sdi(tablespace_id) != DB_SUCCESS) {
    return (DB_ERROR);
  }

  ib_err_t err = ib_cursor_open_table_using_id(
      dict_sdi_get_table_id(tablespace_id), trx, ib_crsr);

  DBUG_EXECUTE_IF("ib_sdi", if (err != DB_SUCCESS) {
    ib::warn(ER_IB_MSG_1) << "Unable to open SDI dict table for tablespace: "
                          << tablespace_id << " error returned is " << err;
  });
  return (err);
}

/** Insert/Update SDI in tablespace
@param[in]	tablespace_id	tablespace id
@param[in]	ib_sdi_key	SDI key to uniquely identify the tablespace
                                object
@param[in]	uncomp_len	uncompressed length of SDI
@param[in]	comp_len	compressed length of SDI
@param[in]	sdi		compressed SDI to be stored in tablespace
@param[in,out]	trx		innodb transaction
@return DB_SUCCESS if SDI Insert/Update is successful, else error */
dberr_t ib_sdi_set(uint32_t tablespace_id, const ib_sdi_key_t *ib_sdi_key,
                   uint32_t uncomp_len, uint32_t comp_len, const void *sdi,
                   trx_t *trx) {
  ut_ad(ib_sdi_key != NULL);
  ut_ad(sdi != NULL);

  DBUG_EXECUTE_IF("ib_sdi", ib::info(ER_IB_MSG_2)
                                << "ib_sdi: sdi_set: " << tablespace_id
                                << " Key: " << ib_sdi_key->sdi_key->type << " "
                                << ib_sdi_key->sdi_key->id
                                << " sdi_len: " << comp_len;);

  ib_crsr_t ib_crsr = NULL;
  ib_err_t err = ib_sdi_open_table(tablespace_id, trx, &ib_crsr);

  if (err != DB_SUCCESS) {
    return (err);
  }

  ib_tpl_t new_tuple = ib_sdi_create_insert_tuple(ib_crsr, ib_sdi_key->sdi_key,
                                                  uncomp_len, comp_len, sdi);

  ib_cursor_set_lock_mode(ib_crsr, IB_LOCK_X);

  /* Do insert. If row exists, handle the duplicate */
  err = ib_cursor_insert_row(ib_crsr, new_tuple);

  if (err == DB_DUPLICATE_KEY) {
    /* Existing row found. We should update it. */

    /* First check if the new row and old row are same */
    /* We only S-lock the record when doing the comparision. */

    ib_tpl_t key_tpl = ib_sdi_create_search_tuple(ib_crsr, ib_sdi_key->sdi_key);

    ib_cursor_set_match_mode(ib_crsr, IB_EXACT_MATCH);
    ib_cursor_set_lock_mode(ib_crsr, IB_LOCK_S);
    err = ib_cursor_moveto(ib_crsr, key_tpl, IB_CUR_LE, 0);
    ut_ad(err == DB_SUCCESS);

    ut_ad(ib_btr_cursor_is_positioned(ib_crsr->prebuilt->pcur));

    ib_tpl_t old_tuple = ib_clust_read_tuple_create(ib_crsr);
    ib_cursor_stmt_begin(ib_crsr);
    ib_cursor_read_row(ib_crsr, old_tuple, NULL, 0, NULL, NULL, NULL);

    /* Move the cursor to position of the record to update
    and X-latch the record */
    upd_t *upd;
    upd = ib_update_vector_create(ib_crsr);

    err = ib_calc_diff(ib_crsr, upd, old_tuple, new_tuple);
    ut_ad(err == DB_SUCCESS);

    if (upd->n_fields == 0) {
      /* Old row is same as new row */
      err = DB_SUCCESS;
      DBUG_EXECUTE_IF("ib_sdi", ib::info(ER_IB_MSG_3)
                                    << "ib_sdi: sdi_set: Update row:"
                                    << " old row same as new: " << tablespace_id
                                    << " Key: " << ib_sdi_key->sdi_key->type
                                    << " " << ib_sdi_key->sdi_key->id
                                    << " trx: " << trx->id;);

    } else {
      /* We compared the record and there is change. X-lock
      the record */
      ib_cursor_set_match_mode(ib_crsr, IB_EXACT_MATCH);
      ib_cursor_set_lock_mode(ib_crsr, IB_LOCK_X);
      err = ib_cursor_moveto(ib_crsr, key_tpl, IB_CUR_LE, 0);
      ut_ad(err == DB_SUCCESS);

      DBUG_EXECUTE_IF("ib_sdi", ib::info(ER_IB_MSG_4)
                                    << "ib_sdi: sdi_set: Existing row found: "
                                    << tablespace_id
                                    << " Key: " << ib_sdi_key->sdi_key->type
                                    << " " << ib_sdi_key->sdi_key->id
                                    << " trx: " << trx->id;);

      err = ib_cursor_update_row(ib_crsr, old_tuple, new_tuple);

      ut_ad(err == DB_SUCCESS || trx_is_interrupted(trx) ||
            !"sdi_update_failed");
    }

    ib_tuple_delete(old_tuple);
    ib_tuple_delete(key_tpl);

  } else if (err == DB_SUCCESS) {
    DBUG_EXECUTE_IF("ib_sdi",
                    ib::info(ER_IB_MSG_5)
                        << "ib_sdi: sdi_set: insert: " << tablespace_id
                        << " Key: " << ib_sdi_key->sdi_key->type << " "
                        << ib_sdi_key->sdi_key->id << " trx: " << trx->id;);
  } else {
    DBUG_EXECUTE_IF("ib_sdi", ib::warn(ER_IB_MSG_6)
                                  << "ib_sdi: sdi_set: failed for"
                                  << " tablespace_id: " << tablespace_id
                                  << " Key: " << ib_sdi_key->sdi_key->type
                                  << " " << ib_sdi_key->sdi_key->id
                                  << " Error returned: " << err
                                  << " by trx->id: " << trx->id;);

    ut_ad(err == DB_SUCCESS || trx_is_interrupted(trx) || !"sdi_insert_failed");
  }

  ib_tuple_delete(new_tuple);
  ib_cursor_close(ib_crsr);
  return (err);
}

/** Get the SDI keys in a tablespace into vector.
@param[in]	tablespace_id	tablespace id
@param[in,out]	ib_sdi_vector	vector to hold objects with tablespace types
and ids
@param[in,out]	trx		data dictionary transaction
@return DB_SUCCESS if retrieval of SDI kyes is successful, else error */
dberr_t ib_sdi_get_keys(uint32_t tablespace_id, ib_sdi_vector_t *ib_sdi_vector,
                        trx_t *trx) {
  ut_ad(ib_sdi_vector != NULL);
  ut_ad(ib_sdi_vector->sdi_vector->m_vec.empty());

  ib_crsr_t ib_crsr = NULL;
  ib_err_t err = ib_sdi_open_table(tablespace_id, trx, &ib_crsr);

  if (err != DB_SUCCESS) {
    return (err);
  }

  ib_cursor_stmt_begin(ib_crsr);
  err = ib_cursor_first(ib_crsr);
  if (err != DB_SUCCESS) {
    ib_cursor_close(ib_crsr);
    return (err);
  }

  ib_tpl_t tuple = ib_clust_read_tuple_create(ib_crsr);
  do {
    /* Read the current row from cursor position */
    err = ib_cursor_read_row(ib_crsr, tuple, NULL, 0, NULL, NULL, NULL);
    if (err != DB_SUCCESS) {
      break;
    }

    dd::sdi_key_t ts;

    ib_tuple_read_u32(tuple, 0, &ts.type);
    ib_tuple_read_u64(tuple, 1, reinterpret_cast<uint64_t *>(&ts.id));
    ib_sdi_vector->sdi_vector->m_vec.push_back(ts);

  } while (ib_cursor_next(ib_crsr) != DB_END_OF_INDEX);

  ib_tuple_delete(tuple);
  ib_cursor_close(ib_crsr);
  return (err);
}

/** Retrieve SDI from tablespace
@param[in]	tablespace_id	tablespace id
@param[in]	ib_sdi_key	SDI key
@param[in,out]	comp_sdi	in: buffer to hold the SDI BLOB
                                out: compressed SDI retrieved from tablespace
@param[in,out]	comp_sdi_len	in:  Size of memory allocated
                                out: compressed length of SDI
@param[out]	uncomp_sdi_len	out: uncompressed length of SDI
@param[in,out]	trx		innodb transaction
@return DB_SUCCESS if SDI retrieval is successful, else error
in case the passed buffer length is smaller than the actual SDI
DB_OUT_OF_MEMORY is thrown and uncompressed length is set in
uncomp_sdi_len */
dberr_t ib_sdi_get(uint32_t tablespace_id, const ib_sdi_key_t *ib_sdi_key,
                   void *comp_sdi, uint32_t *comp_sdi_len,
                   uint32_t *uncomp_sdi_len, trx_t *trx) {
  ut_ad(ib_sdi_key != NULL);
  ut_ad(comp_sdi != NULL);
  ut_ad(comp_sdi_len != NULL);

  if (comp_sdi_len == NULL || comp_sdi == NULL) {
    return (DB_ERROR);
  }

  DBUG_EXECUTE_IF("ib_sdi", ib::info(ER_IB_MSG_7)
                                << "ib_sdi: sdi_get: " << tablespace_id
                                << " Key: " << ib_sdi_key->sdi_key->type << " "
                                << ib_sdi_key->sdi_key->id
                                << " input_buffer_len " << *comp_sdi_len;

  );

  ib_crsr_t ib_crsr = NULL;
  ib_err_t err = ib_sdi_open_table(tablespace_id, trx, &ib_crsr);

  if (err != DB_SUCCESS) {
    *comp_sdi_len = UINT32_MAX;
    return (err);
  }

  ib_tpl_t key_tpl = ib_sdi_create_search_tuple(ib_crsr, ib_sdi_key->sdi_key);

  ib_cursor_set_match_mode(ib_crsr, IB_EXACT_MATCH);

  err = ib_cursor_moveto(ib_crsr, key_tpl, IB_CUR_GE, 0);
  if (err == DB_SUCCESS) {
    /* Read the current row from cursor position */
    ib_tpl_t tuple = ib_clust_read_tuple_create(ib_crsr);
    ib_cursor_stmt_begin(ib_crsr);
    err = ib_cursor_read_row(ib_crsr, tuple, NULL, 0, NULL, NULL, NULL);
    if (err == DB_SUCCESS) {
      uint32_t buf_len = *comp_sdi_len;
      ib_tuple_read_u32(tuple, 2, uncomp_sdi_len);
      ib_tuple_read_u32(tuple, 3, comp_sdi_len);

      /* If the passed memory is not sufficient, we
      return failure and the actual length of SDI. */
      if (buf_len < *uncomp_sdi_len) {
        ib_tuple_delete(tuple);
        ib_tuple_delete(key_tpl);
        ib_cursor_close(ib_crsr);
        return (DB_OUT_OF_MEMORY);
      }

      ib_col_copy_value(tuple, 4, comp_sdi,
                        static_cast<ib_ulint_t>(*comp_sdi_len));
    }

    ib_tuple_delete(tuple);
  } else {
    DBUG_EXECUTE_IF("ib_sdi",
                    if (err == DB_RECORD_NOT_FOUND) {
                      ib::warn(ER_IB_MSG_8)
                          << "sdi_get: Record not found:"
                          << " tablespace " << tablespace_id
                          << " Key: " << ib_sdi_key->sdi_key->type << " "
                          << ib_sdi_key->sdi_key->id;
                    } else if (err != DB_SUCCESS) {
                      ib::warn(ER_IB_MSG_9)
                          << "sdi_get: Get Failed: tablespace " << tablespace_id
                          << " Key: " << ib_sdi_key->sdi_key->type << " "
                          << ib_sdi_key->sdi_key->id << " error: " << err;
                    });
  }

  ib_tuple_delete(key_tpl);
  ib_cursor_close(ib_crsr);

  if (err != DB_SUCCESS) {
    /* Return sdi_len as UINT32_MAX in case of any other failure
    like searching for non-existent row */
    *comp_sdi_len = UINT32_MAX;
    *uncomp_sdi_len = UINT32_MAX;
  }

  return (err);
}

/** Delete SDI from tablespace
@param[in]	tablespace_id	tablespace id
@param[in]	ib_sdi_key	SDI key to uniquely identify the tablespace
                                object
@param[in,out]	trx		innodb transaction
@return DB_SUCCESS if SDI deletion is successful, else error */
ib_err_t ib_sdi_delete(uint32_t tablespace_id, const ib_sdi_key_t *ib_sdi_key,
                       trx_t *trx) {
  ut_ad(ib_sdi_key != NULL);

  DBUG_EXECUTE_IF("ib_sdi", ib::info(ER_IB_MSG_10)
                                << "ib_sdi: sdi_delete: " << tablespace_id
                                << " Key: " << ib_sdi_key->sdi_key->type << " "
                                << ib_sdi_key->sdi_key->id;);

  ib_crsr_t ib_crsr = NULL;
  ib_err_t err = ib_sdi_open_table(tablespace_id, trx, &ib_crsr);

  if (err != DB_SUCCESS) {
    return (err);
  }

  ib_tpl_t key_tpl = ib_sdi_create_search_tuple(ib_crsr, ib_sdi_key->sdi_key);

  ib_cursor_set_match_mode(ib_crsr, IB_EXACT_MATCH);
  ib_cursor_set_lock_mode(ib_crsr, IB_LOCK_X);
  err = ib_cursor_moveto(ib_crsr, key_tpl, IB_CUR_LE, 0);
  if (err == DB_SUCCESS) {
    ib_cursor_stmt_begin(ib_crsr);
    err = ib_cursor_delete_row(ib_crsr);
  }

#ifdef UNIV_DEBUG
  if (err != DB_SUCCESS && !trx_is_interrupted(trx)) {
    if (err == DB_RECORD_NOT_FOUND) {
      ib::warn(ER_IB_MSG_11) << "sdi_delete failed: Record Doesn't exist:"
                             << " tablespace_id: " << tablespace_id
                             << " Key: " << ib_sdi_key->sdi_key->type << " "
                             << ib_sdi_key->sdi_key->id;
      bool sdi_delete_record_not_found = true;
      ut_ad(!sdi_delete_record_not_found);

    } else {
      ib::warn(ER_IB_MSG_12)
          << "sdi_delete failed: tablespace_id: " << tablespace_id
          << " Key: " << ib_sdi_key->sdi_key->type << " "
          << ib_sdi_key->sdi_key->id << " Error returned: " << err;
      bool sdi_delete_failed = true;
      ut_ad(!sdi_delete_failed);
    }
  }
#endif /* UNIV_DEBUG */

  ib_tuple_delete(key_tpl);
  ib_cursor_close(ib_crsr);
  return (err);
}

/** Create SDI in a tablespace
@param[in]	tablespace_id	InnoDB tablespace id
@return DB_SUCCESS if SDI index creation is successful, else error */
ib_err_t ib_sdi_create(space_id_t tablespace_id) {
  /* Check if the FSP_FLAG_SDI has already been set. If it
  is set, then we assume SDI indexes are already created and
  we don't re-create SDI indexes */
  fil_space_t *space = fil_space_acquire(tablespace_id);
  if (space == NULL) {
    return (DB_ERROR);
  }

  bool has_sdi = FSP_FLAGS_HAS_SDI(space->flags);
#ifdef UNIV_DEBUG
  /* Read page 0 to confirm the SDI flag presence */
  const page_size_t page_size(space->flags);
  mtr_t mtr;

  mtr.start();
  const fsp_header_t *header =
      fsp_get_space_header(tablespace_id, page_size, &mtr);
  mtr.commit();
  ut_ad(mach_read_from_4(FSP_SPACE_FLAGS + header) == space->flags);
#endif /* UNIV_DEBUG */

  if (has_sdi) {
    fil_space_release(space);
    return (DB_SUCCESS);
  }

  ib_err_t err = btr_sdi_create_index(tablespace_id, false);

  fil_space_release(space);
  return (err);
}

/** Drop SDI Index from tablespace. This should be used only when SDI
is corrupted.
@param[in]	tablespace_id	InnoDB tablespace id
@return DB_SUCCESS if dropping of SDI index is successful, else error */
ib_err_t ib_sdi_drop(space_id_t tablespace_id) {
  fil_space_t *space = fil_space_acquire(tablespace_id);
  if (space == NULL) {
    return (DB_ERROR);
  }

  rw_lock_x_lock(&space->latch);

  page_size_t page_size(space->flags);

  mtr_t mtr;

  /* We use separate mtrs because latching IBUF BITMAP Page and
  a B-Tree Index page in same mtr will cause latch violation */
  mtr.start();
  page_no_t root_page_num =
      fsp_sdi_get_root_page_num(tablespace_id, page_size, &mtr);

  mtr.commit();

  mtr.start();
  btr_free_if_exists(page_id_t(tablespace_id, root_page_num), page_size,
                     dict_sdi_get_index_id(), &mtr);
  mtr.commit();

  /* Remove SDI Flag presence from Page 0 */
  mtr.start();

  ulint flags = space->flags & ~FSP_FLAGS_MASK_SDI;

  buf_block_t *block =
      buf_page_get(page_id_t(space->id, 0), page_size, RW_SX_LATCH, &mtr);

  buf_block_dbg_add_level(block, SYNC_FSP_PAGE);
  page_t *page = buf_block_get_frame(block);

  mlog_write_ulint(FSP_HEADER_OFFSET + FSP_SPACE_FLAGS + page, flags,
                   MLOG_4BYTES, &mtr);

  fil_space_set_flags(space, flags);

  mtr.commit();
  rw_lock_x_unlock(&space->latch);
  fil_space_release(space);

  dict_sdi_remove_from_cache(space->id, NULL, false);

  return (DB_SUCCESS);
}

/** Flush SDI in a tablespace. The pages of a SDI Index modified by the
transaction will be flushed to disk.
@param[in]	space_id	tablespace id
@return DB_SUCCESS always */
ib_err_t ib_sdi_flush(space_id_t space_id) { return (DB_SUCCESS); }

#ifdef UNIV_MEMCACHED_SDI
/** Parse string a unsigned long number
@param[in]	num_str		input string which has number
@param[out]	dest_num	Number converted from input string
@return DB_SUCCESS on successful converstion, else DB_ERROR */
static ib_err_t parse_string_to_number(const char *num_str,
                                       uint64_t *dest_num) {
  char *endptr;
  errno = 0;
  unsigned long result = strtoul(num_str, &endptr, 10);
  if (endptr == num_str || *endptr != 0) {
    /* nothing parsed from the string, return error */
    return (DB_ERROR);
  }
  if (result == ULONG_MAX && errno == ERANGE) {
    /* out of range */
    return (DB_ERROR);
  }

  *dest_num = static_cast<uint64_t>(result);
  return (DB_SUCCESS);
}

/** Extracts SDI key from the memcached key. For example if the key is
"sdi_3:4", it parses as type:3, id:4
@param[in]	key_str		Memached key
@param[in,out]	sk		SDI key
@return DB_SUCCESS if SDI key extraction is successful, else error */
static ib_err_t parse_mem_key_to_sdi_key(const char *key_str,
                                         dd::sdi_key_t *sk) {
  /* 25 is sufficient here, the prefix will be
  sdi_number:number:number */
  char key[25];
  char *saveptr1;

  strncpy(key, key_str + strlen("sdi_"), sizeof(key));

  char *type_str = strtok_r(key, ":", &saveptr1);
  char *id_str = strtok_r(NULL, ":", &saveptr1);

  if (id_str == NULL || type_str == NULL) {
    return (DB_ERROR);
  }

  uint64_t number;
  if (parse_string_to_number(type_str, &number) == DB_SUCCESS) {
    sk->type = static_cast<uint32_t>(number);
  } else {
    return (DB_ERROR);
  }

  if (parse_string_to_number(id_str, &number) == DB_SUCCESS) {
    sk->id = number;
  } else {
    return (DB_ERROR);
  }

  return (DB_SUCCESS);
}

/** Wrapper function to retrieve SDI from tablespace
@param[in,out]	crsr		Memcached cursor
@param[in]	key_str		Memcached key
@param[in,out]	sdi		SDI data retrieved
@param[in,out]	sdi_len		Length of SDI data
@return DB_SUCCESS if SDI retrieval is successful, else error */
ib_err_t ib_memc_sdi_get(ib_crsr_t crsr, const char *key_str, void *sdi,
                         uint64_t *sdi_len) {
  uint32_t tablespace_id = crsr->prebuilt->table->space;
  ib_trx_t trx = crsr->prebuilt->trx;
  ib_err_t err;
  ib_sdi_key_t sk;
  dd::sdi_key_t sdi_key;
  ut_ad(trx != NULL);

  sk.sdi_key = &sdi_key;
  err = parse_mem_key_to_sdi_key(key_str, &sdi_key);
  if (err != DB_SUCCESS) {
    return (err);
  }

  ut_ad(*sdi_len < UINT32_MAX);
  uint32_t uncompressed_sdi_len;
  uint32_t compressed_sdi_len = static_cast<uint32_t>(*sdi_len);
  byte *compressed_sdi =
      static_cast<byte *>(ut_malloc_nokey(compressed_sdi_len));

  err = ib_sdi_get(tablespace_id, &sk, compressed_sdi, &compressed_sdi_len,
                   &uncompressed_sdi_len, trx);

  if (err == DB_OUT_OF_MEMORY) {
    *sdi_len = uncompressed_sdi_len;
  } else if (err != DB_SUCCESS) {
    *sdi_len = UINT64_MAX;
  } else {
    *sdi_len = uncompressed_sdi_len;
    /* Decompress the data */
    Sdi_Decompressor decompressor(static_cast<byte *>(sdi),
                                  uncompressed_sdi_len, compressed_sdi,
                                  compressed_sdi_len);
    decompressor.decompress();
  }

  return (err);
}

/** Wrapper function to delete SDI from tablespace
@param[in,out]	crsr		Memcached cursor
@param[in]	key_str		Memcached key
@return DB_SUCCESS if SDI deletion is successful, else error */
ib_err_t ib_memc_sdi_delete(ib_crsr_t crsr, const char *key_str) {
  uint32_t tablespace_id = crsr->prebuilt->table->space;
  ib_trx_t trx = crsr->prebuilt->trx;
  ib_sdi_key_t sk;
  dd::sdi_key_t sdi_key;
  ib_err_t err;
  ut_ad(trx != NULL);

  sk.sdi_key = &sdi_key;
  /* We only need sdi key */
  err = parse_mem_key_to_sdi_key(key_str, &sdi_key);
  if (err != DB_SUCCESS) {
    return (err);
  }

  err = ib_sdi_delete(tablespace_id, &sk, trx);

  DBUG_EXECUTE_IF("ib_sdi_delete_crash", DBUG_SUICIDE(););

  return (err);
}

/** Wrapper function to insert SDI into tablespace
@param[in,out]	crsr		Memcached cursor
@param[in]	key_str		Memcached key
@param[in]	sdi		SDI to be stored in tablespace
@param[in]	sdi_len		SDI length
@return DB_SUCCESS if SDI insertion is successful, else error */
ib_err_t ib_memc_sdi_set(ib_crsr_t crsr, const char *key_str, const void *sdi,
                         uint64_t *sdi_len) {
  uint32_t tablespace_id = crsr->prebuilt->table->space;
  ib_trx_t trx = crsr->prebuilt->trx;
  ib_sdi_key_t sk;
  dd::sdi_key_t sdi_key;
  ib_err_t err;
  ut_ad(trx != NULL);

  sk.sdi_key = &sdi_key;

  err = parse_mem_key_to_sdi_key(key_str, &sdi_key);
  if (err != DB_SUCCESS) {
    return (err);
  }

  Sdi_Compressor compressor(*sdi_len, sdi);
  compressor.compress();
  err = ib_sdi_set(tablespace_id, &sk, *sdi_len, compressor.get_comp_len(),
                   compressor.get_data(), trx);

  DBUG_EXECUTE_IF("ib_sdi_set_crash", DBUG_SUICIDE(););

  return (err);
}

/** Wrapper function to create SDI in a tablespace
@param[in,out]	crsr		Memcached cursor
@return DB_SUCCESS if SDI creation is successful, else error */
ib_err_t ib_memc_sdi_create(ib_crsr_t crsr) {
  uint32_t tablespace_id = crsr->prebuilt->table->space;
  return (ib_sdi_create(tablespace_id));
}

/** Wrapper function to drop SDI in a tablespace
@param[in,out]	crsr		Memcached cursor
@return DB_SUCCESS if dropping of SDI is successful, else error */
ib_err_t ib_memc_sdi_drop(ib_crsr_t crsr) {
  uint32_t tablespace_id = crsr->prebuilt->table->space;
  return (ib_sdi_drop(tablespace_id));
}

/** Wrapper function to retreive list of SDI keys into the buffer
The SDI keys are copied in the from x:y and separated by '|'
@param[in,out]	crsr		Memcached cursor
@param[in]	key_str		Memcached key
@param[in,out]	sdi		The keys are copies into this buffer
@return DB_SUCCESS if SDI keys retrieval is successful, else error */
ib_err_t ib_memc_sdi_get_keys(ib_crsr_t crsr, const char *key_str, void *sdi,
                              uint64_t list_buf_len) {
  uint32_t tablespace_id = crsr->prebuilt->table->space;
  ib_trx_t trx = crsr->prebuilt->trx;
  ut_ad(trx != NULL);

  uint32_t pattern_len = strlen("sdi_list_");
  int diff_len = key_str != NULL ? strlen(key_str) - pattern_len : -1;
  if (diff_len >= 0 && strncmp(key_str, "sdi_list_", pattern_len) == 0) {
    /* Pattern matched exactly with "sdi_list_" */
  }

  dd::sdi_vector sdi_vector;
  ib_sdi_vector ib_vector;
  ib_vector.sdi_vector = &sdi_vector;

  ib_err_t err = ib_sdi_get_keys(tablespace_id, &ib_vector, trx);

  char *ptr = static_cast<char *>(sdi);
  uint64_t cur_len = 0;
  uint64_t bytes_printed;
  for (dd::sdi_container::iterator it = ib_vector.sdi_vector->m_vec.begin();
       it != ib_vector.sdi_vector->m_vec.end(); it++) {
    bytes_printed =
        snprintf(ptr, list_buf_len - cur_len, "%llu:%u|", it->id, it->type);
    ptr += bytes_printed;
    cur_len += bytes_printed;
  }
  *ptr = 0;

  return (err);
}
#endif /* UNIV_MEMCACHED_SDI */
