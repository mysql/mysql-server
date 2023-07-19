/*****************************************************************************

Copyright (c) 1996, 2023, Oracle and/or its affiliates.

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

/** @file dict/dict0load.cc
 Loads to the memory cache database object definitions
 from dictionary tables

 Created 4/24/1996 Heikki Tuuri
 *******************************************************/

#include "current_thd.h"
#include "ha_prototypes.h"

#include <set>
#include <stack>
#include "dict0load.h"

#include "btr0btr.h"
#include "btr0pcur.h"
#include "dict0boot.h"
#include "dict0crea.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "dict0load.h"
#include "dict0mem.h"
#include "dict0priv.h"
#include "dict0stats.h"
#include "fsp0file.h"
#include "fsp0sysspace.h"
#include "fts0priv.h"
#include "ha_prototypes.h"
#include "mach0data.h"

#include "my_dbug.h"

#include "fil0fil.h"
#include "fts0fts.h"
#include "mysql_version.h"
#include "page0page.h"
#include "rem0cmp.h"
#include "srv0srv.h"
#include "srv0start.h"

/** Following are the InnoDB system tables. The positions in
this array are referenced by enum dict_system_table_id. */
const char *SYSTEM_TABLE_NAME[] = {
    "SYS_TABLES",      "SYS_INDEXES",   "SYS_COLUMNS",
    "SYS_FIELDS",      "SYS_FOREIGN",   "SYS_FOREIGN_COLS",
    "SYS_TABLESPACES", "SYS_DATAFILES", "SYS_VIRTUAL"};

/** This variant is based on name comparison and is used because
system table id array is not built yet.
@param[in]      name    InnoDB table name
@return true if table name is InnoDB SYSTEM table */
static bool dict_load_is_system_table(const char *name) {
  ut_ad(name != nullptr);
  uint32_t size = sizeof(SYSTEM_TABLE_NAME) / sizeof(char *);
  for (uint32_t i = 0; i < size; i++) {
    if (strcmp(name, SYSTEM_TABLE_NAME[i]) == 0) {
      return (true);
    }
  }
  return (false);
}
/* This is set of tablespaces that are not found in SYS_TABLESPACES.
InnoDB tablespaces before 5.6 are not registered in SYS_TABLESPACES.
So we maintain a std::set, which is later used to register the
tablespaces to dictionary table mysql.tablespaces */
missing_sys_tblsp_t missing_spaces;

/** This bool denotes if we found a Table or Partition with discarded Tablespace
during load of SYS_TABLES (in dict_check_sys_tables).

We use it to stop upgrade from 5.7 to 8.0 if there are discarded Tablespaces. */
bool has_discarded_tablespaces = false;

/** Loads a table definition and also all its index definitions.

Loads those foreign key constraints whose referenced table is already in
dictionary cache.  If a foreign key constraint is not loaded, then the
referenced table is pushed into the output stack (fk_tables), if it is not
NULL.  These tables must be subsequently loaded so that all the foreign
key constraints are loaded into memory.

@param[in]      name            Table name in the db/tablename format
@param[in]      cached          true=add to cache, false=do not
@param[in]      ignore_err      Error to be ignored when loading table
                                and its index definition
@param[out]     fk_tables       Related table names that must also be
                                loaded to ensure that all foreign key
                                constraints are loaded.
@param[in]      prev_table      previous table name. The current table load
                                is happening because of the load of the
                                previous table name.  This parameter is used
                                to check for cyclic calls.
@return table, NULL if does not exist; if the table is stored in an
.ibd file, but the file does not exist, then we set the
ibd_file_missing flag true in the table object we return */
static dict_table_t *dict_load_table_one(table_name_t &name, bool cached,
                                         dict_err_ignore_t ignore_err,
                                         dict_names_t &fk_tables,
                                         const std::string *prev_table);

/** Loads a table definition from a SYS_TABLES record to dict_table_t.
Does not load any columns or indexes.
@param[in]      name    Table name
@param[in]      rec     SYS_TABLES record
@param[out]     table   Table, or NULL
@return error message, or NULL on success */
static const char *dict_load_table_low(table_name_t &name, const rec_t *rec,
                                       dict_table_t **table);

/* If this flag is true, then we will load the cluster index's (and tables')
metadata even if it is marked as "corrupted". */
bool srv_load_corrupted = false;

#ifdef UNIV_DEBUG
/** Compare the name of an index column.
 @return true if the i'th column of index is 'name'. */
static bool name_of_col_is(const dict_table_t *table, /*!< in: table */
                           const dict_index_t *index, /*!< in: index */
                           ulint i,          /*!< in: index field offset */
                           const char *name) /*!< in: name to compare to */
{
  ulint tmp = dict_col_get_no(index->get_field(i)->col);

  return (strcmp(name, table->get_col_name(tmp)) == 0);
}
#endif /* UNIV_DEBUG */

/** Finds the first table name in the given database.
 @return own: table name, NULL if does not exist; the caller must free
 the memory in the string! */
char *dict_get_first_table_name_in_db(
    const char *name) /*!< in: database name which ends in '/' */
{
  dict_table_t *sys_tables;
  btr_pcur_t pcur;
  dict_index_t *sys_index;
  dtuple_t *tuple;
  mem_heap_t *heap;
  dfield_t *dfield;
  const rec_t *rec;
  const byte *field;
  ulint len;
  mtr_t mtr;

  ut_ad(dict_sys_mutex_own());

  heap = mem_heap_create(100, UT_LOCATION_HERE);

  mtr_start(&mtr);

  sys_tables = dict_table_get_low("SYS_TABLES");
  sys_index = UT_LIST_GET_FIRST(sys_tables->indexes);
  ut_ad(!dict_table_is_comp(sys_tables));

  tuple = dtuple_create(heap, 1);
  dfield = dtuple_get_nth_field(tuple, 0);

  dfield_set_data(dfield, name, ut_strlen(name));
  dict_index_copy_types(tuple, sys_index, 1);

  pcur.open_on_user_rec(sys_index, tuple, PAGE_CUR_GE, BTR_SEARCH_LEAF, &mtr,
                        UT_LOCATION_HERE);
loop:
  rec = pcur.get_rec();

  if (!pcur.is_on_user_rec()) {
    /* Not found */

    pcur.close();
    mtr_commit(&mtr);
    mem_heap_free(heap);

    return (nullptr);
  }

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_TABLES__NAME, &len);

  if (len < strlen(name) || ut_memcmp(name, field, strlen(name)) != 0) {
    /* Not found */

    pcur.close();
    mtr_commit(&mtr);
    mem_heap_free(heap);

    return (nullptr);
  }

  if (!rec_get_deleted_flag(rec, 0)) {
    /* We found one */

    char *table_name = mem_strdupl((char *)field, len);

    pcur.close();
    mtr_commit(&mtr);
    mem_heap_free(heap);

    return (table_name);
  }

  pcur.move_to_next_user_rec(&mtr);

  goto loop;
}

/** This function gets the next system table record as it scans the table.
 @return the next record if found, NULL if end of scan */
static const rec_t *dict_getnext_system_low(
    btr_pcur_t *pcur, /*!< in/out: persistent cursor to the
                      record*/
    mtr_t *mtr)       /*!< in: the mini-transaction */
{
  rec_t *rec = nullptr;

  while (!rec || rec_get_deleted_flag(rec, 0)) {
    pcur->move_to_next_user_rec(mtr);

    rec = pcur->get_rec();

    if (!pcur->is_on_user_rec()) {
      /* end of index */
      pcur->close();

      return (nullptr);
    }
  }

  /* Get a record, let's save the position */
  pcur->store_position(mtr);

  return (rec);
}

/** This function opens a system table, and returns the first record.
 @return first record of the system table */
const rec_t *dict_startscan_system(
    btr_pcur_t *pcur,           /*!< out: persistent cursor to
                                the record */
    mtr_t *mtr,                 /*!< in: the mini-transaction */
    dict_system_id_t system_id) /*!< in: which system table to open */
{
  dict_table_t *system_table;
  dict_index_t *clust_index;
  const rec_t *rec;

  ut_a(system_id < SYS_NUM_SYSTEM_TABLES);

  system_table = dict_table_get_low(SYSTEM_TABLE_NAME[system_id]);

  clust_index = UT_LIST_GET_FIRST(system_table->indexes);

  pcur->open_at_side(true, clust_index, BTR_SEARCH_LEAF, true, 0, mtr);

  rec = dict_getnext_system_low(pcur, mtr);

  return (rec);
}

/** This function gets the next system table record as it scans the table.
 @return the next record if found, NULL if end of scan */
const rec_t *dict_getnext_system(btr_pcur_t *pcur, /*!< in/out: persistent
                                                   cursor to the record */
                                 mtr_t *mtr) /*!< in: the mini-transaction */
{
  const rec_t *rec;

  /* Restore the position */
  pcur->restore_position(BTR_SEARCH_LEAF, mtr, UT_LOCATION_HERE);

  /* Get the next record */
  rec = dict_getnext_system_low(pcur, mtr);

  return (rec);
}

/** Error message for a delete-marked record in dict_load_index_low() */
static const char *dict_load_index_del = "delete-marked record in SYS_INDEXES";
/** Error message for table->id mismatch in dict_load_index_low() */
static const char *dict_load_index_id_err = "SYS_INDEXES.TABLE_ID mismatch";

/** Load an index definition from a SYS_INDEXES record to dict_index_t.
If allocate=true, we will create a dict_index_t structure and fill it
accordingly. If allocated=false, the dict_index_t will be supplied by
the caller and filled with information read from the record.  @return
error message, or NULL on success */
static const char *dict_load_index_low(
    byte *table_id,         /*!< in/out: table id (8 bytes),
                            an "in" value if allocate=true
                            and "out" when allocate=false */
    const char *table_name, /*!< in: table name */
    mem_heap_t *heap,       /*!< in/out: temporary memory heap */
    const rec_t *rec,       /*!< in: SYS_INDEXES record */
    bool allocate,          /*!< in: true=allocate *index,
                             false=fill in a pre-allocated
                             *index */
    dict_index_t **index)   /*!< out,own: index, or NULL */
{
  const byte *field;
  ulint len;
  ulint name_len;
  const char *name_buf;
  space_index_t id;
  ulint n_fields;
  ulint type;
  ulint space;
  ulint merge_threshold;

  if (allocate) {
    /* If allocate=true, no dict_index_t will
    be supplied. Initialize "*index" to NULL */
    *index = nullptr;
  }

  if (rec_get_deleted_flag(rec, 0)) {
    return (dict_load_index_del);
  }

  if (rec_get_n_fields_old_raw(rec) == DICT_NUM_FIELDS__SYS_INDEXES) {
    /* MERGE_THRESHOLD exists */
    field = rec_get_nth_field_old(nullptr, rec,
                                  DICT_FLD__SYS_INDEXES__MERGE_THRESHOLD, &len);
    switch (len) {
      case 4:
        merge_threshold = mach_read_from_4(field);
        break;
      case UNIV_SQL_NULL:
        merge_threshold = DICT_INDEX_MERGE_THRESHOLD_DEFAULT;
        break;
      default:
        return (
            "incorrect MERGE_THRESHOLD length"
            " in SYS_INDEXES");
    }
  } else if (rec_get_n_fields_old_raw(rec) ==
             DICT_NUM_FIELDS__SYS_INDEXES - 1) {
    /* MERGE_THRESHOLD doesn't exist */

    merge_threshold = DICT_INDEX_MERGE_THRESHOLD_DEFAULT;
  } else {
    return ("wrong number of columns in SYS_INDEXES record");
  }

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_INDEXES__TABLE_ID,
                                &len);
  if (len != 8) {
  err_len:
    return ("incorrect column length in SYS_INDEXES");
  }

  if (!allocate) {
    /* We are reading a SYS_INDEXES record. Copy the table_id */
    memcpy(table_id, (const char *)field, 8);
  } else if (memcmp(field, table_id, 8)) {
    /* Caller supplied table_id, verify it is the same
    id as on the index record */
    return (dict_load_index_id_err);
  }

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_INDEXES__ID, &len);
  if (len != 8) {
    goto err_len;
  }

  id = mach_read_from_8(field);

  rec_get_nth_field_offs_old(nullptr, rec, DICT_FLD__SYS_INDEXES__DB_TRX_ID,
                             &len);
  if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
    goto err_len;
  }
  rec_get_nth_field_offs_old(nullptr, rec, DICT_FLD__SYS_INDEXES__DB_ROLL_PTR,
                             &len);
  if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
    goto err_len;
  }

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_INDEXES__NAME,
                                &name_len);
  if (name_len == UNIV_SQL_NULL) {
    goto err_len;
  }

  name_buf = mem_heap_strdupl(heap, (const char *)field, name_len);

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_INDEXES__N_FIELDS,
                                &len);
  if (len != 4) {
    goto err_len;
  }
  n_fields = mach_read_from_4(field);

  field =
      rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_INDEXES__TYPE, &len);
  if (len != 4) {
    goto err_len;
  }
  type = mach_read_from_4(field);
  if (type & (~0U << DICT_IT_BITS)) {
    return ("unknown SYS_INDEXES.TYPE bits");
  }

  field =
      rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_INDEXES__SPACE, &len);
  if (len != 4) {
    goto err_len;
  }
  space = mach_read_from_4(field);

  field =
      rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_INDEXES__PAGE_NO, &len);
  if (len != 4) {
    goto err_len;
  }

  if (srv_is_upgrade_mode) {
    if (strcmp(name_buf, innobase_index_reserve_name) == 0) {
      /* Data dictinory uses PRIMARY instead of GEN_CLUST_INDEX. */
      name_buf = "PRIMARY";
    }
  }

  if (srv_is_upgrade_mode) {
    fts_aux_table_t fts_table;
    bool is_fts =
        fts_is_aux_table_name(&fts_table, table_name, strlen(table_name));

    if (is_fts) {
      switch (fts_table.type) {
        case FTS_INDEX_TABLE:
          name_buf = FTS_INDEX_TABLE_IND_NAME;
          break;
        case FTS_COMMON_TABLE:
          name_buf = FTS_COMMON_TABLE_IND_NAME;
          break;
        case FTS_OBSOLETED_TABLE:
          break;
          /* do nothing */
      }
    }
  }

  if (allocate) {
    *index = dict_mem_index_create(table_name, name_buf, space, type, n_fields);
  } else {
    ut_a(*index);

    dict_mem_fill_index_struct(*index, nullptr, nullptr, name_buf, space, type,
                               n_fields);
  }

  (*index)->id = id;
  (*index)->page = mach_read_from_4(field);
  ut_ad((*index)->page);
  (*index)->merge_threshold = merge_threshold;

  return (nullptr);
}

/** Error message for a delete-marked record in dict_load_column_low() */
static const char *dict_load_column_del = "delete-marked record in SYS_COLUMN";

/** Load a table column definition from a SYS_COLUMNS record to
dict_table_t.
@return error message, or NULL on success */
static const char *dict_load_column_low(
    dict_table_t *table,   /*!< in/out: table, could be NULL
                           if we just populate a dict_column_t
                           struct with information from
                           a SYS_COLUMNS record */
    mem_heap_t *heap,      /*!< in/out: memory heap
                           for temporary storage */
    dict_col_t *column,    /*!< out: dict_column_t to fill,
                           or NULL if table != NULL */
    table_id_t *table_id,  /*!< out: table id */
    const char **col_name, /*!< out: column name */
    const rec_t *rec,      /*!< in: SYS_COLUMNS record */
    ulint *nth_v_col)      /*!< out: if not NULL, this
                           records the "n" of "nth" virtual
                           column */
{
  char *name;
  const byte *field;
  ulint len;
  ulint mtype;
  ulint prtype;
  ulint col_len;
  ulint pos;
  ulint num_base;

  ut_ad(table || column);

  if (rec_get_deleted_flag(rec, 0)) {
    return (dict_load_column_del);
  }

  if (rec_get_n_fields_old_raw(rec) != DICT_NUM_FIELDS__SYS_COLUMNS) {
    return ("wrong number of columns in SYS_COLUMNS record");
  }

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_COLUMNS__TABLE_ID,
                                &len);
  if (len != 8) {
  err_len:
    return ("incorrect column length in SYS_COLUMNS");
  }

  if (table_id) {
    *table_id = mach_read_from_8(field);
  } else if (table->id != mach_read_from_8(field)) {
    return ("SYS_COLUMNS.TABLE_ID mismatch");
  }

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_COLUMNS__POS, &len);
  if (len != 4) {
    goto err_len;
  }

  pos = mach_read_from_4(field);

  rec_get_nth_field_offs_old(nullptr, rec, DICT_FLD__SYS_COLUMNS__DB_TRX_ID,
                             &len);
  if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
    goto err_len;
  }
  rec_get_nth_field_offs_old(nullptr, rec, DICT_FLD__SYS_COLUMNS__DB_ROLL_PTR,
                             &len);
  if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
    goto err_len;
  }

  field =
      rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_COLUMNS__NAME, &len);
  if (len == 0 || len == UNIV_SQL_NULL) {
    goto err_len;
  }

  name = mem_heap_strdupl(heap, (const char *)field, len);

  if (col_name) {
    *col_name = name;
  }

  field =
      rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_COLUMNS__MTYPE, &len);
  if (len != 4) {
    goto err_len;
  }

  mtype = mach_read_from_4(field);

  field =
      rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_COLUMNS__PRTYPE, &len);
  if (len != 4) {
    goto err_len;
  }
  prtype = mach_read_from_4(field);

  if (dtype_get_charset_coll(prtype) == 0 && dtype_is_string_type(mtype)) {
    /* The table was created with < 4.1.2. */

    if (dtype_is_binary_string_type(mtype, prtype)) {
      /* Use the binary collation for
      string columns of binary type. */

      prtype = dtype_form_prtype(prtype, DATA_MYSQL_BINARY_CHARSET_COLL);
    } else {
      /* Use the default charset for
      other than binary columns. */

      prtype = dtype_form_prtype(prtype, data_mysql_default_charset_coll);
    }
  }

  if (table && table->n_def != pos && !(prtype & DATA_VIRTUAL)) {
    return ("SYS_COLUMNS.POS mismatch");
  }

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_COLUMNS__LEN, &len);
  if (len != 4) {
    goto err_len;
  }
  col_len = mach_read_from_4(field);
  field =
      rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_COLUMNS__PREC, &len);
  if (len != 4) {
    goto err_len;
  }

  num_base = mach_read_from_4(field);

  if (column == nullptr) {
    if (prtype & DATA_VIRTUAL) {
#ifdef UNIV_DEBUG
      dict_v_col_t *vcol =
#endif
          dict_mem_table_add_v_col(table, heap, name, mtype, prtype, col_len,
                                   dict_get_v_col_mysql_pos(pos), num_base,
                                   true);
      ut_ad(vcol->v_pos == dict_get_v_col_pos(pos));
    } else {
      ut_ad(num_base == 0);
      dict_mem_table_add_col(table, heap, name, mtype, prtype, col_len, true);
    }
  } else {
    dict_mem_fill_column_struct(column, pos, mtype, prtype, col_len, true,
                                UINT32_UNDEFINED, 0, 0);
  }

  /* Report the virtual column number */
  if (prtype & DATA_VIRTUAL && nth_v_col != nullptr) {
    *nth_v_col = dict_get_v_col_pos(pos);
  }

  return (nullptr);
}

/** Error message for a delete-marked record in dict_load_virtual_low() */
static const char *dict_load_virtual_del =
    "delete-marked record in SYS_VIRTUAL";

/** Loads a virtual column "mapping" (to base columns) information
from a SYS_VIRTUAL record
@param[in,out]  table           table
@param[in,out]  column          mapped base column's dict_column_t
@param[in,out]  table_id        table id
@param[in,out]  pos             virtual column position
@param[in,out]  base_pos        base column position
@param[in]      rec             SYS_VIRTUAL record
@return error message, or NULL on success */
static const char *dict_load_virtual_low(dict_table_t *table,
                                         dict_col_t **column,
                                         table_id_t *table_id, ulint *pos,
                                         ulint *base_pos, const rec_t *rec) {
  const byte *field;
  ulint len;
  ulint base;

  if (rec_get_deleted_flag(rec, 0)) {
    return (dict_load_virtual_del);
  }

  if (rec_get_n_fields_old_raw(rec) != DICT_NUM_FIELDS__SYS_VIRTUAL) {
    return ("wrong number of columns in SYS_VIRTUAL record");
  }

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_VIRTUAL__TABLE_ID,
                                &len);
  if (len != 8) {
  err_len:
    return ("incorrect column length in SYS_VIRTUAL");
  }

  if (table_id != nullptr) {
    *table_id = mach_read_from_8(field);
  } else if (table->id != mach_read_from_8(field)) {
    return ("SYS_VIRTUAL.TABLE_ID mismatch");
  }

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_VIRTUAL__POS, &len);
  if (len != 4) {
    goto err_len;
  }

  if (pos != nullptr) {
    *pos = mach_read_from_4(field);
  }

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_VIRTUAL__BASE_POS,
                                &len);
  if (len != 4) {
    goto err_len;
  }

  base = mach_read_from_4(field);

  if (base_pos != nullptr) {
    *base_pos = base;
  }

  rec_get_nth_field_offs_old(nullptr, rec, DICT_FLD__SYS_VIRTUAL__DB_TRX_ID,
                             &len);
  if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
    goto err_len;
  }

  rec_get_nth_field_offs_old(nullptr, rec, DICT_FLD__SYS_VIRTUAL__DB_ROLL_PTR,
                             &len);
  if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
    goto err_len;
  }

  if (column != nullptr) {
    *column = table->get_col(base);
  }

  return (nullptr);
}

/** Loads SYS_VIRTUAL info for one virtual column
@param[in,out]  table           table
@param[in]      nth_v_col       virtual column sequence num
@param[in,out]  v_col           virtual column
@param[in,out]  heap            memory heap
*/
static void dict_load_virtual_one_col(dict_table_t *table, ulint nth_v_col,
                                      dict_v_col_t *v_col, mem_heap_t *heap) {
  dict_table_t *sys_virtual;
  dict_index_t *sys_virtual_index;
  btr_pcur_t pcur;
  dtuple_t *tuple;
  dfield_t *dfield;
  const rec_t *rec;
  byte *buf;
  ulint i = 0;
  mtr_t mtr;
  ulint skipped = 0;

  ut_ad(dict_sys_mutex_own());

  if (v_col->num_base == 0) {
    return;
  }

  mtr_start(&mtr);

  sys_virtual = dict_table_get_low("SYS_VIRTUAL");
  sys_virtual_index = UT_LIST_GET_FIRST(sys_virtual->indexes);
  ut_ad(!dict_table_is_comp(sys_virtual));

  ut_ad(name_of_col_is(sys_virtual, sys_virtual_index,
                       DICT_FLD__SYS_VIRTUAL__POS, "POS"));

  tuple = dtuple_create(heap, 2);

  /* table ID field */
  dfield = dtuple_get_nth_field(tuple, 0);

  buf = static_cast<byte *>(mem_heap_alloc(heap, 8));
  mach_write_to_8(buf, table->id);

  dfield_set_data(dfield, buf, 8);

  /* virtual column pos field */
  dfield = dtuple_get_nth_field(tuple, 1);

  buf = static_cast<byte *>(mem_heap_alloc(heap, 4));
  ulint vcol_pos = dict_create_v_col_pos(nth_v_col, v_col->m_col.ind);
  mach_write_to_4(buf, vcol_pos);

  dfield_set_data(dfield, buf, 4);

  dict_index_copy_types(tuple, sys_virtual_index, 2);

  pcur.open_on_user_rec(sys_virtual_index, tuple, PAGE_CUR_GE, BTR_SEARCH_LEAF,
                        &mtr, UT_LOCATION_HERE);

  for (i = 0; i < v_col->num_base + skipped; i++) {
    const char *err_msg;
    ulint pos;

    ut_ad(pcur.is_on_user_rec());

    rec = pcur.get_rec();

    ut_a(pcur.is_on_user_rec());

    err_msg = dict_load_virtual_low(table, &v_col->base_col[i - skipped],
                                    nullptr, &pos, nullptr, rec);

    if (err_msg) {
      if (err_msg != dict_load_virtual_del) {
        ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_187) << err_msg;
      } else {
        skipped++;
      }
    } else {
      ut_ad(pos == vcol_pos);
    }

    pcur.move_to_next_user_rec(&mtr);
  }

  pcur.close();
  mtr_commit(&mtr);
}

/** Loads info from SYS_VIRTUAL for virtual columns.
@param[in,out]  table   table
@param[in]      heap    memory heap
*/
static void dict_load_virtual(dict_table_t *table, mem_heap_t *heap) {
  for (ulint i = 0; i < table->n_v_cols; i++) {
    dict_v_col_t *v_col = dict_table_get_nth_v_col(table, i);

    dict_load_virtual_one_col(table, i, v_col, heap);
  }
}
/** Error message for a delete-marked record in dict_load_field_low() */
static const char *dict_load_field_del = "delete-marked record in SYS_FIELDS";

/** Loads an index field definition from a SYS_FIELDS record to
dict_index_t.
@return error message
@retval NULL on success */
static const char *dict_load_field_low(
    byte *index_id,      /*!< in/out: index id (8 bytes)
                         an "in" value if index != NULL
                         and "out" if index == NULL */
    dict_index_t *index, /*!< in/out: index, could be NULL
                         if we just populate a dict_field_t
                         struct with information from
                         a SYS_FIELDS record */
    mem_heap_t *heap,    /*!< in/out: memory heap
                         for temporary storage */
    const rec_t *rec)    /*!< in: SYS_FIELDS record */
{
  const byte *field;
  ulint len;
  ulint pos_and_prefix_len;
  ulint prefix_len;
  bool is_ascending;
  bool first_field;

  /* Index is supplied */
  ut_a(index);

  if (rec_get_deleted_flag(rec, 0)) {
    return (dict_load_field_del);
  }

  if (rec_get_n_fields_old_raw(rec) != DICT_NUM_FIELDS__SYS_FIELDS) {
    return ("wrong number of columns in SYS_FIELDS record");
  }

  field =
      rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_FIELDS__INDEX_ID, &len);
  if (len != 8) {
  err_len:
    return ("incorrect column length in SYS_FIELDS");
  }

  first_field = (index->n_def == 0);
  if (memcmp(field, index_id, 8)) {
    return ("SYS_FIELDS.INDEX_ID mismatch");
  }

  /* The next field stores the field position in the index and a
  possible column prefix length if the index field does not
  contain the whole column. The storage format is like this: if
  there is at least one prefix field in the index, then the HIGH
  2 bytes contain the field number (index->n_def) and the low 2
  bytes the prefix length for the field. Otherwise the field
  number (index->n_def) is contained in the 2 LOW bytes. */

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_FIELDS__POS, &len);
  if (len != 4) {
    goto err_len;
  }

  pos_and_prefix_len = mach_read_from_4(field);

  if (index &&
      UNIV_UNLIKELY((pos_and_prefix_len & 0xFFFFUL) != index->n_def &&
                    (pos_and_prefix_len >> 16 & 0xFFFF) != index->n_def)) {
    return ("SYS_FIELDS.POS mismatch");
  }

  if (first_field || pos_and_prefix_len > 0xFFFFUL) {
    prefix_len = pos_and_prefix_len & 0x7FFFUL;
    is_ascending = !(pos_and_prefix_len & 0x8000UL);
  } else {
    prefix_len = 0;
    is_ascending = true;
  }

  rec_get_nth_field_offs_old(nullptr, rec, DICT_FLD__SYS_FIELDS__DB_TRX_ID,
                             &len);
  if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
    goto err_len;
  }
  rec_get_nth_field_offs_old(nullptr, rec, DICT_FLD__SYS_FIELDS__DB_ROLL_PTR,
                             &len);
  if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
    goto err_len;
  }

  field =
      rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_FIELDS__COL_NAME, &len);
  if (len == 0 || len == UNIV_SQL_NULL) {
    goto err_len;
  }

  index->add_field(mem_heap_strdupl(heap, (const char *)field, len), prefix_len,
                   is_ascending);

  return (nullptr);
}

/** This function parses a SYS_TABLESPACES record, extracts necessary
 information from the record and returns to caller.
 @return error message, or NULL on success */
const char *dict_process_sys_tablespaces(
    mem_heap_t *heap,  /*!< in/out: heap memory */
    const rec_t *rec,  /*!< in: current SYS_TABLESPACES rec */
    space_id_t *space, /*!< out: space id */
    const char **name, /*!< out: tablespace name */
    uint32_t *flags)   /*!< out: tablespace flags */
{
  ulint len;
  const byte *field;

  /* Initialize the output values */
  *space = SPACE_UNKNOWN;
  *name = nullptr;
  *flags = UINT32_UNDEFINED;

  if (rec_get_deleted_flag(rec, 0)) {
    return ("delete-marked record in SYS_TABLESPACES");
  }

  if (rec_get_n_fields_old_raw(rec) != DICT_NUM_FIELDS__SYS_TABLESPACES) {
    return ("wrong number of columns in SYS_TABLESPACES record");
  }

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_TABLESPACES__SPACE,
                                &len);
  if (len != DICT_FLD_LEN_SPACE) {
  err_len:
    return ("incorrect column length in SYS_TABLESPACES");
  }
  *space = mach_read_from_4(field);

  rec_get_nth_field_offs_old(nullptr, rec, DICT_FLD__SYS_TABLESPACES__DB_TRX_ID,
                             &len);
  if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
    goto err_len;
  }

  rec_get_nth_field_offs_old(nullptr, rec,
                             DICT_FLD__SYS_TABLESPACES__DB_ROLL_PTR, &len);
  if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
    goto err_len;
  }

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_TABLESPACES__NAME,
                                &len);
  if (len == 0 || len == UNIV_SQL_NULL) {
    goto err_len;
  }
  *name = mem_heap_strdupl(heap, (char *)field, len);

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_TABLESPACES__FLAGS,
                                &len);
  if (len != DICT_FLD_LEN_FLAGS) {
    goto err_len;
  }
  *flags = mach_read_from_4(field);

  return (nullptr);
}

/** Get the first filepath from SYS_DATAFILES for a given space_id.
@param[in]      space_id        Tablespace ID
@return First filepath (caller must invoke ut::free() on it)
@retval NULL if no SYS_DATAFILES entry was found. */
char *dict_get_first_path(ulint space_id) {
  mtr_t mtr;
  dict_table_t *sys_datafiles;
  dict_index_t *sys_index;
  dtuple_t *tuple;
  dfield_t *dfield;
  byte *buf;
  btr_pcur_t pcur;
  const rec_t *rec;
  const byte *field;
  ulint len;
  char *filepath = nullptr;
  mem_heap_t *heap = mem_heap_create(1024, UT_LOCATION_HERE);

  ut_ad(dict_sys_mutex_own());

  mtr_start(&mtr);

  sys_datafiles = dict_table_get_low("SYS_DATAFILES");
  sys_index = UT_LIST_GET_FIRST(sys_datafiles->indexes);

  ut_ad(!dict_table_is_comp(sys_datafiles));
  ut_ad(name_of_col_is(sys_datafiles, sys_index, DICT_FLD__SYS_DATAFILES__SPACE,
                       "SPACE"));
  ut_ad(name_of_col_is(sys_datafiles, sys_index, DICT_FLD__SYS_DATAFILES__PATH,
                       "PATH"));

  tuple = dtuple_create(heap, 1);
  dfield = dtuple_get_nth_field(tuple, DICT_FLD__SYS_DATAFILES__SPACE);

  buf = static_cast<byte *>(mem_heap_alloc(heap, 4));
  mach_write_to_4(buf, space_id);

  dfield_set_data(dfield, buf, 4);
  dict_index_copy_types(tuple, sys_index, 1);

  pcur.open_on_user_rec(sys_index, tuple, PAGE_CUR_GE, BTR_SEARCH_LEAF, &mtr,
                        UT_LOCATION_HERE);

  rec = pcur.get_rec();

  /* Get the filepath from this SYS_DATAFILES record. */
  if (pcur.is_on_user_rec()) {
    field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_DATAFILES__SPACE,
                                  &len);
    ut_a(len == 4);

    if (space_id == mach_read_from_4(field)) {
      /* A record for this space ID was found. */
      field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_DATAFILES__PATH,
                                    &len);

      ut_ad(len > 0);
      ut_ad(len < OS_FILE_MAX_PATH);

      if (len > 0 && len != UNIV_SQL_NULL) {
        filepath = mem_strdupl(reinterpret_cast<const char *>(field), len);
        ut_ad(filepath != nullptr);

        /* The dictionary may have been written on
        another OS. */
        Fil_path::normalize(filepath);
      }
    }
  }

  pcur.close();
  mtr_commit(&mtr);
  mem_heap_free(heap);

  return (filepath);
}

/** Gets the space name from SYS_TABLESPACES for a given space ID.
@param[in]      space_id        Tablespace ID
@param[in]      callers_heap    A heap to allocate from, may be NULL
@return Tablespace name (caller is responsible to free it)
@retval NULL if no dictionary entry was found. */
static char *dict_space_get_name(space_id_t space_id,
                                 mem_heap_t *callers_heap) {
  mtr_t mtr;
  dict_table_t *sys_tablespaces;
  dict_index_t *sys_index;
  dtuple_t *tuple;
  dfield_t *dfield;
  byte *buf;
  btr_pcur_t pcur;
  const rec_t *rec;
  const byte *field;
  ulint len;
  char *space_name = nullptr;
  mem_heap_t *heap = mem_heap_create(1024, UT_LOCATION_HERE);

  ut_ad(dict_sys_mutex_own());

  sys_tablespaces = dict_table_get_low("SYS_TABLESPACES");
  if (sys_tablespaces == nullptr) {
    ut_a(!srv_sys_tablespaces_open);
    return (nullptr);
  }

  sys_index = UT_LIST_GET_FIRST(sys_tablespaces->indexes);

  ut_ad(!dict_table_is_comp(sys_tablespaces));
  ut_ad(name_of_col_is(sys_tablespaces, sys_index,
                       DICT_FLD__SYS_TABLESPACES__SPACE, "SPACE"));
  ut_ad(name_of_col_is(sys_tablespaces, sys_index,
                       DICT_FLD__SYS_TABLESPACES__NAME, "NAME"));

  tuple = dtuple_create(heap, 1);
  dfield = dtuple_get_nth_field(tuple, DICT_FLD__SYS_TABLESPACES__SPACE);

  buf = static_cast<byte *>(mem_heap_alloc(heap, 4));
  mach_write_to_4(buf, space_id);

  dfield_set_data(dfield, buf, 4);
  dict_index_copy_types(tuple, sys_index, 1);

  mtr_start(&mtr);

  pcur.open_on_user_rec(sys_index, tuple, PAGE_CUR_GE, BTR_SEARCH_LEAF, &mtr,
                        UT_LOCATION_HERE);

  rec = pcur.get_rec();

  /* Get the tablespace name from this SYS_TABLESPACES record. */
  if (pcur.is_on_user_rec()) {
    field = rec_get_nth_field_old(nullptr, rec,
                                  DICT_FLD__SYS_TABLESPACES__SPACE, &len);
    ut_a(len == 4);

    if (space_id == mach_read_from_4(field)) {
      /* A record for this space ID was found. */
      field = rec_get_nth_field_old(nullptr, rec,
                                    DICT_FLD__SYS_TABLESPACES__NAME, &len);

      ut_ad(len > 0);
      ut_ad(len < OS_FILE_MAX_PATH);

      if (len > 0 && len != UNIV_SQL_NULL) {
        /* Found a tablespace name. */
        if (callers_heap == nullptr) {
          space_name = mem_strdupl(reinterpret_cast<const char *>(field), len);
        } else {
          space_name = mem_heap_strdupl(
              callers_heap, reinterpret_cast<const char *>(field), len);
        }
        ut_ad(space_name);
      }
    }
  }

  pcur.close();
  mtr_commit(&mtr);
  mem_heap_free(heap);

  return (space_name);
}

/** Check the validity of a SYS_TABLES record
Make sure the fields are the right length and that they
do not contain invalid contents.
@param[in]      rec     SYS_TABLES record
@return error message, or NULL on success */
static const char *dict_sys_tables_rec_check(const rec_t *rec) {
  const byte *field;
  ulint len;

  ut_ad(dict_sys_mutex_own());

  if (rec_get_deleted_flag(rec, 0)) {
    return ("delete-marked record in SYS_TABLES");
  }

  if (rec_get_n_fields_old_raw(rec) != DICT_NUM_FIELDS__SYS_TABLES) {
    return ("wrong number of columns in SYS_TABLES record");
  }

  rec_get_nth_field_offs_old(nullptr, rec, DICT_FLD__SYS_TABLES__NAME, &len);
  if (len == 0 || len == UNIV_SQL_NULL) {
  err_len:
    return ("incorrect column length in SYS_TABLES");
  }
  rec_get_nth_field_offs_old(nullptr, rec, DICT_FLD__SYS_TABLES__DB_TRX_ID,
                             &len);
  if (len != DATA_TRX_ID_LEN && len != UNIV_SQL_NULL) {
    goto err_len;
  }
  rec_get_nth_field_offs_old(nullptr, rec, DICT_FLD__SYS_TABLES__DB_ROLL_PTR,
                             &len);
  if (len != DATA_ROLL_PTR_LEN && len != UNIV_SQL_NULL) {
    goto err_len;
  }

  rec_get_nth_field_offs_old(nullptr, rec, DICT_FLD__SYS_TABLES__ID, &len);
  if (len != 8) {
    goto err_len;
  }

  field =
      rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_TABLES__N_COLS, &len);
  if (field == nullptr || len != 4) {
    goto err_len;
  }

  rec_get_nth_field_offs_old(nullptr, rec, DICT_FLD__SYS_TABLES__TYPE, &len);
  if (len != 4) {
    goto err_len;
  }

  rec_get_nth_field_offs_old(nullptr, rec, DICT_FLD__SYS_TABLES__MIX_ID, &len);
  if (len != 8) {
    goto err_len;
  }

  field =
      rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_TABLES__MIX_LEN, &len);
  if (field == nullptr || len != 4) {
    goto err_len;
  }

  rec_get_nth_field_offs_old(nullptr, rec, DICT_FLD__SYS_TABLES__CLUSTER_ID,
                             &len);
  if (len != UNIV_SQL_NULL) {
    goto err_len;
  }

  field =
      rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_TABLES__SPACE, &len);
  if (field == nullptr || len != 4) {
    goto err_len;
  }

  return (nullptr);
}

/** Read and return the contents of a SYS_TABLESPACES record.
@param[in]      rec     A record of SYS_TABLESPACES
@param[out]     id      Pointer to the space_id for this table
@param[in,out]  name    Buffer for Tablespace Name of length NAME_LEN
@param[out]     flags   Pointer to tablespace flags
@return true if the record was read correctly, false if not. */
static bool dict_sys_tablespaces_rec_read(const rec_t *rec, space_id_t *id,
                                          char *name, uint32_t *flags) {
  const byte *field;
  ulint len;

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_TABLESPACES__SPACE,
                                &len);
  if (len != DICT_FLD_LEN_SPACE) {
    ib::error(ER_IB_MSG_188)
        << "Wrong field length in SYS_TABLESPACES.SPACE: " << len;
    return (false);
  }
  *id = mach_read_from_4(field);

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_TABLESPACES__NAME,
                                &len);
  if (len == 0 || len == UNIV_SQL_NULL) {
    ib::error(ER_IB_MSG_189)
        << "Wrong field length in SYS_TABLESPACES.NAME: " << len;
    return (false);
  }
  strncpy(name, reinterpret_cast<const char *>(field), NAME_LEN);

  /* read the 4 byte flags from the TYPE field */
  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_TABLESPACES__FLAGS,
                                &len);
  if (len != 4) {
    ib::error(ER_IB_MSG_190)
        << "Wrong field length in SYS_TABLESPACES.FLAGS: " << len;
    return (false);
  }
  *flags = mach_read_from_4(field);

  return (true);
}

/** Load and check each general tablespace mentioned in the SYS_TABLESPACES.
Ignore system and file-per-table tablespaces.
If it is valid, add it to the file_system list.
@param[in]      validate        true when the previous shutdown was not clean
@return the highest space ID found. */
static inline space_id_t dict_check_sys_tablespaces(bool validate) {
  space_id_t max_space_id = 0;
  btr_pcur_t pcur;
  const rec_t *rec;
  mtr_t mtr;

  DBUG_TRACE;

  ut_ad(dict_sys_mutex_own());

  /* Before traversing it, let's make sure we have
  SYS_TABLESPACES and SYS_DATAFILES loaded. */
  dict_table_get_low("SYS_TABLESPACES");
  dict_table_get_low("SYS_DATAFILES");

  mtr_start(&mtr);

  for (rec = dict_startscan_system(&pcur, &mtr, SYS_TABLESPACES);
       rec != nullptr; rec = dict_getnext_system(&pcur, &mtr)) {
    char space_name[NAME_LEN + 1];
    space_id_t space_id = 0;
    uint32_t fsp_flags;

    if (!dict_sys_tablespaces_rec_read(rec, &space_id, space_name,
                                       &fsp_flags)) {
      continue;
    }

    /* Ignore system, undo and file-per-table tablespaces,
    and tablespaces that already are in the tablespace cache. */
    if (fsp_is_system_or_temp_tablespace(space_id) ||
        fsp_is_undo_tablespace(space_id) ||
        !fsp_is_shared_tablespace(fsp_flags) ||
        fil_space_exists_in_mem(space_id, space_name, false, true)) {
      continue;
    }

    /* Set the expected filepath from the data dictionary.
    If the file is found elsewhere (from an ISL or the default
    location) or this path is the same file but looks different,
    fil_ibd_open() will update the dictionary with what is
    opened. */
    char *filepath = dict_get_first_path(space_id);

    /* Check that this ibd is in a known location. If not, allow this
    but make some noise. */
    if (!fil_path_is_known(filepath)) {
      ib::warn(ER_IB_MSG_UNPROTECTED_LOCATION_ALLOWED, filepath, space_name);
    }

    /* Check that the .ibd file exists. */
    dberr_t err = fil_ibd_open(validate, FIL_TYPE_TABLESPACE, space_id,
                               fsp_flags, space_name, filepath, true, true);

    if (err != DB_SUCCESS) {
      ib::warn(ER_IB_MSG_191) << "Ignoring tablespace " << id_name_t(space_name)
                              << " because it could not be opened.";
    }

    if (!dict_sys_t::is_reserved(space_id)) {
      max_space_id = std::max(max_space_id, space_id);
    }

    ut::free(filepath);
  }

  mtr_commit(&mtr);

  return max_space_id;
}

/** Read and return 5 integer fields from a SYS_TABLES record.
@param[in]      rec             A record of SYS_TABLES
@param[in]      table_name      Table Name, the same as SYS_TABLES.NAME
@param[out]     table_id        Pointer to the table_id for this table
@param[out]     space_id        Pointer to the space_id for this table
@param[out]     n_cols          Pointer to number of columns for this table.
@param[out]     flags           Pointer to table flags
@param[out]     flags2          Pointer to table flags2
@return true if the record was read correctly, false if not. */
static bool dict_sys_tables_rec_read(const rec_t *rec,
                                     const table_name_t &table_name,
                                     table_id_t *table_id, space_id_t *space_id,
                                     uint32_t *n_cols, uint32_t *flags,
                                     uint32_t *flags2) {
  const byte *field;
  ulint len;
  uint32_t type;

  *flags2 = 0;

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_TABLES__ID, &len);
  ut_ad(len == 8);
  *table_id = static_cast<table_id_t>(mach_read_from_8(field));

  field =
      rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_TABLES__SPACE, &len);
  ut_ad(len == 4);
  *space_id = mach_read_from_4(field);

  /* Read the 4 byte flags from the TYPE field */
  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_TABLES__TYPE, &len);
  ut_a(len == 4);
  type = mach_read_from_4(field);

  /* The low order bit of SYS_TABLES.TYPE is always set to 1. But in
  dict_table_t::flags the low order bit is used to determine if the
  row format is Redundant (0) or Compact (1) when the format is Antelope.
  Read the 4 byte N_COLS field and look at the high order bit.  It
  should be set for COMPACT and later.  It should not be set for
  REDUNDANT. */
  field =
      rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_TABLES__N_COLS, &len);
  ut_a(len == 4);
  *n_cols = mach_read_from_4(field);

  /* This validation function also combines the DICT_N_COLS_COMPACT
  flag in n_cols into the type field to effectively make it a
  dict_table_t::flags. */

  if (UINT32_UNDEFINED == dict_sys_tables_type_validate(type, *n_cols)) {
    ib::error(ER_IB_MSG_192) << "Table " << table_name
                             << " in InnoDB"
                                " data dictionary contains invalid flags."
                                " SYS_TABLES.TYPE="
                             << type << " SYS_TABLES.N_COLS=" << *n_cols;
    *flags = UINT32_UNDEFINED;
    return (false);
  }

  *flags = dict_sys_tables_type_to_tf(type, *n_cols);

  /* Get flags2 from SYS_TABLES.MIX_LEN */
  field =
      rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_TABLES__MIX_LEN, &len);
  *flags2 = mach_read_from_4(field);

  /* DICT_TF2_FTS will be set when indexes are being loaded */
  *flags2 &= ~DICT_TF2_FTS;

  /* Now that we have used this bit, unset it. */
  *n_cols &= ~DICT_N_COLS_COMPACT;

  return (true);
}

/** Load and check each non-predefined tablespace mentioned in SYS_TABLES.
Search SYS_TABLES and check each tablespace mentioned that has not
already been added to the fil_system.  If it is valid, add it to the
file_system list.  Perform extra validation on the table if recovery from
the REDO log occurred.
@param[in]      validate        Whether to do validation on the table.
@return the highest space ID found. */
static inline space_id_t dict_check_sys_tables(bool validate) {
  space_id_t max_space_id = 0;
  btr_pcur_t pcur;
  const rec_t *rec;
  mtr_t mtr;

  DBUG_TRACE;

  ut_ad(dict_sys_mutex_own());

  mtr_start(&mtr);

  /* Before traversing SYS_TABLES, let's make sure we have
  SYS_TABLESPACES and SYS_DATAFILES loaded. */
  dict_table_t *sys_tablespaces;
  dict_table_t *sys_datafiles;
  sys_tablespaces = dict_table_get_low("SYS_TABLESPACES");
  ut_a(sys_tablespaces != nullptr);
  sys_datafiles = dict_table_get_low("SYS_DATAFILES");
  ut_a(sys_datafiles != nullptr);

  for (rec = dict_startscan_system(&pcur, &mtr, SYS_TABLES); rec != nullptr;
       rec = dict_getnext_system(&pcur, &mtr)) {
    const byte *field;
    ulint len;
    const char *space_name;
    std::string tablespace_name;
    table_name_t table_name;
    table_id_t table_id;
    space_id_t space_id;
    uint32_t n_cols;
    uint32_t flags;
    uint32_t flags2;
    const char *tbl_name;
    std::string dict_table_name;

    /* If a table record is not usable, ignore it and continue
    on to the next record. Error messages were logged. */
    if (dict_sys_tables_rec_check(rec) != nullptr) {
      continue;
    }

    /* Copy the table name from rec */
    field =
        rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_TABLES__NAME, &len);
    table_name.m_name = mem_strdupl((char *)field, len);
    DBUG_PRINT("dict_check_sys_tables",
               ("name: %p, '%s'", table_name.m_name, table_name.m_name));

    dict_sys_tables_rec_read(rec, table_name, &table_id, &space_id, &n_cols,
                             &flags, &flags2);
    if (flags == UINT32_UNDEFINED ||
        fsp_is_system_or_temp_tablespace(space_id)) {
      ut_ad(!fsp_is_undo_tablespace(space_id));
      ut::free(table_name.m_name);
      continue;
    }

    if (flags2 & DICT_TF2_DISCARDED) {
      ib::info(ER_IB_MSG_193)
          << "Tablespace " << table_name
          << " is set as DISCARDED. Upgrade will stop, please make sure "
             "there are no discarded Tables/Partitions before upgrading.";
      ut::free(table_name.m_name);
      has_discarded_tablespaces = true;
      continue;
    }

    /* If the table is not a predefined tablespace then it must
    be in a file-per-table or shared tablespace.
    Note that flags2 is not available for REDUNDANT tables and
    tables which are upgraded from 5.5 & earlier,
    so don't check those. */
    bool is_shared_space = DICT_TF_HAS_SHARED_SPACE(flags);

    ut_ad(is_shared_space || !DICT_TF_GET_COMPACT(flags) ||
          (flags2 == 0 || flags2 & DICT_TF2_USE_FILE_PER_TABLE));

    /* Look up the tablespace name in the data dictionary if this
    is a shared tablespace.  For file-per-table, the table_name
    and the tablespace_name are the same.
    Some hidden tables like FTS AUX tables may not be found in
    the dictionary since they can always be found in the default
    location. If so, then dict_space_get_name() will return NULL,
    the space name must be the table_name, and the filepath can be
    discovered in the default location.*/
    char *space_name_from_dict = dict_space_get_name(space_id, nullptr);

    if (space_id == dict_sys_t::s_dict_space_id) {
      tbl_name = dict_sys_t::s_dd_space_name;
      space_name = dict_sys_t::s_dd_space_name;
      tablespace_name.assign(space_name);

    } else {
      tbl_name = (space_name_from_dict != nullptr) ? space_name_from_dict
                                                   : table_name.m_name;

      /* Convert 5.7 name to 8.0 for partitioned table. Skip for shared
      tablespace. */
      dict_table_name.assign(tbl_name);
      if (!is_shared_space) {
        dict_name::rebuild(dict_table_name);
      }
      tbl_name = dict_table_name.c_str();

      /* Convert tablespace name to system cs. Skip for shared tablespace. */
      tablespace_name.assign(tbl_name);
      if (!is_shared_space) {
        dict_name::convert_to_space(tablespace_name);
      }
      space_name = tablespace_name.c_str();
    }

    /* Now that we have the proper name for this tablespace,
    whether it is a shared tablespace or a single table
    tablespace, look to see if it is already in the tablespace
    cache. */
    if (fil_space_exists_in_mem(space_id, space_name, false, true)) {
      ut::free(table_name.m_name);
      ut::free(space_name_from_dict);
      continue;
    }

    /* Build FSP flag */
    uint32_t fsp_flags = dict_tf_to_fsp_flags(flags);
    /* Set tablespace encryption flag */
    if (flags2 & DICT_TF2_ENCRYPTION_FILE_PER_TABLE) {
      fsp_flags_set_encryption(fsp_flags);
    }

    /* Set the expected filepath from the data dictionary. */
    char *filepath = nullptr;
    if (space_id == dict_sys_t::s_dict_space_id) {
      filepath = mem_strdup(dict_sys_t::s_dd_space_file_name);
    } else {
      filepath = dict_get_first_path(space_id);
      if (filepath == nullptr) {
        /* This record in dd::tablespaces does not have a path in
        dd:tablespace_files. This has been shown to occur during
        upgrade of some FTS tablespaces created in 5.6.
        Build a filepath in the default location from the table name. */
        filepath = Fil_path::make_ibd_from_table_name(tbl_name);
      } else {
        std::string dict_path(filepath);
        ut::free(filepath);
        /* Convert 5.7 name to 8.0 for partitioned table path. */
        fil_update_partition_name(space_id, fsp_flags, true, tablespace_name,
                                  dict_path);
        filepath = mem_strdup(dict_path.c_str());
      }
    }

    /* Check that this ibd is in a known location. If not, allow this
    but make some noise. */
    if (!fil_path_is_known(filepath)) {
      ib::warn(ER_IB_MSG_UNPROTECTED_LOCATION_ALLOWED, filepath, space_name);
    }

    /* Check that the .ibd file exists. */
    dberr_t err = fil_ibd_open(validate, FIL_TYPE_TABLESPACE, space_id,
                               fsp_flags, space_name, filepath, true, true);

    if (err != DB_SUCCESS) {
      ib::warn(ER_IB_MSG_194) << "Ignoring tablespace " << id_name_t(space_name)
                              << " because it could not be opened.";
    } else {
      /* This tablespace is not found in
      SYS_TABLESPACES and we are able to
      successfully open it. Add it to std::set.
      It will be later used for register tablespaces
      to mysql.tablespaces */
      if (space_name_from_dict == nullptr) {
        fil_space_t *space = fil_space_get(space_id);
        ut_ad(space != nullptr);
#ifdef UNIV_DEBUG
        auto var =
#endif /* UNIV_DEBUG */
            missing_spaces.insert(space);
        /* duplicate space_ids are not expected */
        ut_ad(var.second == true);
      }
    }

    if (!dict_sys_t::is_reserved(space_id)) {
      max_space_id = std::max(max_space_id, space_id);
    }

    ut::free(table_name.m_name);
    ut::free(space_name_from_dict);
    ut::free(filepath);
  }

  mtr_commit(&mtr);

  return max_space_id;
}

/** Load columns in an innodb cached table object from SYS_COLUMNS table.
@param[in, out]  table  Table cache object
@param[in, out]  heap   memory heap for temporary storage */
static void dict_load_columns(dict_table_t *table, mem_heap_t *heap) {
  ut_ad(dict_sys_mutex_own());

  mtr_t mtr;
  mtr_start(&mtr);

  dict_table_t *sys_columns = dict_table_get_low("SYS_COLUMNS");
  dict_index_t *sys_index = UT_LIST_GET_FIRST(sys_columns->indexes);
  ut_ad(!dict_table_is_comp(sys_columns));

  ut_ad(name_of_col_is(sys_columns, sys_index, DICT_FLD__SYS_COLUMNS__NAME,
                       "NAME"));
  ut_ad(name_of_col_is(sys_columns, sys_index, DICT_FLD__SYS_COLUMNS__PREC,
                       "PREC"));

  dtuple_t *tuple = dtuple_create(heap, 1);
  dfield_t *dfield = dtuple_get_nth_field(tuple, 0);

  byte *buf = static_cast<byte *>(mem_heap_alloc(heap, 8));
  mach_write_to_8(buf, table->id);

  dfield_set_data(dfield, buf, 8);
  dict_index_copy_types(tuple, sys_index, 1);

  btr_pcur_t pcur;
  pcur.open_on_user_rec(sys_index, tuple, PAGE_CUR_GE, BTR_SEARCH_LEAF, &mtr,
                        UT_LOCATION_HERE);

  ut_ad(table->n_t_cols == static_cast<ulint>(table->n_cols) +
                               static_cast<ulint>(table->n_v_cols));

  size_t non_v_cols = 0;
  size_t n_skipped = 0;
  for (size_t i = 0; i + DATA_N_SYS_COLS < table->n_t_cols + n_skipped; i++) {
    const char *err_msg;
    const char *name = nullptr;
    ulint nth_v_col = ULINT_UNDEFINED;

    const rec_t *rec = pcur.get_rec();

    ut_a(pcur.is_on_user_rec());

    err_msg = dict_load_column_low(table, heap, nullptr, nullptr, &name, rec,
                                   &nth_v_col);

    if (err_msg == dict_load_column_del) {
      n_skipped++;
      goto next_rec;
    } else if (err_msg) {
      ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_195) << err_msg;
    }

    if (nth_v_col == ULINT_UNDEFINED) {
      /* Not a virtual column */
      non_v_cols++;
    }

    /* Note: Currently we have one DOC_ID column that is
    shared by all FTS indexes on a table. And only non-virtual
    column can be used for FULLTEXT index */
    if (innobase_strcasecmp(name, FTS_DOC_ID_COL_NAME) == 0 &&
        nth_v_col == ULINT_UNDEFINED) {
      dict_col_t *col;
      /* As part of normal loading of tables the
      table->flag is not set for tables with FTS
      till after the FTS indexes are loaded. So we
      create the fts_t instance here if there isn't
      one already created.

      This case does not arise for table create as
      the flag is set before the table is created. */

      /* We do not add fts tables to optimize thread
      during upgrade because fts tables will be renamed
      as part of upgrade. These tables will be added
      to fts optimize queue when they are opened. */
      if (table->fts == nullptr && !srv_is_upgrade_mode) {
        table->fts = fts_create(table);
        fts_optimize_add_table(table);
      }

      ut_a(table->fts->doc_col == ULINT_UNDEFINED);

      col = table->get_col(i - n_skipped);

      ut_ad(col->len == sizeof(doc_id_t));

      if (col->prtype & DATA_FTS_DOC_ID) {
        DICT_TF2_FLAG_SET(table, DICT_TF2_FTS_HAS_DOC_ID);
        DICT_TF2_FLAG_UNSET(table, DICT_TF2_FTS_ADD_DOC_ID);
      }

      table->fts->doc_col = i - n_skipped;
    }
  next_rec:
    pcur.move_to_next_user_rec(&mtr);
  }

  pcur.close();
  mtr_commit(&mtr);

  /* The table is getting upgraded from 5.7 where there was no row version */
  table->initial_col_count = table->current_col_count = table->total_col_count =
      non_v_cols;
  table->current_row_version = 0;
}

/** Loads definitions for index fields.
 @return DB_SUCCESS if ok, DB_CORRUPTION if corruption */
static ulint dict_load_fields(
    dict_index_t *index, /*!< in/out: index whose fields to load */
    mem_heap_t *heap)    /*!< in: memory heap for temporary storage */
{
  dict_table_t *sys_fields;
  dict_index_t *sys_index;
  btr_pcur_t pcur;
  dtuple_t *tuple;
  dfield_t *dfield;
  const rec_t *rec;
  byte *buf;
  ulint i;
  mtr_t mtr;
  dberr_t error;

  ut_ad(dict_sys_mutex_own());

  mtr_start(&mtr);

  sys_fields = dict_table_get_low("SYS_FIELDS");
  sys_index = UT_LIST_GET_FIRST(sys_fields->indexes);
  ut_ad(!dict_table_is_comp(sys_fields));
  ut_ad(name_of_col_is(sys_fields, sys_index, DICT_FLD__SYS_FIELDS__COL_NAME,
                       "COL_NAME"));

  tuple = dtuple_create(heap, 1);
  dfield = dtuple_get_nth_field(tuple, 0);

  buf = static_cast<byte *>(mem_heap_alloc(heap, 8));
  mach_write_to_8(buf, index->id);

  dfield_set_data(dfield, buf, 8);
  dict_index_copy_types(tuple, sys_index, 1);

  pcur.open_on_user_rec(sys_index, tuple, PAGE_CUR_GE, BTR_SEARCH_LEAF, &mtr,
                        UT_LOCATION_HERE);
  for (i = 0; i < index->n_fields; i++) {
    const char *err_msg;

    rec = pcur.get_rec();

    ut_a(pcur.is_on_user_rec());

    err_msg = dict_load_field_low(buf, index, heap, rec);

    if (err_msg == dict_load_field_del) {
      /* There could be delete marked records in
      SYS_FIELDS because SYS_FIELDS.INDEX_ID can be
      updated by ALTER TABLE ADD INDEX. */

      goto next_rec;
    } else if (err_msg) {
      ib::error(ER_IB_MSG_196) << err_msg;
      error = DB_CORRUPTION;
      goto func_exit;
    }
  next_rec:
    pcur.move_to_next_user_rec(&mtr);
  }

  error = DB_SUCCESS;
func_exit:
  pcur.close();
  mtr_commit(&mtr);
  return (error);
}

/** Loads definitions for table indexes. Adds them to the data dictionary
 cache.
 @return DB_SUCCESS if ok, DB_CORRUPTION if corruption of dictionary
 table or DB_UNSUPPORTED if table has unknown index type */
static dberr_t dict_load_indexes(
    dict_table_t *table, /*!< in/out: table */
    mem_heap_t *heap,    /*!< in: memory heap for temporary storage */
    dict_err_ignore_t ignore_err)
/*!< in: error to be ignored when
loading the index definition */
{
  dict_table_t *sys_indexes;
  dict_index_t *sys_index;
  btr_pcur_t pcur;
  dtuple_t *tuple;
  dfield_t *dfield;
  const rec_t *rec;
  byte *buf;
  mtr_t mtr;
  dberr_t error = DB_SUCCESS;

  ut_ad(dict_sys_mutex_own());

  mtr_start(&mtr);

  sys_indexes = dict_table_get_low("SYS_INDEXES");
  sys_index = UT_LIST_GET_FIRST(sys_indexes->indexes);
  ut_ad(!dict_table_is_comp(sys_indexes));
  ut_ad(name_of_col_is(sys_indexes, sys_index, DICT_FLD__SYS_INDEXES__NAME,
                       "NAME"));
  ut_ad(name_of_col_is(sys_indexes, sys_index, DICT_FLD__SYS_INDEXES__PAGE_NO,
                       "PAGE_NO"));

  tuple = dtuple_create(heap, 1);
  dfield = dtuple_get_nth_field(tuple, 0);

  buf = static_cast<byte *>(mem_heap_alloc(heap, 8));
  mach_write_to_8(buf, table->id);

  dfield_set_data(dfield, buf, 8);
  dict_index_copy_types(tuple, sys_index, 1);

  pcur.open_on_user_rec(sys_index, tuple, PAGE_CUR_GE, BTR_SEARCH_LEAF, &mtr,
                        UT_LOCATION_HERE);
  for (;;) {
    dict_index_t *index = nullptr;
    const char *err_msg;

    if (!pcur.is_on_user_rec()) {
      /* We should allow the table to open even
      without index when DICT_ERR_IGNORE_CORRUPT is set.
      DICT_ERR_IGNORE_CORRUPT is currently only set
      for drop table */
      if (table->first_index() == nullptr &&
          !(ignore_err & DICT_ERR_IGNORE_CORRUPT)) {
        ib::warn(ER_IB_MSG_197) << "Cannot load table " << table->name
                                << " because it has no indexes in"
                                   " InnoDB internal data dictionary.";
        error = DB_CORRUPTION;
        goto func_exit;
      }

      break;
    }

    rec = pcur.get_rec();

    if ((ignore_err & DICT_ERR_IGNORE_RECOVER_LOCK) &&
        (rec_get_n_fields_old_raw(rec) == DICT_NUM_FIELDS__SYS_INDEXES
         /* a record for older SYS_INDEXES table
         (missing merge_threshold column) is acceptable. */
         ||
         rec_get_n_fields_old_raw(rec) == DICT_NUM_FIELDS__SYS_INDEXES - 1)) {
      const byte *field;
      ulint len;
      field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_INDEXES__NAME,
                                    &len);

      if (len != UNIV_SQL_NULL &&
          static_cast<char>(*field) ==
              static_cast<char>(*TEMP_INDEX_PREFIX_STR)) {
        /* Skip indexes whose name starts with
        TEMP_INDEX_PREFIX, because they will
        be dropped during crash recovery. */
        goto next_rec;
      }
    }

    err_msg =
        dict_load_index_low(buf, table->name.m_name, heap, rec, true, &index);
    ut_ad((index == nullptr && err_msg != nullptr) ||
          (index != nullptr && err_msg == nullptr));

    if (err_msg == dict_load_index_id_err) {
      /* TABLE_ID mismatch means that we have
      run out of index definitions for the table. */

      if (table->first_index() == nullptr &&
          !(ignore_err & DICT_ERR_IGNORE_CORRUPT)) {
        ib::warn(ER_IB_MSG_198)
            << "Failed to load the"
               " clustered index for table "
            << table->name << " because of the following error: " << err_msg
            << "."
               " Refusing to load the rest of the"
               " indexes (if any) and the whole table"
               " altogether.";
        error = DB_CORRUPTION;
        goto func_exit;
      }

      break;
    } else if (err_msg == dict_load_index_del) {
      /* Skip delete-marked records. */
      goto next_rec;
    } else if (err_msg) {
      ib::error(ER_IB_MSG_199) << err_msg;
      if (ignore_err & DICT_ERR_IGNORE_CORRUPT) {
        goto next_rec;
      }
      error = DB_CORRUPTION;
      goto func_exit;
    }

    ut_ad(index);

    /* Check whether the index is corrupted */
    if (index->is_corrupted()) {
      ib::error(ER_IB_MSG_200) << "Index " << index->name << " of table "
                               << table->name << " is corrupted";

      if (!srv_load_corrupted && !(ignore_err & DICT_ERR_IGNORE_CORRUPT) &&
          index->is_clustered()) {
        dict_mem_index_free(index);

        error = DB_INDEX_CORRUPT;
        goto func_exit;
      } else {
        /* We will load the index if
        1) srv_load_corrupted is true
        2) ignore_err is set with
        DICT_ERR_IGNORE_CORRUPT
        3) if the index corrupted is a secondary
        index */
        ib::info(ER_IB_MSG_201) << "Load corrupted index " << index->name
                                << " of table " << table->name;
      }
    }

    if (index->type & DICT_FTS && !dict_table_has_fts_index(table)) {
      /* This should have been created by now. */
      ut_a(table->fts != nullptr);
      DICT_TF2_FLAG_SET(table, DICT_TF2_FTS);
    }

    /* We check for unsupported types first, so that the
    subsequent checks are relevant for the supported types. */
    if (index->type & ~(DICT_CLUSTERED | DICT_UNIQUE | DICT_CORRUPT | DICT_FTS |
                        DICT_SPATIAL | DICT_VIRTUAL)) {
      ib::error(ER_IB_MSG_202) << "Unknown type " << index->type << " of index "
                               << index->name << " of table " << table->name;

      error = DB_UNSUPPORTED;
      dict_mem_index_free(index);
      goto func_exit;
    } else if (!index->is_clustered() && nullptr == table->first_index()) {
      ib::error(ER_IB_MSG_203)
          << "Trying to load index " << index->name << " for table "
          << table->name << ", but the first index is not clustered!";

      dict_mem_index_free(index);
      error = DB_CORRUPTION;
      goto func_exit;
    } else if (dict_is_old_sys_table(table->id) &&
               (index->is_clustered() || ((table == dict_sys->sys_tables) &&
                                          !strcmp("ID_IND", index->name)))) {
      /* The index was created in memory already at booting
      of the database server */
      dict_mem_index_free(index);
    } else {
      dict_load_fields(index, heap);

      dict_sys_mutex_exit();

      error = dict_index_add_to_cache(table, index, index->page, false);

      dict_sys_mutex_enter();

      /* The data dictionary tables should never contain
      invalid index definitions. */
      if (UNIV_UNLIKELY(error != DB_SUCCESS)) {
        goto func_exit;
      }
    }
  next_rec:
    pcur.move_to_next_user_rec(&mtr);
  }

  ut_ad(table->fts_doc_id_index == nullptr);

  if (table->fts != nullptr) {
    table->fts_doc_id_index =
        dict_table_get_index_on_name(table, FTS_DOC_ID_INDEX_NAME);
  }

  /* If the table contains FTS indexes, populate table->fts->indexes */
  if (dict_table_has_fts_index(table)) {
    ut_ad(table->fts_doc_id_index != nullptr);
    /* table->fts->indexes should have been created. */
    ut_a(table->fts->indexes != nullptr);
    dict_table_get_all_fts_indexes(table, table->fts->indexes);
  }

func_exit:
  pcur.close();
  mtr_commit(&mtr);

  return (error);
}

/** Loads a table definition from a SYS_TABLES record to dict_table_t.
Does not load any columns or indexes.
@param[in]      name    Table name
@param[in]      rec     SYS_TABLES record
@param[out]     table   Table, or NULL
@return error message, or NULL on success */
static const char *dict_load_table_low(table_name_t &name, const rec_t *rec,
                                       dict_table_t **table) {
  table_id_t table_id;
  space_id_t space_id;
  uint32_t n_cols;
  uint32_t t_num;
  uint32_t flags;
  uint32_t flags2;
  uint32_t n_v_col;

  const char *error_text = dict_sys_tables_rec_check(rec);
  if (error_text != nullptr) {
    return (error_text);
  }

  dict_sys_tables_rec_read(rec, name, &table_id, &space_id, &t_num, &flags,
                           &flags2);

  if (flags == UINT32_UNDEFINED) {
    return ("incorrect flags in SYS_TABLES");
  }

  dict_table_decode_n_col(t_num, &n_cols, &n_v_col);

  std::string table_name(name.m_name);
  /* Check and convert 5.7 table name. */
  if (dict_name::is_partition(table_name)) {
    dict_name::rebuild(table_name);
  }

  *table = dict_mem_table_create(table_name.c_str(), space_id, n_cols + n_v_col,
                                 n_v_col, 0, flags, flags2);

  (*table)->id = table_id;
  (*table)->ibd_file_missing = false;

  return (nullptr);
}

/** Using the table->heap, copy the null-terminated filepath into
table->data_dir_path. The data directory path is derived from the
filepath by stripping the the table->name.m_name component suffix.
If the filepath is not of the correct form (".../db/table.ibd"),
then table->data_dir_path will remain nullptr.
@param[in,out]  table           table instance
@param[in]      filepath        filepath of tablespace */
void dict_save_data_dir_path(dict_table_t *table, char *filepath) {
  ut_ad(dict_sys_mutex_own());
  ut_ad(DICT_TF_HAS_DATA_DIR(table->flags));
  ut_ad(table->data_dir_path == nullptr);
  ut_a(Fil_path::has_suffix(IBD, filepath));

  /* Ensure this filepath is not the default filepath. */
  char *default_filepath = Fil_path::make("", table->name.m_name, IBD);

  if (default_filepath == nullptr) {
    /* Memory allocation problem. */
    return;
  }

  if (strcmp(filepath, default_filepath) != 0) {
    size_t pathlen = strlen(filepath);

    ut_a(pathlen < OS_FILE_MAX_PATH);
    ut_a(Fil_path::has_suffix(IBD, filepath));

    char *data_dir_path = mem_heap_strdup(table->heap, filepath);

    Fil_path::make_data_dir_path(data_dir_path);

    if (strlen(data_dir_path)) {
      table->data_dir_path = data_dir_path;
    }
  }

  ut::free(default_filepath);
}

/** Make sure the data_dir_path is saved in dict_table_t if DATA DIRECTORY
was used. Try to read it from the fil_system first, then from SYS_DATAFILES.
@param[in]      table           Table object
@param[in]      dict_mutex_own  true if dict_sys->mutex is owned already */
void dict_get_and_save_data_dir_path(dict_table_t *table, bool dict_mutex_own) {
  if (!(DICT_TF_HAS_DATA_DIR(table->flags) &&
        table->data_dir_path == nullptr)) {
    return;
  }

  char *path = fil_space_get_first_path(table->space);

  if (!dict_mutex_own) {
    dict_mutex_enter_for_mysql();
  }

  if (path == nullptr) {
    path = dict_get_first_path(table->space);
  }

  if (path != nullptr) {
    dict_save_data_dir_path(table, path);
    ut::free(path);
  }

  ut_ad(table->data_dir_path != nullptr);

  if (!dict_mutex_own) {
    dict_mutex_exit_for_mysql();
  }
}

void dict_get_and_save_space_name(dict_table_t *table) {
  /* Do this only for general tablespaces. */
  if (!DICT_TF_HAS_SHARED_SPACE(table->flags)) {
    return;
  }

  bool use_cache = true;
  if (table->tablespace != nullptr) {
    if (srv_sys_tablespaces_open &&
        dict_table_has_temp_general_tablespace_name(table->tablespace)) {
      /* We previous saved the temporary name,
      get the real one now. */
      use_cache = false;
    } else {
      /* Keep and use this name */
      return;
    }
  }

  if (use_cache) {
    fil_space_t *space = fil_space_acquire_silent(table->space);

    if (space != nullptr) {
      /* Use this name unless it is a temporary general
      tablespace name and we can now replace it. */
      if (!srv_sys_tablespaces_open ||
          !dict_table_has_temp_general_tablespace_name(space->name)) {
        /* Use this tablespace name */
        table->tablespace = mem_heap_strdup(table->heap, space->name);

        fil_space_release(space);
        return;
      }
      fil_space_release(space);
    }
  }
}

dict_table_t *dict_load_table(const char *name, bool cached,
                              dict_err_ignore_t ignore_err,
                              const std::string *prev_table) {
  dict_names_t fk_list;
  dict_names_t::iterator i;
  table_name_t table_name;
  dict_table_t *result;

  DBUG_TRACE;
  DBUG_PRINT("dict_load_table", ("loading table: '%s'", name));

  if (prev_table != nullptr && prev_table->compare(name) == 0) {
    return nullptr;
  }

  const std::string cur_table(name);

  ut_ad(dict_sys_mutex_own());

  result = dict_table_check_if_in_cache_low(name);

  table_name.m_name = const_cast<char *>(name);

  if (!result) {
    result = dict_load_table_one(table_name, cached, ignore_err, fk_list,
                                 &cur_table);
    while (!fk_list.empty()) {
      table_name_t fk_table_name;
      dict_table_t *fk_table;

      fk_table_name.m_name = const_cast<char *>(fk_list.front());
      fk_table = dict_table_check_if_in_cache_low(fk_table_name.m_name);
      if (!fk_table) {
        dict_load_table_one(fk_table_name, cached, ignore_err, fk_list,
                            &cur_table);
      }
      fk_list.pop_front();
    }
  }

  return result;
}

void dict_load_tablespace(dict_table_t *table, dict_err_ignore_t ignore_err) {
  ut_ad(!table->is_temporary());

  /* The system and temporary tablespaces are preloaded and always available. */
  if (fsp_is_system_or_temp_tablespace(table->space)) {
    return;
  }

  if (dict_table_is_discarded(table)) {
    ib::warn(ER_IB_MSG_204)
        << "Tablespace for table " << table->name << " is set as discarded.";
    table->ibd_file_missing = true;
    return;
  }

  /* A file-per-table table name is also the tablespace name.
  A general tablespace name is not the same as the table name.
  Use the general tablespace name if it can be read from the
  dictionary, if not use 'innodb_general_##. */
  char *shared_space_name = nullptr;
  std::string tablespace_name;
  const char *space_name;
  const char *tbl_name;

  if (DICT_TF_HAS_SHARED_SPACE(table->flags)) {
    if (table->space == dict_sys_t::s_dict_space_id) {
      shared_space_name = mem_strdup(dict_sys_t::s_dd_space_name);
    } else if (srv_sys_tablespaces_open) {
      shared_space_name = dict_space_get_name(table->space, nullptr);

    } else {
      /* Make the temporary tablespace name. */
      shared_space_name = static_cast<char *>(ut::malloc_withkey(
          UT_NEW_THIS_FILE_PSI_KEY, strlen(general_space_name) + 20));

      sprintf(shared_space_name, "%s_" ULINTPF, general_space_name,
              static_cast<ulint>(table->space));
    }
    tbl_name = shared_space_name;
    space_name = shared_space_name;

  } else {
    tbl_name = table->name.m_name;

    tablespace_name.assign(tbl_name);
    dict_name::convert_to_space(tablespace_name);
    space_name = tablespace_name.c_str();
  }

  /* The tablespace may already be open. */
  if (fil_space_exists_in_mem(table->space, space_name, false, true)) {
    ut::free(shared_space_name);
    return;
  }

  if (!(ignore_err & DICT_ERR_IGNORE_RECOVER_LOCK) && !srv_is_upgrade_mode) {
    ib::error(ER_IB_MSG_205)
        << "Failed to find tablespace for table " << table->name
        << " in the cache. Attempting"
           " to load the tablespace with space id "
        << table->space;
  }

  /* Use the remote filepath if needed. This parameter is optional
  in the call to fil_ibd_open(). If not supplied, it will be built
  from the space_name. */
  char *filepath = nullptr;
  if (DICT_TF_HAS_DATA_DIR(table->flags)) {
    /* This will set table->data_dir_path from either
    fil_system or SYS_DATAFILES */
    dict_get_and_save_data_dir_path(table, true);

    if (table->data_dir_path != nullptr) {
      filepath = Fil_path::make(table->data_dir_path, table->name.m_name, IBD);
    }

  } else if (DICT_TF_HAS_SHARED_SPACE(table->flags)) {
    /* Set table->tablespace from either
    fil_system or SYS_TABLESPACES */
    dict_get_and_save_space_name(table);

    /* Set the filepath from either
    fil_system or SYS_DATAFILES. */
    filepath = dict_get_first_path(table->space);
    if (filepath == nullptr) {
      ib::warn(ER_IB_MSG_206) << "Could not find the filepath"
                                 " for table "
                              << table->name << ", space ID " << table->space;
    }
  }

  /* Try to open the tablespace.  We set the 2nd param (fix_dict) to
  false because we do not have an x-lock on dict_operation_lock */
  uint32_t fsp_flags = dict_tf_to_fsp_flags(table->flags);
  /* Set tablespace encryption flag */
  if (DICT_TF2_FLAG_IS_SET(table, DICT_TF2_ENCRYPTION_FILE_PER_TABLE)) {
    fsp_flags_set_encryption(fsp_flags);
  }

  /* This dict_load_tablespace() is only used on old 5.7 database during
  upgrade */
  dberr_t err = fil_ibd_open(true, FIL_TYPE_TABLESPACE, table->space, fsp_flags,
                             space_name, filepath, true, true);

  if (err != DB_SUCCESS) {
    /* We failed to find a sensible tablespace file */
    table->ibd_file_missing = true;
  }

  ut::free(shared_space_name);
  ut::free(filepath);
}

static dict_table_t *dict_load_table_one(table_name_t &name, bool cached,
                                         dict_err_ignore_t ignore_err,
                                         dict_names_t &fk_tables,
                                         const std::string *prev_table) {
  dberr_t err;
  dict_table_t *table;
  btr_pcur_t pcur;
  dict_index_t *sys_index;
  dtuple_t *tuple;
  mem_heap_t *heap;
  dfield_t *dfield;
  const rec_t *rec;
  const byte *field;
  ulint len;
  const char *err_msg;
  mtr_t mtr;

  DBUG_TRACE;
  DBUG_PRINT("dict_load_table_one", ("table: %s", name.m_name));

  ut_ad(dict_sys_mutex_own());

  dict_table_t *sys_tables = dict_table_get_low("SYS_TABLES", prev_table);
  if (sys_tables == nullptr) {
    return nullptr;
  }

  heap = mem_heap_create(32000, UT_LOCATION_HERE);

  mtr_start(&mtr);
  sys_index = UT_LIST_GET_FIRST(sys_tables->indexes);
  ut_ad(!dict_table_is_comp(sys_tables));
  ut_ad(name_of_col_is(sys_tables, sys_index, DICT_FLD__SYS_TABLES__ID, "ID"));
  ut_ad(name_of_col_is(sys_tables, sys_index, DICT_FLD__SYS_TABLES__N_COLS,
                       "N_COLS"));
  ut_ad(name_of_col_is(sys_tables, sys_index, DICT_FLD__SYS_TABLES__TYPE,
                       "TYPE"));
  ut_ad(name_of_col_is(sys_tables, sys_index, DICT_FLD__SYS_TABLES__MIX_LEN,
                       "MIX_LEN"));
  ut_ad(name_of_col_is(sys_tables, sys_index, DICT_FLD__SYS_TABLES__SPACE,
                       "SPACE"));

  tuple = dtuple_create(heap, 1);
  dfield = dtuple_get_nth_field(tuple, 0);

  /* We suffix "_backup57" to 5.7 statistics tables/.ibds. This is
  to avoid conflict with 8.0 statistics tables. Since InnoDB dictionary
  refers 5.7 stats tables without the sufix, we strip the suffix and
  search in dictionary. */
  bool is_stats = false;
  if (strcmp(name.m_name, "mysql/innodb_index_stats_backup57") == 0 ||
      strcmp(name.m_name, "mysql/innodb_table_stats_backup57") == 0) {
    is_stats = true;
  }

  std::string orig_name(name.m_name);

  if (is_stats) {
    /* To load 5.7 stats tables, we search the table names
    with "_backup57" suffix. We now strip the suffix before
    searching InnoDB Dictionary */
    std::string substr("_backup57");
    std::size_t found = orig_name.find(substr);
    ut_ad(found != std::string::npos);
    orig_name.erase(found, substr.length());

    dfield_set_data(dfield, orig_name.c_str(), orig_name.length());
  } else {
    dfield_set_data(dfield, name.m_name, ut_strlen(name.m_name));
  }

  dict_index_copy_types(tuple, sys_index, 1);

  pcur.open_on_user_rec(sys_index, tuple, PAGE_CUR_GE, BTR_SEARCH_LEAF, &mtr,
                        UT_LOCATION_HERE);
  rec = pcur.get_rec();

  if (!pcur.is_on_user_rec() || rec_get_deleted_flag(rec, 0)) {
    /* Not found */
  err_exit:
    pcur.close();
    mtr_commit(&mtr);
    mem_heap_free(heap);

    return nullptr;
  }

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_TABLES__NAME, &len);

  /* Check if the table name in record is the searched one */
  if (!is_stats && (len != ut_strlen(name.m_name) ||
                    0 != ut_memcmp(name.m_name, field, len))) {
    goto err_exit;
  }

  err_msg = dict_load_table_low(name, rec, &table);

  if (err_msg) {
    ib::error(ER_IB_MSG_207) << err_msg;
    goto err_exit;
  }

  pcur.close();
  mtr_commit(&mtr);

  dict_load_tablespace(table, ignore_err);

  dict_load_columns(table, heap);

  dict_load_virtual(table, heap);

  dict_table_add_system_columns(table, heap);

  mem_heap_empty(heap);

  /* If there is no tablespace for the table then we only need to
  load the index definitions. So that we can IMPORT the tablespace
  later. When recovering table locks for resurrected incomplete
  transactions, the tablespace should exist, because DDL operations
  were not allowed while the table is being locked by a transaction. */
  dict_err_ignore_t index_load_err =
      !(ignore_err & DICT_ERR_IGNORE_RECOVER_LOCK) && table->ibd_file_missing
          ? DICT_ERR_IGNORE_ALL
          : ignore_err;
  err = dict_load_indexes(table, heap, index_load_err);

  if (err == DB_SUCCESS) {
    if (srv_is_upgrade_mode && !srv_upgrade_old_undo_found &&
        !dict_load_is_system_table(table->name.m_name)) {
      table->id = table->id + DICT_MAX_DD_TABLES;
    }
    if (cached) {
      dict_table_add_to_cache(table, true);
    }
  }

  if (dict_sys->dynamic_metadata != nullptr) {
    dict_table_load_dynamic_metadata(table);
  }

  /* Re-check like we do in dict_load_indexes() */
  if (!srv_load_corrupted && !(index_load_err & DICT_ERR_IGNORE_CORRUPT) &&
      table->is_corrupted()) {
    err = DB_INDEX_CORRUPT;
  }

  if (err == DB_INDEX_CORRUPT) {
    /* Refuse to load the table if the table has a corrupted
    clustered index */
    ut_ad(!srv_load_corrupted);

    ib::error(ER_IB_MSG_208) << "Load table " << table->name
                             << " failed, the table contains a"
                                " corrupted clustered index. Turn on"
                                " 'innodb_force_load_corrupted' to drop it";
    dict_table_remove_from_cache(table);
    table = nullptr;
    goto func_exit;
  }

  /* We don't trust the table->flags2(retrieved from SYS_TABLES.MIX_LEN
  field) if the datafiles are from 3.23.52 version. To identify this
  version, we do the below check and reset the flags. */
  if (!DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID) &&
      table->space == TRX_SYS_SPACE && table->flags == 0) {
    table->flags2 = 0;
  }

  DBUG_EXECUTE_IF(
      "ib_table_invalid_flags",
      if (strcmp(table->name.m_name, "test/t1") == 0) {
        table->flags2 = 255;
        table->flags = 255;
      });

  if (!dict_tf2_is_valid(table->flags, table->flags2)) {
    ib::error(ER_IB_MSG_209) << "Table " << table->name
                             << " in InnoDB"
                                " data dictionary contains invalid flags."
                                " SYS_TABLES.MIX_LEN="
                             << table->flags2;
    table->flags2 &= ~(DICT_TF2_TEMPORARY | DICT_TF2_INTRINSIC);
    dict_table_remove_from_cache(table);
    table = nullptr;
    err = DB_FAIL;
    goto func_exit;
  }

  /* Initialize table foreign_child value. Its value could be
  changed when dict_load_foreigns() is called below */
  table->fk_max_recusive_level = 0;

  /* If the force recovery flag is set, we open the table irrespective
  of the error condition, since the user may want to dump data from the
  clustered index. However we load the foreign key information only if
  all indexes were loaded. */
  if (!cached || table->ibd_file_missing) {
    /* Don't attempt to load the indexes from disk. */
  } else if (err == DB_SUCCESS) {
    err = dict_load_foreigns(table->name.m_name, nullptr, true, true,
                             ignore_err, fk_tables);

    if (err != DB_SUCCESS) {
      ib::warn(ER_IB_MSG_210) << "Load table " << table->name
                              << " failed, the table has missing"
                                 " foreign key indexes. Turn off"
                                 " 'foreign_key_checks' and try again.";

      dict_table_remove_from_cache(table);
      table = nullptr;
    } else {
      dict_mem_table_free_foreign_vcol_set(table);
      dict_mem_table_fill_foreign_vcol_set(table);
      table->fk_max_recusive_level = 0;
    }
  } else {
    dict_index_t *index;

    /* Make sure that at least the clustered index was loaded.
    Otherwise refuse to load the table */
    index = table->first_index();

    if (!srv_force_recovery || !index || !index->is_clustered()) {
      dict_table_remove_from_cache(table);
      table = nullptr;
    }
  }

func_exit:
  mem_heap_free(heap);

  ut_ad(!table || ignore_err != DICT_ERR_IGNORE_NONE ||
        table->ibd_file_missing || !table->is_corrupted());

  if (table && table->fts) {
    /* We do not add fts tables to optimize thread
    during upgrade because fts tables will be renamed
    as part of upgrade. These tables will be added
    to fts optimize queue when they are opened. */

    if (!(dict_table_has_fts_index(table) ||
          DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_HAS_DOC_ID) ||
          DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_ADD_DOC_ID))) {
      /* the table->fts could be created in dict_load_column
      when a user defined FTS_DOC_ID is present, but no
      FTS */
      fts_optimize_remove_table(table);
      fts_free(table);
    } else if (!srv_is_upgrade_mode) {
      fts_optimize_add_table(table);
    }
  }

  ut_ad(err != DB_SUCCESS || dict_foreign_set_validate(*table));

  return table;
}

/** This function is called when the database is booted. Loads system table
 index definitions except for the clustered index which is added to the
 dictionary cache at booting before calling this function. */
void dict_load_sys_table(dict_table_t *table) /*!< in: system table */
{
  mem_heap_t *heap;

  ut_ad(dict_sys_mutex_own());

  heap = mem_heap_create(100, UT_LOCATION_HERE);

  dict_load_indexes(table, heap, DICT_ERR_IGNORE_NONE);

  mem_heap_free(heap);
}

/** Loads foreign key constraint col names (also for the referenced table).
 Members that must be set (and valid) in foreign:
 foreign->heap
 foreign->n_fields
 foreign->id ('\0'-terminated)
 Members that will be created and set by this function:
 foreign->foreign_col_names[i]
 foreign->referenced_col_names[i]
 (for i=0..foreign->n_fields-1) */
static void dict_load_foreign_cols(
    dict_foreign_t *foreign) /*!< in/out: foreign constraint object */
{
  dict_table_t *sys_foreign_cols;
  dict_index_t *sys_index;
  btr_pcur_t pcur;
  dtuple_t *tuple;
  dfield_t *dfield;
  const rec_t *rec;
  const byte *field;
  ulint len;
  ulint i;
  mtr_t mtr;
  size_t id_len;

  ut_ad(dict_sys_mutex_own());

  id_len = strlen(foreign->id);

  foreign->foreign_col_names = static_cast<const char **>(
      mem_heap_alloc(foreign->heap, foreign->n_fields * sizeof(void *)));

  foreign->referenced_col_names = static_cast<const char **>(
      mem_heap_alloc(foreign->heap, foreign->n_fields * sizeof(void *)));

  mtr_start(&mtr);

  sys_foreign_cols = dict_table_get_low("SYS_FOREIGN_COLS");

  sys_index = UT_LIST_GET_FIRST(sys_foreign_cols->indexes);
  ut_ad(!dict_table_is_comp(sys_foreign_cols));

  tuple = dtuple_create(foreign->heap, 1);
  dfield = dtuple_get_nth_field(tuple, 0);

  dfield_set_data(dfield, foreign->id, id_len);
  dict_index_copy_types(tuple, sys_index, 1);

  pcur.open_on_user_rec(sys_index, tuple, PAGE_CUR_GE, BTR_SEARCH_LEAF, &mtr,
                        UT_LOCATION_HERE);
  for (i = 0; i < foreign->n_fields; i++) {
    rec = pcur.get_rec();

    ut_a(pcur.is_on_user_rec());
    ut_a(!rec_get_deleted_flag(rec, 0));

    field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_FOREIGN_COLS__ID,
                                  &len);

    if (len != id_len || ut_memcmp(foreign->id, field, len) != 0) {
      const rec_t *pos;
      ulint pos_len;
      const rec_t *for_col_name;
      ulint for_col_name_len;
      const rec_t *ref_col_name;
      ulint ref_col_name_len;

      pos = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_FOREIGN_COLS__POS,
                                  &pos_len);

      for_col_name = rec_get_nth_field_old(
          nullptr, rec, DICT_FLD__SYS_FOREIGN_COLS__FOR_COL_NAME,
          &for_col_name_len);

      ref_col_name = rec_get_nth_field_old(
          nullptr, rec, DICT_FLD__SYS_FOREIGN_COLS__REF_COL_NAME,
          &ref_col_name_len);

      ib::fatal sout(UT_LOCATION_HERE);

      sout << "Unable to load column names for foreign"
              " key '"
           << foreign->id
           << "' because it was not found in"
              " InnoDB internal table SYS_FOREIGN_COLS. The"
              " closest entry we found is:"
              " (ID='";
      sout.write(field, len);
      sout << "', POS=" << mach_read_from_4(pos) << ", FOR_COL_NAME='";
      sout.write(for_col_name, for_col_name_len);
      sout << "', REF_COL_NAME='";
      sout.write(ref_col_name, ref_col_name_len);
      sout << "')";
    }

    field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_FOREIGN_COLS__POS,
                                  &len);
    ut_a(len == 4);
    ut_a(i == mach_read_from_4(field));

    field = rec_get_nth_field_old(
        nullptr, rec, DICT_FLD__SYS_FOREIGN_COLS__FOR_COL_NAME, &len);
    foreign->foreign_col_names[i] =
        mem_heap_strdupl(foreign->heap, (char *)field, len);

    field = rec_get_nth_field_old(
        nullptr, rec, DICT_FLD__SYS_FOREIGN_COLS__REF_COL_NAME, &len);
    foreign->referenced_col_names[i] =
        mem_heap_strdupl(foreign->heap, (char *)field, len);

    pcur.move_to_next_user_rec(&mtr);
  }

  pcur.close();
  mtr_commit(&mtr);
}

/** Loads a foreign key constraint to the dictionary cache. If the referenced
 table is not yet loaded, it is added in the output parameter (fk_tables).
 @return DB_SUCCESS or error code */
[[nodiscard]] static dberr_t dict_load_foreign(
    const char *id,
    /*!< in: foreign constraint id, must be
    '\0'-terminated */
    const char **col_names,
    /*!< in: column names, or NULL
    to use foreign->foreign_table->col_names */
    bool check_recursive,
    /*!< in: whether to record the foreign table
    parent count to avoid unlimited recursive
    load of chained foreign tables */
    bool check_charsets,
    /*!< in: whether to check charset
    compatibility */
    dict_err_ignore_t ignore_err,
    /*!< in: error to be ignored */
    dict_names_t &fk_tables)
/*!< out: the foreign key constraint is added
to the dictionary cache only if the referenced
table is already in cache.  Otherwise, the
foreign key constraint is not added to cache,
and the referenced table is added to this
stack. */
{
  dict_foreign_t *foreign;
  dict_table_t *sys_foreign;
  btr_pcur_t pcur;
  dict_index_t *sys_index;
  dtuple_t *tuple;
  mem_heap_t *heap2;
  dfield_t *dfield;
  const rec_t *rec;
  const byte *field;
  ulint len;
  ulint n_fields_and_type;
  mtr_t mtr;
  dict_table_t *for_table;
  dict_table_t *ref_table;
  size_t id_len;

  DBUG_TRACE;
  DBUG_PRINT("dict_load_foreign",
             ("id: '%s', check_recursive: %d", id, check_recursive));

  ut_ad(dict_sys_mutex_own());

  id_len = strlen(id);

  heap2 = mem_heap_create(100, UT_LOCATION_HERE);

  mtr_start(&mtr);

  sys_foreign = dict_table_get_low("SYS_FOREIGN");

  sys_index = UT_LIST_GET_FIRST(sys_foreign->indexes);
  ut_ad(!dict_table_is_comp(sys_foreign));

  tuple = dtuple_create(heap2, 1);
  dfield = dtuple_get_nth_field(tuple, 0);

  dfield_set_data(dfield, id, id_len);
  dict_index_copy_types(tuple, sys_index, 1);

  pcur.open_on_user_rec(sys_index, tuple, PAGE_CUR_GE, BTR_SEARCH_LEAF, &mtr,
                        UT_LOCATION_HERE);
  rec = pcur.get_rec();

  if (!pcur.is_on_user_rec() || rec_get_deleted_flag(rec, 0)) {
    /* Not found */

    ib::error(ER_IB_MSG_211) << "Cannot load foreign constraint " << id
                             << ": could not find the relevant record in "
                             << "SYS_FOREIGN";

    pcur.close();
    mtr_commit(&mtr);
    mem_heap_free(heap2);

    return DB_ERROR;
  }

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_FOREIGN__ID, &len);

  /* Check if the id in record is the searched one */
  if (len != id_len || ut_memcmp(id, field, len) != 0) {
    {
      ib::error err(ER_IB_MSG_1227);
      err << "Cannot load foreign constraint " << id << ": found ";
      err.write(field, len);
      err << " instead in SYS_FOREIGN";
    }

    pcur.close();
    mtr_commit(&mtr);
    mem_heap_free(heap2);

    return DB_ERROR;
  }

  /* Read the table names and the number of columns associated
  with the constraint */

  mem_heap_free(heap2);

  foreign = dict_mem_foreign_create();

  n_fields_and_type = mach_read_from_4(
      rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_FOREIGN__N_COLS, &len));

  ut_a(len == 4);

  /* We store the type in the bits 24..29 of n_fields_and_type. */

  foreign->type = (unsigned int)(n_fields_and_type >> 24);
  foreign->n_fields = (unsigned int)(n_fields_and_type & 0x3FFUL);

  foreign->id = mem_heap_strdupl(foreign->heap, id, id_len);

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_FOREIGN__FOR_NAME,
                                &len);

  foreign->foreign_table_name =
      mem_heap_strdupl(foreign->heap, (char *)field, len);
  dict_mem_foreign_table_name_lookup_set(foreign, true);

  const ulint foreign_table_name_len = len;

  field = rec_get_nth_field_old(nullptr, rec, DICT_FLD__SYS_FOREIGN__REF_NAME,
                                &len);
  foreign->referenced_table_name =
      mem_heap_strdupl(foreign->heap, (char *)field, len);
  dict_mem_referenced_table_name_lookup_set(foreign, true);

  pcur.close();
  mtr_commit(&mtr);

  dict_load_foreign_cols(foreign);

  ref_table =
      dict_table_check_if_in_cache_low(foreign->referenced_table_name_lookup);
  for_table =
      dict_table_check_if_in_cache_low(foreign->foreign_table_name_lookup);

  if (!for_table) {
    /* To avoid recursively loading the tables related through
    the foreign key constraints, the child table name is saved
    here.  The child table will be loaded later, along with its
    foreign key constraint. */

    lint old_size = mem_heap_get_size(ref_table->heap);

    ut_a(ref_table != nullptr);
    fk_tables.push_back(mem_heap_strdupl(ref_table->heap,
                                         foreign->foreign_table_name_lookup,
                                         foreign_table_name_len));

    lint new_size = mem_heap_get_size(ref_table->heap);
    dict_sys->size += new_size - old_size;

    dict_foreign_remove_from_cache(foreign);
    return DB_SUCCESS;
  }

  ut_a(for_table || ref_table);

  /* Note that there may already be a foreign constraint object in
  the dictionary cache for this constraint: then the following
  call only sets the pointers in it to point to the appropriate table
  and index objects and frees the newly created object foreign.
  Adding to the cache should always succeed since we are not creating
  a new foreign key constraint but loading one from the data
  dictionary. */

  return dict_foreign_add_to_cache(foreign, col_names, check_charsets, true,
                                   ignore_err);
}

/** Loads foreign key constraints where the table is either the foreign key
 holder or where the table is referenced by a foreign key. Adds these
 constraints to the data dictionary.

 The foreign key constraint is loaded only if the referenced table is also
 in the dictionary cache.  If the referenced table is not in dictionary
 cache, then it is added to the output parameter (fk_tables).

 @return DB_SUCCESS or error code */
dberr_t dict_load_foreigns(
    const char *table_name,       /*!< in: table name */
    const char **col_names,       /*!< in: column names, or NULL
                                  to use table->col_names */
    bool check_recursive,         /*!< in: Whether to check
                                  recursive load of tables
                                  chained by FK */
    bool check_charsets,          /*!< in: whether to check
                                  charset compatibility */
    dict_err_ignore_t ignore_err, /*!< in: error to be ignored */
    dict_names_t &fk_tables)
/*!< out: stack of table
names which must be loaded
subsequently to load all the
foreign key constraints. */
{
  ulint tuple_buf[(DTUPLE_EST_ALLOC(1) + sizeof(ulint) - 1) / sizeof(ulint)];
  btr_pcur_t pcur;
  dtuple_t *tuple;
  dfield_t *dfield;
  dict_index_t *sec_index;
  dict_table_t *sys_foreign;
  const rec_t *rec;
  const byte *field;
  ulint len;
  dberr_t err;
  mtr_t mtr;

  DBUG_TRACE;

  ut_ad(dict_sys_mutex_own());

  sys_foreign = dict_table_get_low("SYS_FOREIGN");

  if (sys_foreign == nullptr) {
    /* No foreign keys defined yet in this database */

    ib::info(ER_IB_MSG_212) << "No foreign key system tables in the database";
    return DB_ERROR;
  }

  ut_ad(!dict_table_is_comp(sys_foreign));
  mtr_start(&mtr);

  /* Get the secondary index based on FOR_NAME from table
  SYS_FOREIGN */

  sec_index = sys_foreign->first_index()->next();
  ut_ad(!sec_index->is_clustered());
start_load:

  tuple = dtuple_create_from_mem(tuple_buf, sizeof(tuple_buf), 1, 0);
  dfield = dtuple_get_nth_field(tuple, 0);

  dfield_set_data(dfield, table_name, ut_strlen(table_name));
  dict_index_copy_types(tuple, sec_index, 1);

  pcur.open_on_user_rec(sec_index, tuple, PAGE_CUR_GE, BTR_SEARCH_LEAF, &mtr,
                        UT_LOCATION_HERE);
loop:
  rec = pcur.get_rec();

  if (!pcur.is_on_user_rec()) {
    /* End of index */

    goto load_next_index;
  }

  /* Now we have the record in the secondary index containing a table
  name and a foreign constraint ID */

  field = rec_get_nth_field_old(nullptr, rec,
                                DICT_FLD__SYS_FOREIGN_FOR_NAME__NAME, &len);

  /* Check if the table name in the record is the one searched for; the
  following call does the comparison in the latin1_swedish_ci
  charset-collation, in a case-insensitive way. */

  if (0 != cmp_data_data(dfield_get_type(dfield)->mtype,
                         dfield_get_type(dfield)->prtype, true,
                         static_cast<const byte *>(dfield_get_data(dfield)),
                         dfield_get_len(dfield), field, len)) {
    goto load_next_index;
  }

  /* Since table names in SYS_FOREIGN are stored in a case-insensitive
  order, we have to check that the table name matches also in a binary
  string comparison. On Unix, MySQL allows table names that only differ
  in character case.  If lower_case_table_names=2 then what is stored
  may not be the same case, but the previous comparison showed that they
  match with no-case.  */

  if (rec_get_deleted_flag(rec, 0)) {
    goto next_rec;
  }

  if ((innobase_get_lower_case_table_names() != 2) &&
      (0 != ut_memcmp(field, table_name, len))) {
    goto next_rec;
  }

  /* Now we get a foreign key constraint id */
  field = rec_get_nth_field_old(nullptr, rec,
                                DICT_FLD__SYS_FOREIGN_FOR_NAME__ID, &len);

  /* Copy the string because the page may be modified or evicted
  after mtr_commit() below. */
  char fk_id[MAX_TABLE_NAME_LEN + 1];

  ut_a(len <= MAX_TABLE_NAME_LEN);
  memcpy(fk_id, field, len);
  fk_id[len] = '\0';

  pcur.store_position(&mtr);

  mtr_commit(&mtr);

  /* Load the foreign constraint definition to the dictionary cache */

  err = dict_load_foreign(fk_id, col_names, check_recursive, check_charsets,
                          ignore_err, fk_tables);

  if (err != DB_SUCCESS) {
    pcur.close();

    return err;
  }

  mtr_start(&mtr);

  pcur.restore_position(BTR_SEARCH_LEAF, &mtr, UT_LOCATION_HERE);
next_rec:
  pcur.move_to_next_user_rec(&mtr);
  goto loop;

load_next_index:
  pcur.close();
  mtr_commit(&mtr);

  sec_index = sec_index->next();

  if (sec_index != nullptr) {
    mtr_start(&mtr);

    /* Switch to scan index on REF_NAME, fk_max_recusive_level
    already been updated when scanning FOR_NAME index, no need to
    update again */
    check_recursive = false;

    goto start_load;
  }

  return DB_SUCCESS;
}

/** Load all tablespaces during upgrade */
void dict_load_tablespaces_for_upgrade() {
  ut_ad(srv_is_upgrade_mode);

  dict_sys_mutex_enter();

  mtr_t mtr;
  mtr_start(&mtr);
  space_id_t max_id = mtr_read_ulint(dict_hdr_get(&mtr) + DICT_HDR_MAX_SPACE_ID,
                                     MLOG_4BYTES, &mtr);
  mtr_commit(&mtr);
  fil_set_max_space_id_if_bigger(max_id);

  dict_check_sys_tablespaces(false);
  dict_check_sys_tables(false);

  dict_sys_mutex_exit();
}
