/*****************************************************************************

Copyright (c) 2005, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file handler/handler0alter.cc
 Smart ALTER TABLE
 *******************************************************/

/* Include necessary SQL headers */
#include <assert.h>
#include <current_thd.h>
#include <debug_sync.h>
#include <key_spec.h>
#include <log.h>
#include <my_bit.h>
#include <mysql/plugin.h>
#include <sql_class.h>
#include <sql_lex.h>
#include <sql_table.h>
#include <sql_thd_internal_api.h>
#include <sys/types.h>
#include "ha_prototypes.h"

#include "dd/cache/dictionary_client.h"
#include "dd/dd.h"
#include "dd/dictionary.h"
#include "dd/impl/properties_impl.h"
#include "dd/properties.h"
#include "dd/types/column.h"
#include "dd/types/index.h"
#include "dd/types/index_element.h"
#include "dd/types/partition.h"
#include "dd/types/partition_index.h"
#include "dd/types/table.h"
#include "dd/types/tablespace_file.h"
#include "dd_table_share.h"

#include "btr0sea.h"
#include "dict0crea.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "dict0priv.h"
#include "dict0stats.h"
#include "dict0stats_bg.h"
#include "fsp0sysspace.h"
#include "fts0plugin.h"
#include "fts0priv.h"
#include "ha_innodb.h"
#include "ha_innopart.h"
#include "ha_prototypes.h"
#include "handler0alter.h"
#include "lex_string.h"
#include "log0log.h"

#include "my_dbug.h"
#include "my_io.h"

#include "clone0api.h"
#include "dict0dd.h"
#include "fts0plugin.h"
#include "fts0priv.h"
#include "handler0alter.h"
#include "lock0lock.h"
#include "pars0pars.h"
#include "partition_info.h"
#include "rem0types.h"
#include "row0ins.h"
#include "row0log.h"
#include "row0merge.h"
#include "row0sel.h"
#include "srv0mon.h"
#include "trx0roll.h"
#include "trx0trx.h"
#include "ut0new.h"
#include "ut0stage.h"

/* For supporting Native InnoDB Partitioning. */
#include "ha_innopart.h"
#include "partition_info.h"

/** Flags indicating if current operation can be done instantly */
enum class Instant_Type : uint16_t {
  /** Impossible to alter instantly */
  INSTANT_IMPOSSIBLE,

  /** Can be instant without any change */
  INSTANT_NO_CHANGE,

  /** Adding or dropping virtual columns only */
  INSTANT_VIRTUAL_ONLY,

  /** ADD COLUMN which can be done instantly, including
  adding stored column only (or along with adding virtual columns) */
  INSTANT_ADD_COLUMN
};

/** Function to convert the Instant_Type to a comparable int */
inline uint16_t instant_type_to_int(Instant_Type type) {
  return (static_cast<typename std::underlying_type<Log_Type>::type>(type));
}

/** Operations for creating secondary indexes (no rebuild needed) */
static const Alter_inplace_info::HA_ALTER_FLAGS INNOBASE_ONLINE_CREATE =
    Alter_inplace_info::ADD_INDEX | Alter_inplace_info::ADD_UNIQUE_INDEX |
    Alter_inplace_info::ADD_SPATIAL_INDEX;

/** Operations for rebuilding a table in place */
static const Alter_inplace_info::HA_ALTER_FLAGS INNOBASE_ALTER_REBUILD =
    Alter_inplace_info::ADD_PK_INDEX | Alter_inplace_info::DROP_PK_INDEX |
    Alter_inplace_info::CHANGE_CREATE_OPTION
    /* CHANGE_CREATE_OPTION needs to check innobase_need_rebuild() */
    | Alter_inplace_info::ALTER_COLUMN_NULLABLE |
    Alter_inplace_info::ALTER_COLUMN_NOT_NULLABLE |
    Alter_inplace_info::ALTER_STORED_COLUMN_ORDER |
    Alter_inplace_info::DROP_STORED_COLUMN |
    Alter_inplace_info::ADD_STORED_BASE_COLUMN
    /* ADD_STORED_BASE_COLUMN needs to check innobase_need_rebuild() */
    | Alter_inplace_info::RECREATE_TABLE
    /*
    | Alter_inplace_info::ALTER_STORED_COLUMN_TYPE
    */
    ;

/** Operations that require changes to data */
static const Alter_inplace_info::HA_ALTER_FLAGS INNOBASE_ALTER_DATA =
    INNOBASE_ONLINE_CREATE | INNOBASE_ALTER_REBUILD;

/** Operations for altering a table that InnoDB does not care about */
static const Alter_inplace_info::HA_ALTER_FLAGS INNOBASE_INPLACE_IGNORE =
    Alter_inplace_info::ALTER_COLUMN_DEFAULT |
    Alter_inplace_info::ALTER_COLUMN_COLUMN_FORMAT |
    Alter_inplace_info::ALTER_COLUMN_STORAGE_TYPE |
    Alter_inplace_info::ALTER_RENAME |
    Alter_inplace_info::ALTER_VIRTUAL_GCOL_EXPR |
    Alter_inplace_info::CHANGE_INDEX_OPTION;

/** Operations on foreign key definitions (changing the schema only) */
static const Alter_inplace_info::HA_ALTER_FLAGS INNOBASE_FOREIGN_OPERATIONS =
    Alter_inplace_info::DROP_FOREIGN_KEY | Alter_inplace_info::ADD_FOREIGN_KEY;

/** Operations that InnoDB cares about and can perform without rebuild */
static const Alter_inplace_info::HA_ALTER_FLAGS INNOBASE_ALTER_NOREBUILD =
    INNOBASE_ONLINE_CREATE | INNOBASE_FOREIGN_OPERATIONS |
    Alter_inplace_info::DROP_INDEX | Alter_inplace_info::DROP_UNIQUE_INDEX |
    Alter_inplace_info::RENAME_INDEX | Alter_inplace_info::ALTER_COLUMN_NAME |
    Alter_inplace_info::ALTER_COLUMN_EQUAL_PACK_LENGTH |
    Alter_inplace_info::ALTER_INDEX_COMMENT |
    Alter_inplace_info::ADD_VIRTUAL_COLUMN |
    Alter_inplace_info::DROP_VIRTUAL_COLUMN |
    Alter_inplace_info::ALTER_VIRTUAL_COLUMN_ORDER |
    Alter_inplace_info::ALTER_COLUMN_INDEX_LENGTH |
    Alter_inplace_info::SECONDARY_LOAD | Alter_inplace_info::SECONDARY_UNLOAD;
/* | Alter_inplace_info::ALTER_VIRTUAL_COLUMN_TYPE; */

struct ha_innobase_inplace_ctx : public inplace_alter_handler_ctx {
  /** Dummy query graph */
  que_thr_t *thr;
  /** The prebuilt struct of the creating instance */
  row_prebuilt_t *prebuilt;
  /** InnoDB indexes being created */
  dict_index_t **add_index;
  /** MySQL key numbers for the InnoDB indexes that are being created */
  const ulint *add_key_numbers;
  /** number of InnoDB indexes being created */
  ulint num_to_add_index;
  /** InnoDB indexes being dropped */
  dict_index_t **drop_index;
  /** number of InnoDB indexes being dropped */
  const ulint num_to_drop_index;
  /** InnoDB indexes being renamed */
  dict_index_t **rename;
  /** number of InnoDB indexes being renamed */
  const ulint num_to_rename;
  /** InnoDB foreign key constraints being dropped */
  dict_foreign_t **drop_fk;
  /** number of InnoDB foreign key constraints being dropped */
  const ulint num_to_drop_fk;
  /** InnoDB foreign key constraints being added */
  dict_foreign_t **add_fk;
  /** number of InnoDB foreign key constraints being dropped */
  const ulint num_to_add_fk;
  /** whether to create the indexes online */
  bool online;
  /** memory heap */
  mem_heap_t *heap;
  /** dictionary transaction */
  trx_t *trx;
  /** original table (if rebuilt, differs from indexed_table) */
  dict_table_t *old_table;
  /** table where the indexes are being created or dropped */
  dict_table_t *new_table;
  /** mapping of old column numbers to new ones, or NULL */
  const ulint *col_map;
  /** new column names, or NULL if nothing was renamed */
  const char **col_names;
  /** added AUTO_INCREMENT column position, or ULINT_UNDEFINED */
  const ulint add_autoinc;
  /** default values of ADD COLUMN, or NULL */
  const dtuple_t *add_cols;
  /** autoinc sequence to use */
  ib_sequence_t sequence;
  /** maximum auto-increment value */
  ulonglong max_autoinc;
  /** temporary table name to use for old table when renaming tables */
  const char *tmp_name;
  /** whether the order of the clustered index is unchanged */
  bool skip_pk_sort;
  /** number of virtual columns to be added */
  ulint num_to_add_vcol;
  /** virtual columns to be added */
  dict_v_col_t *add_vcol;
  const char **add_vcol_name;
  /** number of virtual columns to be dropped */
  ulint num_to_drop_vcol;
  /** virtual columns to be dropped */
  dict_v_col_t *drop_vcol;
  const char **drop_vcol_name;
  /** ALTER TABLE stage progress recorder */
  ut_stage_alter_t *m_stage;
  /** FTS AUX Tables to drop */
  aux_name_vec_t *fts_drop_aux_vec;

  ha_innobase_inplace_ctx(row_prebuilt_t *prebuilt_arg, dict_index_t **drop_arg,
                          ulint num_to_drop_arg, dict_index_t **rename_arg,
                          ulint num_to_rename_arg, dict_foreign_t **drop_fk_arg,
                          ulint num_to_drop_fk_arg, dict_foreign_t **add_fk_arg,
                          ulint num_to_add_fk_arg, bool online_arg,
                          mem_heap_t *heap_arg, dict_table_t *new_table_arg,
                          const char **col_names_arg, ulint add_autoinc_arg,
                          ulonglong autoinc_col_min_value_arg,
                          ulonglong autoinc_col_max_value_arg,
                          ulint num_to_drop_vcol_arg)
      : inplace_alter_handler_ctx(),
        prebuilt(prebuilt_arg),
        add_index(0),
        add_key_numbers(0),
        num_to_add_index(0),
        drop_index(drop_arg),
        num_to_drop_index(num_to_drop_arg),
        rename(rename_arg),
        num_to_rename(num_to_rename_arg),
        drop_fk(drop_fk_arg),
        num_to_drop_fk(num_to_drop_fk_arg),
        add_fk(add_fk_arg),
        num_to_add_fk(num_to_add_fk_arg),
        online(online_arg),
        heap(heap_arg),
        trx(0),
        old_table(prebuilt_arg->table),
        new_table(new_table_arg),
        col_map(0),
        col_names(col_names_arg),
        add_autoinc(add_autoinc_arg),
        add_cols(0),
        sequence(prebuilt->trx->mysql_thd, autoinc_col_min_value_arg,
                 autoinc_col_max_value_arg),
        max_autoinc(0),
        tmp_name(0),
        skip_pk_sort(false),
        num_to_add_vcol(0),
        add_vcol(0),
        add_vcol_name(0),
        num_to_drop_vcol(0),
        drop_vcol(0),
        drop_vcol_name(0),
        m_stage(NULL),
        fts_drop_aux_vec(nullptr) {
#ifdef UNIV_DEBUG
    for (ulint i = 0; i < num_to_add_index; i++) {
      ut_ad(!add_index[i]->to_be_dropped);
    }
    for (ulint i = 0; i < num_to_drop_index; i++) {
      ut_ad(drop_index[i]->to_be_dropped);
    }
#endif /* UNIV_DEBUG */

    thr = pars_complete_graph_for_exec(NULL, prebuilt->trx, heap, prebuilt);
  }

  ~ha_innobase_inplace_ctx() {
    if (fts_drop_aux_vec != nullptr) {
      fts_free_aux_names(fts_drop_aux_vec);
      delete fts_drop_aux_vec;
    }
    UT_DELETE(m_stage);
    mem_heap_free(heap);
  }

  /** Determine if the table will be rebuilt.
  @return whether the table will be rebuilt */
  bool need_rebuild() const { return (old_table != new_table); }

 private:
  // Disable copying
  ha_innobase_inplace_ctx(const ha_innobase_inplace_ctx &);
  ha_innobase_inplace_ctx &operator=(const ha_innobase_inplace_ctx &);
};

/** Structure to remember table information for updating DD */
struct alter_table_old_info_t {
  /** Constructor */
  alter_table_old_info_t() : m_discarded(), m_fts_doc_id(), m_rebuild() {}

  /** If old table is discarded one */
  bool m_discarded;

  /** If old table has FTS DOC ID */
  bool m_fts_doc_id;

  /** If this ATLER TABLE requires rebuild */
  bool m_rebuild;

  /** Update the old table information
  @param[in]	old_table	Old InnoDB table object
  @param[in]	rebuild		True if rebuild is necessary */
  void update(const dict_table_t *old_table, bool rebuild) {
    m_discarded = dict_table_is_discarded(old_table);
    m_fts_doc_id = DICT_TF2_FLAG_IS_SET(old_table, DICT_TF2_FTS_HAS_DOC_ID);
    m_rebuild = rebuild;
  }
};

/* Report an InnoDB error to the client by invoking my_error(). */
static UNIV_COLD void my_error_innodb(
    dberr_t error,     /*!< in: InnoDB error code */
    const char *table, /*!< in: table name */
    ulint flags)       /*!< in: table flags */
{
  switch (error) {
    case DB_MISSING_HISTORY:
      my_error(ER_TABLE_DEF_CHANGED, MYF(0));
      break;
    case DB_RECORD_NOT_FOUND:
      my_error(ER_KEY_NOT_FOUND, MYF(0), table);
      break;
    case DB_DEADLOCK:
      my_error(ER_LOCK_DEADLOCK, MYF(0));
      break;
    case DB_LOCK_WAIT_TIMEOUT:
      my_error(ER_LOCK_WAIT_TIMEOUT, MYF(0));
      break;
    case DB_INTERRUPTED:
      my_error(ER_QUERY_INTERRUPTED, MYF(0));
      break;
    case DB_OUT_OF_MEMORY:
      my_error(ER_OUT_OF_RESOURCES, MYF(0));
      break;
    case DB_OUT_OF_FILE_SPACE:
      my_error(ER_RECORD_FILE_FULL, MYF(0), table);
      break;
    case DB_OUT_OF_DISK_SPACE:
      my_error(ER_DISK_FULL_NOWAIT, MYF(0), table);
      break;
    case DB_TEMP_FILE_WRITE_FAIL:
      my_error(ER_TEMP_FILE_WRITE_FAILURE, MYF(0));
      break;
    case DB_TOO_BIG_INDEX_COL:
      my_error(ER_INDEX_COLUMN_TOO_LONG, MYF(0),
               DICT_MAX_FIELD_LEN_BY_FORMAT_FLAG(flags));
      break;
    case DB_TOO_MANY_CONCURRENT_TRXS:
      my_error(ER_TOO_MANY_CONCURRENT_TRXS, MYF(0));
      break;
    case DB_LOCK_TABLE_FULL:
      my_error(ER_LOCK_TABLE_FULL, MYF(0));
      break;
    case DB_UNDO_RECORD_TOO_BIG:
      my_error(ER_UNDO_RECORD_TOO_BIG, MYF(0));
      break;
    case DB_CORRUPTION:
      my_error(ER_NOT_KEYFILE, MYF(0), table);
      break;
    case DB_TOO_BIG_RECORD:
      /* We limit max record size to 16k for 64k page size. */
      my_error(ER_TOO_BIG_ROWSIZE, MYF(0),
               srv_page_size == UNIV_PAGE_SIZE_MAX
                   ? REC_MAX_DATA_SIZE - 1
                   : page_get_free_space_of_empty(flags & DICT_TF_COMPACT) / 2);
      break;
    case DB_INVALID_NULL:
      /* TODO: report the row, as we do for DB_DUPLICATE_KEY */
      my_error(ER_INVALID_USE_OF_NULL, MYF(0));
      break;
    case DB_CANT_CREATE_GEOMETRY_OBJECT:
      my_error(ER_CANT_CREATE_GEOMETRY_OBJECT, MYF(0));
      break;
    case DB_TABLESPACE_EXISTS:
      my_error(ER_TABLESPACE_EXISTS, MYF(0), table);
      break;

#ifdef UNIV_DEBUG
    case DB_SUCCESS:
    case DB_DUPLICATE_KEY:
    case DB_ONLINE_LOG_TOO_BIG:
      /* These codes should not be passed here. */
      ut_error;
#endif /* UNIV_DEBUG */
    default:
      my_error(ER_GET_ERRNO, MYF(0), error, "InnoDB error");
      break;
  }
}

/** Determine if fulltext indexes exist in a given table.
@param table MySQL table
@return whether fulltext indexes exist on the table */
static bool innobase_fulltext_exist(const TABLE *table) {
  for (uint i = 0; i < table->s->keys; i++) {
    if (table->key_info[i].flags & HA_FULLTEXT) {
      return (true);
    }
  }

  return (false);
}

/** Determine if spatial indexes exist in a given table.
@param table MySQL table
@return whether spatial indexes exist on the table */
static bool innobase_spatial_exist(const TABLE *table) {
  for (uint i = 0; i < table->s->keys; i++) {
    if (table->key_info[i].flags & HA_SPATIAL) {
      return (true);
    }
  }

  return (false);
}

/** Check if virtual column in old and new table are in order, excluding
those dropped column. This is needed because when we drop a virtual column,
ALTER_VIRTUAL_COLUMN_ORDER is also turned on, so we can't decide if this
is a real ORDER change or just DROP COLUMN
@param[in]	table		old TABLE
@param[in]	altered_table	new TABLE
@param[in]	ha_alter_info	Structure describing changes to be done
by ALTER TABLE and holding data used during in-place alter.
@return	true is all columns in order, false otherwise. */
static bool check_v_col_in_order(const TABLE *table, const TABLE *altered_table,
                                 const Alter_inplace_info *ha_alter_info) {
  ulint j = 0;

  /* We don't support any adding new virtual column before
  existed virtual column. */
  if (ha_alter_info->handler_flags & Alter_inplace_info::ADD_VIRTUAL_COLUMN) {
    bool has_new = false;

    List_iterator_fast<Create_field> cf_it(
        ha_alter_info->alter_info->create_list);

    cf_it.rewind();

    while (const Create_field *new_field = cf_it++) {
      if (!new_field->is_virtual_gcol()) {
        /* We do not support add virtual col
        before autoinc column */
        if (has_new && (new_field->flags & AUTO_INCREMENT_FLAG)) {
          return (false);
        }
        continue;
      }

      /* Found a new added virtual column. */
      if (!new_field->field) {
        has_new = true;
        continue;
      }

      /* If there's any old virtual column
      after the new added virtual column,
      order must be changed. */
      if (has_new) {
        return (false);
      }
    }
  }

  /* directly return true if ALTER_VIRTUAL_COLUMN_ORDER is not on */
  if (!(ha_alter_info->handler_flags &
        Alter_inplace_info::ALTER_VIRTUAL_COLUMN_ORDER)) {
    return (true);
  }

  for (ulint i = 0; i < table->s->fields; i++) {
    Field *field = table->s->field[i];
    bool dropped = false;

    if (field->stored_in_db) {
      continue;
    }

    ut_ad(innobase_is_v_fld(field));

    /* Check if this column is in drop list */
    for (const Alter_drop *drop : ha_alter_info->alter_info->drop_list) {
      if (my_strcasecmp(system_charset_info, field->field_name, drop->name) ==
          0) {
        dropped = true;
        break;
      }
    }

    if (dropped) {
      continue;
    }

    /* Now check if the next virtual column in altered table
    matches this column */
    while (j < altered_table->s->fields) {
      Field *new_field = altered_table->s->field[j];

      if (new_field->stored_in_db) {
        j++;
        continue;
      }

      if (my_strcasecmp(system_charset_info, field->field_name,
                        new_field->field_name) != 0) {
        /* different column */
        return (false);
      } else {
        j++;
        break;
      }
    }

    if (j > altered_table->s->fields) {
      /* there should not be less column in new table
      without them being in drop list */
      ut_ad(0);
      return (false);
    }
  }

  return (true);
}

/** Drop the statistics for a specified table, and mark it as discard
after DDL
@param[in,out]	thd	THD object
@param[in,out]	table	InnoDB table object */
static void innobase_discard_table(THD *thd, dict_table_t *table) {
  char errstr[1024];
  if (dict_stats_drop_table(table->name.m_name, errstr, sizeof(errstr)) !=
      DB_SUCCESS) {
    push_warning_printf(thd, Sql_condition::SL_WARNING, ER_ALTER_INFO,
                        "Deleting persistent statistics"
                        " for table '%s' in"
                        " InnoDB failed: %s",
                        table->name.m_name, errstr);
  }

  table->discard_after_ddl = true;
}

/** Determine if one ALTER TABLE can be done instantly on the table
@param[in]	ha_alter_info	The DDL operation
@param[in]	table		InnoDB table
@param[in]	old_table	old TABLE
@param[in]	altered_table	new TABLE
@return Instant_Type accordingly */
static inline Instant_Type innobase_support_instant(
    const Alter_inplace_info *ha_alter_info, const dict_table_t *table,
    const TABLE *old_table, const TABLE *altered_table) {
  if (!(ha_alter_info->handler_flags & ~INNOBASE_INPLACE_IGNORE)) {
    return (Instant_Type::INSTANT_NO_CHANGE);
  }

  Alter_inplace_info::HA_ALTER_FLAGS alter_inplace_flags =
      ha_alter_info->handler_flags & ~INNOBASE_INPLACE_IGNORE;

  /* If it's only adding and(or) dropping virtual columns */
  if ((!(alter_inplace_flags & ~(Alter_inplace_info::ADD_VIRTUAL_COLUMN |
                                 Alter_inplace_info::DROP_VIRTUAL_COLUMN))) &&
      check_v_col_in_order(old_table, altered_table, ha_alter_info)) {
    return (Instant_Type::INSTANT_VIRTUAL_ONLY);
  }

  if (!table->support_instant_add()) {
    return (Instant_Type::INSTANT_IMPOSSIBLE);
  }

  /* If it's an ADD COLUMN without changing existing column orders,
  or including ADD VIRTUAL COLUMN */
  if ((alter_inplace_flags == Alter_inplace_info::ADD_STORED_BASE_COLUMN) ||
      (alter_inplace_flags == (Alter_inplace_info::ADD_STORED_BASE_COLUMN |
                               Alter_inplace_info::ADD_VIRTUAL_COLUMN))) {
    return (Instant_Type::INSTANT_ADD_COLUMN);
  } else {
    return (Instant_Type::INSTANT_IMPOSSIBLE);
  }
}

/** Determine if this is an instant ALTER TABLE.
This can be checked in *inplace_alter_table() functions, which are called
after check_if_supported_inplace_alter()
@param[in]	ha_alter_info	The DDL operation
@return whether it's an instant ALTER TABLE */
static inline bool is_instant(const Alter_inplace_info *ha_alter_info) {
  return (ha_alter_info->handler_trivial_ctx !=
          instant_type_to_int(Instant_Type::INSTANT_IMPOSSIBLE));
}

/** Determine if ALTER TABLE needs to rebuild the table.
@param[in]	ha_alter_info	The DDL operation
@return whether it is necessary to rebuild the table */
static MY_ATTRIBUTE((warn_unused_result)) bool innobase_need_rebuild(
    const Alter_inplace_info *ha_alter_info) {
  if (is_instant(ha_alter_info)) {
    return (false);
  }

  Alter_inplace_info::HA_ALTER_FLAGS alter_inplace_flags =
      ha_alter_info->handler_flags & ~(INNOBASE_INPLACE_IGNORE);

  if (alter_inplace_flags == Alter_inplace_info::CHANGE_CREATE_OPTION &&
      !(ha_alter_info->create_info->used_fields &
        (HA_CREATE_USED_ROW_FORMAT | HA_CREATE_USED_KEY_BLOCK_SIZE |
         HA_CREATE_USED_TABLESPACE))) {
    /* Any other CHANGE_CREATE_OPTION than changing
    ROW_FORMAT, KEY_BLOCK_SIZE or TABLESPACE can be done
    without rebuilding the table. */
    return (false);
  }

  return (!!(ha_alter_info->handler_flags & INNOBASE_ALTER_REBUILD));
}

/** Check if InnoDB supports a particular alter table in-place
@param altered_table TABLE object for new version of table.
@param ha_alter_info Structure describing changes to be done
by ALTER TABLE and holding data used during in-place alter.

@retval HA_ALTER_INPLACE_NOT_SUPPORTED Not supported
@retval HA_ALTER_INPLACE_NO_LOCK Supported
@retval HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE Supported, but requires
lock during main phase and exclusive lock during prepare phase.
@retval HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE Supported, prepare phase
requires exclusive lock (any transactions that have accessed the table
must commit or roll back first, and no transactions can access the table
while prepare_inplace_alter_table() is executing)
*/

enum_alter_inplace_result ha_innobase::check_if_supported_inplace_alter(
    TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
  DBUG_ENTER("check_if_supported_inplace_alter");

  if (high_level_read_only || srv_sys_space.created_new_raw() ||
      srv_force_recovery) {
    if (srv_force_recovery) {
      my_error(ER_INNODB_FORCED_RECOVERY, MYF(0));
    } else {
      my_error(ER_READ_ONLY_MODE, MYF(0));
    }
    DBUG_RETURN(HA_ALTER_ERROR);
  }

  if (altered_table->s->fields > REC_MAX_N_USER_FIELDS) {
    /* Deny the inplace ALTER TABLE. MySQL will try to
    re-create the table and ha_innobase::create() will
    return an error too. This is how we effectively
    deny adding too many columns to a table. */
    ha_alter_info->unsupported_reason =
        innobase_get_err_msg(ER_TOO_MANY_FIELDS);
    DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
  }

  /* We don't support change encryption attribute with
  inplace algorithm. */
  char *old_encryption = this->table->s->encrypt_type.str;
  char *new_encryption = altered_table->s->encrypt_type.str;

  if (Encryption::is_none(old_encryption) !=
      Encryption::is_none(new_encryption)) {
    ha_alter_info->unsupported_reason =
        innobase_get_err_msg(ER_UNSUPPORTED_ALTER_ENCRYPTION_INPLACE);
    DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
  }

  update_thd();

  if (ha_alter_info->handler_flags &
      ~(INNOBASE_INPLACE_IGNORE | INNOBASE_ALTER_NOREBUILD |
        INNOBASE_ALTER_REBUILD)) {
    if (ha_alter_info->handler_flags &
        Alter_inplace_info::ALTER_STORED_COLUMN_TYPE) {
      ha_alter_info->unsupported_reason = innobase_get_err_msg(
          ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_COLUMN_TYPE);
    }
    DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
  }

  /* Only support online add foreign key constraint when
  check_foreigns is turned off */
  if ((ha_alter_info->handler_flags & Alter_inplace_info::ADD_FOREIGN_KEY) &&
      m_prebuilt->trx->check_foreigns) {
    ha_alter_info->unsupported_reason =
        innobase_get_err_msg(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_FK_CHECK);
    DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
  }

  if (altered_table->file->ht != ht) {
    /* Non-native partitioning table engine. No longer supported,
    due to implementation of native InnoDB partitioning. */
    DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
  }

  Instant_Type instant_type = innobase_support_instant(
      ha_alter_info, m_prebuilt->table, this->table, altered_table);

  ha_alter_info->handler_trivial_ctx =
      instant_type_to_int(Instant_Type::INSTANT_IMPOSSIBLE);

  if (!dict_table_is_partition(m_prebuilt->table)) {
    switch (instant_type) {
      case Instant_Type::INSTANT_IMPOSSIBLE:
        break;
      case Instant_Type::INSTANT_ADD_COLUMN:
        if (ha_alter_info->alter_info->requested_algorithm ==
            Alter_info::ALTER_TABLE_ALGORITHM_INPLACE) {
          /* Still fall back to INPLACE since the behaviour is different */
          break;
        } else if (ha_alter_info->error_if_not_empty) {
          /* In this case, it can't be instant because the table
          may not be empty. Have to fall back to INPLACE */
          break;
        }
        /* Fall through */
      case Instant_Type::INSTANT_NO_CHANGE:
      case Instant_Type::INSTANT_VIRTUAL_ONLY:
        ha_alter_info->handler_trivial_ctx = instant_type_to_int(instant_type);
        DBUG_RETURN(HA_ALTER_INPLACE_INSTANT);
    }
  }

  /* Only support NULL -> NOT NULL change if strict table sql_mode
  is set. Fall back to COPY for conversion if not strict tables.
  In-Place will fail with an error when trying to convert
  NULL to a NOT NULL value. */
  if ((ha_alter_info->handler_flags &
       Alter_inplace_info::ALTER_COLUMN_NOT_NULLABLE) &&
      !thd_is_strict_mode(m_user_thd)) {
    ha_alter_info->unsupported_reason =
        innobase_get_err_msg(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_NOT_NULL);
    DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
  }

  /* DROP PRIMARY KEY is only allowed in combination with ADD
  PRIMARY KEY. */
  if ((ha_alter_info->handler_flags & (Alter_inplace_info::ADD_PK_INDEX |
                                       Alter_inplace_info::DROP_PK_INDEX)) ==
      Alter_inplace_info::DROP_PK_INDEX) {
    ha_alter_info->unsupported_reason =
        innobase_get_err_msg(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_NOPK);
    DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
  }

  /* If a column change from NOT NULL to NULL,
  and there's a implict pk on this column. the
  table should be rebuild. The change should
  only go through the "Copy" method. */
  if ((ha_alter_info->handler_flags &
       Alter_inplace_info::ALTER_COLUMN_NULLABLE)) {
    const uint my_primary_key = altered_table->s->primary_key;

    /* See if MYSQL table has no pk but we do. */
    if (UNIV_UNLIKELY(my_primary_key >= MAX_KEY) &&
        !row_table_got_default_clust_index(m_prebuilt->table)) {
      ha_alter_info->unsupported_reason =
          innobase_get_err_msg(ER_PRIMARY_CANT_HAVE_NULL);
      DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
    }
  }

  bool add_drop_v_cols = false;

  /* If there is add or drop virtual columns, we will support operations
  with these 3 options alone with inplace interface for now */
  if (ha_alter_info->handler_flags &
      (Alter_inplace_info::ADD_VIRTUAL_COLUMN |
       Alter_inplace_info::DROP_VIRTUAL_COLUMN |
       Alter_inplace_info::ALTER_VIRTUAL_COLUMN_ORDER)) {
    ulonglong flags = ha_alter_info->handler_flags;

    /* TODO: uncomment the flags below, once we start to
    support them */
    flags &=
        ~(Alter_inplace_info::ADD_VIRTUAL_COLUMN |
          Alter_inplace_info::DROP_VIRTUAL_COLUMN |
          Alter_inplace_info::ALTER_VIRTUAL_COLUMN_ORDER |
          Alter_inplace_info::ALTER_VIRTUAL_GCOL_EXPR
          /*
          | Alter_inplace_info::ALTER_STORED_COLUMN_ORDER
          | Alter_inplace_info::ADD_STORED_BASE_COLUMN
          | Alter_inplace_info::DROP_STORED_COLUMN
          | Alter_inplace_info::ALTER_STORED_COLUMN_ORDER
          | Alter_inplace_info::ADD_UNIQUE_INDEX
          */
          | Alter_inplace_info::ADD_INDEX | Alter_inplace_info::DROP_INDEX);

    if (flags != 0 ||
        (altered_table->s->partition_info_str &&
         altered_table->s->partition_info_str_len) ||
        (!check_v_col_in_order(this->table, altered_table, ha_alter_info))) {
      ha_alter_info->unsupported_reason =
          innobase_get_err_msg(ER_UNSUPPORTED_ALTER_INPLACE_ON_VIRTUAL_COLUMN);
      DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
    }

    add_drop_v_cols = true;
  }

  /* We should be able to do the operation in-place.
  See if we can do it online (LOCK=NONE). */
  bool online = true;

  List_iterator_fast<Create_field> cf_it(
      ha_alter_info->alter_info->create_list);

  /* Fix the key parts. */
  for (KEY *new_key = ha_alter_info->key_info_buffer;
       new_key < ha_alter_info->key_info_buffer + ha_alter_info->key_count;
       new_key++) {
    /* Do not support adding/droping a vritual column, while
    there is a table rebuild caused by adding a new FTS_DOC_ID */
    if ((new_key->flags & HA_FULLTEXT) && add_drop_v_cols &&
        !DICT_TF2_FLAG_IS_SET(m_prebuilt->table, DICT_TF2_FTS_HAS_DOC_ID)) {
      ha_alter_info->unsupported_reason =
          innobase_get_err_msg(ER_UNSUPPORTED_ALTER_INPLACE_ON_VIRTUAL_COLUMN);
      DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
    }

    for (KEY_PART_INFO *key_part = new_key->key_part;
         key_part < new_key->key_part + new_key->user_defined_key_parts;
         key_part++) {
      const Create_field *new_field;

      DBUG_ASSERT(key_part->fieldnr < altered_table->s->fields);

      cf_it.rewind();
      for (uint fieldnr = 0; (new_field = cf_it++); fieldnr++) {
        if (fieldnr == key_part->fieldnr) {
          break;
        }
      }

      DBUG_ASSERT(new_field);

      key_part->field = altered_table->field[key_part->fieldnr];
      /* In some special cases InnoDB emits "false"
      duplicate key errors with NULL key values. Let
      us play safe and ensure that we can correctly
      print key values even in such cases. */
      key_part->null_offset = key_part->field->null_offset();
      key_part->null_bit = key_part->field->null_bit;

      if (new_field->field) {
        /* This is an existing column. */
        continue;
      }

      /* This is an added column. */
      DBUG_ASSERT(ha_alter_info->handler_flags &
                  Alter_inplace_info::ADD_COLUMN);

      /* We cannot replace a hidden FTS_DOC_ID
      with a user-visible FTS_DOC_ID. */
      if (m_prebuilt->table->fts && innobase_fulltext_exist(altered_table) &&
          !my_strcasecmp(system_charset_info, key_part->field->field_name,
                         FTS_DOC_ID_COL_NAME)) {
        ha_alter_info->unsupported_reason = innobase_get_err_msg(
            ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_HIDDEN_FTS);
        DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
      }

      DBUG_ASSERT((key_part->field->auto_flags & Field::NEXT_NUMBER) ==
                  !!(key_part->field->flags & AUTO_INCREMENT_FLAG));

      if (key_part->field->flags & AUTO_INCREMENT_FLAG) {
        /* We cannot assign an AUTO_INCREMENT
        column values during online ALTER. */
        DBUG_ASSERT(key_part->field == altered_table->found_next_number_field);
        ha_alter_info->unsupported_reason = innobase_get_err_msg(
            ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_AUTOINC);
        online = false;
      }

      if (key_part->field->is_virtual_gcol()) {
        /* Do not support adding index on newly added
        virtual column, while there is also a drop
        virtual column in the same clause */
        if (ha_alter_info->handler_flags &
            Alter_inplace_info::DROP_VIRTUAL_COLUMN) {
          ha_alter_info->unsupported_reason = innobase_get_err_msg(
              ER_UNSUPPORTED_ALTER_INPLACE_ON_VIRTUAL_COLUMN);

          DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
        }

        ha_alter_info->unsupported_reason =
            innobase_get_err_msg(ER_UNSUPPORTED_ALTER_ONLINE_ON_VIRTUAL_COLUMN);
        online = false;
      }
    }
  }

  DBUG_ASSERT(!m_prebuilt->table->fts ||
              m_prebuilt->table->fts->doc_col <= table->s->fields);
  DBUG_ASSERT(!m_prebuilt->table->fts ||
              m_prebuilt->table->fts->doc_col <
                  m_prebuilt->table->get_n_user_cols());

  if (ha_alter_info->handler_flags & Alter_inplace_info::ADD_SPATIAL_INDEX) {
    ha_alter_info->unsupported_reason =
        innobase_get_err_msg(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_GIS);
    online = false;
  }

  if (m_prebuilt->table->fts && innobase_fulltext_exist(altered_table)) {
    /* FULLTEXT indexes are supposed to remain. */
    /* Disallow DROP INDEX FTS_DOC_ID_INDEX */

    for (uint i = 0; i < ha_alter_info->index_drop_count; i++) {
      if (!my_strcasecmp(system_charset_info,
                         ha_alter_info->index_drop_buffer[i]->name,
                         FTS_DOC_ID_INDEX_NAME)) {
        ha_alter_info->unsupported_reason = innobase_get_err_msg(
            ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_CHANGE_FTS);
        DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
      }
    }

    /* InnoDB can have a hidden FTS_DOC_ID_INDEX on a
    visible FTS_DOC_ID column as well. Prevent dropping or
    renaming the FTS_DOC_ID. */

    for (Field **fp = table->field; *fp; fp++) {
      if (!((*fp)->flags & (FIELD_IS_RENAMED | FIELD_IS_DROPPED))) {
        continue;
      }

      if (!my_strcasecmp(system_charset_info, (*fp)->field_name,
                         FTS_DOC_ID_COL_NAME)) {
        ha_alter_info->unsupported_reason = innobase_get_err_msg(
            ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_CHANGE_FTS);
        DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
      }
    }
  }

  m_prebuilt->trx->will_lock++;

  if (!online) {
    /* We already determined that only a non-locking
    operation is possible. */
  } else if (((ha_alter_info->handler_flags &
               Alter_inplace_info::ADD_PK_INDEX) ||
              innobase_need_rebuild(ha_alter_info)) &&
             (innobase_fulltext_exist(altered_table) ||
              innobase_spatial_exist(altered_table))) {
    /* Refuse to rebuild the table online, if
    FULLTEXT OR SPATIAL indexes are to survive the rebuild. */
    online = false;
    /* If the table already contains fulltext indexes,
    refuse to rebuild the table natively altogether. */
    if (m_prebuilt->table->fts) {
      ha_alter_info->unsupported_reason =
          innobase_get_err_msg(ER_INNODB_FT_LIMIT);
      DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
    }

    if (innobase_spatial_exist(altered_table)) {
      ha_alter_info->unsupported_reason =
          innobase_get_err_msg(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_GIS);
    } else {
      ha_alter_info->unsupported_reason =
          innobase_get_err_msg(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_FTS);
    }
  } else if ((ha_alter_info->handler_flags & Alter_inplace_info::ADD_INDEX)) {
    /* Building a full-text index requires a lock.
    We could do without a lock if the table already contains
    an FTS_DOC_ID column, but in that case we would have
    to apply the modification log to the full-text indexes. */

    for (uint i = 0; i < ha_alter_info->index_add_count; i++) {
      const KEY *key =
          &ha_alter_info->key_info_buffer[ha_alter_info->index_add_buffer[i]];
      if (key->flags & HA_FULLTEXT) {
        DBUG_ASSERT(!(key->flags & HA_KEYFLAG_MASK &
                      ~(HA_FULLTEXT | HA_PACK_KEY | HA_GENERATED_KEY |
                        HA_BINARY_PACK_KEY)));
        ha_alter_info->unsupported_reason =
            innobase_get_err_msg(ER_ALTER_OPERATION_NOT_SUPPORTED_REASON_FTS);
        online = false;
        break;
      }
    }
  }

  DBUG_RETURN(online ? HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE
                     : HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE);
}

/** Update the metadata in prepare phase. This only check if dd::Tablespace
should be removed or(and) created, because to remove and store dd::Tablespace
could fail, so it's better to do it earlier, to prevent a late rollback
@param[in,out]	thd		MySQL connection
@param[in]	old_table	Old InnoDB table object
@param[in,out]	new_table	New InnoDB table object
@param[in]	old_dd_tab	Old dd::Table or dd::Partition
@param[in,out]	new_dd_tab	New dd::Table or dd::Partition
@return	false	On success
@retval	true	On failure */
template <typename Table>
static MY_ATTRIBUTE((warn_unused_result)) bool dd_prepare_inplace_alter_table(
    THD *thd, const dict_table_t *old_table, dict_table_t *new_table,
    const Table *old_dd_tab, Table *new_dd_tab);

/** Update metadata in commit phase. Note this function should only update
the metadata which would not result in failure
@param[in]	old_info	Some table information for the old table
@param[in,out]	new_table	New InnoDB table object
@param[in]	old_dd_tab	Old dd::Table or dd::Partition
@param[in,out]	new_dd_tab	New dd::Table or dd::Partition */
template <typename Table>
static void dd_commit_inplace_alter_table(
    const alter_table_old_info_t &old_info, dict_table_t *new_table,
    const Table *old_dd_tab, Table *new_dd_tab);

/** Update metadata in commit phase when the alter table does
no change to the table
@param[in]	old_dd_tab	Old dd::Table or dd::Partition
@param[in]	new_dd_tab	New dd::Table or dd::Partition
@param[in]	ignore_fts	ignore FTS update if true */
template <typename Table>
static void dd_commit_inplace_no_change(const Table *old_dd_tab,
                                        Table *new_dd_tab, bool ignore_fts);

/** Update metadata in commit phase if it is instant ALTER TABLE
@param[in]	ha_alter_info	the DDL operation
@param[in,out]	thd		THD object
@param[in,out]	trx		transaction
@param[in,out]	table		new InnoDB table
@param[in]	old_table	MySQL table as it is before the ALTER operation
@param[in]	altered_table	MySQL table that is being altered
@param[in]	old_dd_tab	Old dd::Table or dd::Partition
@param[in,out]	new_dd_tab	New dd::Table or dd::Partition
@param[in]	autoinc		autoinc counter pointer if AUTO_INCREMENT
                                is defined for the table, otherwise nullptr */
template <typename Table>
static void dd_commit_inplace_instant(Alter_inplace_info *ha_alter_info,
                                      THD *thd, trx_t *trx, dict_table_t *table,
                                      const TABLE *old_table,
                                      const TABLE *altered_table,
                                      const Table *old_dd_tab,
                                      Table *new_dd_tab, uint64_t *autoinc);

/** Update table level instant metadata in commit phase
@param[in]	table		InnoDB table object
@param[in]	old_dd_tab	old dd::Table
@param[in]	new_dd_tab	new dd::Table */
static void dd_commit_inplace_update_instant_meta(const dict_table_t *table,
                                                  const dd::Table *old_dd_tab,
                                                  dd::Table *new_dd_tab);

/** Update metadata in commit phase, especially table level metadata
for instant ADD COLUMN. Note this function should only update the metadata
which would not result in failure
@param[in]	new_table	New InnoDB table object
@param[in]	old_table	MySQL table as it is before the ALTER operation
@param[in]	altered_table	MySQL table that is being altered
@param[in]	old_dd_tab	Old dd::Table
@param[in,out]	new_dd_tab	New dd::Table */
static void dd_commit_instant_table(const dict_table_t *new_table,
                                    const TABLE *old_table,
                                    const TABLE *altered_table,
                                    const dd::Table *old_dd_tab,
                                    dd::Table *new_dd_tab);

/** Allows InnoDB to update internal structures with concurrent
writes blocked (provided that check_if_supported_inplace_alter()
did not return HA_ALTER_INPLACE_NO_LOCK).
This will be invoked before inplace_alter_table().

@param[in]	altered_table	TABLE object for new version of table.
@param[in,out]	ha_alter_info	Structure describing changes to be done
by ALTER TABLE and holding data used during in-place alter.
@param[in]	old_dd_tab	dd::Table object representing old
version of the table
@param[in,out]	new_dd_tab	dd::Table object representing new
version of the table
@retval	true Failure
@retval	false Success */
bool ha_innobase::prepare_inplace_alter_table(TABLE *altered_table,
                                              Alter_inplace_info *ha_alter_info,
                                              const dd::Table *old_dd_tab,
                                              dd::Table *new_dd_tab) {
  DBUG_ENTER("ha_innobase::prepare_inplace_alter_table");
  ut_ad(old_dd_tab != NULL);
  ut_ad(new_dd_tab != NULL);

  if (dict_sys_t::is_dd_table_id(m_prebuilt->table->id) &&
      innobase_need_rebuild(ha_alter_info)) {
    ut_ad(!m_prebuilt->table->is_temporary());
    my_error(ER_NOT_ALLOWED_COMMAND, MYF(0));
    DBUG_RETURN(true);
  }

  if (altered_table->found_next_number_field != NULL) {
    dd_copy_autoinc(old_dd_tab->se_private_data(),
                    new_dd_tab->se_private_data());
    dd_set_autoinc(new_dd_tab->se_private_data(),
                   ha_alter_info->create_info->auto_increment_value);
  }

  DBUG_RETURN(prepare_inplace_alter_table_impl<dd::Table>(
      altered_table, ha_alter_info, old_dd_tab, new_dd_tab));
}

/** Alter the table structure in-place with operations
specified using Alter_inplace_info.
The level of concurrency allowed during this operation depends
on the return value from check_if_supported_inplace_alter().

@param[in]	altered_table	TABLE object for new version of table.
@param[in,out]	ha_alter_info	Structure describing changes to be done
by ALTER TABLE and holding data used during in-place alter.
@param[in]	old_dd_tab	dd::Table object representing old
version of the table
@param[in,out]	new_dd_tab	dd::Table object representing new
version of the table
@retval	true Failure
@retval	false Success */
bool ha_innobase::inplace_alter_table(TABLE *altered_table,
                                      Alter_inplace_info *ha_alter_info,
                                      const dd::Table *old_dd_tab,
                                      dd::Table *new_dd_tab) {
  DBUG_ENTER("ha_innobase::inplace_alter_table");
  ut_ad(old_dd_tab != NULL);
  ut_ad(new_dd_tab != NULL);

  /* Don't allow database clone during in place operations */
  clone_mark_abort(true);

  auto ret = inplace_alter_table_impl<dd::Table>(altered_table, ha_alter_info,
                                                 old_dd_tab, new_dd_tab);

  clone_mark_active();

  DBUG_RETURN(ret);
}

/** Commit or rollback the changes made during
prepare_inplace_alter_table() and inplace_alter_table() inside
the storage engine. Note that the allowed level of concurrency
during this operation will be the same as for
inplace_alter_table() and thus might be higher than during
prepare_inplace_alter_table(). (E.g concurrent writes were
blocked during prepare, but might not be during commit).

@param[in]	altered_table	TABLE object for new version of table.
@param[in,out]	ha_alter_info	Structure describing changes to be done
by ALTER TABLE and holding data used during in-place alter.
@param[in]	commit		True to commit or false to rollback.
@param[in]	old_dd_tab	dd::Table object representing old
version of the table
@param[in,out]	new_dd_tab	dd::Table object representing new
version of the table
@retval	true Failure
@retval	false Success */
bool ha_innobase::commit_inplace_alter_table(TABLE *altered_table,
                                             Alter_inplace_info *ha_alter_info,
                                             bool commit,
                                             const dd::Table *old_dd_tab,
                                             dd::Table *new_dd_tab) {
  DBUG_ENTER("ha_innobase::commit_inplace_alter_table");
  ut_ad(old_dd_tab != NULL);
  ut_ad(new_dd_tab != NULL);

  ha_innobase_inplace_ctx *ctx =
      static_cast<ha_innobase_inplace_ctx *>(ha_alter_info->handler_ctx);

  alter_table_old_info_t old_info;
  ut_d(bool old_info_updated = false);
  if (commit && ctx != NULL) {
    ut_ad(!!(ha_alter_info->handler_flags & ~INNOBASE_INPLACE_IGNORE));
    old_info.update(ctx->old_table, ctx->need_rebuild());
    ut_d(old_info_updated = true);
  }

  bool res = commit_inplace_alter_table_impl<dd::Table>(
      altered_table, ha_alter_info, commit, old_dd_tab, new_dd_tab);

  if (res || !commit) {
    DBUG_RETURN(true);
  }

  ut_ad(ctx == nullptr || !(ctx->need_rebuild() && is_instant(ha_alter_info)));

  if (is_instant(ha_alter_info)) {
    ut_ad(!res);
    dd_commit_inplace_instant(ha_alter_info, m_user_thd, m_prebuilt->trx,
                              m_prebuilt->table, table, altered_table,
                              old_dd_tab, new_dd_tab,
                              altered_table->found_next_number_field != nullptr
                                  ? &m_prebuilt->table->autoinc
                                  : nullptr);
  } else if (!(ha_alter_info->handler_flags & ~INNOBASE_INPLACE_IGNORE) ||
             ctx == nullptr) {
    ut_ad(!res);
    dd_commit_inplace_no_change(old_dd_tab, new_dd_tab, false);
  } else {
    ut_ad(old_info_updated);
    dd_commit_inplace_alter_table<dd::Table>(old_info, ctx->new_table,
                                             old_dd_tab, new_dd_tab);
    if (!ctx->need_rebuild()) {
      dd_commit_inplace_update_instant_meta(ctx->new_table, old_dd_tab,
                                            new_dd_tab);
    }
    ut_ad(dd_table_match(ctx->new_table, new_dd_tab));
  }

#ifdef UNIV_DEBUG
  if (dd_table_has_instant_cols(*old_dd_tab) &&
      (ctx == nullptr || !ctx->need_rebuild())) {
    ut_ad(dd_table_has_instant_cols(*new_dd_tab));
  }
#endif /* UNIV_DEBUG */

  DBUG_RETURN(res);
}

/** Initialize the dict_foreign_t structure with supplied info
 @return true if added, false if duplicate foreign->id */
static bool innobase_init_foreign(
    dict_foreign_t *foreign,              /*!< in/out: structure to
                                          initialize */
    const char *constraint_name,          /*!< in/out: constraint name if
                                          exists */
    dict_table_t *table,                  /*!< in: foreign table */
    dict_index_t *index,                  /*!< in: foreign key index */
    const char **column_names,            /*!< in: foreign key column
                                          names */
    ulint num_field,                      /*!< in: number of columns */
    const char *referenced_table_name,    /*!< in: referenced table
                                          name */
    dict_table_t *referenced_table,       /*!< in: referenced table */
    dict_index_t *referenced_index,       /*!< in: referenced index */
    const char **referenced_column_names, /*!< in: referenced column
                                          names */
    ulint referenced_num_field)           /*!< in: number of referenced
                                          columns */
{
  ut_ad(mutex_own(&dict_sys->mutex));

  if (constraint_name) {
    ulint db_len;

    /* Catenate 'databasename/' to the constraint name specified
    by the user: we conceive the constraint as belonging to the
    same MySQL 'database' as the table itself. We store the name
    to foreign->id. */

    db_len = dict_get_db_name_len(table->name.m_name);

    foreign->id = static_cast<char *>(
        mem_heap_alloc(foreign->heap, db_len + strlen(constraint_name) + 2));

    ut_memcpy(foreign->id, table->name.m_name, db_len);
    foreign->id[db_len] = '/';
    strcpy(foreign->id + db_len + 1, constraint_name);

    /* Check if any existing foreign key has the same id,
    this is needed only if user supplies the constraint name */

    if (table->foreign_set.find(foreign) != table->foreign_set.end()) {
      return (false);
    }
  }

  foreign->foreign_table = table;
  foreign->foreign_table_name =
      mem_heap_strdup(foreign->heap, table->name.m_name);
  dict_mem_foreign_table_name_lookup_set(foreign, TRUE);

  foreign->foreign_index = index;
  foreign->n_fields = (unsigned int)num_field;

  foreign->foreign_col_names = static_cast<const char **>(
      mem_heap_alloc(foreign->heap, num_field * sizeof(void *)));

  for (ulint i = 0; i < foreign->n_fields; i++) {
    foreign->foreign_col_names[i] =
        mem_heap_strdup(foreign->heap, column_names[i]);
  }

  foreign->referenced_index = referenced_index;
  foreign->referenced_table = referenced_table;

  foreign->referenced_table_name =
      mem_heap_strdup(foreign->heap, referenced_table_name);
  dict_mem_referenced_table_name_lookup_set(foreign, TRUE);

  foreign->referenced_col_names = static_cast<const char **>(
      mem_heap_alloc(foreign->heap, referenced_num_field * sizeof(void *)));

  for (ulint i = 0; i < foreign->n_fields; i++) {
    foreign->referenced_col_names[i] =
        mem_heap_strdup(foreign->heap, referenced_column_names[i]);
  }

  return (true);
}

/** Check whether the foreign key options is legit
 @return true if it is */
static MY_ATTRIBUTE((warn_unused_result)) bool innobase_check_fk_option(
    const dict_foreign_t *foreign) /*!< in: foreign key */
{
  if (!foreign->foreign_index) {
    return (true);
  }

  if (foreign->type &
      (DICT_FOREIGN_ON_UPDATE_SET_NULL | DICT_FOREIGN_ON_DELETE_SET_NULL)) {
    for (ulint j = 0; j < foreign->n_fields; j++) {
      if ((foreign->foreign_index->get_col(j)->prtype) & DATA_NOT_NULL) {
        /* It is not sensible to define
        SET NULL if the column is not
        allowed to be NULL! */
        return (false);
      }
    }
  }

  return (true);
}

/** Set foreign key options
 @return true if successfully set */
static MY_ATTRIBUTE((warn_unused_result)) bool innobase_set_foreign_key_option(
    dict_foreign_t *foreign,        /*!< in:InnoDB Foreign key */
    const Foreign_key_spec *fk_key) /*!< in: Foreign key info from
                                    MySQL */
{
  ut_ad(!foreign->type);

  switch (fk_key->delete_opt) {
    case FK_OPTION_NO_ACTION:
    case FK_OPTION_RESTRICT:
    case FK_OPTION_DEFAULT:
      foreign->type = DICT_FOREIGN_ON_DELETE_NO_ACTION;
      break;
    case FK_OPTION_CASCADE:
      foreign->type = DICT_FOREIGN_ON_DELETE_CASCADE;
      break;
    case FK_OPTION_SET_NULL:
      foreign->type = DICT_FOREIGN_ON_DELETE_SET_NULL;
      break;
    case FK_OPTION_UNDEF:
      break;
  }

  switch (fk_key->update_opt) {
    case FK_OPTION_NO_ACTION:
    case FK_OPTION_RESTRICT:
    case FK_OPTION_DEFAULT:
      foreign->type |= DICT_FOREIGN_ON_UPDATE_NO_ACTION;
      break;
    case FK_OPTION_CASCADE:
      foreign->type |= DICT_FOREIGN_ON_UPDATE_CASCADE;
      break;
    case FK_OPTION_SET_NULL:
      foreign->type |= DICT_FOREIGN_ON_UPDATE_SET_NULL;
      break;
    case FK_OPTION_UNDEF:
      break;
  }

  return (innobase_check_fk_option(foreign));
}

/** Check if a foreign key constraint can make use of an index
 that is being created.
 @return useable index, or NULL if none found */
static MY_ATTRIBUTE((warn_unused_result)) const KEY *innobase_find_equiv_index(
    const char *const *col_names,
    /*!< in: column names */
    uint n_cols,     /*!< in: number of columns */
    const KEY *keys, /*!< in: index information */
    const uint *add, /*!< in: indexes being created */
    uint n_add)      /*!< in: number of indexes to create */
{
  for (uint i = 0; i < n_add; i++) {
    const KEY *key = &keys[add[i]];

    if (key->user_defined_key_parts < n_cols || key->flags & HA_SPATIAL) {
    no_match:
      continue;
    }

    for (uint j = 0; j < n_cols; j++) {
      const KEY_PART_INFO &key_part = key->key_part[j];
      uint32 col_len = key_part.field->pack_length();

      /* Any index on virtual columns cannot be used
      for reference constaint */
      if (innobase_is_v_fld(key_part.field)) {
        goto no_match;
      }

      /* The MySQL pack length contains 1 or 2 bytes
      length field for a true VARCHAR. */

      if (key_part.field->type() == MYSQL_TYPE_VARCHAR) {
        col_len -=
            static_cast<const Field_varstring *>(key_part.field)->length_bytes;
      }

      if (key_part.length < col_len) {
        /* Column prefix indexes cannot be
        used for FOREIGN KEY constraints. */
        goto no_match;
      }

      if (innobase_strcasecmp(col_names[j], key_part.field->field_name)) {
        /* Name mismatch */
        goto no_match;
      }
    }

    return (key);
  }

  return (NULL);
}

/** Find an index whose first fields are the columns in the array
 in the same order and is not marked for deletion
 @return matching index, NULL if not found */
static MY_ATTRIBUTE((warn_unused_result)) dict_index_t *innobase_find_fk_index(
    Alter_inplace_info *ha_alter_info,
    /*!< in: alter table info */
    dict_table_t *table, /*!< in: table */
    const char **col_names,
    /*!< in: column names, or NULL
    to use table->col_names */
    dict_index_t **drop_index,
    /*!< in: indexes to be dropped */
    ulint n_drop_index,
    /*!< in: size of drop_index[] */
    const char **columns, /*!< in: array of column names */
    ulint n_cols)         /*!< in: number of columns */
{
  dict_index_t *index;

  index = table->first_index();

  while (index != NULL) {
    if (!(index->type & DICT_FTS) &&
        dict_foreign_qualify_index(table, col_names, columns, n_cols, index,
                                   NULL, true, 0)) {
      for (ulint i = 0; i < n_drop_index; i++) {
        if (index == drop_index[i]) {
          /* Skip to-be-dropped indexes. */
          goto next_rec;
        }
      }

      return (index);
    }

  next_rec:
    index = index->next();
  }

  return (NULL);
}

/** Check whether given column is a base of stored column.
@param[in]	col_name	column name
@param[in]	table		table
@param[in]	s_cols		list of stored columns
@return true if the given column is a base of stored column,else false. */
static bool innobase_col_check_fk(const char *col_name,
                                  const dict_table_t *table,
                                  dict_s_col_list *s_cols) {
  dict_s_col_list::const_iterator it;

  for (it = s_cols->begin(); it != s_cols->end(); ++it) {
    dict_s_col_t s_col = *it;

    for (ulint j = 0; j < s_col.num_base; j++) {
      if (strcmp(col_name, table->get_col_name(s_col.base_col[j]->ind)) == 0) {
        return (true);
      }
    }
  }

  return (false);
}

/** Check whether the foreign key constraint is on base of any stored columns.
@param[in]	foreign		Foriegn key constraing information
@param[in]	table		table to which the foreign key objects
to be added
@param[in]	s_cols		list of stored column information in the table.
@return true if yes, otherwise false. */
static bool innobase_check_fk_stored(const dict_foreign_t *foreign,
                                     const dict_table_t *table,
                                     dict_s_col_list *s_cols) {
  ulint type = foreign->type;

  type &=
      ~(DICT_FOREIGN_ON_DELETE_NO_ACTION | DICT_FOREIGN_ON_UPDATE_NO_ACTION);

  if (type == 0 || s_cols == NULL) {
    return (false);
  }

  for (ulint i = 0; i < foreign->n_fields; i++) {
    if (innobase_col_check_fk(foreign->foreign_col_names[i], table, s_cols)) {
      return (true);
    }
  }

  return (false);
}

/** Create InnoDB foreign key structure from MySQL alter_info
@param[in]	ha_alter_info	alter table info
@param[in]	table_share	TABLE_SHARE
@param[in]	table		table object
@param[in]	col_names	column names, or NULL to use
table->col_names
@param[in]	drop_index	indexes to be dropped
@param[in]	n_drop_index	size of drop_index
@param[out]	add_fk		foreign constraint added
@param[out]	n_add_fk	number of foreign constraints
added
@param[in]	trx		user transaction
@param[in]	s_cols		list of stored column information
@retval true if successful
@retval false on error (will call my_error()) */
static MY_ATTRIBUTE((warn_unused_result)) bool innobase_get_foreign_key_info(
    Alter_inplace_info *ha_alter_info, const TABLE_SHARE *table_share,
    dict_table_t *table, const char **col_names, dict_index_t **drop_index,
    ulint n_drop_index, dict_foreign_t **add_fk, ulint *n_add_fk,
    const trx_t *trx, dict_s_col_list *s_cols) {
  const Foreign_key_spec *fk_key;
  dict_table_t *referenced_table = NULL;
  char *referenced_table_name = NULL;
  ulint num_fk = 0;
  Alter_info *alter_info = ha_alter_info->alter_info;
  MDL_ticket *mdl;

  DBUG_ENTER("innobase_get_foreign_key_info");

  *n_add_fk = 0;

  for (const Key_spec *key : alter_info->key_list) {
    if (key->type != KEYTYPE_FOREIGN) {
      continue;
    }

    const char *column_names[MAX_NUM_FK_COLUMNS];
    dict_index_t *index = NULL;
    const char *referenced_column_names[MAX_NUM_FK_COLUMNS];
    dict_index_t *referenced_index = NULL;
    ulint num_col = 0;
    ulint referenced_num_col = 0;
    bool correct_option;
    char *db_namep = NULL;
    char *tbl_namep = NULL;
    ulint db_name_len = 0;
    ulint tbl_name_len = 0;
    char db_name[MAX_DATABASE_NAME_LEN];
    char tbl_name[MAX_TABLE_NAME_LEN];

    fk_key = down_cast<const Foreign_key_spec *>(key);

    if (fk_key->columns.size() > 0) {
      size_t i = 0;

      /* Get all the foreign key column info for the
      current table */
      while (i < fk_key->columns.size()) {
        column_names[i] = fk_key->columns[i]->get_field_name();
        ut_ad(i < MAX_NUM_FK_COLUMNS);
        i++;
      }

      index = innobase_find_fk_index(ha_alter_info, table, col_names,
                                     drop_index, n_drop_index, column_names, i);

      /* MySQL would add a index in the creation
      list if no such index for foreign table,
      so we have to use DBUG_EXECUTE_IF to simulate
      the scenario */
      DBUG_EXECUTE_IF("innodb_test_no_foreign_idx", index = NULL;);

      /* Check whether there exist such
      index in the the index create clause */
      if (!index &&
          !innobase_find_equiv_index(column_names, static_cast<uint>(i),
                                     ha_alter_info->key_info_buffer,
                                     ha_alter_info->index_add_buffer,
                                     ha_alter_info->index_add_count)) {
        my_error(ER_FK_NO_INDEX_CHILD, MYF(0),
                 fk_key->name.str ? fk_key->name.str : "",
                 table_share->table_name.str);
        goto err_exit;
      }

      num_col = i;
    }

    add_fk[num_fk] = dict_mem_foreign_create();

#ifndef _WIN32
    if (fk_key->ref_db.str) {
      tablename_to_filename(fk_key->ref_db.str, db_name, MAX_DATABASE_NAME_LEN);
      db_namep = db_name;
      db_name_len = strlen(db_name);
    }
    if (fk_key->ref_table.str) {
      tablename_to_filename(fk_key->ref_table.str, tbl_name,
                            MAX_TABLE_NAME_LEN);
      tbl_namep = tbl_name;
      tbl_name_len = strlen(tbl_name);
    }
#else
    ut_ad(fk_key->ref_table.str);
    tablename_to_filename(fk_key->ref_table.str, tbl_name, MAX_TABLE_NAME_LEN);
    innobase_casedn_str(tbl_name);
    tbl_name_len = strlen(tbl_name);
    tbl_namep = &tbl_name[0];

    if (fk_key->ref_db.str != NULL) {
      tablename_to_filename(fk_key->ref_db.str, db_name, MAX_DATABASE_NAME_LEN);
      innobase_casedn_str(db_name);
      db_name_len = strlen(db_name);
      db_namep = &db_name[0];
    }
#endif
    mutex_enter(&dict_sys->mutex);

    referenced_table_name = dd_get_referenced_table(
        table->name.m_name, db_namep, db_name_len, tbl_namep, tbl_name_len,
        &referenced_table, &mdl, add_fk[num_fk]->heap);

    /* Test the case when referenced_table failed to
    open, if trx->check_foreigns is not set, we should
    still be able to add the foreign key */
    DBUG_EXECUTE_IF("innodb_test_open_ref_fail", if (referenced_table) {
      dd_table_close(referenced_table, current_thd, &mdl, true);
      referenced_table = NULL;
    });

    if (!referenced_table && trx->check_foreigns) {
      mutex_exit(&dict_sys->mutex);
      my_error(ER_FK_CANNOT_OPEN_PARENT, MYF(0), tbl_namep);

      goto err_exit;
    }

    if (fk_key->ref_columns.size() > 0) {
      size_t i = 0;

      while (i < fk_key->ref_columns.size()) {
        referenced_column_names[i] = fk_key->ref_columns[i]->get_field_name();
        ut_ad(i < MAX_NUM_FK_COLUMNS);
        i++;
      }

      if (referenced_table) {
        referenced_index = dict_foreign_find_index(referenced_table, 0,
                                                   referenced_column_names, i,
                                                   index, TRUE, FALSE);

        DBUG_EXECUTE_IF("innodb_test_no_reference_idx",
                        referenced_index = NULL;);

        /* Check whether there exist such
        index in the the index create clause */
        if (!referenced_index) {
          dd_table_close(referenced_table, current_thd, &mdl, true);
          mutex_exit(&dict_sys->mutex);
          my_error(ER_FK_NO_INDEX_PARENT, MYF(0),
                   fk_key->name.str ? fk_key->name.str : "", tbl_namep);
          goto err_exit;
        }
      } else {
        ut_a(!trx->check_foreigns);
      }

      referenced_num_col = i;
    } else {
      /* Not possible to add a foreign key without a
      referenced column */
      if (referenced_table) {
        dd_table_close(referenced_table, current_thd, &mdl, true);
      }
      mutex_exit(&dict_sys->mutex);
      my_error(ER_CANNOT_ADD_FOREIGN, MYF(0), tbl_namep);
      goto err_exit;
    }

    if (!innobase_init_foreign(add_fk[num_fk], fk_key->name.str, table, index,
                               column_names, num_col, referenced_table_name,
                               referenced_table, referenced_index,
                               referenced_column_names, referenced_num_col)) {
      if (referenced_table) {
        dd_table_close(referenced_table, current_thd, &mdl, true);
      }
      mutex_exit(&dict_sys->mutex);
      my_error(ER_FK_DUP_NAME, MYF(0), add_fk[num_fk]->id);
      goto err_exit;
    }

    if (referenced_table) {
      dd_table_close(referenced_table, current_thd, &mdl, true);
    }
    mutex_exit(&dict_sys->mutex);

    correct_option = innobase_set_foreign_key_option(add_fk[num_fk], fk_key);

    DBUG_EXECUTE_IF("innodb_test_wrong_fk_option", correct_option = false;);

    if (!correct_option) {
      my_error(ER_FK_INCORRECT_OPTION, MYF(0), table_share->table_name.str,
               add_fk[num_fk]->id);
      goto err_exit;
    }

    if (innobase_check_fk_stored(add_fk[num_fk], table, s_cols)) {
      my_error(ER_CANNOT_ADD_FOREIGN_BASE_COL_STORED, MYF(0));
      goto err_exit;
    }

    num_fk++;
  }

  *n_add_fk = num_fk;

  DBUG_RETURN(true);
err_exit:
  for (ulint i = 0; i <= num_fk; i++) {
    if (add_fk[i]) {
      dict_foreign_free(add_fk[i]);
    }
  }

  DBUG_RETURN(false);
}

/** Copies an InnoDB column to a MySQL field.  This function is
 adapted from row_sel_field_store_in_mysql_format(). */
static void innobase_col_to_mysql(
    const dict_col_t *col, /*!< in: InnoDB column */
    const uchar *data,     /*!< in: InnoDB column data */
    ulint len,             /*!< in: length of data, in bytes */
    Field *field)          /*!< in/out: MySQL field */
{
  uchar *ptr;
  uchar *dest = field->ptr;
  ulint flen = field->pack_length();

  switch (col->mtype) {
    case DATA_INT:
      ut_ad(len == flen);

      /* Convert integer data from Innobase to little-endian
      format, sign bit restored to normal */

      for (ptr = dest + len; ptr != dest;) {
        *--ptr = *data++;
      }

      if (!(field->flags & UNSIGNED_FLAG)) {
        ((byte *)dest)[len - 1] ^= 0x80;
      }

      break;

    case DATA_VARCHAR:
    case DATA_VARMYSQL:
    case DATA_BINARY:
      field->reset();

      if (field->type() == MYSQL_TYPE_VARCHAR) {
        /* This is a >= 5.0.3 type true VARCHAR. Store the
        length of the data to the first byte or the first
        two bytes of dest. */

        dest =
            row_mysql_store_true_var_len(dest, len, flen - field->key_length());
      }

      /* Copy the actual data */
      memcpy(dest, data, len);
      break;

    case DATA_VAR_POINT:
    case DATA_GEOMETRY:
    case DATA_BLOB:
      /* Skip MySQL BLOBs when reporting an erroneous row
      during index creation or table rebuild. */
      field->set_null();
      break;

#ifdef UNIV_DEBUG
    case DATA_MYSQL:
      ut_ad(flen >= len);
      ut_ad(DATA_MBMAXLEN(col->mbminmaxlen) >= DATA_MBMINLEN(col->mbminmaxlen));
      memcpy(dest, data, len);
      break;

    default:
    case DATA_SYS_CHILD:
    case DATA_SYS:
      /* These column types should never be shipped to MySQL. */
      ut_ad(0);

    case DATA_FLOAT:
    case DATA_DOUBLE:
    case DATA_DECIMAL:
    case DATA_POINT:
      /* Above are the valid column types for MySQL data. */
      ut_ad(flen == len);
      /* fall through */
    case DATA_FIXBINARY:
    case DATA_CHAR:
      /* We may have flen > len when there is a shorter
      prefix on the CHAR and BINARY column. */
      ut_ad(flen >= len);
#else  /* UNIV_DEBUG */
    default:
#endif /* UNIV_DEBUG */
      memcpy(dest, data, len);
  }
}

/** Copies an InnoDB record to table->record[0]. */
void innobase_rec_to_mysql(struct TABLE *table, /*!< in/out: MySQL table */
                           const rec_t *rec,    /*!< in: record */
                           const dict_index_t *index, /*!< in: index */
                           const ulint *offsets)      /*!< in: rec_get_offsets(
                                                      rec, index, ...) */
{
  uint n_fields = table->s->fields;

  ut_ad(n_fields ==
        dict_table_get_n_tot_u_cols(index->table) -
            !!(DICT_TF2_FLAG_IS_SET(index->table, DICT_TF2_FTS_HAS_DOC_ID)));

  for (uint i = 0; i < n_fields; i++) {
    Field *field = table->field[i];
    ulint ipos;
    ulint ilen;
    const uchar *ifield;

    field->reset();

    ipos = index->get_col_pos(i, true, false);

    if (ipos == ULINT_UNDEFINED || rec_offs_nth_extern(offsets, ipos)) {
    null_field:
      field->set_null();
      continue;
    }

    ifield = rec_get_nth_field_instant(rec, offsets, ipos, index, &ilen);

    /* Assign the NULL flag */
    if (ilen == UNIV_SQL_NULL) {
      ut_ad(field->real_maybe_null());
      goto null_field;
    }

    field->set_notnull();

    innobase_col_to_mysql(index->get_field(ipos)->col, ifield, ilen, field);
  }
}

/** Copies an InnoDB index entry to table->record[0]. */
void innobase_fields_to_mysql(
    struct TABLE *table,       /*!< in/out: MySQL table */
    const dict_index_t *index, /*!< in: InnoDB index */
    const dfield_t *fields)    /*!< in: InnoDB index fields */
{
  uint n_fields = table->s->fields;
  ulint num_v = 0;

  ut_ad(n_fields ==
        index->table->get_n_user_cols() +
            dict_table_get_n_v_cols(index->table) -
            !!(DICT_TF2_FLAG_IS_SET(index->table, DICT_TF2_FTS_HAS_DOC_ID)));

  for (uint i = 0; i < n_fields; i++) {
    Field *field = table->field[i];
    ulint ipos;
    ulint col_n;

    field->reset();

    if (innobase_is_v_fld(field)) {
      col_n = num_v;
      num_v++;
    } else {
      col_n = i - num_v;
    }

    ipos = index->get_col_pos(col_n, true, innobase_is_v_fld(field));

    if (ipos == ULINT_UNDEFINED || dfield_is_ext(&fields[ipos]) ||
        dfield_is_null(&fields[ipos])) {
      field->set_null();
    } else {
      field->set_notnull();

      const dfield_t *df = &fields[ipos];

      innobase_col_to_mysql(index->get_field(ipos)->col,
                            static_cast<const uchar *>(dfield_get_data(df)),
                            dfield_get_len(df), field);
    }
  }
}

/** Copies an InnoDB row to table->record[0]. */
void innobase_row_to_mysql(struct TABLE *table,      /*!< in/out: MySQL table */
                           const dict_table_t *itab, /*!< in: InnoDB table */
                           const dtuple_t *row)      /*!< in: InnoDB row */
{
  uint n_fields = table->s->fields;
  ulint num_v = 0;

  /* The InnoDB row may contain an extra FTS_DOC_ID column at the end. */
  ut_ad(row->n_fields == itab->get_n_cols());
  ut_ad(n_fields ==
        row->n_fields - DATA_N_SYS_COLS + dict_table_get_n_v_cols(itab) -
            !!(DICT_TF2_FLAG_IS_SET(itab, DICT_TF2_FTS_HAS_DOC_ID)));

  for (uint i = 0; i < n_fields; i++) {
    Field *field = table->field[i];

    field->reset();

    if (innobase_is_v_fld(field)) {
      /* Virtual column are not stored in InnoDB table, so
      skip it */
      num_v++;
      continue;
    }

    const dfield_t *df = dtuple_get_nth_field(row, i - num_v);

    if (dfield_is_ext(df) || dfield_is_null(df)) {
      field->set_null();
    } else {
      field->set_notnull();

      innobase_col_to_mysql(itab->get_col(i - num_v),
                            static_cast<const uchar *>(dfield_get_data(df)),
                            dfield_get_len(df), field);
    }
  }
}

/** Resets table->record[0]. */
void innobase_rec_reset(TABLE *table) /*!< in/out: MySQL table */
{
  uint n_fields = table->s->fields;
  uint i;

  for (i = 0; i < n_fields; i++) {
    table->field[i]->set_default();
  }
}

/** This function checks that index keys are sensible.
 @return 0 or error number */
static MY_ATTRIBUTE((warn_unused_result)) int innobase_check_index_keys(
    const Alter_inplace_info *info,
    /*!< in: indexes to be created or dropped */
    const dict_table_t *innodb_table)
/*!< in: Existing indexes */
{
  for (uint key_num = 0; key_num < info->index_add_count; key_num++) {
    const KEY &key = info->key_info_buffer[info->index_add_buffer[key_num]];

    /* Check that the same index name does not appear
    twice in indexes to be created. */

    for (ulint i = 0; i < key_num; i++) {
      const KEY &key2 = info->key_info_buffer[info->index_add_buffer[i]];

      if (0 == strcmp(key.name, key2.name)) {
        my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key.name);

        return (ER_WRONG_NAME_FOR_INDEX);
      }
    }

    /* Check that the same index name does not already exist. */

    const dict_index_t *index;

    for (index = innodb_table->first_index(); index; index = index->next()) {
      if (index->is_committed() && !strcmp(key.name, index->name)) {
        break;
      }
    }

    /* Now we are in a situation where we have "ADD INDEX x"
    and an index by the same name already exists. We have 4
    possible cases:
    1. No further clauses for an index x are given. Should reject
    the operation.
    2. "DROP INDEX x" is given. Should allow the operation.
    3. "RENAME INDEX x TO y" is given. Should allow the operation.
    4. "DROP INDEX x, RENAME INDEX x TO y" is given. Should allow
    the operation, since no name clash occurs. In this particular
    case MySQL cancels the operation without calling InnoDB
    methods. */

    if (index) {
      /* If a key by the same name is being created and
      dropped, the name clash is OK. */
      for (uint i = 0; i < info->index_drop_count; i++) {
        const KEY *drop_key = info->index_drop_buffer[i];

        if (0 == strcmp(key.name, drop_key->name)) {
          goto name_ok;
        }
      }

      /* If a key by the same name is being created and
      renamed, the name clash is OK. E.g.
      ALTER TABLE t ADD INDEX i (col), RENAME INDEX i TO x
      where the index "i" exists prior to the ALTER command.
      In this case we:
      1. rename the existing index from "i" to "x"
      2. add the new index "i" */
      for (uint i = 0; i < info->index_rename_count; i++) {
        const KEY_PAIR *pair = &info->index_rename_buffer[i];

        if (0 == strcmp(key.name, pair->old_key->name)) {
          goto name_ok;
        }
      }

      my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), key.name);

      return (ER_WRONG_NAME_FOR_INDEX);
    }

  name_ok:
    for (ulint i = 0; i < key.user_defined_key_parts; i++) {
      const KEY_PART_INFO &key_part1 = key.key_part[i];
      const Field *field = key_part1.field;
      ibool is_unsigned;

      switch (get_innobase_type_from_mysql_type(&is_unsigned, field)) {
        default:
          break;
        case DATA_INT:
        case DATA_FLOAT:
        case DATA_DOUBLE:
        case DATA_DECIMAL:
          /* Check that MySQL does not try to
          create a column prefix index field on
          an inappropriate data type. */

          if (field->type() == MYSQL_TYPE_VARCHAR) {
            if (key_part1.length >=
                field->pack_length() -
                    ((Field_varstring *)field)->length_bytes) {
              break;
            }
          } else {
            if (key_part1.length >= field->pack_length()) {
              break;
            }
          }

          my_error(ER_WRONG_KEY_COLUMN, MYF(0), field->field_name);
          return (ER_WRONG_KEY_COLUMN);
      }

      /* Check that the same column does not appear
      twice in the index. */

      for (ulint j = 0; j < i; j++) {
        const KEY_PART_INFO &key_part2 = key.key_part[j];

        if (key_part1.fieldnr != key_part2.fieldnr) {
          continue;
        }

        my_error(ER_WRONG_KEY_COLUMN, MYF(0), field->field_name);
        return (ER_WRONG_KEY_COLUMN);
      }
    }
  }

  return (0);
}

/** Create index field definition for key part
@param[in]	altered_table		MySQL table that is being altered,
                                        or NULL if a new clustered index
                                        is not being created
@param[in]	key_part		MySQL key definition
@param[in,out]	index_field		index field
@param[in]	new_clustered		new cluster */
static void innobase_create_index_field_def(const TABLE *altered_table,
                                            const KEY_PART_INFO *key_part,
                                            index_field_t *index_field,
                                            bool new_clustered) {
  const Field *field;
  ibool is_unsigned;
  ulint col_type;
  ulint num_v = 0;

  DBUG_ENTER("innobase_create_index_field_def");

  ut_ad(key_part);
  ut_ad(index_field);

  field =
      new_clustered ? altered_table->field[key_part->fieldnr] : key_part->field;
  ut_a(field);

  for (ulint i = 0; i < key_part->fieldnr; i++) {
    if (innobase_is_v_fld(altered_table->field[i])) {
      num_v++;
    }
  }

  col_type = get_innobase_type_from_mysql_type(&is_unsigned, field);

  if (!field->stored_in_db && field->gcol_info) {
    index_field->is_v_col = true;
    index_field->col_no = num_v;
  } else {
    index_field->is_v_col = false;
    index_field->col_no = key_part->fieldnr - num_v;
  }
  index_field->is_ascending = !(key_part->key_part_flag & HA_REVERSE_SORT);

  if (DATA_LARGE_MTYPE(col_type) ||
      (key_part->length < field->pack_length() &&
       field->type() != MYSQL_TYPE_VARCHAR) ||
      (field->type() == MYSQL_TYPE_VARCHAR &&
       key_part->length <
           field->pack_length() - ((Field_varstring *)field)->length_bytes)) {
    index_field->prefix_len = key_part->length;
  } else {
    index_field->prefix_len = 0;
  }

  DBUG_VOID_RETURN;
}

template <typename Index>
const dd::Index *get_dd_index(const Index *index);

template <>
const dd::Index *get_dd_index<dd::Index>(const dd::Index *dd_index) {
  return dd_index;
}

template <>
const dd::Index *get_dd_index<dd::Partition_index>(
    const dd::Partition_index *dd_index) {
  return (dd_index != nullptr) ? &dd_index->index() : nullptr;
}

/** Create index definition for key
@param[in]	altered_table		MySQL table that is being altered
@param[in]	new_dd_tab		new dd table
@param[in]	keys			key definitions
@param[in]	key_number		MySQL key number
@param[in]	new_clustered		true if generating a new clustered
index on the table
@param[in]	key_clustered		true if this is the new clustered index
@param[out]	index			index definition
@param[in]	heap			heap where memory is allocated */
template <typename Table>
static void innobase_create_index_def(const TABLE *altered_table,
                                      const Table *new_dd_tab, const KEY *keys,
                                      ulint key_number, bool new_clustered,
                                      bool key_clustered, index_def_t *index,
                                      mem_heap_t *heap) {
  const KEY *key = &keys[key_number];
  ulint i;
  ulint n_fields = key->user_defined_key_parts;

  DBUG_ENTER("innobase_create_index_def");
  DBUG_ASSERT(!key_clustered || new_clustered);

  index->fields = static_cast<index_field_t *>(
      mem_heap_alloc(heap, n_fields * sizeof *index->fields));

  index->parser = NULL;
  index->is_ngram = false;
  index->key_number = key_number;
  index->n_fields = n_fields;
  index->name = mem_heap_strdup(heap, key->name);
  index->rebuild = new_clustered;

  /* If this is a spatial index, we need to fetch the SRID */
  if (key->flags & HA_SPATIAL) {
    ulint dd_key_num =
        key_number + ((altered_table->s->primary_key == MAX_KEY) ? 1 : 0);

    const auto *dd_index_auto =
        (index->key_number != ULINT_UNDEFINED)
            ? const_cast<const Table *>(new_dd_tab)->indexes()[dd_key_num]
            : nullptr;

    const dd::Index *dd_index = get_dd_index(dd_index_auto);

    if (dd_index != nullptr) {
      ut_ad(dd_index->name() == key->name);
      /* Spatial index indexes on only one column */
      size_t geom_col_idx;
      for (geom_col_idx = 0; geom_col_idx < dd_index->elements().size();
           ++geom_col_idx) {
        if (!dd_index->elements()[geom_col_idx]->column().is_se_hidden()) break;
      }
      const dd::Column &col = dd_index->elements()[geom_col_idx]->column();
      bool has_value = col.srs_id().has_value();
      index->srid_is_valid = has_value;
      index->srid = has_value ? col.srs_id().value() : 0;
    }
  }

  if (key_clustered) {
    DBUG_ASSERT(!(key->flags & (HA_FULLTEXT | HA_SPATIAL)));
    DBUG_ASSERT(key->flags & HA_NOSAME);
    index->ind_type = DICT_CLUSTERED | DICT_UNIQUE;
  } else if (key->flags & HA_FULLTEXT) {
    DBUG_ASSERT(!(key->flags & (HA_SPATIAL | HA_NOSAME)));
    DBUG_ASSERT(!(key->flags & HA_KEYFLAG_MASK &
                  ~(HA_FULLTEXT | HA_PACK_KEY | HA_BINARY_PACK_KEY)));
    index->ind_type = DICT_FTS;

    /* Set plugin parser */
    /* Note: key->parser is only parser name,
             we need to get parser from altered_table instead */
    if (key->flags & HA_USES_PARSER) {
      for (ulint j = 0; j < altered_table->s->keys; j++) {
        if (ut_strcmp(altered_table->key_info[j].name, key->name) == 0) {
          ut_ad(altered_table->key_info[j].flags & HA_USES_PARSER);

          plugin_ref parser = altered_table->key_info[j].parser;
          index->parser =
              static_cast<st_mysql_ftparser *>(plugin_decl(parser)->info);

          index->is_ngram =
              strncmp(plugin_name(parser)->str, FTS_NGRAM_PARSER_NAME,
                      plugin_name(parser)->length) == 0;

          break;
        }
      }

      DBUG_EXECUTE_IF("fts_instrument_use_default_parser",
                      index->parser = &fts_default_parser;);
      ut_ad(index->parser);
    }
  } else if (key->flags & HA_SPATIAL) {
    DBUG_ASSERT(!(key->flags & HA_NOSAME));
    index->ind_type = DICT_SPATIAL;
    ut_ad(n_fields == 1);
    ulint num_v = 0;

    /* Need to count the virtual fields before this spatial
    indexed field */
    for (ulint i = 0; i < key->key_part->fieldnr; i++) {
      if (innobase_is_v_fld(altered_table->field[i])) {
        num_v++;
      }
    }
    index->fields[0].col_no = key->key_part[0].fieldnr - num_v;
    index->fields[0].prefix_len = 0;
    index->fields[0].is_v_col = false;

    /* Currently only ascending order is supported in spatial
    index. */
    ut_ad(!(key->key_part[0].key_part_flag & HA_REVERSE_SORT));
    index->fields[0].is_ascending = true;

    if (!key->key_part[0].field->stored_in_db &&
        key->key_part[0].field->gcol_info) {
      /* Currently, the spatial index cannot be created
      on virtual columns. It is blocked in server
      layer */
      ut_ad(0);
      index->fields[0].is_v_col = true;
    } else {
      index->fields[0].is_v_col = false;
    }
  } else {
    index->ind_type = (key->flags & HA_NOSAME) ? DICT_UNIQUE : 0;
  }

  if (!(key->flags & HA_SPATIAL)) {
    for (i = 0; i < n_fields; i++) {
      innobase_create_index_field_def(altered_table, &key->key_part[i],
                                      &index->fields[i], new_clustered);

      if (index->fields[i].is_v_col) {
        index->ind_type |= DICT_VIRTUAL;
      }
    }
  }

  DBUG_VOID_RETURN;
}

/** Check whether the table has the FTS_DOC_ID column
 @return whether there exists an FTS_DOC_ID column */
bool innobase_fts_check_doc_id_col(
    const dict_table_t *table, /*!< in: InnoDB table with
                               fulltext index */
    const TABLE *altered_table,
    /*!< in: MySQL table with
    fulltext index */
    ulint *fts_doc_col_no,
    /*!< out: The column number for
    Doc ID, or ULINT_UNDEFINED
    if it is of wrong type */
    ulint *num_v) /*!< out: number of virtual column */
{
  *fts_doc_col_no = ULINT_UNDEFINED;

  const uint n_cols = altered_table->s->fields;
  ulint i;

  *num_v = 0;

  for (i = 0; i < n_cols; i++) {
    const Field *field = altered_table->field[i];

    if (innobase_is_v_fld(field)) {
      (*num_v)++;
    }

    if (my_strcasecmp(system_charset_info, field->field_name,
                      FTS_DOC_ID_COL_NAME)) {
      continue;
    }

    if (strcmp(field->field_name, FTS_DOC_ID_COL_NAME)) {
      my_error(ER_WRONG_COLUMN_NAME, MYF(0), field->field_name);
    } else if (field->type() != MYSQL_TYPE_LONGLONG ||
               field->pack_length() != 8 || field->real_maybe_null() ||
               !(field->flags & UNSIGNED_FLAG) || innobase_is_v_fld(field)) {
      my_error(ER_INNODB_FT_WRONG_DOCID_COLUMN, MYF(0), field->field_name);
    } else {
      *fts_doc_col_no = i - *num_v;
    }

    return (true);
  }

  if (!table) {
    return (false);
  }

  /* Not to count the virtual columns */
  i -= *num_v;

  for (; i + DATA_N_SYS_COLS < (uint)table->n_cols; i++) {
    const char *name = table->get_col_name(i);

    if (strcmp(name, FTS_DOC_ID_COL_NAME) == 0) {
#ifdef UNIV_DEBUG
      const dict_col_t *col;

      col = table->get_col(i);

      /* Because the FTS_DOC_ID does not exist in
      the MySQL data dictionary, this must be the
      internally created FTS_DOC_ID column. */
      ut_ad(col->mtype == DATA_INT);
      ut_ad(col->len == 8);
      ut_ad(col->prtype & DATA_NOT_NULL);
      ut_ad(col->prtype & DATA_UNSIGNED);
#endif /* UNIV_DEBUG */
      *fts_doc_col_no = i;
      return (true);
    }
  }

  return (false);
}

/** Check whether the table has a unique index with FTS_DOC_ID_INDEX_NAME
 on the Doc ID column.
 @return the status of the FTS_DOC_ID index */
enum fts_doc_id_index_enum innobase_fts_check_doc_id_index(
    const dict_table_t *table,  /*!< in: table definition */
    const TABLE *altered_table, /*!< in: MySQL table
                                that is being altered */
    ulint *fts_doc_col_no)      /*!< out: The column number for
                                Doc ID, or ULINT_UNDEFINED
                                if it is being created in
                                ha_alter_info */
{
  const dict_index_t *index;
  const dict_field_t *field;

  if (altered_table) {
    /* Check if a unique index with the name of
    FTS_DOC_ID_INDEX_NAME is being created. */

    for (uint i = 0; i < altered_table->s->keys; i++) {
      const KEY &key = altered_table->key_info[i];

      if (innobase_strcasecmp(key.name, FTS_DOC_ID_INDEX_NAME)) {
        continue;
      }

      if ((key.flags & HA_NOSAME) &&
          key.user_defined_key_parts == 1
          /* For now, we do not allow a descending index,
          because fts_doc_fetch_by_doc_id() uses the
          InnoDB SQL interpreter to look up FTS_DOC_ID. */
          && !(key.key_part[0].key_part_flag & HA_REVERSE_SORT) &&
          !strcmp(key.name, FTS_DOC_ID_INDEX_NAME) &&
          !strcmp(key.key_part[0].field->field_name, FTS_DOC_ID_COL_NAME)) {
        if (fts_doc_col_no) {
          *fts_doc_col_no = ULINT_UNDEFINED;
        }
        return (FTS_EXIST_DOC_ID_INDEX);
      } else {
        return (FTS_INCORRECT_DOC_ID_INDEX);
      }
    }
  }

  if (!table) {
    return (FTS_NOT_EXIST_DOC_ID_INDEX);
  }

  for (index = table->first_index(); index; index = index->next()) {
    /* Check if there exists a unique index with the name of
    FTS_DOC_ID_INDEX_NAME */
    if (innobase_strcasecmp(index->name, FTS_DOC_ID_INDEX_NAME)) {
      continue;
    }

    if (!dict_index_is_unique(index) ||
        dict_index_get_n_unique(index) > 1
        /* For now, we do not allow a descending index,
        because fts_doc_fetch_by_doc_id() uses the
        InnoDB SQL interpreter to look up FTS_DOC_ID. */
        || !index->get_field(0)->is_ascending ||
        strcmp(index->name, FTS_DOC_ID_INDEX_NAME)) {
      return (FTS_INCORRECT_DOC_ID_INDEX);
    }

    /* Check whether the index has FTS_DOC_ID as its
    first column */
    field = index->get_field(0);

    /* The column would be of a BIGINT data type */
    if (strcmp(field->name, FTS_DOC_ID_COL_NAME) == 0 &&
        field->col->mtype == DATA_INT && field->col->len == 8 &&
        field->col->prtype & DATA_NOT_NULL && !field->col->is_virtual()) {
      if (fts_doc_col_no) {
        *fts_doc_col_no = dict_col_get_no(field->col);
      }
      return (FTS_EXIST_DOC_ID_INDEX);
    } else {
      return (FTS_INCORRECT_DOC_ID_INDEX);
    }
  }

  /* Not found */
  return (FTS_NOT_EXIST_DOC_ID_INDEX);
}
/** Check whether the table has a unique index with FTS_DOC_ID_INDEX_NAME
 on the Doc ID column in MySQL create index definition.
 @return FTS_EXIST_DOC_ID_INDEX if there exists the FTS_DOC_ID index,
 FTS_INCORRECT_DOC_ID_INDEX if the FTS_DOC_ID index is of wrong format */
enum fts_doc_id_index_enum innobase_fts_check_doc_id_index_in_def(
    ulint n_key,         /*!< in: Number of keys */
    const KEY *key_info) /*!< in: Key definition */
{
  /* Check whether there is a "FTS_DOC_ID_INDEX" in the to be built index
  list */
  for (ulint j = 0; j < n_key; j++) {
    const KEY *key = &key_info[j];

    if (innobase_strcasecmp(key->name, FTS_DOC_ID_INDEX_NAME)) {
      continue;
    }

    /* Do a check on FTS DOC ID_INDEX, it must be unique,
    named as "FTS_DOC_ID_INDEX" and on column "FTS_DOC_ID" */
    if (!(key->flags & HA_NOSAME) ||
        key->user_defined_key_parts != 1
        /* For now, we do not allow a descending index,
        because fts_doc_fetch_by_doc_id() uses the
        InnoDB SQL interpreter to look up FTS_DOC_ID. */
        || (key->key_part[0].key_part_flag & HA_REVERSE_SORT) ||
        strcmp(key->name, FTS_DOC_ID_INDEX_NAME) ||
        strcmp(key->key_part[0].field->field_name, FTS_DOC_ID_COL_NAME)) {
      return (FTS_INCORRECT_DOC_ID_INDEX);
    }

    return (FTS_EXIST_DOC_ID_INDEX);
  }

  return (FTS_NOT_EXIST_DOC_ID_INDEX);
}

/** Create an index table where indexes are ordered as follows:

 IF a new primary key is defined for the table THEN

         1) New primary key
         2) The remaining keys in key_info

 ELSE

         1) All new indexes in the order they arrive from MySQL

 ENDIF

 @return key definitions */
template <typename Table>
static MY_ATTRIBUTE((warn_unused_result, malloc)) index_def_t
    *innobase_create_key_defs(mem_heap_t *heap,
                              /*!< in/out: memory heap where space for key
                              definitions are allocated */
                              const Alter_inplace_info *ha_alter_info,
                              /*!< in: alter operation */
                              const TABLE *altered_table,
                              /*!< in: MySQL table that is being altered */
                              const Table *new_dd_table,
                              /*!< in: new dd table */
                              ulint &n_add,
                              /*!< in/out: number of indexes to be created */
                              ulint &n_fts_add,
                              /*!< out: number of FTS indexes to be created */
                              bool got_default_clust,
                              /*!< in: whether the table lacks a primary key */
                              ulint &fts_doc_id_col,
                              /*!< in: The column number for Doc ID */
                              bool &add_fts_doc_id,
                              /*!< in: whether we need to add new DOC ID
                              column for FTS index */
                              bool &add_fts_doc_idx)
/*!< in: whether we need to add new DOC ID
index for FTS index */
{
  index_def_t *indexdef;
  index_def_t *indexdefs;
  bool new_primary;
  const uint *const add = ha_alter_info->index_add_buffer;
  const KEY *const key_info = ha_alter_info->key_info_buffer;

  DBUG_ENTER("innobase_create_key_defs");
  DBUG_ASSERT(!add_fts_doc_id || add_fts_doc_idx);
  DBUG_ASSERT(ha_alter_info->index_add_count == n_add);

  /* If there is a primary key, it is always the first index
  defined for the innodb_table. */

  new_primary = n_add > 0 && !my_strcasecmp(system_charset_info,
                                            key_info[*add].name, "PRIMARY");
  n_fts_add = 0;

  /* If there is a UNIQUE INDEX consisting entirely of NOT NULL
  columns and if the index does not contain column prefix(es)
  (only prefix/part of the column is indexed), MySQL will treat the
  index as a PRIMARY KEY unless the table already has one. */

  ut_ad(altered_table->s->primary_key == 0 ||
        altered_table->s->primary_key == MAX_KEY);

  if (got_default_clust && !new_primary) {
    new_primary = (altered_table->s->primary_key != MAX_KEY);
  }

  const bool rebuild =
      new_primary || add_fts_doc_id || innobase_need_rebuild(ha_alter_info);

  /* Reserve one more space if new_primary is true, and we might
  need to add the FTS_DOC_ID_INDEX */
  indexdef = indexdefs = static_cast<index_def_t *>(mem_heap_alloc(
      heap, sizeof *indexdef *
                (ha_alter_info->key_count + rebuild + got_default_clust)));

  if (rebuild) {
    ulint primary_key_number;

    if (new_primary) {
      if (n_add == 0) {
        DBUG_ASSERT(got_default_clust);
        DBUG_ASSERT(altered_table->s->primary_key == 0);
        primary_key_number = 0;
      } else if (ha_alter_info->handler_flags &
                 Alter_inplace_info::ALTER_COLUMN_NOT_NULLABLE) {
        primary_key_number = altered_table->s->primary_key;
      } else {
        primary_key_number = *add;
      }
    } else if (got_default_clust) {
      /* Create the GEN_CLUST_INDEX */
      index_def_t *index = indexdef++;

      index->fields = NULL;
      index->n_fields = 0;
      index->ind_type = DICT_CLUSTERED;
      index->name = innobase_index_reserve_name;
      index->rebuild = true;
      index->key_number = ~0;
      index->is_ngram = false;
      primary_key_number = ULINT_UNDEFINED;
      goto created_clustered;
    } else {
      primary_key_number = 0;
    }

    /* Create the PRIMARY key index definition */
    innobase_create_index_def(altered_table, new_dd_table, key_info,
                              primary_key_number, true, true, indexdef++, heap);

  created_clustered:
    n_add = 1;

    for (ulint i = 0; i < ha_alter_info->key_count; i++) {
      if (i == primary_key_number) {
        continue;
      }
      /* Copy the index definitions. */
      innobase_create_index_def(altered_table, new_dd_table, key_info, i, true,
                                false, indexdef, heap);

      if (indexdef->ind_type & DICT_FTS) {
        n_fts_add++;
      }

      indexdef++;
      n_add++;
    }

    if (n_fts_add > 0) {
      ulint num_v = 0;

      if (!add_fts_doc_id &&
          !innobase_fts_check_doc_id_col(NULL, altered_table, &fts_doc_id_col,
                                         &num_v)) {
        fts_doc_id_col = altered_table->s->fields - num_v;
        add_fts_doc_id = true;
      }

      if (!add_fts_doc_idx) {
        fts_doc_id_index_enum ret;
        ulint doc_col_no;

        ret = innobase_fts_check_doc_id_index(NULL, altered_table, &doc_col_no);

        /* This should have been checked before */
        ut_ad(ret != FTS_INCORRECT_DOC_ID_INDEX);

        if (ret == FTS_NOT_EXIST_DOC_ID_INDEX) {
          add_fts_doc_idx = true;
        } else {
          ut_ad(ret == FTS_EXIST_DOC_ID_INDEX);
          ut_ad(doc_col_no == ULINT_UNDEFINED || doc_col_no == fts_doc_id_col);
        }
      }
    }
  } else {
    /* Create definitions for added secondary indexes. */

    for (ulint i = 0; i < n_add; i++) {
      innobase_create_index_def(altered_table, new_dd_table, key_info, add[i],
                                false, false, indexdef, heap);

      if (indexdef->ind_type & DICT_FTS) {
        n_fts_add++;
      }

      indexdef++;
    }
  }

  DBUG_ASSERT(indexdefs + n_add == indexdef);

  if (add_fts_doc_idx) {
    index_def_t *index = indexdef++;

    index->fields = static_cast<index_field_t *>(
        mem_heap_alloc(heap, sizeof *index->fields));
    index->n_fields = 1;
    index->fields->col_no = fts_doc_id_col;
    index->fields->prefix_len = 0;
    index->fields->is_ascending = true;
    index->fields->is_v_col = false;
    index->ind_type = DICT_UNIQUE;
    ut_ad(!rebuild || !add_fts_doc_id ||
          fts_doc_id_col <= altered_table->s->fields);

    index->name = FTS_DOC_ID_INDEX_NAME;
    index->is_ngram = false;
    index->rebuild = rebuild;

    /* TODO: assign a real MySQL key number for this */
    index->key_number = ULINT_UNDEFINED;
    n_add++;
  }

  DBUG_ASSERT(indexdef > indexdefs);
  DBUG_ASSERT((ulint)(indexdef - indexdefs) <=
              ha_alter_info->key_count + add_fts_doc_idx + got_default_clust);
  DBUG_ASSERT(ha_alter_info->index_add_count <= n_add);
  DBUG_RETURN(indexdefs);
}

/** Check each index column size, make sure they do not exceed the max limit
 @return true if index column size exceeds limit */
static MY_ATTRIBUTE((warn_unused_result)) bool innobase_check_column_length(
    ulint max_col_len,   /*!< in: maximum column length */
    const KEY *key_info) /*!< in: Indexes to be created */
{
  for (ulint key_part = 0; key_part < key_info->user_defined_key_parts;
       key_part++) {
    if (key_info->key_part[key_part].length > max_col_len) {
      return (true);
    }
  }
  return (false);
}

/** Drop in-memory metadata for index (dict_index_t) left from previous
online ALTER operation.
@param[in]	table	table to check
@param[in]	locked	if it is dict_sys mutex locked */
static void online_retry_drop_dict_indexes(dict_table_t *table, bool locked) {
  if (!locked) {
    mutex_enter(&dict_sys->mutex);
  }

  bool modify = false;
  dict_index_t *index = table->first_index();

  while ((index = index->next())) {
    if (dict_index_get_online_status(index) == ONLINE_INDEX_ABORTED_DROPPED) {
      dict_index_t *prev = UT_LIST_GET_PREV(indexes, index);

      dict_index_remove_from_cache(table, index);

      index = prev;

      modify = true;
    }
  }

  if (modify) {
    /* Since the table has been modified, table->def_trx_id should be
    adjusted like row_merge_drop_indexes(). However, this function may
    be called before the DDL transaction starts, so it is impossible to
    get current DDL transaction ID. Thus advancing def_trx_id by 1 to
    simply inform other threads about this change. */
    ++table->def_trx_id;
  }

  if (!locked) {
    mutex_exit(&dict_sys->mutex);
  }
}

/** Determines if InnoDB is dropping a foreign key constraint.
@param foreign the constraint
@param drop_fk constraints being dropped
@param n_drop_fk number of constraints that are being dropped
@return whether the constraint is being dropped */
inline MY_ATTRIBUTE((warn_unused_result)) bool innobase_dropping_foreign(
    const dict_foreign_t *foreign, dict_foreign_t **drop_fk, ulint n_drop_fk) {
  while (n_drop_fk--) {
    if (*drop_fk++ == foreign) {
      return (true);
    }
  }

  return (false);
}

/** Determines if an InnoDB FOREIGN KEY constraint depends on a
column that is being dropped or modified to NOT NULL.
@param user_table InnoDB table as it is before the ALTER operation
@param col_name Name of the column being altered
@param drop_fk constraints being dropped
@param n_drop_fk number of constraints that are being dropped
@param drop true=drop column, false=set NOT NULL
@retval true Not allowed (will call my_error())
@retval false Allowed
*/
static MY_ATTRIBUTE((warn_unused_result)) bool innobase_check_foreigns_low(
    const dict_table_t *user_table, dict_foreign_t **drop_fk, ulint n_drop_fk,
    const char *col_name, bool drop) {
  dict_foreign_t *foreign;
  ut_ad(mutex_own(&dict_sys->mutex));

  /* Check if any FOREIGN KEY constraints are defined on this
  column. */

  for (dict_foreign_set::iterator it = user_table->foreign_set.begin();
       it != user_table->foreign_set.end(); ++it) {
    foreign = *it;

    if (!drop && !(foreign->type & (DICT_FOREIGN_ON_DELETE_SET_NULL |
                                    DICT_FOREIGN_ON_UPDATE_SET_NULL))) {
      continue;
    }

    if (innobase_dropping_foreign(foreign, drop_fk, n_drop_fk)) {
      continue;
    }

    for (unsigned f = 0; f < foreign->n_fields; f++) {
      if (!strcmp(foreign->foreign_col_names[f], col_name)) {
        my_error(drop ? ER_FK_COLUMN_CANNOT_DROP : ER_FK_COLUMN_NOT_NULL,
                 MYF(0), col_name, foreign->id);
        return (true);
      }
    }
  }

  if (!drop) {
    /* SET NULL clauses on foreign key constraints of
    child tables affect the child tables, not the parent table.
    The column can be NOT NULL in the parent table. */
    return (false);
  }

  /* Check if any FOREIGN KEY constraints in other tables are
  referring to the column that is being dropped. */
  for (dict_foreign_set::iterator it = user_table->referenced_set.begin();
       it != user_table->referenced_set.end(); ++it) {
    foreign = *it;

    if (innobase_dropping_foreign(foreign, drop_fk, n_drop_fk)) {
      continue;
    }

    for (unsigned f = 0; f < foreign->n_fields; f++) {
      char display_name[FN_REFLEN];

      if (strcmp(foreign->referenced_col_names[f], col_name)) {
        continue;
      }

      char *buf_end = innobase_convert_name(
          display_name, (sizeof display_name) - 1, foreign->foreign_table_name,
          strlen(foreign->foreign_table_name), NULL);
      *buf_end = '\0';
      my_error(ER_FK_COLUMN_CANNOT_DROP_CHILD, MYF(0), col_name, foreign->id,
               display_name);

      return (true);
    }
  }

  return (false);
}

/** Determines if an InnoDB FOREIGN KEY constraint depends on a
column that is being dropped or modified to NOT NULL.
@param ha_alter_info Data used during in-place alter
@param altered_table MySQL table that is being altered
@param old_table MySQL table as it is before the ALTER operation
@param user_table InnoDB table as it is before the ALTER operation
@param drop_fk constraints being dropped
@param n_drop_fk number of constraints that are being dropped
@retval true Not allowed (will call my_error())
@retval false Allowed
*/
static MY_ATTRIBUTE((warn_unused_result)) bool innobase_check_foreigns(
    Alter_inplace_info *ha_alter_info, const TABLE *altered_table,
    const TABLE *old_table, const dict_table_t *user_table,
    dict_foreign_t **drop_fk, ulint n_drop_fk) {
  List_iterator_fast<Create_field> cf_it(
      ha_alter_info->alter_info->create_list);

  for (Field **fp = old_table->field; *fp; fp++) {
    cf_it.rewind();
    const Create_field *new_field;

    ut_ad(!(*fp)->real_maybe_null() == !!((*fp)->flags & NOT_NULL_FLAG));

    while ((new_field = cf_it++)) {
      if (new_field->field == *fp) {
        break;
      }
    }

    if (!new_field || (new_field->flags & NOT_NULL_FLAG)) {
      if (innobase_check_foreigns_low(user_table, drop_fk, n_drop_fk,
                                      (*fp)->field_name, !new_field)) {
        return (true);
      }
    }
  }

  return (false);
}

/** Convert a default value for ADD COLUMN.

@param heap Memory heap where allocated
@param dfield InnoDB data field to copy to
@param field MySQL value for the column
@param comp nonzero if in compact format */
static void innobase_build_col_map_add(mem_heap_t *heap, dfield_t *dfield,
                                       const Field *field, ulint comp) {
  if (field->is_real_null()) {
    dfield_set_null(dfield);
    return;
  }

  ulint size = field->pack_length();

  byte *buf = static_cast<byte *>(mem_heap_alloc(heap, size));

  const byte *mysql_data = field->ptr;

  row_mysql_store_col_in_innobase_format(dfield, buf, true, mysql_data, size,
                                         comp);
}

/** Construct the translation table for reordering, dropping or
adding columns.

@param ha_alter_info Data used during in-place alter
@param altered_table MySQL table that is being altered
@param table MySQL table as it is before the ALTER operation
@param new_table InnoDB table corresponding to MySQL altered_table
@param old_table InnoDB table corresponding to MYSQL table
@param add_cols Default values for ADD COLUMN, or NULL if no ADD COLUMN
@param heap Memory heap where allocated
@return array of integers, mapping column numbers in the table
to column numbers in altered_table */
static MY_ATTRIBUTE((warn_unused_result)) const ulint *innobase_build_col_map(
    Alter_inplace_info *ha_alter_info, const TABLE *altered_table,
    const TABLE *table, const dict_table_t *new_table,
    const dict_table_t *old_table, dtuple_t *add_cols, mem_heap_t *heap) {
  DBUG_ENTER("innobase_build_col_map");
  DBUG_ASSERT(altered_table != table);
  DBUG_ASSERT(new_table != old_table);
  DBUG_ASSERT(new_table->get_n_cols() + dict_table_get_n_v_cols(new_table) >=
              altered_table->s->fields + DATA_N_SYS_COLS);
  DBUG_ASSERT(old_table->get_n_cols() + dict_table_get_n_v_cols(old_table) >=
              table->s->fields + DATA_N_SYS_COLS);
  DBUG_ASSERT(!!add_cols == !!(ha_alter_info->handler_flags &
                               Alter_inplace_info::ADD_COLUMN));
  DBUG_ASSERT(!add_cols ||
              dtuple_get_n_fields(add_cols) == new_table->get_n_cols());

  ulint *col_map = static_cast<ulint *>(mem_heap_alloc(
      heap, (old_table->n_cols + old_table->n_v_cols) * sizeof *col_map));

  List_iterator_fast<Create_field> cf_it(
      ha_alter_info->alter_info->create_list);
  uint i = 0;
  uint num_v = 0;

  /* Any dropped columns will map to ULINT_UNDEFINED. */
  for (uint old_i = 0; old_i + DATA_N_SYS_COLS < old_table->n_cols; old_i++) {
    col_map[old_i] = ULINT_UNDEFINED;
  }

  for (uint old_i = 0; old_i < old_table->n_v_cols; old_i++) {
    col_map[old_i + old_table->n_cols] = ULINT_UNDEFINED;
  }

  while (const Create_field *new_field = cf_it++) {
    bool is_v = false;

    if (innobase_is_v_fld(new_field)) {
      is_v = true;
    }

    ulint num_old_v = 0;

    for (uint old_i = 0; table->field[old_i]; old_i++) {
      const Field *field = table->field[old_i];
      if (innobase_is_v_fld(field)) {
        if (is_v && new_field->field == field) {
          col_map[old_table->n_cols + num_v] = num_old_v;
          num_old_v++;
          goto found_col;
        }
        num_old_v++;
        continue;
      }

      if (new_field->field == field) {
        col_map[old_i - num_old_v] = i;
        goto found_col;
      }
    }

    ut_ad(!is_v);
    innobase_build_col_map_add(heap, dtuple_get_nth_field(add_cols, i),
                               altered_table->field[i + num_v],
                               dict_table_is_comp(new_table));
  found_col:
    if (is_v) {
      num_v++;
    } else {
      i++;
    }
  }

  DBUG_ASSERT(i == altered_table->s->fields - num_v);

  i = table->s->fields - old_table->n_v_cols;

  /* Add the InnoDB hidden FTS_DOC_ID column, if any. */
  if (i + DATA_N_SYS_COLS < old_table->n_cols) {
    /* There should be exactly one extra field,
    the FTS_DOC_ID. */
    DBUG_ASSERT(DICT_TF2_FLAG_IS_SET(old_table, DICT_TF2_FTS_HAS_DOC_ID));
    DBUG_ASSERT(i + DATA_N_SYS_COLS + 1 == old_table->n_cols);
    DBUG_ASSERT(!strcmp(old_table->get_col_name(i), FTS_DOC_ID_COL_NAME));
    if (altered_table->s->fields + DATA_N_SYS_COLS - new_table->n_v_cols <
        new_table->n_cols) {
      DBUG_ASSERT(DICT_TF2_FLAG_IS_SET(new_table, DICT_TF2_FTS_HAS_DOC_ID));
      DBUG_ASSERT(altered_table->s->fields + DATA_N_SYS_COLS + 1 ==
                  static_cast<ulint>(new_table->n_cols + new_table->n_v_cols));
      col_map[i] = altered_table->s->fields - new_table->n_v_cols;
    } else {
      DBUG_ASSERT(!DICT_TF2_FLAG_IS_SET(new_table, DICT_TF2_FTS_HAS_DOC_ID));
      col_map[i] = ULINT_UNDEFINED;
    }

    i++;
  } else {
    DBUG_ASSERT(!DICT_TF2_FLAG_IS_SET(old_table, DICT_TF2_FTS_HAS_DOC_ID));
  }

  for (; i < old_table->n_cols; i++) {
    col_map[i] = i + new_table->n_cols - old_table->n_cols;
  }

  DBUG_RETURN(col_map);
}

/** Drop newly create FTS index related auxiliary table during
FIC create index process, before fts_add_index is called
@param table table that was being rebuilt online
@param trx transaction
@return DB_SUCCESS if successful, otherwise last error code
*/
static dberr_t innobase_drop_fts_index_table(dict_table_t *table, trx_t *trx) {
  dberr_t ret_err = DB_SUCCESS;

  for (dict_index_t *index = table->first_index(); index != NULL;
       index = index->next()) {
    if (index->type & DICT_FTS) {
      dberr_t err;

      err = fts_drop_index_tables(trx, index, nullptr);

      if (err != DB_SUCCESS) {
        ret_err = err;
      }
    }
  }

  return (ret_err);
}

/** Get the new non-virtual column names if any columns were renamed
@param ha_alter_info	Data used during in-place alter
@param altered_table	MySQL table that is being altered
@param table		MySQL table as it is before the ALTER operation
@param user_table	InnoDB table as it is before the ALTER operation
@param heap		Memory heap for the allocation
@return array of new column names in rebuilt_table, or NULL if not renamed */
static MY_ATTRIBUTE((warn_unused_result)) const char **innobase_get_col_names(
    Alter_inplace_info *ha_alter_info, const TABLE *altered_table,
    const TABLE *table, const dict_table_t *user_table, mem_heap_t *heap) {
  const char **cols;
  uint i;

  DBUG_ENTER("innobase_get_col_names");
  DBUG_ASSERT(user_table->n_t_def > table->s->fields);
  DBUG_ASSERT(ha_alter_info->handler_flags &
              Alter_inplace_info::ALTER_COLUMN_NAME);

  cols = static_cast<const char **>(
      mem_heap_zalloc(heap, user_table->n_def * sizeof *cols));

  i = 0;
  List_iterator_fast<Create_field> cf_it(
      ha_alter_info->alter_info->create_list);
  while (const Create_field *new_field = cf_it++) {
    ulint num_v = 0;
    DBUG_ASSERT(i < altered_table->s->fields);

    if (innobase_is_v_fld(new_field)) {
      continue;
    }

    for (uint old_i = 0; table->field[old_i]; old_i++) {
      if (innobase_is_v_fld(table->field[old_i])) {
        num_v++;
      }

      if (new_field->field == table->field[old_i]) {
        cols[old_i - num_v] = new_field->field_name;
        break;
      }
    }

    i++;
  }

  /* Copy the internal column names. */
  i = table->s->fields - user_table->n_v_def;
  cols[i] = user_table->get_col_name(i);

  while (++i < user_table->n_def) {
    cols[i] = cols[i - 1] + strlen(cols[i - 1]) + 1;
  }

  DBUG_RETURN(cols);
}

/** Check whether the column prefix is increased, decreased, or unchanged.
@param[in]	new_prefix_len	new prefix length
@param[in]	old_prefix_len	new prefix length
@retval	1	prefix is increased
@retval	0	prefix is unchanged
@retval	-1	prefix is decreased */
static inline lint innobase_pk_col_prefix_compare(ulint new_prefix_len,
                                                  ulint old_prefix_len) {
  ut_ad(new_prefix_len < REC_MAX_DATA_SIZE);
  ut_ad(old_prefix_len < REC_MAX_DATA_SIZE);

  if (new_prefix_len == old_prefix_len) {
    return (0);
  }

  if (new_prefix_len == 0) {
    new_prefix_len = ULINT_MAX;
  }

  if (old_prefix_len == 0) {
    old_prefix_len = ULINT_MAX;
  }

  if (new_prefix_len > old_prefix_len) {
    return (1);
  } else {
    return (-1);
  }
}

/** Check whether the column is existing in old table.
@param[in]	new_col_no	new column no
@param[in]	col_map		mapping of old column numbers to new ones
@param[in]	col_map_size	the column map size
@return true if the column is existing, otherwise false. */
static inline bool innobase_pk_col_is_existing(const ulint new_col_no,
                                               const ulint *col_map,
                                               const ulint col_map_size) {
  for (ulint i = 0; i < col_map_size; i++) {
    if (col_map[i] == new_col_no) {
      return (true);
    }
  }

  return (false);
}

/** Determine whether both the indexes have same set of primary key
fields arranged in the same order.

Rules when we cannot skip sorting:
(1) Removing existing PK columns somewhere else than at the end of the PK;
(2) Adding existing columns to the PK, except at the end of the PK when no
columns are removed from the PK;
(3) Changing the order of existing PK columns;
(4) Decreasing the prefix length just like removing existing PK columns
follows rule(1), Increasing the prefix length just like adding existing
PK columns follows rule(2);
(5) Changing the ascending order of the existing PK columns.
@param[in]	col_map		mapping of old column numbers to new ones
@param[in]	old_clust_index	index to be compared
@param[in]	new_clust_index index to be compared
@retval true if both indexes have same order.
@retval false. */
static MY_ATTRIBUTE((warn_unused_result)) bool innobase_pk_order_preserved(
    const ulint *col_map, const dict_index_t *old_clust_index,
    const dict_index_t *new_clust_index) {
  ulint old_n_uniq = dict_index_get_n_ordering_defined_by_user(old_clust_index);
  ulint new_n_uniq = dict_index_get_n_ordering_defined_by_user(new_clust_index);

  ut_ad(old_clust_index->is_clustered());
  ut_ad(new_clust_index->is_clustered());
  ut_ad(old_clust_index->table != new_clust_index->table);
  ut_ad(col_map != NULL);

  if (old_n_uniq == 0) {
    /* There was no PRIMARY KEY in the table.
    If there is no PRIMARY KEY after the ALTER either,
    no sorting is needed. */
    return (new_n_uniq == old_n_uniq);
  }

  /* DROP PRIMARY KEY is only allowed in combination with
  ADD PRIMARY KEY. */
  ut_ad(new_n_uniq > 0);

  /* The order of the last processed new_clust_index key field,
  not counting ADD COLUMN, which are constant. */
  lint last_field_order = -1;
  ulint existing_field_count = 0;
  ulint old_n_cols = old_clust_index->table->get_n_cols();
  for (ulint new_field = 0; new_field < new_n_uniq; new_field++) {
    ulint new_col_no = new_clust_index->fields[new_field].col->ind;

    /* Check if there is a match in old primary key. */
    ulint old_field = 0;
    while (old_field < old_n_uniq) {
      ulint old_col_no = old_clust_index->fields[old_field].col->ind;

      if (col_map[old_col_no] == new_col_no) {
        break;
      }

      old_field++;
    }

    /* The order of key field in the new primary key.
    1. old PK column:      idx in old primary key
    2. existing column:    old_n_uniq + sequence no
    3. newly added column: no order */
    lint new_field_order;
    const bool old_pk_column = old_field < old_n_uniq;

    if (old_pk_column) {
      new_field_order = old_field;
    } else if (innobase_pk_col_is_existing(new_col_no, col_map, old_n_cols)) {
      new_field_order = old_n_uniq + existing_field_count++;
    } else {
      /* Skip newly added column. */
      continue;
    }

    if (last_field_order + 1 != new_field_order) {
      /* Old PK order is not kept, or existing column
      is not added at the end of old PK. */
      return (false);
    }

    last_field_order = new_field_order;

    if (!old_pk_column) {
      continue;
    }

    /* Check prefix length change. */
    const lint prefix_change = innobase_pk_col_prefix_compare(
        new_clust_index->fields[new_field].prefix_len,
        old_clust_index->fields[old_field].prefix_len);

    if (prefix_change < 0) {
      /* If a column's prefix length is decreased, it should
      be the last old PK column in new PK.
      Note: we set last_field_order to -2, so that if	there
      are any old PK colmns or existing columns after it in
      new PK, the comparison to new_field_order will fail in
      the next round.*/
      last_field_order = -2;
    } else if (prefix_change > 0) {
      /* If a column's prefix length is increased, it	should
      be the last PK column in old PK. */
      if (old_field != old_n_uniq - 1) {
        return (false);
      }
    }

    /* Check new primary key field ascending or descending changes
    compared to old primary key field. */
    bool change_asc = (new_clust_index->fields[new_field].is_ascending ==
                       old_clust_index->fields[old_field].is_ascending);

    if (!change_asc) {
      return (false);
    }
  }

  return (true);
}

/** Check if we are creating spatial indexes on GIS columns, which are
legacy columns from earlier MySQL, such as 5.6. If so, we have to update
the mtypes of the old GIS columns to DATA_GEOMETRY.
In 5.6, we store GIS columns as DATA_BLOB in InnoDB layer, it will introduce
confusion when we run latest server on older data. That's why we need to
do the upgrade.
@param[in] ha_alter_info	Data used during in-place alter
@param[in] table		Table on which we want to add indexes
@param[in] trx			Transaction
@return DB_SUCCESS if update successfully or no columns need to be updated,
otherwise DB_ERROR, which means we can't update the mtype for some
column, and creating spatial index on it should be dangerous */
static dberr_t innobase_check_gis_columns(Alter_inplace_info *ha_alter_info,
                                          dict_table_t *table, trx_t *trx) {
  DBUG_ENTER("innobase_check_gis_columns");

  for (uint key_num = 0; key_num < ha_alter_info->index_add_count; key_num++) {
    const KEY &key =
        ha_alter_info
            ->key_info_buffer[ha_alter_info->index_add_buffer[key_num]];

    if (!(key.flags & HA_SPATIAL)) {
      continue;
    }

    ut_ad(key.user_defined_key_parts == 1);
    const KEY_PART_INFO &key_part = key.key_part[0];

    /* Does not support spatial index on virtual columns */
    if (innobase_is_v_fld(key_part.field)) {
      DBUG_RETURN(DB_UNSUPPORTED);
    }

    ulint col_nr = dict_table_has_column(table, key_part.field->field_name,
                                         key_part.fieldnr);
    ut_ad(col_nr != table->n_def);
    dict_col_t *col = &table->cols[col_nr];

    if (col->mtype != DATA_BLOB) {
      ut_ad(DATA_GEOMETRY_MTYPE(col->mtype));
      continue;
    }

    const char *col_name = table->get_col_name(col_nr);
    col->mtype = DATA_GEOMETRY;

    ib::info(ER_IB_MSG_598)
        << "Updated mtype of column" << col_name << " in table " << table->name
        << ", whose id is " << table->id << " to DATA_GEOMETRY";
  }

  DBUG_RETURN(DB_SUCCESS);
}

/** Collect virtual column info for its addition
@param[in] ha_alter_info	Data used during in-place alter
@param[in] altered_table	MySQL table that is being altered to
@param[in] table		MySQL table as it is before the ALTER operation
@retval true Failure
@retval false Success */
static bool prepare_inplace_add_virtual(Alter_inplace_info *ha_alter_info,
                                        const TABLE *altered_table,
                                        const TABLE *table) {
  ha_innobase_inplace_ctx *ctx;
  ulint i = 0;
  ulint j = 0;
  const Create_field *new_field;

  ctx = static_cast<ha_innobase_inplace_ctx *>(ha_alter_info->handler_ctx);

  ctx->num_to_add_vcol =
      altered_table->s->fields + ctx->num_to_drop_vcol - table->s->fields;

  ctx->add_vcol = static_cast<dict_v_col_t *>(
      mem_heap_zalloc(ctx->heap, ctx->num_to_add_vcol * sizeof *ctx->add_vcol));
  ctx->add_vcol_name = static_cast<const char **>(mem_heap_alloc(
      ctx->heap, ctx->num_to_add_vcol * sizeof *ctx->add_vcol_name));

  List_iterator_fast<Create_field> cf_it(
      ha_alter_info->alter_info->create_list);

  while ((new_field = (cf_it++)) != NULL) {
    const Field *field = new_field->field;
    ulint old_i;

    for (old_i = 0; table->field[old_i]; old_i++) {
      const Field *n_field = table->field[old_i];
      if (field == n_field) {
        break;
      }
    }

    i++;

    if (table->field[old_i]) {
      continue;
    }

    ut_ad(!field);

    ulint col_len;
    ulint is_unsigned;
    ulint field_type;
    ulint charset_no;

    field = altered_table->field[i - 1];

    ulint col_type = get_innobase_type_from_mysql_type(&is_unsigned, field);

    if (!field->gcol_info || field->stored_in_db) {
      my_error(ER_WRONG_KEY_COLUMN, MYF(0), field->field_name);
      return (true);
    }

    col_len = field->pack_length();
    field_type = (ulint)field->type();

    if (!field->real_maybe_null()) {
      field_type |= DATA_NOT_NULL;
    }

    if (field->binary()) {
      field_type |= DATA_BINARY_TYPE;
    }

    if (is_unsigned) {
      field_type |= DATA_UNSIGNED;
    }

    if (dtype_is_string_type(col_type)) {
      charset_no = (ulint)field->charset()->number;

      DBUG_EXECUTE_IF("ib_alter_add_virtual_fail",
                      charset_no += MAX_CHAR_COLL_NUM;);

      if (charset_no > MAX_CHAR_COLL_NUM) {
        my_error(ER_WRONG_KEY_COLUMN, MYF(0), field->field_name);
        return (true);
      }
    } else {
      charset_no = 0;
    }

    if (field->type() == MYSQL_TYPE_VARCHAR) {
      uint32 length_bytes =
          static_cast<const Field_varstring *>(field)->length_bytes;

      col_len -= length_bytes;

      if (length_bytes == 2) {
        field_type |= DATA_LONG_TRUE_VARCHAR;
      }
    }

    ctx->add_vcol[j].m_col.prtype = dtype_form_prtype(field_type, charset_no);

    ctx->add_vcol[j].m_col.prtype |= DATA_VIRTUAL;

    ctx->add_vcol[j].m_col.mtype = col_type;

    ctx->add_vcol[j].m_col.len = col_len;

    ctx->add_vcol[j].m_col.ind = i - 1;
    ctx->add_vcol[j].num_base = field->gcol_info->non_virtual_base_columns();
    ctx->add_vcol_name[j] = field->field_name;
    ctx->add_vcol[j].base_col = static_cast<dict_col_t **>(mem_heap_alloc(
        ctx->heap,
        ctx->add_vcol[j].num_base * sizeof *(ctx->add_vcol[j].base_col)));
    ctx->add_vcol[j].v_pos =
        ctx->old_table->n_v_cols - ctx->num_to_drop_vcol + j;

    /* No need to track the list */
    ctx->add_vcol[j].v_indexes = NULL;
    innodb_base_col_setup(ctx->old_table, field, &ctx->add_vcol[j]);
    j++;
  }

  return (false);
}

/** Collect virtual column info for its addition
@param[in] ha_alter_info	Data used during in-place alter
@param[in] altered_table	MySQL table that is being altered to
@param[in] table		MySQL table as it is before the ALTER operation
@retval true Failure
@retval false Success */
static bool prepare_inplace_drop_virtual(Alter_inplace_info *ha_alter_info,
                                         const TABLE *altered_table,
                                         const TABLE *table) {
  ha_innobase_inplace_ctx *ctx;
  ulint j = 0;

  ctx = static_cast<ha_innobase_inplace_ctx *>(ha_alter_info->handler_ctx);

  ctx->num_to_drop_vcol = ha_alter_info->alter_info->drop_list.size();

  ctx->drop_vcol = static_cast<dict_v_col_t *>(mem_heap_alloc(
      ctx->heap, ctx->num_to_drop_vcol * sizeof *ctx->drop_vcol));
  ctx->drop_vcol_name = static_cast<const char **>(mem_heap_alloc(
      ctx->heap, ctx->num_to_drop_vcol * sizeof *ctx->drop_vcol_name));

  for (const Alter_drop *drop : ha_alter_info->alter_info->drop_list) {
    const Field *field;
    ulint old_i;

    ut_ad(drop->type == Alter_drop::COLUMN);

    for (old_i = 0; table->field[old_i]; old_i++) {
      const Field *n_field = table->field[old_i];
      if (!my_strcasecmp(system_charset_info, n_field->field_name,
                         drop->name)) {
        break;
      }
    }

    if (!table->field[old_i]) {
      continue;
    }

    ulint col_len;
    ulint is_unsigned;
    ulint field_type;
    ulint charset_no;

    field = table->field[old_i];

    ulint col_type = get_innobase_type_from_mysql_type(&is_unsigned, field);

    if (!field->gcol_info || field->stored_in_db) {
      my_error(ER_WRONG_KEY_COLUMN, MYF(0), field->field_name);
      return (true);
    }

    col_len = field->pack_length();
    field_type = (ulint)field->type();

    if (!field->real_maybe_null()) {
      field_type |= DATA_NOT_NULL;
    }

    if (field->binary()) {
      field_type |= DATA_BINARY_TYPE;
    }

    if (is_unsigned) {
      field_type |= DATA_UNSIGNED;
    }

    if (dtype_is_string_type(col_type)) {
      charset_no = (ulint)field->charset()->number;

      DBUG_EXECUTE_IF("ib_alter_add_virtual_fail",
                      charset_no += MAX_CHAR_COLL_NUM;);

      if (charset_no > MAX_CHAR_COLL_NUM) {
        my_error(ER_WRONG_KEY_COLUMN, MYF(0), field->field_name);
        return (true);
      }
    } else {
      charset_no = 0;
    }

    if (field->type() == MYSQL_TYPE_VARCHAR) {
      uint32 length_bytes =
          static_cast<const Field_varstring *>(field)->length_bytes;

      col_len -= length_bytes;

      if (length_bytes == 2) {
        field_type |= DATA_LONG_TRUE_VARCHAR;
      }
    }

    ctx->drop_vcol[j].m_col.prtype = dtype_form_prtype(field_type, charset_no);

    ctx->drop_vcol[j].m_col.prtype |= DATA_VIRTUAL;

    ctx->drop_vcol[j].m_col.mtype = col_type;

    ctx->drop_vcol[j].m_col.len = col_len;

    ctx->drop_vcol[j].m_col.ind = old_i;

    ctx->drop_vcol_name[j] = field->field_name;

    dict_v_col_t *v_col = dict_table_get_nth_v_col_mysql(ctx->old_table, old_i);
    ctx->drop_vcol[j].v_pos = v_col->v_pos;
    j++;
  }

  return (false);
}

/** Adjust the create index column number from "New table" to
"old InnoDB table" while we are doing dropping virtual column. Since we do
not create separate new table for the dropping/adding virtual columns.
To correctly find the indexed column, we will need to find its col_no
in the "Old Table", not the "New table".
@param[in]	ha_alter_info	Data used during in-place alter
@param[in]	old_table	MySQL table as it is before the ALTER operation
@param[in]	num_v_dropped	number of virtual column dropped
@param[in,out]	index_def	index definition */
static void innodb_v_adjust_idx_col(const Alter_inplace_info *ha_alter_info,
                                    const TABLE *old_table, ulint num_v_dropped,
                                    index_def_t *index_def) {
  List_iterator_fast<Create_field> cf_it(
      ha_alter_info->alter_info->create_list);
  for (ulint i = 0; i < index_def->n_fields; i++) {
#ifdef UNIV_DEBUG
    bool col_found = false;
#endif /* UNIV_DEBUG */
    ulint num_v = 0;

    index_field_t *index_field = &index_def->fields[i];

    /* Only adjust virtual column col_no, since non-virtual
    column position (in non-vcol list) won't change unless
    table rebuild */
    if (!index_field->is_v_col) {
      continue;
    }

    const Field *field = NULL;

    cf_it.rewind();

    /* Found the field in the new table */
    while (const Create_field *new_field = cf_it++) {
      if (!new_field->is_virtual_gcol()) {
        continue;
      }

      field = new_field->field;

      if (num_v == index_field->col_no) {
        break;
      }
      num_v++;
    }

    if (!field) {
      /* this means the field is a newly added field, this
      should have been blocked when we drop virtual column
      at the same time */
      ut_ad(num_v_dropped > 0);
      ut_a(0);
    }

    ut_ad(field->is_virtual_gcol());

    num_v = 0;

    /* Look for its position in old table */
    for (uint old_i = 0; old_table->field[old_i]; old_i++) {
      if (old_table->field[old_i] == field) {
        /* Found it, adjust its col_no to its position
        in old table */
        index_def->fields[i].col_no = num_v;
        ut_d(col_found = true);
        break;
      }

      if (old_table->field[old_i]->is_virtual_gcol()) {
        num_v++;
      }
    }

    ut_ad(col_found);
  }
}

/** Replace the table name in filename with the specified one
@param[in]	filename	original file name
@param[out]	new_filename	new file name
@param[in]	table_name	to replace with this table name,
                                in the format of db/name */
static void replace_table_name(const char *filename, char *new_filename,
                               const char *table_name) {
  const char *slash = strrchr(filename, OS_PATH_SEPARATOR);
  size_t len = 0;

  if (slash == NULL) {
    len = 0;
  } else {
    len = slash - filename + 1;
  }

  memcpy(new_filename, filename, len);

  slash = strchr(table_name, '/');
  ut_ad(slash != NULL);

  strcpy(new_filename + len, slash + 1);

  len += strlen(slash + 1);

  strcpy(new_filename + len, dot_ext[IBD]);
}

/** Update the metadata in prepare phase. This only check if dd::Tablespace
should be removed or(and) created, because to remove and store dd::Tablespace
could fail, so it's better to do it earlier, to prevent a late rollback
@param[in,out]	thd		MySQL connection
@param[in]	old_table	Old InnoDB table object
@param[in,out]	new_table	New InnoDB table object
@param[in]	old_dd_tab	Old dd::Table or dd::Partition
@param[in,out]	new_dd_tab	New dd::Table or dd::Partition
@return	false	On success
@retval	true	On failure */
template <typename Table>
static MY_ATTRIBUTE((warn_unused_result)) bool dd_prepare_inplace_alter_table(
    THD *thd, const dict_table_t *old_table, dict_table_t *new_table,
    const Table *old_dd_tab, Table *new_dd_tab) {
  if (new_table->is_temporary() || old_table == new_table) {
    /* No need to fill in metadata for temporary tables,
    which would not be stored in Global DD */
    return (false);
  }

  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  if (dict_table_is_file_per_table(old_table)) {
    dd::Object_id old_space_id = dd_first_index(old_dd_tab)->tablespace_id();

    if (dd_drop_tablespace(client, thd, old_space_id)) {
      return (true);
    }
  }

  if (dict_table_is_file_per_table(new_table)) {
    /* Replace the table name with the final correct one */
    char *path = fil_space_get_first_path(new_table->space);
    char filename[FN_REFLEN + 1];
    replace_table_name(path, filename, old_table->name.m_name);
    ut_free(path);

    bool discarded = false;
    const dd::Properties &p = old_dd_tab->se_private_data();
    if (dict_table_is_file_per_table(old_table) &&
        p.exists(dd_table_key_strings[DD_TABLE_DISCARD])) {
      p.get_bool(dd_table_key_strings[DD_TABLE_DISCARD], &discarded);
    }

    dd::Object_id dd_space_id;

    if (dd_create_implicit_tablespace(client, thd, new_table->space,
                                      old_table->name.m_name, filename,
                                      discarded, dd_space_id)) {
      my_error(ER_INTERNAL_ERROR, MYF(0),
               " InnoDB can't create tablespace object"
               " for ",
               new_table->name);
      return (true);
    }

    new_table->dd_space_id = dd_space_id;
  }

  return (false);
}

/** Update table level instant metadata in commit phase
@param[in]	table		InnoDB table object
@param[in]	old_dd_tab	old dd::Table
@param[in]	new_dd_tab	new dd::Table */
static void dd_commit_inplace_update_instant_meta(const dict_table_t *table,
                                                  const dd::Table *old_dd_tab,
                                                  dd::Table *new_dd_tab) {
  if (!dd_table_has_instant_cols(*old_dd_tab)) {
    return;
  }

  ut_ad(table->has_instant_cols());

  new_dd_tab->se_private_data().set_uint32(
      dd_table_key_strings[DD_TABLE_INSTANT_COLS], table->get_instant_cols());

  for (uint16_t i = 0; i < table->get_n_user_cols(); ++i) {
    const dict_col_t *col = table->get_col(i);

    if (col->instant_default == nullptr) {
      continue;
    }

    dd::Column *dd_col = const_cast<dd::Column *>(
        dd_find_column(new_dd_tab, table->get_col_name(i)));
    ut_ad(dd_col != nullptr);

    dd_write_default_value(col, dd_col);
  }
}

/** Update instant metadata in commit phase for partitioned table
@param[in]	part_share	partition share object to get each
partitioned table
@param[in]	n_parts		number of partitions
@param[in]	old_dd_tab	old dd::Table
@param[in]	new_dd_tab	new dd::Table */
static void dd_commit_inplace_update_partition_instant_meta(
    const Ha_innopart_share *part_share, uint16_t n_parts,
    const dd::Table *old_dd_tab, dd::Table *new_dd_tab) {
  if (!dd_table_has_instant_cols(*old_dd_tab)) {
    return;
  }

  const dict_table_t *table = part_share->get_table_part(0);

  for (uint16_t i = 1; i < n_parts; ++i) {
    if (part_share->get_table_part(i)->get_instant_cols() <
        table->get_instant_cols()) {
      table = part_share->get_table_part(i);
    }
  }

  ut_ad(table->has_instant_cols());

  dd_commit_inplace_update_instant_meta(table, old_dd_tab, new_dd_tab);

  uint16_t i = 0;
  for (auto part : *new_dd_tab->leaf_partitions()) {
    if (part_share->get_table_part(i)->has_instant_cols()) {
      part->se_private_data().set_uint32(
          dd_partition_key_strings[DD_PARTITION_INSTANT_COLS],
          part_share->get_table_part(i)->get_instant_cols());
    }

    ++i;
  }
}

/** Update metadata in commit phase. Note this function should only update
the metadata which would not result in failure
@param[in]	old_info	Some table information for the old table
@param[in,out]	new_table	New InnoDB table object
@param[in]	old_dd_tab	Old dd::Table or dd::Partition
@param[in,out]	new_dd_tab	New dd::Table or dd::Partition */
template <typename Table>
static void dd_commit_inplace_alter_table(
    const alter_table_old_info_t &old_info, dict_table_t *new_table,
    const Table *old_dd_tab, Table *new_dd_tab) {
  if (new_table->is_temporary()) {
    /* No need to fill in metadata for temporary tables,
    which would not be stored in Global DD */
    return;
  }

  dd::Object_id dd_space_id;

  if (old_info.m_rebuild) {
    ut_ad(!new_table->has_instant_cols());

    if (dict_table_is_file_per_table(new_table)) {
      /* Get the one created in prepare phase */
      dd_space_id = new_table->dd_space_id;
    } else if (new_table->space == TRX_SYS_SPACE) {
      dd_space_id = dict_sys_t::s_dd_sys_space_id;
    } else {
      /* Currently, even if specifying a new TABLESPACE
      for partitioned table, existing partitions would not
      be moved to new tablespaces. Thus, the old
      tablespace id should still be used for new partition */
      if (dd_table_is_partitioned(new_dd_tab->table())) {
        dd_space_id = dd_first_index(old_dd_tab)->tablespace_id();
      } else {
        dd_space_id = dd_get_space_id(new_dd_tab->table());
      }
      ut_ad(dd_space_id != dd::INVALID_OBJECT_ID);
    }
  } else {
    if (old_info.m_fts_doc_id &&
        !dd_find_column(&new_dd_tab->table(), FTS_DOC_ID_COL_NAME)) {
      dd::Column *col =
          dd_add_hidden_column(&new_dd_tab->table(), FTS_DOC_ID_COL_NAME,
                               FTS_DOC_ID_LEN, dd::enum_column_types::LONGLONG);

      dd_set_hidden_unique_index(new_dd_tab->table().add_index(),
                                 FTS_DOC_ID_INDEX_NAME, col);
    }

    dd_space_id = dd_first_index(old_dd_tab)->tablespace_id();
  }

  dd_set_table_options(new_dd_tab, new_table);

  new_table->dd_space_id = dd_space_id;

  dd_write_table(dd_space_id, new_dd_tab, new_table);

  /* For discarded table, need set this to dd. */
  if (old_info.m_discarded) {
    dd::Properties &p = new_dd_tab->se_private_data();
    p.set_bool(dd_table_key_strings[DD_TABLE_DISCARD], true);
  }
}

/** Update metadata in commit phase for instant ADD COLUMN.
Basically, it should remember number of instant columns,
and the default value of newly added columns.
Note this function should only update the metadata
which would not result in failure
@param[in]	new_table	New InnoDB table object
@param[in]	old_table	MySQL table as it is before the ALTER operation
@param[in]	altered_table	MySQL table that is being altered
@param[in]	old_dd_tab	Old dd::Table
@param[in,out]	new_dd_tab	New dd::Table */
static void dd_commit_instant_table(const dict_table_t *new_table,
                                    const TABLE *old_table,
                                    const TABLE *altered_table,
                                    const dd::Table *old_dd_tab,
                                    dd::Table *new_dd_tab) {
  ut_ad(!new_table->is_temporary());
  ut_ad(old_dd_tab->columns().size() <= new_dd_tab->columns()->size());

  if (!new_dd_tab->se_private_data().exists(
          dd_table_key_strings[DD_TABLE_INSTANT_COLS])) {
    uint32_t instant_cols = new_table->get_n_user_cols();

    if (dd_table_has_instant_cols(*old_dd_tab)) {
      old_dd_tab->se_private_data().get_uint32(
          dd_table_key_strings[DD_TABLE_INSTANT_COLS], &instant_cols);
    }

    new_dd_tab->se_private_data().set_uint32(
        dd_table_key_strings[DD_TABLE_INSTANT_COLS], instant_cols);
  }

  /* To remember old default values if exist */
  dd_copy_table_columns(*new_dd_tab, *old_dd_tab);

  /* Then add all new default values */
  dd_add_instant_columns(old_table, altered_table, new_dd_tab, new_table);

  /* Keep the metadata for newly added virtual columns if exist */
  dd_update_v_cols(new_dd_tab, new_table->id);

  ut_ad(dd_table_has_instant_cols(*new_dd_tab));
}

/** Update metadata in commit phase for instant ADD COLUMN.
Basically, it should remember the number of instant columns
for the specified partitioned table.
@param[in]	new_table	New InnoDB table object
@param[in,out]	new_part	New dd::Partition */
static void dd_commit_instant_part(const dict_table_t *new_table,
                                   dd::Partition *new_part) {
  if (!new_part->se_private_data().exists(
          dd_partition_key_strings[DD_PARTITION_INSTANT_COLS])) {
    new_part->se_private_data().set_uint32(
        dd_partition_key_strings[DD_PARTITION_INSTANT_COLS],
        new_table->get_n_user_cols());
  }
#ifdef UNIV_DEBUG
  uint32_t part_instant;
  uint32_t table_instant;
  bool fail;
  fail = new_part->se_private_data().get_uint32(
      dd_partition_key_strings[DD_PARTITION_INSTANT_COLS], &part_instant);
  ut_ad(!fail);
  ut_ad(part_instant <= new_table->get_n_user_cols());
  fail = new_part->table().se_private_data().get_uint32(
      dd_table_key_strings[DD_TABLE_INSTANT_COLS], &table_instant);
  ut_ad(!fail);
  ut_ad(table_instant <= part_instant);
#endif /* UNIV_DEBUG */
}

template <typename Table>
static void dd_commit_inplace_no_change(const Table *old_dd_tab,
                                        Table *new_dd_tab, bool ignore_fts) {
  if (!ignore_fts) {
    dd_add_fts_doc_id_index(new_dd_tab->table(), old_dd_tab->table());
  }

  dd_copy_private(*new_dd_tab, *old_dd_tab);

  if (!dd_table_is_partitioned(new_dd_tab->table()) ||
      dd_part_is_first(reinterpret_cast<dd::Partition *>(new_dd_tab))) {
    dd_copy_table(new_dd_tab->table(), old_dd_tab->table());
  }
}

template <typename Table>
static void dd_commit_inplace_instant(Alter_inplace_info *ha_alter_info,
                                      THD *thd, trx_t *trx, dict_table_t *table,
                                      const TABLE *old_table,
                                      const TABLE *altered_table,
                                      const Table *old_dd_tab,
                                      Table *new_dd_tab, uint64_t *autoinc) {
  ut_ad(is_instant(ha_alter_info));

  Instant_Type type =
      static_cast<Instant_Type>(ha_alter_info->handler_trivial_ctx);

  switch (type) {
    case Instant_Type::INSTANT_NO_CHANGE:
      dd_commit_inplace_no_change(old_dd_tab, new_dd_tab, false);
      break;
    case Instant_Type::INSTANT_VIRTUAL_ONLY:
      dd_commit_inplace_no_change(old_dd_tab, new_dd_tab, true);

      if (!dd_table_is_partitioned(new_dd_tab->table()) ||
          dd_part_is_first(reinterpret_cast<dd::Partition *>(new_dd_tab))) {
        dd_update_v_cols(&new_dd_tab->table(), table->id);
      }

      row_mysql_lock_data_dictionary(trx);
      innobase_discard_table(thd, table);
      row_mysql_unlock_data_dictionary(trx);
      break;
    case Instant_Type::INSTANT_ADD_COLUMN:
      dd_copy_private(*new_dd_tab, *old_dd_tab);

      if (!dd_table_is_partitioned(new_dd_tab->table()) ||
          dd_part_is_first(reinterpret_cast<dd::Partition *>(new_dd_tab))) {
        dd_commit_instant_table(table, old_table, altered_table,
                                &old_dd_tab->table(), &new_dd_tab->table());
      }

      if (dd_table_is_partitioned(new_dd_tab->table())) {
        dd_commit_instant_part(table,
                               reinterpret_cast<dd::Partition *>(new_dd_tab));
      }

      row_mysql_lock_data_dictionary(trx);
      innobase_discard_table(thd, table);
      row_mysql_unlock_data_dictionary(trx);
      break;
    case Instant_Type::INSTANT_IMPOSSIBLE:
    default:
      ut_ad(0);
  }

  if (autoinc != nullptr) {
    ut_ad(altered_table->found_next_number_field != nullptr);
    if (!dd_table_is_partitioned(new_dd_tab->table()) ||
        dd_part_is_first(reinterpret_cast<dd::Partition *>(new_dd_tab))) {
      dd_set_autoinc(new_dd_tab->table().se_private_data(), *autoinc);
    }
  }
}

/** Check if a new table's index will exceed the index limit for the table
row format
@param[in]	form		MySQL table that is being altered
@param[in]	max_len		max index length allowed
@return	true if within limits false otherwise */
static bool innobase_check_index_len(const TABLE *form, ulint max_len) {
  for (uint key_num = 0; key_num < form->s->keys; key_num++) {
    const KEY &key = form->key_info[key_num];

    for (unsigned i = 0; i < key.user_defined_key_parts; i++) {
      const KEY_PART_INFO *key_part = &key.key_part[i];
      unsigned prefix_len = 0;

      if (key.flags & HA_SPATIAL) {
        prefix_len = 0;
      } else if (key.flags & HA_FULLTEXT) {
        prefix_len = 0;
      } else if (key_part->key_part_flag & HA_PART_KEY_SEG) {
        /* SPATIAL and FULLTEXT index always are on
        full columns. */
        ut_ad(!(key.flags & (HA_SPATIAL | HA_FULLTEXT)));
        prefix_len = key_part->length;
        ut_ad(prefix_len > 0);
      } else {
        prefix_len = 0;
      }

      if (key_part->length > max_len || prefix_len > max_len) {
        return (false);
      }
    }
  }
  return (true);
}

/** Update internal structures with concurrent writes blocked,
while preparing ALTER TABLE.

@param ha_alter_info Data used during in-place alter
@param altered_table MySQL table that is being altered
@param old_table MySQL table as it is before the ALTER operation
@param old_dd_tab old dd table
@param new_dd_tab new dd table
@param table_name Table name in MySQL
@param flags Table and tablespace flags
@param flags2 Additional table flags
@param fts_doc_id_col The column number of FTS_DOC_ID
@param add_fts_doc_id Flag: add column FTS_DOC_ID?
@param add_fts_doc_id_idx Flag: add index FTS_DOC_ID_INDEX (FTS_DOC_ID)?

@retval true Failure
@retval false Success */
template <typename Table>
static MY_ATTRIBUTE((warn_unused_result)) bool prepare_inplace_alter_table_dict(
    Alter_inplace_info *ha_alter_info, const TABLE *altered_table,
    const TABLE *old_table, const Table *old_dd_tab, Table *new_dd_tab,
    const char *table_name, ulint flags, ulint flags2, ulint fts_doc_id_col,
    bool add_fts_doc_id, bool add_fts_doc_id_idx) {
  bool dict_locked = false;
  ulint *add_key_nums;     /* MySQL key numbers */
  index_def_t *index_defs; /* index definitions */
  dict_table_t *user_table;
  dict_index_t *fts_index = NULL;
  ulint new_clustered = 0;
  dberr_t error;
  const char *punch_hole_warning = NULL;
  ulint num_fts_index;
  dict_add_v_col_t *add_v = NULL;
  dict_table_t *table;
  MDL_ticket *mdl = nullptr;
  THD *thd = current_thd;
  bool build_fts_common = false;

  ha_innobase_inplace_ctx *ctx;

  DBUG_ENTER("prepare_inplace_alter_table_dict");

  ctx = static_cast<ha_innobase_inplace_ctx *>(ha_alter_info->handler_ctx);

  DBUG_ASSERT((ctx->add_autoinc != ULINT_UNDEFINED) ==
              (ctx->sequence.m_max_value > 0));
  DBUG_ASSERT(!ctx->num_to_drop_index == !ctx->drop_index);
  DBUG_ASSERT(!ctx->num_to_drop_fk == !ctx->drop_fk);
  DBUG_ASSERT(!add_fts_doc_id || add_fts_doc_id_idx);
  DBUG_ASSERT(!add_fts_doc_id_idx || innobase_fulltext_exist(altered_table));
  DBUG_ASSERT(!ctx->add_cols);
  DBUG_ASSERT(!ctx->add_index);
  DBUG_ASSERT(!ctx->add_key_numbers);
  DBUG_ASSERT(!ctx->num_to_add_index);

  user_table = ctx->new_table;

  trx_start_if_not_started_xa(ctx->prebuilt->trx, true);

  if (ha_alter_info->handler_flags & Alter_inplace_info::DROP_VIRTUAL_COLUMN) {
    if (prepare_inplace_drop_virtual(ha_alter_info, altered_table, old_table)) {
      DBUG_RETURN(true);
    }
  }

  if (ha_alter_info->handler_flags & Alter_inplace_info::ADD_VIRTUAL_COLUMN) {
    if (prepare_inplace_add_virtual(ha_alter_info, altered_table, old_table)) {
      DBUG_RETURN(true);
    }

    /* Need information for newly added virtual columns
    for create index */
    if (ha_alter_info->handler_flags & Alter_inplace_info::ADD_INDEX) {
      for (ulint i = 0; i < ctx->num_to_add_vcol; i++) {
        /* Set mbminmax for newly added column */
        ulint i_mbminlen, i_mbmaxlen;
        dtype_get_mblen(ctx->add_vcol[i].m_col.mtype,
                        ctx->add_vcol[i].m_col.prtype, &i_mbminlen,
                        &i_mbmaxlen);

        ctx->add_vcol[i].m_col.set_mbminmaxlen(i_mbminlen, i_mbmaxlen);
      }
      add_v = static_cast<dict_add_v_col_t *>(
          mem_heap_alloc(ctx->heap, sizeof *add_v));
      add_v->n_v_col = ctx->num_to_add_vcol;
      add_v->v_col = ctx->add_vcol;
      add_v->v_col_name = ctx->add_vcol_name;
    }
  }

  /* There should be no order change for virtual columns coming in
  here */
  ut_ad(check_v_col_in_order(old_table, altered_table, ha_alter_info));

  ctx->trx = ctx->prebuilt->trx;

  /* Create table containing all indexes to be built in this
  ALTER TABLE ADD INDEX so that they are in the correct order
  in the table. */

  ctx->num_to_add_index = ha_alter_info->index_add_count;

  ut_ad(ctx->prebuilt->trx->mysql_thd != NULL);
  const char *path = thd_innodb_tmpdir(ctx->prebuilt->trx->mysql_thd);

  index_defs = innobase_create_key_defs(
      ctx->heap, ha_alter_info, altered_table, new_dd_tab,
      ctx->num_to_add_index, num_fts_index,
      row_table_got_default_clust_index(ctx->new_table), fts_doc_id_col,
      add_fts_doc_id, add_fts_doc_id_idx);

  new_clustered = DICT_CLUSTERED & index_defs[0].ind_type;

  if (num_fts_index > 1) {
    my_error(ER_INNODB_FT_LIMIT, MYF(0));
    goto error_handled;
  }

  if (new_clustered) {
    /* If max index length is reduced due to row format change
    make sure the index can all be accomodated in new row format */
    ulint max_len = DICT_MAX_FIELD_LEN_BY_FORMAT_FLAG(flags);

    if (max_len < DICT_MAX_FIELD_LEN_BY_FORMAT(ctx->old_table)) {
      if (!innobase_check_index_len(altered_table, max_len)) {
        my_error(ER_INDEX_COLUMN_TOO_LONG, MYF(0), max_len);
        goto error_handled;
      }
    }
  }

  if (!ctx->online) {
    /* This is not an online operation (LOCK=NONE). */
  } else if (ctx->add_autoinc == ULINT_UNDEFINED && num_fts_index == 0 &&
             (!innobase_need_rebuild(ha_alter_info) ||
              !innobase_fulltext_exist(altered_table))) {
    /* InnoDB can perform an online operation (LOCK=NONE). */
  } else {
    /* This should have been blocked in
    check_if_supported_inplace_alter(). */
    ut_ad(0);
    my_error(ER_NOT_SUPPORTED_YET, MYF(0),
             thd_query_unsafe(ctx->prebuilt->trx->mysql_thd).str);
    goto error_handled;
  }

  /* The primary index would be rebuilt if a FTS Doc ID
  column is to be added, and the primary index definition
  is just copied from old table and stored in indexdefs[0] */
  DBUG_ASSERT(!add_fts_doc_id || new_clustered);
  DBUG_ASSERT(!!new_clustered ==
              (innobase_need_rebuild(ha_alter_info) || add_fts_doc_id));

  /* Allocate memory for dictionary index definitions */

  ctx->add_index = static_cast<dict_index_t **>(mem_heap_alloc(
      ctx->heap, ctx->num_to_add_index * sizeof *ctx->add_index));
  ctx->add_key_numbers = add_key_nums = static_cast<ulint *>(mem_heap_alloc(
      ctx->heap, ctx->num_to_add_index * sizeof *ctx->add_key_numbers));

  /* Acquire a lock on the table before creating any indexes. */
  if (ctx->online) {
    error = DB_SUCCESS;
  } else {
    error = row_merge_lock_table(ctx->prebuilt->trx, ctx->new_table, LOCK_S);

    if (error != DB_SUCCESS) {
      goto error_handling;
    }
  }

  /* Latch the InnoDB data dictionary exclusively so that no deadlocks
  or lock waits can happen in it during an index create operation. */

  row_mysql_lock_data_dictionary(ctx->prebuilt->trx);
  ut_ad(ctx->trx == ctx->prebuilt->trx);
  dict_locked = true;

  /* Wait for background stats processing to stop using the table that
  we are going to alter. We know bg stats will not start using it again
  until we are holding the data dict locked and we are holding it here
  at least until checking ut_ad(user_table->n_ref_count == 1) below.
  XXX what may happen if bg stats opens the table after we
  have unlocked data dictionary below? */
  dict_stats_wait_bg_to_stop_using_table(user_table, ctx->trx);

  online_retry_drop_dict_indexes(ctx->new_table, true);

  ut_d(dict_table_check_for_dup_indexes(ctx->new_table, CHECK_ABORTED_OK));

  /* If a new clustered index is defined for the table we need
  to rebuild the table with a temporary name. */

  if (new_clustered) {
    const char *new_table_name = dict_mem_create_temporary_tablename(
        ctx->heap, ctx->new_table->name.m_name, ctx->new_table->id);
    ulint n_cols = 0;
    ulint n_v_cols = 0;
    dtuple_t *add_cols;
    space_id_t space_id = 0;
    ulint z = 0;

    if (innobase_check_foreigns(ha_alter_info, altered_table, old_table,
                                user_table, ctx->drop_fk,
                                ctx->num_to_drop_fk)) {
      goto new_clustered_failed;
    }

    for (uint i = 0; i < altered_table->s->fields; i++) {
      const Field *field = altered_table->field[i];

      if (innobase_is_v_fld(field)) {
        n_v_cols++;
      } else {
        n_cols++;
      }
    }

    ut_ad(n_cols + n_v_cols == altered_table->s->fields);

    if (add_fts_doc_id) {
      n_cols++;
      DBUG_ASSERT(flags2 & DICT_TF2_FTS);
      DBUG_ASSERT(add_fts_doc_id_idx);
      flags2 |=
          DICT_TF2_FTS_ADD_DOC_ID | DICT_TF2_FTS_HAS_DOC_ID | DICT_TF2_FTS;
    }

    DBUG_ASSERT(!add_fts_doc_id_idx || (flags2 & DICT_TF2_FTS));

    /* Create the table. */
    table = dd_table_open_on_name(thd, &mdl, new_table_name, true,
                                  DICT_ERR_IGNORE_NONE);

    if (table) {
      my_error(ER_TABLE_EXISTS_ERROR, MYF(0), new_table_name);
      dd_table_close(table, thd, &mdl, true);
      goto new_clustered_failed;
    }

    /* Use the old tablespace unless the tablespace
    is changing. */
    if (DICT_TF_HAS_SHARED_SPACE(user_table->flags) &&
        (ha_alter_info->create_info->tablespace == NULL ||
         (0 == strcmp(ha_alter_info->create_info->tablespace,
                      user_table->tablespace)))) {
      space_id = user_table->space;
    } else if (tablespace_is_shared_space(ha_alter_info->create_info)) {
      space_id =
          fil_space_get_id_by_name(ha_alter_info->create_info->tablespace);
      ut_a(space_id != SPACE_UNKNOWN);
    }

    /* The initial space id 0 may be overridden later if this
    table is going to be a file_per_table tablespace. */
    ctx->new_table = dict_mem_table_create(
        new_table_name, space_id, n_cols + n_v_cols, n_v_cols, flags, flags2);

    /* TODO: Fix this problematic assignment */
    ctx->new_table->dd_space_id = new_dd_tab->tablespace_id();

    /* The rebuilt indexed_table will use the renamed
    column names. */
    ctx->col_names = NULL;

    if (DICT_TF_HAS_DATA_DIR(flags)) {
      ctx->new_table->data_dir_path =
          mem_heap_strdup(ctx->new_table->heap, user_table->data_dir_path);
    }

    for (uint i = 0; i < altered_table->s->fields; i++) {
      const Field *field = altered_table->field[i];
      ulint is_unsigned;
      ulint field_type = (ulint)field->type();
      ulint col_type = get_innobase_type_from_mysql_type(&is_unsigned, field);
      ulint charset_no;
      ulint col_len;
      bool is_virtual = innobase_is_v_fld(field);

      /* we assume in dtype_form_prtype() that this
      fits in two bytes */
      ut_a(field_type <= MAX_CHAR_COLL_NUM);

      if (!field->real_maybe_null()) {
        field_type |= DATA_NOT_NULL;
      }

      if (field->binary()) {
        field_type |= DATA_BINARY_TYPE;
      }

      if (is_unsigned) {
        field_type |= DATA_UNSIGNED;
      }

      if (dtype_is_string_type(col_type)) {
        charset_no = (ulint)field->charset()->number;

        if (charset_no > MAX_CHAR_COLL_NUM) {
          dict_mem_table_free(ctx->new_table);
          my_error(ER_WRONG_KEY_COLUMN, MYF(0), field->field_name);
          goto new_clustered_failed;
        }
      } else {
        charset_no = 0;
      }

      col_len = field->pack_length();

      /* The MySQL pack length contains 1 or 2 bytes
      length field for a true VARCHAR. Let us
      subtract that, so that the InnoDB column
      length in the InnoDB data dictionary is the
      real maximum byte length of the actual data. */

      if (field->type() == MYSQL_TYPE_VARCHAR) {
        uint32 length_bytes =
            static_cast<const Field_varstring *>(field)->length_bytes;

        col_len -= length_bytes;

        if (length_bytes == 2) {
          field_type |= DATA_LONG_TRUE_VARCHAR;
        }
      }

      if (col_type == DATA_POINT) {
        /* DATA_POINT should be of fixed length,
        instead of the pack_length(blob length). */
        col_len = DATA_POINT_LEN;
      }

      if (dict_col_name_is_reserved(field->field_name)) {
        dict_mem_table_free(ctx->new_table);
        my_error(ER_WRONG_COLUMN_NAME, MYF(0), field->field_name);
        goto new_clustered_failed;
      }

      if (is_virtual) {
        dict_mem_table_add_v_col(
            ctx->new_table, ctx->heap, field->field_name, col_type,
            dtype_form_prtype(field_type, charset_no) | DATA_VIRTUAL, col_len,
            i, field->gcol_info->non_virtual_base_columns());
      } else {
        dict_mem_table_add_col(
            ctx->new_table, ctx->heap, field->field_name, col_type,
            dtype_form_prtype(field_type, charset_no), col_len);
      }
    }

    if (n_v_cols) {
      for (uint i = 0; i < altered_table->s->fields; i++) {
        dict_v_col_t *v_col;
        const Field *field = altered_table->field[i];

        if (!innobase_is_v_fld(field)) {
          continue;
        }
        v_col = dict_table_get_nth_v_col(ctx->new_table, z);
        z++;
        innodb_base_col_setup(ctx->new_table, field, v_col);
      }
    }

    if (add_fts_doc_id) {
      fts_add_doc_id_column(ctx->new_table, ctx->heap);
      ctx->new_table->fts->doc_col = fts_doc_id_col;
      ut_ad(fts_doc_id_col == altered_table->s->fields - n_v_cols);
    } else if (ctx->new_table->fts) {
      ctx->new_table->fts->doc_col = fts_doc_id_col;
    }

    const char *compression;

    compression = ha_alter_info->create_info->compress.str;

    if (Compression::validate(compression) != DB_SUCCESS) {
      compression = NULL;
    }

    const char *encrypt;
    encrypt = ha_alter_info->create_info->encrypt_type.str;
    /* If encryption option is specified, then it must be
    innodb-file-per-table tablespace. Otherwise case would
    have already been blocked at
    create_option_tablespace_is_valid(). */
    if (encrypt) {
      ut_ad(flags2 & DICT_TF2_USE_FILE_PER_TABLE);
      ut_ad(!DICT_TF_HAS_SHARED_SPACE(flags));
    }

    if (!Encryption::is_none(encrypt)) {
      /* Check if keyring is ready. */
      if (!Encryption::check_keyring()) {
        dict_mem_table_free(ctx->new_table);
        my_error(ER_CANNOT_FIND_KEY_IN_KEYRING, MYF(0));
        goto new_clustered_failed;
      } else {
        /* This flag will be used to set encryption
        option for file-per-table tablespace. */
        DICT_TF2_FLAG_SET(ctx->new_table, DICT_TF2_ENCRYPTION_FILE_PER_TABLE);
      }
    }

    mutex_exit(&dict_sys->mutex);

    error = row_create_table_for_mysql(ctx->new_table, compression, ctx->trx);

    mutex_enter(&dict_sys->mutex);

    punch_hole_warning = (error == DB_IO_NO_PUNCH_HOLE_FS)
                             ? "Punch hole is not supported by the file system"
                             : "Page Compression is not supported for this"
                               " tablespace";

    switch (error) {
      dict_table_t *temp_table;
      case DB_IO_NO_PUNCH_HOLE_FS:
      case DB_IO_NO_PUNCH_HOLE_TABLESPACE:

        push_warning_printf(ctx->prebuilt->trx->mysql_thd,
                            Sql_condition::SL_WARNING, HA_ERR_UNSUPPORTED,
                            "%s. Compression disabled for '%s'",
                            punch_hole_warning, ctx->old_table->name.m_name);

        error = DB_SUCCESS;
        // Fall through.

      case DB_SUCCESS:
        /* To bump up the table ref count and move it
        to LRU list if it's not temporary table */
        ut_ad(mutex_own(&dict_sys->mutex));
        if (!ctx->new_table->is_temporary() &&
            !ctx->new_table->explicitly_non_lru) {
          dict_table_allow_eviction(ctx->new_table);
        }
        if ((ctx->new_table->flags2 &
             (DICT_TF2_FTS | DICT_TF2_FTS_ADD_DOC_ID)) ||
            ctx->new_table->fts != nullptr) {
          fts_freeze_aux_tables(ctx->new_table);
        }
        temp_table =
            dd_table_open_on_name_in_mem(ctx->new_table->name.m_name, true);
        ut_a(ctx->new_table == temp_table);
        /* n_ref_count must be 1, because purge cannot
        be executing on this very table as we are
        holding MDL lock. */
        DBUG_ASSERT(ctx->new_table->get_ref_count() == 1);
        break;
      case DB_TABLESPACE_EXISTS:
        my_error(ER_TABLESPACE_EXISTS, MYF(0), new_table_name);
        goto new_clustered_failed;
      case DB_DUPLICATE_KEY:
        my_error(HA_ERR_TABLE_EXIST, MYF(0), altered_table->s->table_name.str);
        goto new_clustered_failed;
      case DB_UNSUPPORTED:
        my_error(ER_UNSUPPORTED_EXTENSION, MYF(0), ctx->new_table->name.m_name);
        goto new_clustered_failed;
      default:
        my_error_innodb(error, table_name, flags);
      new_clustered_failed:
        ut_ad(user_table->get_ref_count() == 1);

        goto err_exit;
    }

    if (ha_alter_info->handler_flags & Alter_inplace_info::ADD_COLUMN) {
      add_cols =
          dtuple_create_with_vcol(ctx->heap, ctx->new_table->get_n_cols(),
                                  dict_table_get_n_v_cols(ctx->new_table));

      dict_table_copy_types(add_cols, ctx->new_table);
    } else {
      add_cols = NULL;
    }

    ctx->col_map =
        innobase_build_col_map(ha_alter_info, altered_table, old_table,
                               ctx->new_table, user_table, add_cols, ctx->heap);
    ctx->add_cols = add_cols;
  } else {
    DBUG_ASSERT(!innobase_need_rebuild(ha_alter_info));
    DBUG_ASSERT(old_table->s->primary_key == altered_table->s->primary_key);

    for (dict_index_t *index = user_table->first_index(); index != NULL;
         index = index->next()) {
      if (!index->to_be_dropped && index->is_corrupted()) {
        my_error(ER_CHECK_NO_SUCH_TABLE, MYF(0));
        goto error_handled;
      }
    }

    if (!ctx->new_table->fts && innobase_fulltext_exist(altered_table)) {
      ctx->new_table->fts = fts_create(ctx->new_table);
      ctx->new_table->fts->doc_col = fts_doc_id_col;
    }

    /* Check if we need to update mtypes of legacy GIS columns.
    This check is only needed when we don't have to rebuild
    the table, since rebuild would update all mtypes for GIS
    columns */
    error = innobase_check_gis_columns(ha_alter_info, ctx->new_table, ctx->trx);
    if (error != DB_SUCCESS) {
      ut_ad(error == DB_ERROR);
      error = DB_UNSUPPORTED;
      goto error_handling;
    }
  }

  ut_ad(!dict_table_is_compressed_temporary(ctx->new_table));

  /* Assign table_id, so that no table id of
  fts_create_index_tables() will be written to the undo logs. */
  DBUG_ASSERT(ctx->new_table->id != 0);

  /* Create the indexes and load into dictionary. */

  for (ulint a = 0; a < ctx->num_to_add_index; a++) {
    if (index_defs[a].ind_type & DICT_VIRTUAL && ctx->num_to_drop_vcol > 0 &&
        !new_clustered) {
      innodb_v_adjust_idx_col(ha_alter_info, old_table, ctx->num_to_drop_vcol,
                              &index_defs[a]);
    }

    ctx->add_index[a] =
        row_merge_create_index(ctx->trx, ctx->new_table, &index_defs[a], add_v);

    add_key_nums[a] = index_defs[a].key_number;

    if (!ctx->add_index[a]) {
      error = ctx->trx->error_state;
      DBUG_ASSERT(error != DB_SUCCESS);
      goto error_handling;
    }

    DBUG_ASSERT(ctx->add_index[a]->is_committed() == !!new_clustered);

    if (ctx->add_index[a]->type & DICT_FTS) {
      DBUG_ASSERT(num_fts_index);
      DBUG_ASSERT(!fts_index);
      DBUG_ASSERT(ctx->add_index[a]->type == DICT_FTS);
      fts_index = ctx->add_index[a];
    }

    /* If only online ALTER TABLE operations have been
    requested, allocate a modification log. If the table
    will be locked anyway, the modification
    log is unnecessary. When rebuilding the table
    (new_clustered), we will allocate the log for the
    clustered index of the old table, later. */
    if (new_clustered || !ctx->online || user_table->ibd_file_missing ||
        dict_table_is_discarded(user_table)) {
      /* No need to allocate a modification log. */
      ut_ad(!ctx->add_index[a]->online_log);
    } else if (ctx->add_index[a]->type & DICT_FTS) {
      /* Fulltext indexes are not covered
      by a modification log. */
    } else {
      DBUG_EXECUTE_IF("innodb_OOM_prepare_inplace_alter",
                      error = DB_OUT_OF_MEMORY;
                      goto error_handling;);
      rw_lock_x_lock(&ctx->add_index[a]->lock);
      bool ok =
          row_log_allocate(ctx->add_index[a], NULL, true, NULL, NULL, path);
      rw_lock_x_unlock(&ctx->add_index[a]->lock);

      if (!ok) {
        error = DB_OUT_OF_MEMORY;
        goto error_handling;
      }
    }
  }

  ut_ad(new_clustered == ctx->need_rebuild());

  DBUG_EXECUTE_IF("innodb_OOM_prepare_inplace_alter", error = DB_OUT_OF_MEMORY;
                  goto error_handling;);

  if (new_clustered) {
    dict_index_t *clust_index = user_table->first_index();
    dict_index_t *new_clust_index = ctx->new_table->first_index();
    ctx->skip_pk_sort =
        innobase_pk_order_preserved(ctx->col_map, clust_index, new_clust_index);

    DBUG_EXECUTE_IF("innodb_alter_table_pk_assert_no_sort",
                    DBUG_ASSERT(ctx->skip_pk_sort););

    if (ctx->online) {
      /* Allocate a log for online table rebuild. */
      rw_lock_x_lock(&clust_index->lock);
      bool ok = row_log_allocate(
          clust_index, ctx->new_table,
          !(ha_alter_info->handler_flags & Alter_inplace_info::ADD_PK_INDEX),
          ctx->add_cols, ctx->col_map, path);
      rw_lock_x_unlock(&clust_index->lock);

      if (!ok) {
        error = DB_OUT_OF_MEMORY;
        goto error_handling;
      }
    }
  }

  if (ctx->online) {
    /* Assign a consistent read view for
    row_merge_read_clustered_index(). */
    trx_assign_read_view(ctx->prebuilt->trx);
  }

  if (fts_index) {
  /* Ensure that the dictionary operation mode will
  not change while creating the auxiliary tables. */
#ifdef UNIV_DEBUG
    trx_dict_op_t op = trx_get_dict_operation(ctx->trx);
#endif
    ut_ad(ctx->trx->dict_operation_lock_mode == RW_X_LATCH);
    ut_ad(mutex_own(&dict_sys->mutex));
    ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));

    DICT_TF2_FLAG_SET(ctx->new_table, DICT_TF2_FTS);
    if (new_clustered) {
      /* For !new_clustered, this will be set at
      commit_cache_norebuild(). */
      ctx->new_table->fts_doc_id_index =
          dict_table_get_index_on_name(ctx->new_table, FTS_DOC_ID_INDEX_NAME);
      DBUG_ASSERT(ctx->new_table->fts_doc_id_index != NULL);
    }

    /* This function will commit the transaction and reset
    the trx_t::dict_operation flag on success. */

    mutex_exit(&dict_sys->mutex);
    error = fts_create_index_tables(ctx->trx, fts_index);
    mutex_enter(&dict_sys->mutex);

    DBUG_EXECUTE_IF("innodb_test_fail_after_fts_index_table",
                    error = DB_LOCK_WAIT_TIMEOUT;
                    goto error_handling;);

    if (error != DB_SUCCESS) {
      goto error_handling;
    }

    if (!ctx->new_table->fts ||
        ib_vector_size(ctx->new_table->fts->indexes) == 0) {
      bool exist_fts_common;

      mutex_exit(&dict_sys->mutex);
      exist_fts_common = fts_check_common_tables_exist(ctx->new_table);

      if (!exist_fts_common) {
        error = fts_create_common_tables(ctx->trx, ctx->new_table,
                                         user_table->name.m_name, TRUE);

        DBUG_EXECUTE_IF("innodb_test_fail_after_fts_common_table",
                        error = DB_LOCK_WAIT_TIMEOUT;);

        if (error != DB_SUCCESS) {
          mutex_enter(&dict_sys->mutex);
          goto error_handling;
        }

        build_fts_common = true;
      }

      error = innobase_fts_load_stopword(ctx->new_table, nullptr,
                                         ctx->prebuilt->trx->mysql_thd)
                  ? DB_SUCCESS
                  : DB_ERROR;

      mutex_enter(&dict_sys->mutex);

      if (error != DB_SUCCESS) {
        goto error_handling;
      }
    }

    ut_ad(trx_get_dict_operation(ctx->trx) == op);
  }

  DBUG_ASSERT(error == DB_SUCCESS);

  if (build_fts_common || fts_index) {
    fts_freeze_aux_tables(ctx->new_table);
  }

  row_mysql_unlock_data_dictionary(ctx->prebuilt->trx);
  ut_ad(ctx->trx == ctx->prebuilt->trx);
  dict_locked = false;

  if (dd_prepare_inplace_alter_table(ctx->prebuilt->trx->mysql_thd, user_table,
                                     ctx->new_table, old_dd_tab, new_dd_tab)) {
    error = DB_ERROR;
  }

  if (error == DB_SUCCESS) {
    if (build_fts_common) {
      if (!fts_create_common_dd_tables(ctx->new_table)) {
        error = DB_ERROR;
        goto error_handling;
      }
    }

    if (fts_index) {
      error = fts_create_index_dd_tables(ctx->new_table);
      if (error != DB_SUCCESS) {
        goto error_handling;
      }
    }
  }

error_handling:

  if (build_fts_common || fts_index) {
    fts_detach_aux_tables(ctx->new_table, dict_locked);
  }

  /* After an error, remove all those index definitions from the
  dictionary which were defined. */

  switch (error) {
    case DB_SUCCESS:
      ut_a(!dict_locked);

      ut_d(mutex_enter(&dict_sys->mutex));
      ut_d(dict_table_check_for_dup_indexes(user_table, CHECK_PARTIAL_OK));
      ut_d(mutex_exit(&dict_sys->mutex));
      DBUG_RETURN(false);
    case DB_TABLESPACE_EXISTS:
      my_error(ER_TABLESPACE_EXISTS, MYF(0), "(unknown)");
      break;
    case DB_DUPLICATE_KEY:
      my_error(ER_DUP_KEY, MYF(0));
      break;
    case DB_UNSUPPORTED:
      my_error(ER_TABLE_CANT_HANDLE_SPKEYS, MYF(0));
      break;
    default:
      my_error_innodb(error, table_name, user_table->flags);
  }

error_handled:

  ctx->prebuilt->trx->error_info = NULL;
  ctx->trx->error_state = DB_SUCCESS;

  if (!dict_locked) {
    row_mysql_lock_data_dictionary(ctx->prebuilt->trx);
    ut_ad(ctx->trx == ctx->prebuilt->trx);
  }

  if (new_clustered) {
    if (ctx->need_rebuild()) {
      if (DICT_TF2_FLAG_IS_SET(ctx->new_table, DICT_TF2_FTS)) {
        innobase_drop_fts_index_table(ctx->new_table, ctx->trx);
      }

      dict_table_close_and_drop(ctx->trx, ctx->new_table);

      /* Free the log for online table rebuild, if
      one was allocated. */

      dict_index_t *clust_index = user_table->first_index();

      rw_lock_x_lock(&clust_index->lock);

      if (clust_index->online_log) {
        ut_ad(ctx->online);
        row_log_free(clust_index->online_log);
        clust_index->online_status = ONLINE_INDEX_COMPLETE;
      }

      rw_lock_x_unlock(&clust_index->lock);
    }

    /* n_ref_count must be 1, because purge cannot
    be executing on this very table as we are
    holding MDL. */
    DBUG_ASSERT(user_table->get_ref_count() == 1 || ctx->online);
  } else {
    ut_ad(!ctx->need_rebuild());
    row_merge_drop_indexes(ctx->trx, user_table, TRUE);
  }

  ut_d(dict_table_check_for_dup_indexes(user_table, CHECK_ALL_COMPLETE));
  ut_ad(!user_table->drop_aborted);

err_exit:
#ifdef UNIV_DEBUG
  /* Clear the to_be_dropped flag in the data dictionary cache. */
  for (ulint i = 0; i < ctx->num_to_drop_index; i++) {
    DBUG_ASSERT(ctx->drop_index[i]->is_committed());
    DBUG_ASSERT(ctx->drop_index[i]->to_be_dropped);
    ctx->drop_index[i]->to_be_dropped = 0;
  }
#endif /* UNIV_DEBUG */

  row_mysql_unlock_data_dictionary(ctx->prebuilt->trx);
  ut_ad(ctx->trx == ctx->prebuilt->trx);

  destroy(ctx);
  ha_alter_info->handler_ctx = NULL;

  DBUG_RETURN(true);
}

/* Check whether an index is needed for the foreign key constraint.
If so, if it is dropped, is there an equivalent index can play its role.
@return true if the index is needed and can't be dropped */
static MY_ATTRIBUTE((warn_unused_result)) bool innobase_check_foreign_key_index(
    Alter_inplace_info *ha_alter_info, /*!< in: Structure describing
                                       changes to be done by ALTER
                                       TABLE */
    dict_index_t *index,               /*!< in: index to check */
    dict_table_t *indexed_table,       /*!< in: table that owns the
                                       foreign keys */
    const char **col_names,            /*!< in: column names, or NULL
                                       for indexed_table->col_names */
    trx_t *trx,                        /*!< in/out: transaction */
    dict_foreign_t **drop_fk,          /*!< in: Foreign key constraints
                                       to drop */
    ulint n_drop_fk)                   /*!< in: Number of foreign keys
                                       to drop */
{
  ut_ad(index != NULL);
  ut_ad(indexed_table != NULL);

  const dict_foreign_set *fks = &indexed_table->referenced_set;

  /* Check for all FK references from other tables to the index. */
  for (dict_foreign_set::const_iterator it = fks->begin(); it != fks->end();
       ++it) {
    dict_foreign_t *foreign = *it;
    if (foreign->referenced_index != index) {
      continue;
    }
    ut_ad(indexed_table == foreign->referenced_table);

    if (NULL == dict_foreign_find_index(indexed_table, col_names,
                                        foreign->referenced_col_names,
                                        foreign->n_fields, index,
                                        /*check_charsets=*/TRUE,
                                        /*check_null=*/FALSE) &&
        NULL == innobase_find_equiv_index(foreign->referenced_col_names,
                                          foreign->n_fields,
                                          ha_alter_info->key_info_buffer,
                                          ha_alter_info->index_add_buffer,
                                          ha_alter_info->index_add_count)) {
      /* Index cannot be dropped. */
      trx->error_info = index;
      return (true);
    }
  }

  fks = &indexed_table->foreign_set;

  /* Check for all FK references in current table using the index. */
  for (dict_foreign_set::const_iterator it = fks->begin(); it != fks->end();
       ++it) {
    dict_foreign_t *foreign = *it;
    if (foreign->foreign_index != index) {
      continue;
    }

    ut_ad(indexed_table == foreign->foreign_table);

    if (!innobase_dropping_foreign(foreign, drop_fk, n_drop_fk) &&
        NULL == dict_foreign_find_index(indexed_table, col_names,
                                        foreign->foreign_col_names,
                                        foreign->n_fields, index,
                                        /*check_charsets=*/TRUE,
                                        /*check_null=*/FALSE) &&
        NULL == innobase_find_equiv_index(foreign->foreign_col_names,
                                          foreign->n_fields,
                                          ha_alter_info->key_info_buffer,
                                          ha_alter_info->index_add_buffer,
                                          ha_alter_info->index_add_count)) {
      /* Index cannot be dropped. */
      trx->error_info = index;
      return (true);
    }
  }

  return (false);
}

/** Rename a given index in the InnoDB data dictionary cache.
@param[in,out] index index to rename
@param new_name new index name */
static void rename_index_in_cache(dict_index_t *index, const char *new_name) {
  DBUG_ENTER("rename_index_in_cache");

  ut_ad(mutex_own(&dict_sys->mutex));
  ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));

  size_t old_name_len = strlen(index->name);
  size_t new_name_len = strlen(new_name);

  if (old_name_len >= new_name_len) {
    /* reuse the old buffer for the name if it is large enough */
    memcpy(const_cast<char *>(index->name()), new_name, new_name_len + 1);
  } else {
    /* Free the old chunk of memory if it is at the topmost
    place in the heap, otherwise the old chunk will be freed
    when the index is evicted from the cache. This code will
    kick-in in a repeated ALTER sequences where the old name is
    alternately longer/shorter than the new name:
    1. ALTER TABLE t RENAME INDEX a TO aa;
    2. ALTER TABLE t RENAME INDEX aa TO a;
    3. go to 1. */
    index->name =
        mem_heap_strdup_replace(index->heap,
                                /* Presumed topmost element of the heap: */
                                index->name, old_name_len + 1, new_name);
  }

  DBUG_VOID_RETURN;
}

/**
Rename all indexes in data dictionary cache of a given table that are
specified in ha_alter_info.

@param ctx alter context, used to fetch the list of indexes to rename
@param ha_alter_info fetch the new names from here
*/
static void rename_indexes_in_cache(const ha_innobase_inplace_ctx *ctx,
                                    const Alter_inplace_info *ha_alter_info) {
  DBUG_ENTER("rename_indexes_in_cache");

  ut_ad(ctx->num_to_rename == ha_alter_info->index_rename_count);

  for (ulint i = 0; i < ctx->num_to_rename; i++) {
    KEY_PAIR *pair = &ha_alter_info->index_rename_buffer[i];
    dict_index_t *index;

    index = ctx->rename[i];

    ut_ad(strcmp(index->name, pair->old_key->name) == 0);

    rename_index_in_cache(index, pair->new_key->name);
  }

  DBUG_VOID_RETURN;
}

/** Fill the stored column information in s_cols list.
@param[in]	altered_table	mysql table object
@param[in]	table		innodb table object
@param[out]	s_cols		list of stored column
@param[out]	s_heap		heap for storing stored
column information. */
static void alter_fill_stored_column(const TABLE *altered_table,
                                     dict_table_t *table,
                                     dict_s_col_list **s_cols,
                                     mem_heap_t **s_heap) {
  ulint n_cols = altered_table->s->fields;
  ulint stored_col_no = 0;

  for (ulint i = 0; i < n_cols; i++) {
    Field *field = altered_table->field[i];
    dict_s_col_t s_col;

    if (!innobase_is_v_fld(field)) {
      stored_col_no++;
    }

    if (!innobase_is_s_fld(field)) {
      continue;
    }

    ulint num_base = field->gcol_info->non_virtual_base_columns();
    dict_col_t *col = table->get_col(stored_col_no);

    s_col.m_col = col;
    s_col.s_pos = i;

    if (*s_cols == NULL) {
      *s_cols = UT_NEW_NOKEY(dict_s_col_list());
      *s_heap = mem_heap_create(1000);
    }

    if (num_base != 0) {
      s_col.base_col = static_cast<dict_col_t **>(
          mem_heap_zalloc(*s_heap, num_base * sizeof(dict_col_t)));
    } else {
      s_col.base_col = NULL;
    }

    s_col.num_base = num_base;
    innodb_base_col_setup_for_stored(table, field, &s_col);

    (*s_cols)->push_back(s_col);
  }
}

/** Implementation of prepare_inplace_alter_table()
@tparam		Table		dd::Table or dd::Partition
@param[in]	altered_table	TABLE object for new version of table.
@param[in,out]	ha_alter_info	Structure describing changes to be done
                                by ALTER TABLE and holding data used
                                during in-place alter.
@param[in]	old_dd_tab	dd::Table object representing old
version of the table
@param[in,out]	new_dd_tab	dd::Table object representing new
version of the table
@retval	true Failure
@retval	false Success */
template <typename Table>
bool ha_innobase::prepare_inplace_alter_table_impl(
    TABLE *altered_table, Alter_inplace_info *ha_alter_info,
    const Table *old_dd_tab, Table *new_dd_tab) {
  dict_index_t **drop_index = NULL; /*!< Index to be dropped */
  ulint n_drop_index;               /*!< Number of indexes to drop */
  dict_index_t **rename_index;      /*!< Indexes to be dropped */
  ulint n_rename_index;             /*!< Number of indexes to rename */
  dict_foreign_t **drop_fk;         /*!< Foreign key constraints to drop */
  ulint n_drop_fk;                  /*!< Number of foreign keys to drop */
  dict_foreign_t **add_fk = NULL;   /*!< Foreign key constraints to drop */
  ulint n_add_fk;                   /*!< Number of foreign keys to drop */
  dict_table_t *indexed_table;      /*!< Table where indexes are created */
  mem_heap_t *heap;
  const char **col_names;
  int error;
  ulint max_col_len;
  ulint add_autoinc_col_no = ULINT_UNDEFINED;
  ulonglong autoinc_col_max_value = 0;
  ulint fts_doc_col_no = ULINT_UNDEFINED;
  bool add_fts_doc_id = false;
  bool add_fts_doc_id_idx = false;
  bool add_fts_idx = false;
  dict_s_col_list *s_cols = NULL;
  mem_heap_t *s_heap = NULL;

  DBUG_ENTER("ha_innobase::prepare_inplace_alter_table_impl");
  DBUG_ASSERT(!ha_alter_info->handler_ctx);
  DBUG_ASSERT(ha_alter_info->create_info);
  DBUG_ASSERT(!srv_read_only_mode);

  MONITOR_ATOMIC_INC(MONITOR_PENDING_ALTER_TABLE);

#ifdef UNIV_DEBUG
  for (dict_index_t *index = m_prebuilt->table->first_index(); index;
       index = index->next()) {
    ut_ad(!index->to_be_dropped);
  }
#endif /* UNIV_DEBUG */

  ut_d(mutex_enter(&dict_sys->mutex));
  ut_d(dict_table_check_for_dup_indexes(m_prebuilt->table, CHECK_ABORTED_OK));
  ut_d(mutex_exit(&dict_sys->mutex));

  if (!(ha_alter_info->handler_flags & ~INNOBASE_INPLACE_IGNORE) ||
      is_instant(ha_alter_info)) {
    /* Nothing to do. Since there is no MDL protected, don't
    try to drop aborted indexes here. */
    DBUG_ASSERT(m_prebuilt->trx->dict_operation_lock_mode == 0);
    DBUG_RETURN(false);
  }

  indexed_table = m_prebuilt->table;

  if (indexed_table->is_corrupted()) {
    /* The clustered index is corrupted. */
    my_error(ER_CHECK_NO_SUCH_TABLE, MYF(0));
    DBUG_RETURN(true);
  }

  if (dict_table_is_discarded(indexed_table) &&
      innobase_need_rebuild(ha_alter_info)) {
    my_error(ER_TABLESPACE_DISCARDED, MYF(0), indexed_table->name.m_name);
    DBUG_RETURN(true);
  }

  /* ALTER TABLE will not implicitly move a table from a single-table
  tablespace to the system tablespace when innodb_file_per_table=OFF.
  But it will implicitly move a table from the system tablespace to a
  single-table tablespace if innodb_file_per_table = ON.
  Tables found in a general tablespace will stay there unless ALTER
  TABLE contains another TABLESPACE=name.  If that is found it will
  explicitly move a table to the named tablespace.
  So if you specify TABLESPACE=`innodb_system` a table can be moved
  into the system tablespace from either a general or file-per-table
  tablespace. But from then on, it is labeled as using a shared space
  (the create options have tablespace=='innodb_system' and the
  SHARED_SPACE flag is set in the table flags) so it can no longer be
  implicitly moved to a file-per-table tablespace. */
  bool in_system_space = fsp_is_system_or_temp_tablespace(indexed_table->space);
  bool is_file_per_table =
      !in_system_space && !DICT_TF_HAS_SHARED_SPACE(indexed_table->flags);
#ifdef UNIV_DEBUG
  bool in_general_space =
      !in_system_space && DICT_TF_HAS_SHARED_SPACE(indexed_table->flags);

  /* The table being altered can only be in a system tablespace,
  or its own file-per-table tablespace, or a general tablespace. */
  ut_ad(1 == in_system_space + is_file_per_table + in_general_space);
#endif /* UNIV_DEBUG */

  /* Make a copy for existing tablespace name */
  char tablespace[NAME_LEN] = {'\0'};
  if (indexed_table->tablespace) {
    strcpy(tablespace, indexed_table->tablespace());
  }

  create_table_info_t info(m_user_thd, altered_table,
                           ha_alter_info->create_info, nullptr, nullptr,
                           indexed_table->tablespace ? tablespace : nullptr,
                           is_file_per_table, false, 0, 0);

  info.set_tablespace_type(is_file_per_table);

  if (ha_alter_info->handler_flags & Alter_inplace_info::CHANGE_CREATE_OPTION) {
    const char *invalid_opt = info.create_options_are_invalid();
    if (invalid_opt) {
      my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0), table_type(), invalid_opt);
      goto err_exit_no_heap;
    }
  }

  /* If target tablespace is shared tablespace, remove encrypt option from
  table definition as table is moved to shared tablespace. */
  if (is_shared_tablespace(ha_alter_info->create_info->tablespace) &&
      old_dd_tab->options().exists("encrypt_type")) {
    new_dd_tab->options().remove("encrypt_type");
  }

  /* Check if any index name is reserved. */
  if (innobase_index_name_is_reserved(m_user_thd,
                                      ha_alter_info->key_info_buffer,
                                      ha_alter_info->key_count)) {
  err_exit_no_heap:
    DBUG_ASSERT(m_prebuilt->trx->dict_operation_lock_mode == 0);
    if (ha_alter_info->handler_flags & ~INNOBASE_INPLACE_IGNORE) {
      online_retry_drop_dict_indexes(m_prebuilt->table, false);
    }
    DBUG_RETURN(true);
  }

  indexed_table = m_prebuilt->table;

  /* Check that index keys are sensible */
  error = innobase_check_index_keys(ha_alter_info, indexed_table);

  if (error) {
    goto err_exit_no_heap;
  }

  /* Prohibit renaming a column to something that the table
  already contains. */
  if (ha_alter_info->handler_flags & Alter_inplace_info::ALTER_COLUMN_NAME) {
    List_iterator_fast<Create_field> cf_it(
        ha_alter_info->alter_info->create_list);

    for (Field **fp = table->field; *fp; fp++) {
      if (!((*fp)->flags & FIELD_IS_RENAMED)) {
        continue;
      }

      const char *name = 0;

      cf_it.rewind();
      while (const Create_field *cf = cf_it++) {
        if (cf->field == *fp) {
          name = cf->field_name;
          goto check_if_ok_to_rename;
        }
      }

      ut_error;
    check_if_ok_to_rename:
      /* Prohibit renaming a column from FTS_DOC_ID
      if full-text indexes exist. */
      if (!my_strcasecmp(system_charset_info, (*fp)->field_name,
                         FTS_DOC_ID_COL_NAME) &&
          innobase_fulltext_exist(altered_table)) {
        my_error(ER_INNODB_FT_WRONG_DOCID_COLUMN, MYF(0), name);
        goto err_exit_no_heap;
      }

      /* Prohibit renaming a column to an internal column. */
      const char *s = m_prebuilt->table->col_names;
      unsigned j;
      /* Skip user columns.
      MySQL should have checked these already.
      We want to allow renaming of c1 to c2, c2 to c1. */
      for (j = 0; j < table->s->fields; j++) {
        if (!innobase_is_v_fld(table->field[j])) {
          s += strlen(s) + 1;
        }
      }

      for (; j < m_prebuilt->table->n_def; j++) {
        if (!my_strcasecmp(system_charset_info, name, s)) {
          my_error(ER_WRONG_COLUMN_NAME, MYF(0), s);
          goto err_exit_no_heap;
        }

        s += strlen(s) + 1;
      }
    }
  }

  if (!info.innobase_table_flags()) {
    goto err_exit_no_heap;
  }

  max_col_len = DICT_MAX_FIELD_LEN_BY_FORMAT_FLAG(info.flags());

  /* Check each index's column length to make sure they do not
  exceed limit */
  for (ulint i = 0; i < ha_alter_info->index_add_count; i++) {
    const KEY *key =
        &ha_alter_info->key_info_buffer[ha_alter_info->index_add_buffer[i]];

    if (key->flags & HA_FULLTEXT) {
      /* The column length does not matter for
      fulltext search indexes. But, UNIQUE
      fulltext indexes are not supported. */
      DBUG_ASSERT(!(key->flags & HA_NOSAME));
      DBUG_ASSERT(!(key->flags & HA_KEYFLAG_MASK &
                    ~(HA_FULLTEXT | HA_PACK_KEY | HA_BINARY_PACK_KEY)));
      add_fts_idx = true;
      continue;
    }

    if (innobase_check_column_length(max_col_len, key)) {
      my_error(ER_INDEX_COLUMN_TOO_LONG, MYF(0), max_col_len);
      goto err_exit_no_heap;
    }
  }

  /* Check existing index definitions for too-long column
  prefixes as well, in case max_col_len shrunk. */
  for (const dict_index_t *index = indexed_table->first_index(); index;
       index = index->next()) {
    if (index->type & DICT_FTS) {
      DBUG_ASSERT(index->type == DICT_FTS || index->is_corrupted());

      /* We need to drop any corrupted fts indexes
      before we add a new fts index. */
      if (add_fts_idx && index->type & DICT_CORRUPT) {
        ib_errf(m_user_thd, IB_LOG_LEVEL_ERROR, ER_INNODB_INDEX_CORRUPT,
                "Fulltext index '%s' is corrupt. "
                "you should drop this index first.",
                index->name());

        goto err_exit_no_heap;
      }

      continue;
    }

    for (ulint i = 0; i < dict_index_get_n_fields(index); i++) {
      const dict_field_t *field = index->get_field(i);
      if (field->prefix_len > max_col_len) {
        my_error(ER_INDEX_COLUMN_TOO_LONG, MYF(0), max_col_len);
        goto err_exit_no_heap;
      }
    }
  }

  n_drop_index = 0;
  n_drop_fk = 0;

  if (ha_alter_info->handler_flags &
      (INNOBASE_ALTER_NOREBUILD | INNOBASE_ALTER_REBUILD)) {
    heap = mem_heap_create(1024);

    if (ha_alter_info->handler_flags & Alter_inplace_info::ALTER_COLUMN_NAME) {
      col_names = innobase_get_col_names(ha_alter_info, altered_table, table,
                                         indexed_table, heap);
    } else {
      col_names = NULL;
    }
  } else {
    heap = NULL;
    col_names = NULL;
  }

  if (ha_alter_info->handler_flags & Alter_inplace_info::DROP_FOREIGN_KEY) {
    DBUG_ASSERT(ha_alter_info->alter_info->drop_list.size() > 0);

    drop_fk = static_cast<dict_foreign_t **>(
        mem_heap_alloc(heap, ha_alter_info->alter_info->drop_list.size() *
                                 sizeof(dict_foreign_t *)));

    for (const Alter_drop *drop : ha_alter_info->alter_info->drop_list) {
      if (drop->type != Alter_drop::FOREIGN_KEY) {
        continue;
      }

      for (dict_foreign_set::iterator it =
               m_prebuilt->table->foreign_set.begin();
           it != m_prebuilt->table->foreign_set.end(); ++it) {
        dict_foreign_t *foreign = *it;
        const char *fid = strchr(foreign->id, '/');

        DBUG_ASSERT(fid);
        /* If no database/ prefix was present in
        the FOREIGN KEY constraint name, compare
        to the full constraint name. */
        fid = fid ? fid + 1 : foreign->id;

        if (!my_strcasecmp(system_charset_info, fid, drop->name)) {
          drop_fk[n_drop_fk++] = foreign;
          goto found_fk;
        }
      }

      my_error(ER_CANT_DROP_FIELD_OR_KEY, MYF(0), drop->name);
      goto err_exit;
    found_fk:
      continue;
    }

    DBUG_ASSERT(n_drop_fk > 0);

    DBUG_ASSERT(n_drop_fk == ha_alter_info->alter_info->drop_list.size());
  } else {
    drop_fk = NULL;
  }

  if (ha_alter_info->index_drop_count) {
    dict_index_t *drop_primary = NULL;

    DBUG_ASSERT(ha_alter_info->handler_flags &
                (Alter_inplace_info::DROP_INDEX |
                 Alter_inplace_info::DROP_UNIQUE_INDEX |
                 Alter_inplace_info::DROP_PK_INDEX));
    /* Check which indexes to drop. */
    drop_index = static_cast<dict_index_t **>(mem_heap_alloc(
        heap, (ha_alter_info->index_drop_count + 1) * sizeof *drop_index));

    for (uint i = 0; i < ha_alter_info->index_drop_count; i++) {
      const KEY *key = ha_alter_info->index_drop_buffer[i];
      dict_index_t *index =
          dict_table_get_index_on_name(indexed_table, key->name);

      if (!index) {
        push_warning_printf(m_user_thd, Sql_condition::SL_WARNING,
                            HA_ERR_WRONG_INDEX,
                            "InnoDB could not find key"
                            " with name %s",
                            key->name);
      } else {
        ut_ad(!index->to_be_dropped);
        if (!index->is_clustered()) {
          drop_index[n_drop_index++] = index;
        } else {
          drop_primary = index;
        }
      }
    }

    /* If all FULLTEXT indexes were removed, drop an
    internal FTS_DOC_ID_INDEX as well, unless it exists in
    the table. */

    if (innobase_fulltext_exist(table) &&
        !innobase_fulltext_exist(altered_table) &&
        !DICT_TF2_FLAG_IS_SET(indexed_table, DICT_TF2_FTS_HAS_DOC_ID)) {
      dict_index_t *fts_doc_index = indexed_table->fts_doc_id_index;
      ut_ad(fts_doc_index);

      // Add some fault tolerance for non-debug builds.
      if (fts_doc_index == NULL) {
        goto check_if_can_drop_indexes;
      }

      DBUG_ASSERT(!fts_doc_index->to_be_dropped);

      for (uint i = 0; i < table->s->keys; i++) {
        if (!my_strcasecmp(system_charset_info, FTS_DOC_ID_INDEX_NAME,
                           table->key_info[i].name)) {
          /* The index exists in the MySQL
          data dictionary. Do not drop it,
          even though it is no longer needed
          by InnoDB fulltext search. */
          goto check_if_can_drop_indexes;
        }
      }

      drop_index[n_drop_index++] = fts_doc_index;
    }

  check_if_can_drop_indexes:
    /* Check if the indexes can be dropped. */

    /* Prevent a race condition between DROP INDEX and
    CREATE TABLE adding FOREIGN KEY constraints. */
    row_mysql_lock_data_dictionary(m_prebuilt->trx);

    if (!n_drop_index) {
      drop_index = NULL;
    } else {
      /* Flag all indexes that are to be dropped. */
      for (ulint i = 0; i < n_drop_index; i++) {
        ut_ad(!drop_index[i]->to_be_dropped);
        drop_index[i]->to_be_dropped = 1;
      }
    }

    for (uint i = 0; i < n_drop_index; i++) {
      dict_index_t *index = drop_index[i];

      if (innobase_check_foreign_key_index(ha_alter_info, index, indexed_table,
                                           col_names, m_prebuilt->trx, drop_fk,
                                           n_drop_fk)) {
        row_mysql_unlock_data_dictionary(m_prebuilt->trx);
        m_prebuilt->trx->error_info = index;
        print_error(HA_ERR_DROP_INDEX_FK, MYF(0));
        goto err_exit;
      }
    }

    /* If a primary index is dropped, need to check
    any depending foreign constraints get affected */
    if (drop_primary && innobase_check_foreign_key_index(
                            ha_alter_info, drop_primary, indexed_table,
                            col_names, m_prebuilt->trx, drop_fk, n_drop_fk)) {
      row_mysql_unlock_data_dictionary(m_prebuilt->trx);
      print_error(HA_ERR_DROP_INDEX_FK, MYF(0));
      goto err_exit;
    }

    row_mysql_unlock_data_dictionary(m_prebuilt->trx);
  } else {
    drop_index = NULL;
  }

  n_rename_index = ha_alter_info->index_rename_count;
  rename_index = NULL;

  /* Create a list of dict_index_t objects that are to be renamed,
  also checking for requests to rename nonexistent indexes. If
  the table is going to be rebuilt (new_clustered == true in
  prepare_inplace_alter_table_dict()), then this can be skipped,
  but we don't for simplicity (we have not determined the value of
  new_clustered yet). */
  if (n_rename_index > 0) {
    rename_index = static_cast<dict_index_t **>(
        mem_heap_alloc(heap, n_rename_index * sizeof(*rename_index)));
    for (ulint i = 0; i < n_rename_index; i++) {
      dict_index_t *index;
      const char *old_name =
          ha_alter_info->index_rename_buffer[i].old_key->name;

      index = dict_table_get_index_on_name(indexed_table, old_name);

      if (index == NULL) {
        my_error(ER_KEY_DOES_NOT_EXITS, MYF(0), old_name,
                 m_prebuilt->table->name.m_name);
        goto err_exit;
      }

      rename_index[i] = index;
    }
  }

  n_add_fk = 0;

  if (ha_alter_info->handler_flags & Alter_inplace_info::ADD_FOREIGN_KEY) {
    ut_ad(!m_prebuilt->trx->check_foreigns);

    alter_fill_stored_column(altered_table, m_prebuilt->table, &s_cols,
                             &s_heap);

    add_fk = static_cast<dict_foreign_t **>(mem_heap_zalloc(
        heap,
        ha_alter_info->alter_info->key_list.size() * sizeof(dict_foreign_t *)));

    if (!innobase_get_foreign_key_info(ha_alter_info, table_share,
                                       m_prebuilt->table, col_names, drop_index,
                                       n_drop_index, add_fk, &n_add_fk,
                                       m_prebuilt->trx, s_cols)) {
    err_exit:
      if (n_drop_index) {
        row_mysql_lock_data_dictionary(m_prebuilt->trx);

        /* Clear the to_be_dropped flags, which might
        have been set at this point. */
        for (ulint i = 0; i < n_drop_index; i++) {
          ut_ad(drop_index[i]->is_committed());
          drop_index[i]->to_be_dropped = 0;
        }

        row_mysql_unlock_data_dictionary(m_prebuilt->trx);
      }

      if (heap) {
        mem_heap_free(heap);
      }

      if (s_cols != NULL) {
        UT_DELETE(s_cols);
        mem_heap_free(s_heap);
      }

      goto err_exit_no_heap;
    }

    if (s_cols != NULL) {
      UT_DELETE(s_cols);
      mem_heap_free(s_heap);
    }
  }

  if (!(ha_alter_info->handler_flags & INNOBASE_ALTER_DATA) ||
      ((ha_alter_info->handler_flags & ~INNOBASE_INPLACE_IGNORE) ==
           Alter_inplace_info::CHANGE_CREATE_OPTION &&
       !innobase_need_rebuild(ha_alter_info))) {
    if (heap) {
      ha_alter_info->handler_ctx = new (*THR_MALLOC) ha_innobase_inplace_ctx(
          m_prebuilt, drop_index, n_drop_index, rename_index, n_rename_index,
          drop_fk, n_drop_fk, add_fk, n_add_fk, ha_alter_info->online, heap,
          indexed_table, col_names, ULINT_UNDEFINED, 0, 0, 0);
    }

    DBUG_ASSERT(m_prebuilt->trx->dict_operation_lock_mode == 0);
    if (ha_alter_info->handler_flags & ~INNOBASE_INPLACE_IGNORE) {
      online_retry_drop_dict_indexes(m_prebuilt->table, false);
    }

    if ((ha_alter_info->handler_flags &
         Alter_inplace_info::DROP_VIRTUAL_COLUMN) &&
        prepare_inplace_drop_virtual(ha_alter_info, altered_table, table)) {
      DBUG_RETURN(true);
    }

    if ((ha_alter_info->handler_flags &
         Alter_inplace_info::ADD_VIRTUAL_COLUMN) &&
        prepare_inplace_add_virtual(ha_alter_info, altered_table, table)) {
      DBUG_RETURN(true);
    }

    if (ha_alter_info->handler_ctx != NULL) {
      ha_innobase_inplace_ctx *ctx =
          static_cast<ha_innobase_inplace_ctx *>(ha_alter_info->handler_ctx);
      DBUG_RETURN(dd_prepare_inplace_alter_table(
          m_user_thd, ctx->old_table, ctx->new_table, old_dd_tab, new_dd_tab));
    } else {
      DBUG_RETURN(false);
    }
  }

  /* If we are to build a full-text search index, check whether
  the table already has a DOC ID column.  If not, we will need to
  add a Doc ID hidden column and rebuild the primary index */
  if (innobase_fulltext_exist(altered_table)) {
    ulint doc_col_no;
    ulint num_v = 0;

    if (!innobase_fts_check_doc_id_col(m_prebuilt->table, altered_table,
                                       &fts_doc_col_no, &num_v)) {
      fts_doc_col_no = altered_table->s->fields - num_v;
      add_fts_doc_id = true;
      add_fts_doc_id_idx = true;

      push_warning_printf(m_user_thd, Sql_condition::SL_WARNING,
                          HA_ERR_WRONG_INDEX,
                          "InnoDB rebuilding table to add"
                          " column " FTS_DOC_ID_COL_NAME);
    } else if (fts_doc_col_no == ULINT_UNDEFINED) {
      goto err_exit;
    }

    switch (innobase_fts_check_doc_id_index(m_prebuilt->table, altered_table,
                                            &doc_col_no)) {
      case FTS_NOT_EXIST_DOC_ID_INDEX:
        add_fts_doc_id_idx = true;
        break;
      case FTS_INCORRECT_DOC_ID_INDEX:
        my_error(ER_INNODB_FT_WRONG_DOCID_INDEX, MYF(0), FTS_DOC_ID_INDEX_NAME);
        goto err_exit;
      case FTS_EXIST_DOC_ID_INDEX:
        DBUG_ASSERT(doc_col_no == fts_doc_col_no ||
                    doc_col_no == ULINT_UNDEFINED ||
                    (ha_alter_info->handler_flags &
                     (Alter_inplace_info::ALTER_STORED_COLUMN_ORDER |
                      Alter_inplace_info::DROP_STORED_COLUMN |
                      Alter_inplace_info::ADD_STORED_BASE_COLUMN)));
    }
  }

  /* See if an AUTO_INCREMENT column was added. */
  uint i = 0;
  ulint num_v = 0;
  List_iterator_fast<Create_field> cf_it(
      ha_alter_info->alter_info->create_list);
  while (const Create_field *new_field = cf_it++) {
    const Field *field;

    DBUG_ASSERT(i < altered_table->s->fields);

    for (uint old_i = 0; table->field[old_i]; old_i++) {
      if (new_field->field == table->field[old_i]) {
        goto found_col;
      }
    }

    /* This is an added column. */
    DBUG_ASSERT(!new_field->field);
    DBUG_ASSERT(ha_alter_info->handler_flags & Alter_inplace_info::ADD_COLUMN);

    field = altered_table->field[i];

    DBUG_ASSERT((field->auto_flags & Field::NEXT_NUMBER) ==
                !!(field->flags & AUTO_INCREMENT_FLAG));

    if (field->flags & AUTO_INCREMENT_FLAG) {
      if (add_autoinc_col_no != ULINT_UNDEFINED) {
        /* This should have been blocked earlier. */
        ut_ad(0);
        my_error(ER_WRONG_AUTO_KEY, MYF(0));
        goto err_exit;
      }

      /* Get the col no of the old table non-virtual column array */
      add_autoinc_col_no = i - num_v;

      autoinc_col_max_value = field->get_max_int_value();
    }
  found_col:
    if (innobase_is_v_fld(new_field)) {
      ++num_v;
    }

    i++;
  }

  DBUG_ASSERT(heap);
  DBUG_ASSERT(m_user_thd == m_prebuilt->trx->mysql_thd);
  DBUG_ASSERT(!ha_alter_info->handler_ctx);

  ha_alter_info->handler_ctx = new (*THR_MALLOC) ha_innobase_inplace_ctx(
      m_prebuilt, drop_index, n_drop_index, rename_index, n_rename_index,
      drop_fk, n_drop_fk, add_fk, n_add_fk, ha_alter_info->online, heap,
      m_prebuilt->table, col_names, add_autoinc_col_no,
      ha_alter_info->create_info->auto_increment_value, autoinc_col_max_value,
      0);

  DBUG_RETURN(prepare_inplace_alter_table_dict(
      ha_alter_info, altered_table, table, old_dd_tab, new_dd_tab,
      table_share->table_name.str, info.flags(), info.flags2(), fts_doc_col_no,
      add_fts_doc_id, add_fts_doc_id_idx));
}

/** Check that the column is part of a virtual index(index contains
virtual column) in the table
@param[in]	table		Table containing column
@param[in]	col		column to be checked
@return true if this column is indexed with other virtual columns */
static bool dict_col_in_v_indexes(dict_table_t *table, dict_col_t *col) {
  for (dict_index_t *index = table->first_index()->next(); index != NULL;
       index = index->next()) {
    if (!dict_index_has_virtual(index)) {
      continue;
    }
    for (ulint k = 0; k < index->n_fields; k++) {
      dict_field_t *field = index->get_field(k);
      if (field->col->ind == col->ind) {
        return (true);
      }
    }
  }

  return (false);
}

/* Check whether a columnn length change alter operation requires
to rebuild the template.
@param[in]	altered_table	TABLE object for new version of table.
@param[in]	ha_alter_info	Structure describing changes to be done
                                by ALTER TABLE and holding data used
                                during in-place alter.
@param[in]	table		table being altered
@return true if needs rebuild. */
static bool alter_templ_needs_rebuild(TABLE *altered_table,
                                      Alter_inplace_info *ha_alter_info,
                                      dict_table_t *table) {
  ulint i = 0;
  List_iterator_fast<Create_field> cf_it(
      ha_alter_info->alter_info->create_list);

  for (Field **fp = altered_table->field; *fp; fp++, i++) {
    cf_it.rewind();
    while (const Create_field *cf = cf_it++) {
      for (ulint j = 0; j < table->n_cols; j++) {
        dict_col_t *cols = table->get_col(j);
        if (cf->length > cols->len && dict_col_in_v_indexes(table, cols)) {
          return (true);
        }
      }
    }
  }

  return (false);
}

/** Get the name of an erroneous key.
@param[in]	error_key_num	InnoDB number of the erroneus key
@param[in]	ha_alter_info	changes that were being performed
@param[in]	table		InnoDB table
@return	the name of the erroneous key */
static const char *get_error_key_name(ulint error_key_num,
                                      const Alter_inplace_info *ha_alter_info,
                                      const dict_table_t *table) {
  if (error_key_num == ULINT_UNDEFINED) {
    return (FTS_DOC_ID_INDEX_NAME);
  } else if (ha_alter_info->key_count == 0) {
    return (table->first_index()->name);
  } else {
    return (ha_alter_info->key_info_buffer[error_key_num].name);
  }
}

/** Implementation of inplace_alter_table()
@tparam		Table		dd::Table or dd::Partition
@param[in]	altered_table	TABLE object for new version of table.
@param[in,out]	ha_alter_info	Structure describing changes to be done
                                by ALTER TABLE and holding data used
                                during in-place alter.
@param[in]	old_dd_tab	dd::Table object describing old version
                                of the table.
@param[in,out]	new_dd_tab	dd::Table object for the new version of the
                                table. Can be adjusted by this call.
                                Changes to the table definition will be
                                persisted in the data-dictionary at statement
                                commit time.
@retval true Failure
@retval false Success
*/
template <typename Table>
bool ha_innobase::inplace_alter_table_impl(TABLE *altered_table,
                                           Alter_inplace_info *ha_alter_info,
                                           const Table *old_dd_tab,
                                           Table *new_dd_tab) {
  dberr_t error;
  dict_add_v_col_t *add_v = NULL;
  dict_vcol_templ_t *s_templ = NULL;
  dict_vcol_templ_t *old_templ = NULL;
  struct TABLE *eval_table = altered_table;
  bool rebuild_templ = false;
  DBUG_ENTER("ha_innobase::inplace_alter_table_impl");
  DBUG_ASSERT(!srv_read_only_mode);

  ut_ad(!rw_lock_own(dict_operation_lock, RW_LOCK_X));
  ut_ad(!rw_lock_own(dict_operation_lock, RW_LOCK_S));

  DEBUG_SYNC(m_user_thd, "innodb_inplace_alter_table_enter");

  if (!(ha_alter_info->handler_flags & INNOBASE_ALTER_DATA) ||
      is_instant(ha_alter_info)) {
  ok_exit:
    DEBUG_SYNC(m_user_thd, "innodb_after_inplace_alter_table");
    DBUG_RETURN(false);
  }

  if (((ha_alter_info->handler_flags & ~INNOBASE_INPLACE_IGNORE) ==
           Alter_inplace_info::CHANGE_CREATE_OPTION &&
       !innobase_need_rebuild(ha_alter_info))) {
    goto ok_exit;
  }

  ha_innobase_inplace_ctx *ctx =
      static_cast<ha_innobase_inplace_ctx *>(ha_alter_info->handler_ctx);

  DBUG_ASSERT(ctx);
  DBUG_ASSERT(ctx->trx);
  DBUG_ASSERT(ctx->prebuilt == m_prebuilt);

  dict_index_t *pk = m_prebuilt->table->first_index();
  ut_ad(pk != NULL);

  /* For partitioned tables this could be already allocated from a
  previous partition invocation. For normal tables this is NULL. */
  UT_DELETE(ctx->m_stage);

  ctx->m_stage = UT_NEW_NOKEY(ut_stage_alter_t(pk));

  if (m_prebuilt->table->ibd_file_missing ||
      dict_table_is_discarded(m_prebuilt->table)) {
    goto all_done;
  }

  /* If we are doing a table rebuilding or having added virtual
  columns in the same clause, we will need to build a table template
  that carries translation information between MySQL TABLE and InnoDB
  table, which indicates the virtual columns and their base columns
  info. This is used to do the computation callback, so that the
  data in base columns can be extracted send to server.
  If the Column length changes and it is a part of virtual
  index then we need to rebuild the template. */
  rebuild_templ =
      ctx->need_rebuild() ||
      ((ha_alter_info->handler_flags &
        Alter_inplace_info::ALTER_COLUMN_EQUAL_PACK_LENGTH) &&
       alter_templ_needs_rebuild(altered_table, ha_alter_info, ctx->new_table));

  if ((ctx->new_table->n_v_cols > 0) && rebuild_templ) {
    /* Save the templ if isn't NULL so as to restore the
    original state in case of alter operation failures. */
    if (ctx->new_table->vc_templ != NULL && !ctx->need_rebuild()) {
      old_templ = ctx->new_table->vc_templ;
    }
    s_templ = UT_NEW_NOKEY(dict_vcol_templ_t());
    s_templ->vtempl = NULL;

    innobase_build_v_templ(altered_table, ctx->new_table, s_templ, NULL, false,
                           NULL);

    ctx->new_table->vc_templ = s_templ;
  } else if (ctx->num_to_add_vcol > 0 && ctx->num_to_drop_vcol == 0) {
    /* if there is ongoing drop virtual column, then we disallow
    inplace add index on newly added virtual column, so it does
    not need to come in here to rebuild template with add_v.
    Please also see the assertion in innodb_v_adjust_idx_col() */

    s_templ = UT_NEW_NOKEY(dict_vcol_templ_t());

    add_v = static_cast<dict_add_v_col_t *>(
        mem_heap_alloc(ctx->heap, sizeof *add_v));
    add_v->n_v_col = ctx->num_to_add_vcol;
    add_v->v_col = ctx->add_vcol;
    add_v->v_col_name = ctx->add_vcol_name;

    s_templ->vtempl = NULL;

    innobase_build_v_templ(altered_table, ctx->new_table, s_templ, add_v, false,
                           NULL);
    old_templ = ctx->new_table->vc_templ;
    ctx->new_table->vc_templ = s_templ;
  }

  /* Drop virtual column without rebuild will keep dict table
  unchanged, we use old table to evaluate virtual column value
  in innobase_get_computed_value(). */
  if (!ctx->need_rebuild() && ctx->num_to_drop_vcol > 0) {
    eval_table = table;
  }

  /* Read the clustered index of the table and build
  indexes based on this information using temporary
  files and merge sort. */
  DBUG_EXECUTE_IF("innodb_OOM_inplace_alter", error = DB_OUT_OF_MEMORY;
                  goto oom;);
  error = row_merge_build_indexes(
      m_prebuilt->trx, m_prebuilt->table, ctx->new_table, ctx->online,
      ctx->add_index, ctx->add_key_numbers, ctx->num_to_add_index,
      altered_table, ctx->add_cols, ctx->col_map, ctx->add_autoinc,
      ctx->sequence, ctx->skip_pk_sort, ctx->m_stage, add_v, eval_table);

#ifdef UNIV_DEBUG
oom:
#endif /* UNIV_DEBUG */
  if (error == DB_SUCCESS && ctx->online && ctx->need_rebuild()) {
    DEBUG_SYNC_C("row_log_table_apply1_before");
    error = row_log_table_apply(ctx->thr, m_prebuilt->table, altered_table,
                                ctx->m_stage);
  }

  if (s_templ) {
    ut_ad(ctx->need_rebuild() || ctx->num_to_add_vcol > 0 || rebuild_templ);
    dict_free_vc_templ(s_templ);
    UT_DELETE(s_templ);

    ctx->new_table->vc_templ = old_templ;
  }

  DEBUG_SYNC_C("inplace_after_index_build");

  DBUG_EXECUTE_IF("create_index_fail", error = DB_DUPLICATE_KEY;
                  m_prebuilt->trx->error_key_num = ULINT_UNDEFINED;);

  /* After an error, remove all those index definitions
  from the dictionary which were defined. */

  switch (error) {
    KEY *dup_key;
  all_done:
  case DB_SUCCESS:
    ut_d(mutex_enter(&dict_sys->mutex));
    ut_d(dict_table_check_for_dup_indexes(m_prebuilt->table, CHECK_PARTIAL_OK));
    ut_d(mutex_exit(&dict_sys->mutex));
    /* prebuilt->table->n_ref_count can be anything here,
    given that we hold at most a shared lock on the table. */
    goto ok_exit;
    case DB_DUPLICATE_KEY:
      if (m_prebuilt->trx->error_key_num == ULINT_UNDEFINED ||
          ha_alter_info->key_count == 0) {
        /* This should be the hidden index on
        FTS_DOC_ID, or there is no PRIMARY KEY in the
        table. Either way, we should be seeing and
        reporting a bogus duplicate key error. */
        dup_key = NULL;
      } else {
        DBUG_ASSERT(m_prebuilt->trx->error_key_num < ha_alter_info->key_count);
        dup_key =
            &ha_alter_info->key_info_buffer[m_prebuilt->trx->error_key_num];
      }
      print_keydup_error(altered_table, dup_key, MYF(0));
      break;
    case DB_ONLINE_LOG_TOO_BIG:
      DBUG_ASSERT(ctx->online);
      my_error(ER_INNODB_ONLINE_LOG_TOO_BIG, MYF(0),
               get_error_key_name(m_prebuilt->trx->error_key_num, ha_alter_info,
                                  m_prebuilt->table));
      break;
    case DB_INDEX_CORRUPT:
      my_error(ER_INDEX_CORRUPT, MYF(0),
               get_error_key_name(m_prebuilt->trx->error_key_num, ha_alter_info,
                                  m_prebuilt->table));
      break;
    default:
      my_error_innodb(error, table_share->table_name.str,
                      m_prebuilt->table->flags);
  }

  /* prebuilt->table->n_ref_count can be anything here, given
  that we hold at most a shared lock on the table. */
  m_prebuilt->trx->error_info = NULL;
  ctx->trx->error_state = DB_SUCCESS;

  DBUG_RETURN(true);
}

/** Free the modification log for online table rebuild.
@param table table that was being rebuilt online */
static void innobase_online_rebuild_log_free(dict_table_t *table) {
  dict_index_t *clust_index = table->first_index();

  ut_ad(mutex_own(&dict_sys->mutex));
  ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));

  rw_lock_x_lock(&clust_index->lock);

  if (clust_index->online_log) {
    ut_ad(dict_index_get_online_status(clust_index) == ONLINE_INDEX_CREATION);
    clust_index->online_status = ONLINE_INDEX_COMPLETE;
    row_log_free(clust_index->online_log);
    DEBUG_SYNC_C("innodb_online_rebuild_log_free_aborted");
  }

  DBUG_ASSERT(dict_index_get_online_status(clust_index) ==
              ONLINE_INDEX_COMPLETE);
  rw_lock_x_unlock(&clust_index->lock);
}

/** For each user column, which is part of an index which is not going to be
dropped, it checks if the column number of the column is same as col_no
argument passed.
@param[in]	table	table object
@param[in]	col_no	column number of the column which is to be checked
@param[in]	is_v	if this is a virtual column
@retval true column exists
@retval false column does not exist, true if column is system column or
it is in the index. */
static bool check_col_exists_in_indexes(const dict_table_t *table, ulint col_no,
                                        bool is_v) {
  /* This function does not check system columns */
  if (!is_v && table->get_col(col_no)->mtype == DATA_SYS) {
    return (true);
  }

  for (const dict_index_t *index = table->first_index(); index;
       index = index->next()) {
    if (index->to_be_dropped) {
      continue;
    }

    for (ulint i = 0; i < index->n_user_defined_cols; i++) {
      const dict_col_t *idx_col = index->get_col(i);

      if (is_v && idx_col->is_virtual()) {
        const dict_v_col_t *v_col =
            reinterpret_cast<const dict_v_col_t *>(idx_col);
        if (v_col->v_pos == col_no) {
          return (true);
        }
      }

      if (!is_v && !idx_col->is_virtual() &&
          dict_col_get_no(idx_col) == col_no) {
        return (true);
      }
    }
  }

  return (false);
}

/** Rollback a secondary index creation, drop the indexes with
temparary index prefix
@param user_table InnoDB table
@param table the TABLE
@param locked TRUE=table locked, FALSE=may need to do a lazy drop
@param trx the transaction
*/
static void innobase_rollback_sec_index(dict_table_t *user_table,
                                        const TABLE *table, ibool locked,
                                        trx_t *trx) {
  row_merge_drop_indexes(trx, user_table, locked);

  /* Free the table->fts only if there is no FTS_DOC_ID
  in the table */
  if (user_table->fts &&
      !DICT_TF2_FLAG_IS_SET(user_table, DICT_TF2_FTS_HAS_DOC_ID) &&
      !innobase_fulltext_exist(table)) {
    fts_free(user_table);
  }
}

/** Roll back the changes made during prepare_inplace_alter_table()
and inplace_alter_table() inside the storage engine. Note that the
allowed level of concurrency during this operation will be the same as
for inplace_alter_table() and thus might be higher than during
prepare_inplace_alter_table(). (E.g concurrent writes were blocked
during prepare, but might not be during commit).

@param[in]	ha_alter_info	Data used during in-place alter.
@param[in]	table		the TABLE
@param[in,out]	prebuilt	the prebuilt struct
@retval true Failure
@retval false Success
*/
inline MY_ATTRIBUTE((warn_unused_result)) bool rollback_inplace_alter_table(
    const Alter_inplace_info *ha_alter_info, const TABLE *table,
    row_prebuilt_t *prebuilt) {
  bool fail = false;

  ha_innobase_inplace_ctx *ctx =
      static_cast<ha_innobase_inplace_ctx *>(ha_alter_info->handler_ctx);

  DBUG_ENTER("rollback_inplace_alter_table");

  if (!ctx || !ctx->trx) {
    /* If we have not started a transaction yet,
    (almost) nothing has been or needs to be done. */
    goto func_exit;
  }

  row_mysql_lock_data_dictionary(ctx->trx);

  if (ctx->need_rebuild()) {
    /* The table could have been closed in commit phase */
    if (ctx->new_table != nullptr) {
      dberr_t err = DB_SUCCESS;
      ulint flags = ctx->new_table->flags;
      /* DML threads can access ctx->new_table via the
      online rebuild log. Free it first. */
      innobase_online_rebuild_log_free(prebuilt->table);

      dict_table_close(ctx->new_table, TRUE, FALSE);

      switch (err) {
        case DB_SUCCESS:
          break;
        default:
          my_error_innodb(err, table->s->table_name.str, flags);
          fail = true;
      }
    }
  } else {
    DBUG_ASSERT(
        !(ha_alter_info->handler_flags & Alter_inplace_info::ADD_PK_INDEX));
    DBUG_ASSERT(ctx->new_table == prebuilt->table);

    innobase_rollback_sec_index(prebuilt->table, table, FALSE, ctx->trx);
  }

  row_mysql_unlock_data_dictionary(ctx->trx);

func_exit:
#ifdef UNIV_DEBUG
  dict_index_t *clust_index = prebuilt->table->first_index();
  DBUG_ASSERT(!clust_index->online_log);
  DBUG_ASSERT(dict_index_get_online_status(clust_index) ==
              ONLINE_INDEX_COMPLETE);
#endif /* UNIV_DEBUG */

  if (ctx) {
    DBUG_ASSERT(ctx->prebuilt == prebuilt);

    if (ctx->num_to_add_fk) {
      for (ulint i = 0; i < ctx->num_to_add_fk; i++) {
        dict_foreign_free(ctx->add_fk[i]);
      }
    }

    if (ctx->num_to_drop_index) {
      row_mysql_lock_data_dictionary(prebuilt->trx);

      /* Clear the to_be_dropped flags
      in the data dictionary cache.
      The flags may already have been cleared,
      in case an error was detected in
      commit_inplace_alter_table(). */
      for (ulint i = 0; i < ctx->num_to_drop_index; i++) {
        dict_index_t *index = ctx->drop_index[i];
        DBUG_ASSERT(index->is_committed());
        index->to_be_dropped = 0;
      }

      row_mysql_unlock_data_dictionary(prebuilt->trx);
    }
  }

  /* Reset dict_col_t::ord_part for those columns fail to be indexed,
  we do this by checking every existing column, if any current
  index would index them */
  for (ulint i = 0; i < prebuilt->table->get_n_cols(); i++) {
    if (!check_col_exists_in_indexes(prebuilt->table, i, false)) {
      prebuilt->table->cols[i].ord_part = 0;
    }
  }

  for (ulint i = 0; i < dict_table_get_n_v_cols(prebuilt->table); i++) {
    if (!check_col_exists_in_indexes(prebuilt->table, i, true)) {
      prebuilt->table->v_cols[i].m_col.ord_part = 0;
    }
  }

  /* Do not commit/rollback prebuilt->trx, assume mysql will
  rollback it */

  MONITOR_ATOMIC_DEC(MONITOR_PENDING_ALTER_TABLE);
  DBUG_RETURN(fail);
}

/** Rename or enlarge columns in the data dictionary cache
as part of commit_cache_norebuild().
@param ha_alter_info Data used during in-place alter.
@param table the TABLE
@param user_table InnoDB table that was being altered */
static void innobase_rename_or_enlarge_columns_cache(
    Alter_inplace_info *ha_alter_info, const TABLE *table,
    dict_table_t *user_table) {
  if (!(ha_alter_info->handler_flags &
        (Alter_inplace_info::ALTER_COLUMN_EQUAL_PACK_LENGTH |
         Alter_inplace_info::ALTER_COLUMN_NAME))) {
    return;
  }

  List_iterator_fast<Create_field> cf_it(
      ha_alter_info->alter_info->create_list);
  uint i = 0;
  ulint num_v = 0;

  for (Field **fp = table->field; *fp; fp++, i++) {
    bool is_virtual = innobase_is_v_fld(*fp);

    cf_it.rewind();
    while (const Create_field *cf = cf_it++) {
      if (cf->field != *fp) {
        continue;
      }

      ulint col_n = is_virtual ? num_v : i - num_v;

      if ((*fp)->is_equal(cf) == IS_EQUAL_PACK_LENGTH) {
        if (is_virtual) {
          dict_table_get_nth_v_col(user_table, col_n)->m_col.len = cf->length;
        } else {
          user_table->get_col(col_n)->len = cf->length;
        }
      }

      if ((*fp)->flags & FIELD_IS_RENAMED) {
        dict_mem_table_col_rename(user_table, col_n, cf->field->field_name,
                                  cf->field_name, is_virtual);
      }

      break;
    }

    if (is_virtual) {
      num_v++;
    }
  }
}
/** Get the auto-increment value of the table on commit.
@param[in] ha_alter_info Data used during in-place alter
@param[in,out] ctx In-place ALTER TABLE context
               return autoinc value in ctx->max_autoinc
@param[in] altered_table MySQL table that is being altered
@param[in] old_table MySQL table as it is before the ALTER operation
@retval true Failure
@retval false Success*/
static MY_ATTRIBUTE((warn_unused_result)) bool commit_get_autoinc(
    Alter_inplace_info *ha_alter_info, ha_innobase_inplace_ctx *ctx,
    const TABLE *altered_table, const TABLE *old_table) {
  DBUG_ENTER("commit_get_autoinc");

  if (!altered_table->found_next_number_field) {
    /* There is no AUTO_INCREMENT column in the table
    after the ALTER operation. */
    ctx->max_autoinc = 0;
  } else if (ctx->add_autoinc != ULINT_UNDEFINED) {
    /* An AUTO_INCREMENT column was added. Get the last
    value from the sequence, which may be based on a
    supplied AUTO_INCREMENT value. */
    ctx->max_autoinc = ctx->sequence.last();
  } else if ((ha_alter_info->handler_flags &
              Alter_inplace_info::CHANGE_CREATE_OPTION) &&
             (ha_alter_info->create_info->used_fields & HA_CREATE_USED_AUTO)) {
    /* Check if the table is discarded */
    if (dict_table_is_discarded(ctx->old_table)) {
      DBUG_RETURN(true);
    }

    /* An AUTO_INCREMENT value was supplied, but the table was not
    rebuilt. Get the user-supplied value or the last value from the
    sequence. */
    ib_uint64_t max_value_table;

    Field *autoinc_field = old_table->found_next_number_field;

    ctx->max_autoinc = ha_alter_info->create_info->auto_increment_value;

    dict_table_autoinc_lock(ctx->old_table);

    max_value_table = ctx->old_table->autoinc_persisted;

    /* We still have to search the index here when we want to
    set the AUTO_INCREMENT value to a smaller or equal one.

    Here is an example:
    Let's say we have a table t1 with one AUTOINC column, existing
    rows (1), (2), (100), (200), (1000), after following SQLs:
    DELETE FROM t1 WHERE a > 200;
    ALTER TABLE t1 AUTO_INCREMENT = 150;
    we expect the next value allocated from 201, but not 150.

    We could only search the tree to know current max counter
    in the table and compare. */
    if (ctx->max_autoinc <= max_value_table) {
      dberr_t err;
      dict_index_t *index;

      index = dict_table_get_index_on_first_col(ctx->old_table,
                                                autoinc_field->field_index);

      err = row_search_max_autoinc(index, autoinc_field->field_name,
                                   &max_value_table);

      if (err != DB_SUCCESS) {
        ut_ad(0);
        ctx->max_autoinc = 0;
      } else if (ctx->max_autoinc <= max_value_table) {
        ulonglong col_max_value;
        ulonglong offset;

        col_max_value = autoinc_field->get_max_int_value();
        offset = ctx->prebuilt->autoinc_offset;
        ctx->max_autoinc =
            innobase_next_autoinc(max_value_table, 1, 1, offset, col_max_value);
      }
    }

    dict_table_autoinc_unlock(ctx->old_table);
  } else {
    /* An AUTO_INCREMENT value was not specified.
    Read the old counter value from the table. */
    ut_ad(old_table->found_next_number_field);
    dict_table_autoinc_lock(ctx->old_table);
    ctx->max_autoinc = ctx->old_table->autoinc;
    dict_table_autoinc_unlock(ctx->old_table);
  }

  DBUG_RETURN(false);
}

/** Add or drop foreign key constraints to the data dictionary tables,
but do not touch the data dictionary cache.
@param ctx In-place ALTER TABLE context
@param trx Data dictionary transaction
@param table_name Table name in MySQL
@retval true Failure
@retval false Success
*/
static MY_ATTRIBUTE((warn_unused_result)) bool innobase_update_foreign_try(
    ha_innobase_inplace_ctx *ctx, trx_t *trx, const char *table_name) {
  ulint foreign_id;
  ulint i;

  DBUG_ENTER("innobase_update_foreign_try");
  DBUG_ASSERT(ctx);

  foreign_id = dict_table_get_highest_foreign_id(ctx->new_table);

  foreign_id++;

  for (i = 0; i < ctx->num_to_add_fk; i++) {
    dict_foreign_t *fk = ctx->add_fk[i];

    ut_ad(fk->foreign_table == ctx->new_table ||
          fk->foreign_table == ctx->old_table);

    dberr_t error = dict_create_add_foreign_id(&foreign_id,
                                               ctx->old_table->name.m_name, fk);

    if (error != DB_SUCCESS) {
      my_error(ER_TOO_LONG_IDENT, MYF(0), fk->id);
      DBUG_RETURN(true);
    }
    if (!fk->foreign_index) {
      fk->foreign_index = dict_foreign_find_index(
          ctx->new_table, ctx->col_names, fk->foreign_col_names, fk->n_fields,
          fk->referenced_index, TRUE,
          fk->type & (DICT_FOREIGN_ON_DELETE_SET_NULL |
                      DICT_FOREIGN_ON_UPDATE_SET_NULL));
      if (!fk->foreign_index) {
        my_error(ER_FK_INCORRECT_OPTION, MYF(0), table_name, fk->id);
        DBUG_RETURN(true);
      }
    }

    /* During upgrade, inserts into SYS_* should be avoided. */
    if (!srv_is_upgrade_mode) {
      DBUG_EXECUTE_IF("innodb_test_cannot_add_fk_system", error = DB_ERROR;);

      if (error != DB_SUCCESS) {
        my_error(ER_FK_FAIL_ADD_SYSTEM, MYF(0), fk->id);
        DBUG_RETURN(true);
      }
    }
  }
  DBUG_EXECUTE_IF("ib_drop_foreign_error",
                  my_error_innodb(DB_OUT_OF_FILE_SPACE, table_name, 0);
                  trx->error_state = DB_SUCCESS; DBUG_RETURN(true););
  DBUG_RETURN(false);
}

/** Update the foreign key constraint definitions in the data dictionary cache
after the changes to data dictionary tables were committed.
@param[in,out]	ctx		In-place ALTER TABLE context
@param[in]	user_thd	MySQL connection
@param[in,out]	dd_table	dd table instance
@return		InnoDB error code (should always be DB_SUCCESS) */
static MY_ATTRIBUTE((warn_unused_result)) dberr_t
    innobase_update_foreign_cache(ha_innobase_inplace_ctx *ctx, THD *user_thd,
                                  dd::Table *dd_table) {
  dict_table_t *user_table;
  dberr_t err = DB_SUCCESS;

  DBUG_ENTER("innobase_update_foreign_cache");

  ut_ad(mutex_own(&dict_sys->mutex));

  user_table = ctx->old_table;

  /* Discard the added foreign keys, because we will
  load them from the data dictionary. */
  for (ulint i = 0; i < ctx->num_to_add_fk; i++) {
    dict_foreign_t *fk = ctx->add_fk[i];
    dict_foreign_free(fk);
  }

  if (ctx->need_rebuild()) {
    /* The rebuilt table is already using the renamed
    column names. No need to pass col_names or to drop
    constraints from the data dictionary cache. */
    DBUG_ASSERT(!ctx->col_names);
    DBUG_ASSERT(user_table->foreign_set.empty());
    DBUG_ASSERT(user_table->referenced_set.empty());
    user_table = ctx->new_table;
  } else {
    /* Drop the foreign key constraints if the
    table was not rebuilt. If the table is rebuilt,
    there would not be any foreign key contraints for
    it yet in the data dictionary cache. */
    for (ulint i = 0; i < ctx->num_to_drop_fk; i++) {
      dict_foreign_t *fk = ctx->drop_fk[i];
      dict_foreign_remove_from_cache(fk);
    }
  }

  /* Load the old or added foreign keys from the data dictionary
  and prevent the table from being evicted from the data
  dictionary cache (work around the lack of WL#6049). */
  dict_names_t fk_tables;

  dd::cache::Dictionary_client *client = dd::get_dd_client(user_thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);
  err =
      dd_table_load_fk(client, user_table->name.m_name, ctx->col_names,
                       user_table, dd_table, user_thd, true, true, &fk_tables);

  if (err == DB_CANNOT_ADD_CONSTRAINT) {
    fk_tables.clear();

    /* It is possible there are existing foreign key are
    loaded with "foreign_key checks" off,
    so let's retry the loading with charset_check is off */
    err = dd_table_load_fk(client, user_table->name.m_name, ctx->col_names,
                           user_table, dd_table, user_thd, true, false,
                           &fk_tables);

    /* The load with "charset_check" off is successful, warn
    the user that the foreign key has loaded with mis-matched
    charset */
    if (err == DB_SUCCESS) {
      push_warning_printf(user_thd, Sql_condition::SL_WARNING, ER_ALTER_INFO,
                          "Foreign key constraints for table '%s'"
                          " are loaded with charset check off",
                          user_table->name.m_name);
    }
  }

  /* For complete loading of foreign keys, all associated tables must
  also be loaded. */

  while (err == DB_SUCCESS && !fk_tables.empty()) {
    mutex_exit(&dict_sys->mutex);
    dd::cache::Dictionary_client *client = dd::get_dd_client(user_thd);

    dd::cache::Dictionary_client::Auto_releaser releaser(client);

    dd_open_fk_tables(fk_tables, false, user_thd);
    mutex_enter(&dict_sys->mutex);
  }

  DBUG_RETURN(err);
}

/** Commit the changes made during prepare_inplace_alter_table()
and inplace_alter_table() inside the data dictionary tables,
when rebuilding the table.
@param ha_alter_info Data used during in-place alter
@param ctx In-place ALTER TABLE context
@param altered_table MySQL table that is being altered
@param old_table MySQL table as it is before the ALTER operation
@param trx Data dictionary transaction
@param table_name Table name in MySQL
@retval true Failure
@retval false Success
*/
inline MY_ATTRIBUTE((warn_unused_result)) bool commit_try_rebuild(
    Alter_inplace_info *ha_alter_info, ha_innobase_inplace_ctx *ctx,
    TABLE *altered_table, const TABLE *old_table, trx_t *trx,
    const char *table_name) {
  dict_table_t *rebuilt_table = ctx->new_table;
  dict_table_t *user_table = ctx->old_table;

  DBUG_ENTER("commit_try_rebuild");
  DBUG_ASSERT(ctx->need_rebuild());
  DBUG_ASSERT(trx->dict_operation_lock_mode == RW_X_LATCH);
  DBUG_ASSERT(
      !(ha_alter_info->handler_flags & Alter_inplace_info::DROP_FOREIGN_KEY) ||
      ctx->num_to_drop_fk > 0);

  for (dict_index_t *index = rebuilt_table->first_index(); index;
       index = index->next()) {
    DBUG_ASSERT(dict_index_get_online_status(index) == ONLINE_INDEX_COMPLETE);
    DBUG_ASSERT(index->is_committed());
    if (index->is_corrupted()) {
      my_error(ER_INDEX_CORRUPT, MYF(0), index->name());
      DBUG_RETURN(true);
    }
  }

  if (innobase_update_foreign_try(ctx, trx, table_name)) {
    DBUG_RETURN(true);
  }

  dberr_t error = DB_SUCCESS;

  /* Clear the to_be_dropped flag in the data dictionary cache
  of user_table. */
  for (ulint i = 0; i < ctx->num_to_drop_index; i++) {
    dict_index_t *index = ctx->drop_index[i];
    DBUG_ASSERT(index->table == user_table);
    DBUG_ASSERT(index->is_committed());
    DBUG_ASSERT(index->to_be_dropped);
    index->to_be_dropped = 0;
  }

  /* We copied the table. Any indexes that were requested to be
  dropped were not created in the copy of the table. Apply any
  last bit of the rebuild log and then rename the tables. */

  if (ctx->online) {
    DEBUG_SYNC_C("row_log_table_apply2_before");

    dict_vcol_templ_t *s_templ = NULL;

    if (ctx->new_table->n_v_cols > 0) {
      s_templ = UT_NEW_NOKEY(dict_vcol_templ_t());
      s_templ->vtempl = NULL;

      innobase_build_v_templ(altered_table, ctx->new_table, s_templ, NULL, true,
                             NULL);
      ctx->new_table->vc_templ = s_templ;
    }

    error = row_log_table_apply(
        ctx->thr, user_table, altered_table,
        static_cast<ha_innobase_inplace_ctx *>(ha_alter_info->handler_ctx)
            ->m_stage);

    if (s_templ) {
      ut_ad(ctx->need_rebuild());
      dict_free_vc_templ(s_templ);
      UT_DELETE(s_templ);
      ctx->new_table->vc_templ = NULL;
    }

    ulint err_key = thr_get_trx(ctx->thr)->error_key_num;

    switch (error) {
      KEY *dup_key;
      case DB_SUCCESS:
        break;
      case DB_DUPLICATE_KEY:
        if (err_key == ULINT_UNDEFINED) {
          /* This should be the hidden index on
          FTS_DOC_ID. */
          dup_key = NULL;
        } else {
          DBUG_ASSERT(err_key < ha_alter_info->key_count);
          dup_key = &ha_alter_info->key_info_buffer[err_key];
        }
        print_keydup_error(altered_table, dup_key, MYF(0));
        DBUG_RETURN(true);
      case DB_ONLINE_LOG_TOO_BIG:
        my_error(ER_INNODB_ONLINE_LOG_TOO_BIG, MYF(0),
                 get_error_key_name(err_key, ha_alter_info, rebuilt_table));
        DBUG_RETURN(true);
      case DB_INDEX_CORRUPT:
        my_error(ER_INDEX_CORRUPT, MYF(0),
                 get_error_key_name(err_key, ha_alter_info, rebuilt_table));
        DBUG_RETURN(true);
      default:
        my_error_innodb(error, table_name, user_table->flags);
        DBUG_RETURN(true);
    }
  }
  DBUG_EXECUTE_IF("ib_rename_column_error",
                  my_error_innodb(DB_OUT_OF_FILE_SPACE, table_name, 0);
                  trx->error_state = DB_SUCCESS; trx->op_info = "";
                  DBUG_RETURN(true););
  DBUG_EXECUTE_IF("ib_ddl_crash_before_rename", DBUG_SUICIDE(););

  /* The new table must inherit the flag from the
  "parent" table. */
  if (dict_table_is_discarded(user_table)) {
    rebuilt_table->ibd_file_missing = true;
    rebuilt_table->flags2 |= DICT_TF2_DISCARDED;
  }
  /* We must be still holding a table handle. */
  DBUG_ASSERT(user_table->get_ref_count() >= 1);

  DBUG_EXECUTE_IF("ib_ddl_crash_after_rename", DBUG_SUICIDE(););
  DBUG_EXECUTE_IF("ib_rebuild_cannot_rename", error = DB_ERROR;);

  if (user_table->get_ref_count() > 1) {
    /* This should only occur when an innodb_memcached
    connection with innodb_api_enable_mdl=off was started
    before commit_inplace_alter_table() locked the data
    dictionary. We must roll back the ALTER TABLE, because
    we cannot drop a table while it is being used. */

    /* Normally, n_ref_count must be 1, because purge
    cannot be executing on this very table as we are
    holding MDL lock. */
    my_error(ER_TABLE_REFERENCED, MYF(0));
    DBUG_RETURN(true);
  }

  switch (error) {
    case DB_SUCCESS:
      DBUG_RETURN(false);
    case DB_TABLESPACE_EXISTS:
      ut_a(rebuilt_table->get_ref_count() == 1);
      my_error(ER_TABLESPACE_EXISTS, MYF(0), ctx->tmp_name);
      DBUG_RETURN(true);
    case DB_DUPLICATE_KEY:
      ut_a(rebuilt_table->get_ref_count() == 1);
      my_error(ER_TABLE_EXISTS_ERROR, MYF(0), ctx->tmp_name);
      DBUG_RETURN(true);
    default:
      my_error_innodb(error, table_name, user_table->flags);
      DBUG_RETURN(true);
  }
}

/** Apply the changes made during commit_try_rebuild(),
to the data dictionary cache and the file system.
@param ctx In-place ALTER TABLE context */
inline void commit_cache_rebuild(ha_innobase_inplace_ctx *ctx) {
  dberr_t error;

  DBUG_ENTER("commit_cache_rebuild");
  DEBUG_SYNC_C("commit_cache_rebuild");
  DBUG_ASSERT(ctx->need_rebuild());
  DBUG_ASSERT(dict_table_is_discarded(ctx->old_table) ==
              dict_table_is_discarded(ctx->new_table));

  const char *old_name =
      mem_heap_strdup(ctx->heap, ctx->old_table->name.m_name);

  /* We already committed and redo logged the renames,
  so this must succeed. */
  error = dict_table_rename_in_cache(ctx->old_table, ctx->tmp_name, FALSE);
  ut_a(error == DB_SUCCESS);

  error = dict_table_rename_in_cache(ctx->new_table, old_name, FALSE);
  ut_a(error == DB_SUCCESS);

  DBUG_VOID_RETURN;
}

/** Set of column numbers */
typedef std::set<ulint, std::less<ulint>, ut_allocator<ulint>> col_set;

/** Store the column number of the columns in a list belonging
to indexes which are not being dropped.
@param[in]	ctx		In-place ALTER TABLE context
@param[in, out]	drop_col_list	list which will be set, containing columns
                                which is part of index being dropped
@param[in, out]	drop_v_col_list	list which will be set, containing
                                virtual columns which is part of index
                                being dropped */
static void get_col_list_to_be_dropped(const ha_innobase_inplace_ctx *ctx,
                                       col_set &drop_col_list,
                                       col_set &drop_v_col_list) {
  for (ulint index_count = 0; index_count < ctx->num_to_drop_index;
       index_count++) {
    const dict_index_t *index = ctx->drop_index[index_count];

    for (ulint col = 0; col < index->n_user_defined_cols; col++) {
      const dict_col_t *idx_col = index->get_col(col);

      if (idx_col->is_virtual()) {
        const dict_v_col_t *v_col =
            reinterpret_cast<const dict_v_col_t *>(idx_col);
        drop_v_col_list.insert(v_col->v_pos);

      } else {
        ulint col_no = dict_col_get_no(idx_col);
        drop_col_list.insert(col_no);
      }
    }
  }
}

/** Commit the changes made during prepare_inplace_alter_table() and
inplace_alter_table() inside the data dictionary tables, when not rebuilding
the table.
@param[in]	ha_alter_info	Data used during in-place alter
@param[in]	ctx		In-place ALTER TABLE context
@param[in]	altered_table	MySQL table that is being altered
@param[in]	old_table	MySQL table as it is before the ALTER operation
@param[in]	trx		Data dictionary transaction
@param[in]	table_name	Table name in MySQL
@retval true Failure
@retval false Success */
inline MY_ATTRIBUTE((warn_unused_result)) bool commit_try_norebuild(
    Alter_inplace_info *ha_alter_info, ha_innobase_inplace_ctx *ctx,
    TABLE *altered_table, const TABLE *old_table, trx_t *trx,
    const char *table_name) {
  DBUG_ENTER("commit_try_norebuild");
  DBUG_ASSERT(!ctx->need_rebuild());
  DBUG_ASSERT(trx->dict_operation_lock_mode == RW_X_LATCH);
  DBUG_ASSERT(
      !(ha_alter_info->handler_flags & Alter_inplace_info::DROP_FOREIGN_KEY) ||
      ctx->num_to_drop_fk > 0);
  DBUG_ASSERT(
      ctx->num_to_drop_fk == ha_alter_info->alter_info->drop_list.size() ||
      ctx->num_to_drop_vcol == ha_alter_info->alter_info->drop_list.size());

  for (ulint i = 0; i < ctx->num_to_add_index; i++) {
    dict_index_t *index = ctx->add_index[i];
    DBUG_ASSERT(dict_index_get_online_status(index) == ONLINE_INDEX_COMPLETE);
    DBUG_ASSERT(!index->is_committed());
    if (index->is_corrupted()) {
      /* Report a duplicate key
      error for the index that was
      flagged corrupted, most likely
      because a duplicate value was
      inserted (directly or by
      rollback) after
      ha_innobase::inplace_alter_table()
      completed.
      TODO: report this as a corruption
      with a detailed reason once
      WL#6379 has been implemented. */
      my_error(ER_DUP_UNKNOWN_IN_INDEX, MYF(0), index->name());
      DBUG_RETURN(true);
    }
  }

  if (innobase_update_foreign_try(ctx, trx, table_name)) {
    DBUG_RETURN(true);
  }

  DBUG_EXECUTE_IF("ib_rename_column_error",
                  my_error_innodb(DB_OUT_OF_FILE_SPACE, table_name, 0);
                  trx->error_state = DB_SUCCESS; trx->op_info = "";
                  DBUG_RETURN(true););

  DBUG_EXECUTE_IF("ib_resize_column_error",
                  my_error_innodb(DB_OUT_OF_FILE_SPACE, table_name, 0);
                  trx->error_state = DB_SUCCESS; trx->op_info = "";
                  DBUG_RETURN(true););

  DBUG_EXECUTE_IF(
      "ib_rename_index_fail1", my_error_innodb(DB_DEADLOCK, table_name, 0);
      trx->error_state = DB_SUCCESS; trx->op_info = ""; DBUG_RETURN(true););

  DBUG_RETURN(false);
}

/** Commit the changes to the data dictionary cache
after a successful commit_try_norebuild() call.
@param ctx In-place ALTER TABLE context
@param table the TABLE before the ALTER
@param trx Data dictionary transaction object
(will be started and committed)
@return whether all replacements were found for dropped indexes */
inline MY_ATTRIBUTE((warn_unused_result)) bool commit_cache_norebuild(
    ha_innobase_inplace_ctx *ctx, const TABLE *table, trx_t *trx) {
  DBUG_ENTER("commit_cache_norebuild");

  bool found = true;

  DBUG_ASSERT(!ctx->need_rebuild());

  col_set drop_list;
  col_set v_drop_list;
  col_set::const_iterator col_it;

  /* Check if the column, part of an index to be dropped is part of any
  other index which is not being dropped. If it so, then set the ord_part
  of the column to 0. */
  get_col_list_to_be_dropped(ctx, drop_list, v_drop_list);

  for (col_it = drop_list.begin(); col_it != drop_list.end(); ++col_it) {
    if (!check_col_exists_in_indexes(ctx->new_table, *col_it, false)) {
      ctx->new_table->cols[*col_it].ord_part = 0;
    }
  }

  for (col_it = v_drop_list.begin(); col_it != v_drop_list.end(); ++col_it) {
    if (!check_col_exists_in_indexes(ctx->new_table, *col_it, true)) {
      ctx->new_table->v_cols[*col_it].m_col.ord_part = 0;
    }
  }

  for (ulint i = 0; i < ctx->num_to_add_index; i++) {
    dict_index_t *index = ctx->add_index[i];
    DBUG_ASSERT(dict_index_get_online_status(index) == ONLINE_INDEX_COMPLETE);
    DBUG_ASSERT(!index->is_committed());
    index->set_committed(true);
  }

  if (ctx->num_to_drop_index) {
    /* Drop indexes in data dictionary cache and write
    DDL log for them */
    for (ulint i = 0; i < ctx->num_to_drop_index; i++) {
      dict_index_t *index = ctx->drop_index[i];
      DBUG_ASSERT(index->is_committed());
      DBUG_ASSERT(index->table == ctx->new_table);
      DBUG_ASSERT(index->to_be_dropped);

      /* Replace the indexes in foreign key
      constraints if needed. */
      if (!dict_foreign_replace_index(index->table, ctx->col_names, index)) {
        found = false;
      }
    }

    for (ulint i = 0; i < ctx->num_to_drop_index; i++) {
      dict_index_t *index = ctx->drop_index[i];
      DBUG_ASSERT(index->is_committed());
      DBUG_ASSERT(index->table == ctx->new_table);

      if (index->type & DICT_FTS) {
        DBUG_ASSERT(index->type == DICT_FTS || index->is_corrupted());
        DBUG_ASSERT(index->table->fts);
        ctx->fts_drop_aux_vec = new aux_name_vec_t;
        fts_drop_index(index->table, index, trx, ctx->fts_drop_aux_vec);
      }

      /* It is a single table tablespace and the .ibd file is
      missing if root is FIL_NULL, do nothing. */
      if (index->page != FIL_NULL) {
        mutex_exit(&dict_sys->mutex);
        log_ddl->write_free_tree_log(trx, index, true);
        mutex_enter(&dict_sys->mutex);
      }

      btr_drop_ahi_for_index(index);
      dict_index_remove_from_cache(index->table, index);
    }
  }

  ctx->new_table->fts_doc_id_index =
      ctx->new_table->fts
          ? dict_table_get_index_on_name(ctx->new_table, FTS_DOC_ID_INDEX_NAME)
          : NULL;
  DBUG_ASSERT((ctx->new_table->fts == NULL) ==
              (ctx->new_table->fts_doc_id_index == NULL));

  DBUG_RETURN(found);
}

/** Adjust the persistent statistics after non-rebuilding ALTER TABLE.
Remove statistics for dropped indexes, add statistics for created indexes
and rename statistics for renamed indexes.
@param ha_alter_info Data used during in-place alter
@param ctx In-place ALTER TABLE context
@param altered_table MySQL table that is being altered
@param table_name Table name in MySQL
@param thd MySQL connection
*/
static void alter_stats_norebuild(Alter_inplace_info *ha_alter_info,
                                  ha_innobase_inplace_ctx *ctx,
                                  TABLE *altered_table, const char *table_name,
                                  THD *thd) {
  ulint i;

  DBUG_ENTER("alter_stats_norebuild");
  DBUG_ASSERT(!ctx->need_rebuild());

  if (!dict_stats_is_persistent_enabled(ctx->new_table)) {
    DBUG_VOID_RETURN;
  }

  /* Delete corresponding rows from the stats table. We do this
  in a separate transaction from trx, because lock waits are not
  allowed in a data dictionary transaction. (Lock waits are possible
  on the statistics table, because it is directly accessible by users,
  not covered by the dict_operation_lock.)

  Because the data dictionary changes were already committed, orphaned
  rows may be left in the statistics table if the system crashes.

  FIXME: each change to the statistics tables is being committed in a
  separate transaction, meaning that the operation is not atomic

  FIXME: This will not drop the (unused) statistics for
  FTS_DOC_ID_INDEX if it was a hidden index, dropped together
  with the last renamining FULLTEXT index. */
  for (i = 0; i < ha_alter_info->index_drop_count; i++) {
    const KEY *key = ha_alter_info->index_drop_buffer[i];

    if (key->flags & HA_FULLTEXT) {
      /* There are no index cardinality
      statistics for FULLTEXT indexes. */
      continue;
    }

    char errstr[1024];

    if (dict_stats_drop_index(ctx->new_table->name.m_name, key->name, errstr,
                              sizeof errstr) != DB_SUCCESS) {
      push_warning(thd, Sql_condition::SL_WARNING, ER_LOCK_WAIT_TIMEOUT,
                   errstr);
    }
  }

  for (i = 0; i < ha_alter_info->index_rename_count; i++) {
    KEY_PAIR *pair = &ha_alter_info->index_rename_buffer[i];
    dberr_t err;

    err = dict_stats_rename_index(ctx->new_table, pair->old_key->name,
                                  pair->new_key->name);

    if (err != DB_SUCCESS) {
      push_warning_printf(thd, Sql_condition::SL_WARNING, ER_ERROR_ON_RENAME,
                          "Error renaming an index of table '%s'"
                          " from '%s' to '%s' in InnoDB persistent"
                          " statistics storage: %s",
                          table_name, pair->old_key->name, pair->new_key->name,
                          ut_strerr(err));
    }
  }

  for (i = 0; i < ctx->num_to_add_index; i++) {
    dict_index_t *index = ctx->add_index[i];
    DBUG_ASSERT(index->table == ctx->new_table);

    if (!(index->type & DICT_FTS)) {
      dict_stats_init(ctx->new_table);
      dict_stats_update_for_index(index);
    }
  }

  DBUG_VOID_RETURN;
}

/** Adjust the persistent statistics after rebuilding ALTER TABLE.
Remove statistics for dropped indexes, add statistics for created indexes
and rename statistics for renamed indexes.
@param table InnoDB table that was rebuilt by ALTER TABLE
@param table_name Table name in MySQL
@param thd MySQL connection
*/
static void alter_stats_rebuild(dict_table_t *table, const char *table_name,
                                THD *thd) {
  DBUG_ENTER("alter_stats_rebuild");
  DBUG_EXECUTE_IF("ib_ddl_crash_before_rename", DBUG_SUICIDE(););

  if (dict_table_is_discarded(table) ||
      !dict_stats_is_persistent_enabled(table)) {
    DBUG_VOID_RETURN;
  }

#ifdef UNIV_DEBUG
  bool ibd_file_missing_orig = false;
#endif /* UNIV_DEBUG */

  DBUG_EXECUTE_IF("ib_rename_index_fail2",
                  ibd_file_missing_orig = table->ibd_file_missing;
                  table->ibd_file_missing = TRUE;);

  dberr_t ret = dict_stats_update(table, DICT_STATS_RECALC_PERSISTENT);

  DBUG_EXECUTE_IF("ib_rename_index_fail2",
                  table->ibd_file_missing = ibd_file_missing_orig;);

  if (ret != DB_SUCCESS) {
    push_warning_printf(thd, Sql_condition::SL_WARNING, ER_ALTER_INFO,
                        "Error updating stats for table '%s'"
                        " after table rebuild: %s",
                        table_name, ut_strerr(ret));
  }

  DBUG_VOID_RETURN;
}

/** Implementation of commit_inplace_alter_table()
@tparam		Table		dd::Table or dd::Partition
@param[in]	altered_table	TABLE object for new version of table.
@param[in,out]	ha_alter_info	Structure describing changes to be done
                                by ALTER TABLE and holding data used
                                during in-place alter.
@param[in]	commit		true => Commit, false => Rollback.
@param[in]	old_dd_tab	dd::Table object describing old version
                                of the table.
@param[in,out]	new_dd_tab	dd::Table object for the new version of the
                                table. Can be adjusted by this call.
                                Changes to the table definition will be
                                persisted in the data-dictionary at statement
                                commit time.
@retval true Failure
@retval false Success
*/
template <typename Table>
bool ha_innobase::commit_inplace_alter_table_impl(
    TABLE *altered_table, Alter_inplace_info *ha_alter_info, bool commit,
    const Table *old_dd_tab, Table *new_dd_tab) {
  dberr_t error;
  ha_innobase_inplace_ctx *ctx0;
  struct mtr_buf_copy_t logs;

  ctx0 = static_cast<ha_innobase_inplace_ctx *>(ha_alter_info->handler_ctx);

#ifdef UNIV_DEBUG
  uint crash_inject_count = 1;
  uint crash_fail_inject_count = 1;
  uint failure_inject_count = 1;
#endif /* UNIV_DEBUG */

  DBUG_ENTER("ha_innobase::commit_inplace_alter_table_impl");
  DBUG_ASSERT(!srv_read_only_mode);
  DBUG_ASSERT(!ctx0 || ctx0->prebuilt == m_prebuilt);
  DBUG_ASSERT(!ctx0 || ctx0->old_table == m_prebuilt->table);

  DEBUG_SYNC_C("innodb_commit_inplace_alter_table_enter");

  DEBUG_SYNC_C("innodb_commit_inplace_alter_table_wait");

  if (ctx0 != NULL && ctx0->m_stage != NULL) {
    ctx0->m_stage->begin_phase_end();
  }

  if (!commit) {
    /* A rollback is being requested. So far we may at
    most have created some indexes. If any indexes were to
    be dropped, they would actually be dropped in this
    method if commit=true. */
    const bool ret =
        rollback_inplace_alter_table(ha_alter_info, table, m_prebuilt);
    DBUG_RETURN(ret);
  }

  if (!(ha_alter_info->handler_flags & ~INNOBASE_INPLACE_IGNORE) ||
      is_instant(ha_alter_info)) {
    DBUG_ASSERT(!ctx0);
    MONITOR_ATOMIC_DEC(MONITOR_PENDING_ALTER_TABLE);
    ha_alter_info->group_commit_ctx = NULL;
    DBUG_RETURN(false);
  }

  DBUG_ASSERT(ctx0);

  inplace_alter_handler_ctx **ctx_array;
  inplace_alter_handler_ctx *ctx_single[2];

  if (ha_alter_info->group_commit_ctx) {
    ctx_array = ha_alter_info->group_commit_ctx;
  } else {
    ctx_single[0] = ctx0;
    ctx_single[1] = NULL;
    ctx_array = ctx_single;
  }

  DBUG_ASSERT(ctx0 == ctx_array[0]);
  ut_ad(m_prebuilt->table == ctx0->old_table);
  ha_alter_info->group_commit_ctx = NULL;

  trx_start_if_not_started_xa(m_prebuilt->trx, true);

  for (inplace_alter_handler_ctx **pctx = ctx_array; *pctx; pctx++) {
    ha_innobase_inplace_ctx *ctx =
        static_cast<ha_innobase_inplace_ctx *>(*pctx);
    DBUG_ASSERT(ctx->prebuilt->trx == m_prebuilt->trx);

    /* Exclusively lock the table, to ensure that no other
    transaction is holding locks on the table while we
    change the table definition. The MySQL meta-data lock
    should normally guarantee that no conflicting locks
    exist. However, FOREIGN KEY constraints checks and any
    transactions collected during crash recovery could be
    holding InnoDB locks only, not MySQL locks. */

    error = row_merge_lock_table(m_prebuilt->trx, ctx->old_table, LOCK_X);

    if (error != DB_SUCCESS) {
      my_error_innodb(error, table_share->table_name.str, 0);
      DBUG_RETURN(true);
    }
  }

  DEBUG_SYNC(m_user_thd, "innodb_alter_commit_after_lock_table");

  const bool new_clustered = ctx0->need_rebuild();
  trx_t *trx = ctx0->trx;
  bool fail = false;

  if (new_clustered) {
    for (inplace_alter_handler_ctx **pctx = ctx_array; *pctx; pctx++) {
      ha_innobase_inplace_ctx *ctx =
          static_cast<ha_innobase_inplace_ctx *>(*pctx);
      DBUG_ASSERT(ctx->need_rebuild());

      if (ctx->old_table->fts) {
        ut_ad(!ctx->old_table->fts->add_wq);
        fts_optimize_remove_table(ctx->old_table);
      }

      if (ctx->new_table->fts) {
        ut_ad(!ctx->new_table->fts->add_wq);
        fts_optimize_remove_table(ctx->new_table);
      }
    }
  }

  if (trx == nullptr) {
    trx = m_prebuilt->trx;
    ctx0->trx = trx;
    DBUG_ASSERT(!new_clustered);
  }

  /* Generate the temporary name for old table, and acquire mdl
  lock on it. */
  THD *thd = current_thd;
  for (inplace_alter_handler_ctx **pctx = ctx_array; *pctx && !fail; pctx++) {
    ha_innobase_inplace_ctx *ctx =
        static_cast<ha_innobase_inplace_ctx *>(*pctx);

    if (ctx->need_rebuild()) {
      char db_buf[NAME_LEN + 1];
      char tbl_buf[NAME_LEN + 1];
      MDL_ticket *mdl_ticket = NULL;

      ctx->tmp_name = dict_mem_create_temporary_tablename(
          ctx->heap, ctx->new_table->name.m_name, ctx->new_table->id);

      /* Acquire mdl lock on the temporary table name. */
      dd_parse_tbl_name(ctx->tmp_name, db_buf, tbl_buf, nullptr, nullptr,
                        nullptr);

      if (dd::acquire_exclusive_table_mdl(thd, db_buf, tbl_buf, false,
                                          &mdl_ticket)) {
        DBUG_RETURN(true);
      }
    }
  }

  /* Latch the InnoDB data dictionary exclusively so that no deadlocks
  or lock waits can happen in it during the data dictionary operation. */
  row_mysql_lock_data_dictionary(trx);

  /* Prevent the background statistics collection from accessing
  the tables. */
  for (;;) {
    bool retry = false;

    for (inplace_alter_handler_ctx **pctx = ctx_array; *pctx; pctx++) {
      ha_innobase_inplace_ctx *ctx =
          static_cast<ha_innobase_inplace_ctx *>(*pctx);

      DBUG_ASSERT(new_clustered == ctx->need_rebuild());

      if (new_clustered && !dict_stats_stop_bg(ctx->old_table)) {
        retry = true;
      }

      if (!dict_stats_stop_bg(ctx->new_table)) {
        retry = true;
      }
    }

    if (!retry) {
      break;
    }

    DICT_STATS_BG_YIELD(trx);
  }

  /* Apply the changes to the data dictionary tables, for all
  partitions. */

  for (inplace_alter_handler_ctx **pctx = ctx_array; *pctx && !fail; pctx++) {
    ha_innobase_inplace_ctx *ctx =
        static_cast<ha_innobase_inplace_ctx *>(*pctx);

    DBUG_ASSERT(new_clustered == ctx->need_rebuild());

    if (commit_get_autoinc(ha_alter_info, ctx, altered_table, table)) {
      fail = true;
      my_error(ER_TABLESPACE_DISCARDED, MYF(0), table->s->table_name.str);
      goto rollback_trx;
    }

    if (ctx->need_rebuild()) {
      fail = commit_try_rebuild(ha_alter_info, ctx, altered_table, table, trx,
                                table_share->table_name.str);

      if (!fail) {
        log_ddl->write_drop_log(trx, ctx->old_table->id);
      }
    } else {
      fail = commit_try_norebuild(ha_alter_info, ctx, altered_table, table, trx,
                                  table_share->table_name.str);
    }
    DBUG_INJECT_CRASH("ib_commit_inplace_crash", crash_inject_count++);
#ifdef UNIV_DEBUG
    {
      /* Generate a dynamic dbug text. */
      char buf[32];

      snprintf(buf, sizeof buf, "ib_commit_inplace_fail_%u",
               failure_inject_count++);

      DBUG_EXECUTE_IF(buf,
                      my_error(ER_INTERNAL_ERROR, MYF(0), "Injected error!");
                      fail = true;);
    }
#endif
  }

rollback_trx:

  /* Commit or roll back the changes to the data dictionary. */

  if (!fail && new_clustered) {
    for (inplace_alter_handler_ctx **pctx = ctx_array; *pctx; pctx++) {
      ha_innobase_inplace_ctx *ctx =
          static_cast<ha_innobase_inplace_ctx *>(*pctx);

      DBUG_ASSERT(ctx->need_rebuild());
      /* Check for any possible problems for any
      file operations that will be performed in
      commit_cache_rebuild(). */
      error =
          fil_rename_precheck(ctx->old_table, ctx->new_table, ctx->tmp_name);
      if (error != DB_SUCCESS) {
        /* Out of memory or a problem will occur
        when renaming files. */
        fail = true;
        my_error_innodb(error, ctx->old_table->name.m_name,
                        ctx->old_table->flags);
      }
      DBUG_INJECT_CRASH("ib_commit_inplace_crash", crash_inject_count++);
    }

    /* Test what happens on crash here.
    The data dictionary transaction should be
    rolled back, restoring the old table. */
    DBUG_EXECUTE_IF("innodb_alter_commit_crash_before_commit",
                    log_buffer_flush_to_disk();
                    DBUG_SUICIDE(););
    ut_ad(!trx->fts_trx);

    DBUG_EXECUTE_IF("innodb_alter_commit_crash_after_commit",
                    log_make_latest_checkpoint();
                    log_buffer_flush_to_disk(); DBUG_SUICIDE(););
  }

  /* Update the in-memory structures, close some handles, release
  temporary files, and (unless we rolled back) update persistent
  statistics. */
  for (inplace_alter_handler_ctx **pctx = ctx_array; *pctx; pctx++) {
    ha_innobase_inplace_ctx *ctx =
        static_cast<ha_innobase_inplace_ctx *>(*pctx);

    DBUG_ASSERT(ctx->need_rebuild() == new_clustered);

    if (new_clustered) {
      innobase_online_rebuild_log_free(ctx->old_table);
    }

    if (fail) {
      if (new_clustered) {
        dict_table_close(ctx->new_table, TRUE, FALSE);
        ctx->new_table = NULL;
      } else {
        /* We failed, but did not rebuild the table.
        Roll back any ADD INDEX, or get rid of garbage
        ADD INDEX that was left over from a previous
        ALTER TABLE statement. */
        innobase_rollback_sec_index(ctx->new_table, table, TRUE, trx);
      }
      DBUG_INJECT_CRASH("ib_commit_inplace_crash_fail",
                        crash_fail_inject_count++);

      continue;
    }

    innobase_copy_frm_flags_from_table_share(ctx->new_table, altered_table->s);

    if (new_clustered) {
      /* We will reload and refresh the
      in-memory foreign key constraint
      metadata. This is a rename operation
      in preparing for dropping the old
      table. Set the table to_be_dropped bit
      here, so to make sure DML foreign key
      constraint check does not use the
      stale dict_foreign_t. This is done
      because WL#6049 (FK MDL) has not been
      implemented yet. */
      ctx->old_table->to_be_dropped = true;

      DBUG_PRINT("to_be_dropped", ("table: %s", ctx->old_table->name.m_name));

      /* Rename the tablespace files. */
      commit_cache_rebuild(ctx);

      /* Discard the added foreign keys, because we will
      load them from the data dictionary. */
      for (ulint i = 0; i < ctx->num_to_add_fk; i++) {
        dict_foreign_t *fk = ctx->add_fk[i];
        dict_foreign_free(fk);
      }

      /* There is no FK on partition table */
      if (m_share) {
        ctx->new_table->discard_after_ddl = true;
      }
    } else {
      error =
          innobase_update_foreign_cache(ctx, m_user_thd, &new_dd_tab->table());

      if (error != DB_SUCCESS) {
        /* The data dictionary cache
        should be corrupted now.  The
        best solution should be to
        kill and restart the server,
        but the *.frm file has not
        been replaced yet. */
        push_warning_printf(m_user_thd, Sql_condition::SL_WARNING,
                            ER_ALTER_INFO,
                            "InnoDB: Could not add foreign"
                            " key constraints.");
      } else {
        if (!commit_cache_norebuild(ctx, table, trx)) {
          ut_a(!m_prebuilt->trx->check_foreigns);
        }

        innobase_rename_or_enlarge_columns_cache(ha_alter_info, table,
                                                 ctx->new_table);

        rename_indexes_in_cache(ctx, ha_alter_info);
      }
    }

    dict_mem_table_free_foreign_vcol_set(ctx->new_table);
    dict_mem_table_fill_foreign_vcol_set(ctx->new_table);

    DBUG_INJECT_CRASH("ib_commit_inplace_crash", crash_inject_count++);
  }

  /* Invalidate the index translation table. In partitioned
  tables, there is no share. */
  if (m_share) {
    m_share->idx_trans_tbl.index_count = 0;
  }

  /* Tell the InnoDB server that there might be work for
  utility threads: */

  srv_active_wake_master_thread();

  if (fail) {
    for (inplace_alter_handler_ctx **pctx = ctx_array; *pctx; pctx++) {
      ha_innobase_inplace_ctx *ctx =
          static_cast<ha_innobase_inplace_ctx *>(*pctx);
      DBUG_ASSERT(ctx->need_rebuild() == new_clustered);

      ut_d(dict_table_check_for_dup_indexes(ctx->old_table, CHECK_ABORTED_OK));
      ut_a(fts_check_cached_index(ctx->old_table));
      DBUG_INJECT_CRASH("ib_commit_inplace_crash_fail",
                        crash_fail_inject_count++);
    }

    row_mysql_unlock_data_dictionary(trx);
    DBUG_RETURN(true);
  }

  if (ctx0->num_to_drop_vcol || ctx0->num_to_add_vcol) {
    if (ctx0->old_table->get_ref_count() > 1) {
      row_mysql_unlock_data_dictionary(trx);
      my_error(ER_TABLE_REFERENCED, MYF(0));
      DBUG_RETURN(true);
    }

    for (inplace_alter_handler_ctx **pctx = ctx_array; *pctx; pctx++) {
      ha_innobase_inplace_ctx *ctx =
          static_cast<ha_innobase_inplace_ctx *>(*pctx);

      /* Drop outdated table stats. */
      innobase_discard_table(m_user_thd, ctx->old_table);
    }

    row_mysql_unlock_data_dictionary(trx);
    MONITOR_ATOMIC_DEC(MONITOR_PENDING_ALTER_TABLE);
    DBUG_RETURN(false);
  }

  DBUG_EXECUTE_IF("ib_ddl_crash_after_user_trx_commit", DBUG_SUICIDE(););

  uint64 autoinc = 0;
  for (inplace_alter_handler_ctx **pctx = ctx_array; *pctx; pctx++) {
    ha_innobase_inplace_ctx *ctx =
        static_cast<ha_innobase_inplace_ctx *>(*pctx);
    DBUG_ASSERT(ctx->need_rebuild() == new_clustered);

    if (altered_table->found_next_number_field) {
      if (ctx->max_autoinc > autoinc) {
        autoinc = ctx->max_autoinc;
      }

      dict_table_t *t = ctx->new_table;
      Field *field = altered_table->found_next_number_field;

      dict_table_autoinc_lock(t);
      dict_table_autoinc_initialize(t, ctx->max_autoinc);
      t->autoinc_persisted = ctx->max_autoinc - 1;
      dict_table_autoinc_set_col_pos(t, field->field_index);
      dict_table_autoinc_unlock(t);
    }

    bool add_fts = false;

    /* Publish the created fulltext index, if any.
    Note that a fulltext index can be created without
    creating the clustered index, if there already exists
    a suitable FTS_DOC_ID column. If not, one will be
    created, implying new_clustered */
    for (ulint i = 0; i < ctx->num_to_add_index; i++) {
      dict_index_t *index = ctx->add_index[i];

      if (index->type & DICT_FTS) {
        DBUG_ASSERT(index->type == DICT_FTS);
        /* We reset DICT_TF2_FTS here because the bit
        is left unset when a drop proceeds the add. */
        DICT_TF2_FLAG_SET(ctx->new_table, DICT_TF2_FTS);
        fts_add_index(index, ctx->new_table);
        add_fts = true;
      }
    }

    ut_d(dict_table_check_for_dup_indexes(ctx->new_table, CHECK_ALL_COMPLETE));

    if (add_fts && !ctx->new_table->discard_after_ddl) {
      fts_optimize_add_table(ctx->new_table);
    }

    ut_d(dict_table_check_for_dup_indexes(ctx->new_table, CHECK_ABORTED_OK));
    ut_a(fts_check_cached_index(ctx->new_table));

    if (new_clustered) {
      /* Since the table has been rebuilt, we remove
      all persistent statistics corresponding to the
      old copy of the table (which was renamed to
      ctx->tmp_name). */

      char errstr[1024];

      DBUG_ASSERT(0 == strcmp(ctx->old_table->name.m_name, ctx->tmp_name));

      DBUG_EXECUTE_IF("ib_rename_index_fail3",
                      DBUG_SET("+d,innodb_report_deadlock"););

      if (dict_stats_drop_table(ctx->new_table->name.m_name, errstr,
                                sizeof(errstr)) != DB_SUCCESS) {
        push_warning_printf(m_user_thd, Sql_condition::SL_WARNING,
                            ER_ALTER_INFO,
                            "Deleting persistent statistics"
                            " for rebuilt table '%s' in"
                            " InnoDB failed: %s",
                            table->s->table_name.str, errstr);
      }

      DBUG_EXECUTE_IF("ib_rename_index_fail3",
                      DBUG_SET("-d,innodb_report_deadlock"););

      DBUG_EXECUTE_IF("ib_ddl_crash_before_commit", DBUG_SUICIDE(););

      ut_ad(m_prebuilt != ctx->prebuilt || ctx == ctx0);
      bool update_own_prebuilt = (m_prebuilt == ctx->prebuilt);
      trx_t *const user_trx = m_prebuilt->trx;

      row_prebuilt_free(ctx->prebuilt, TRUE);

      /* Drop the copy of the old table, which was
      renamed to ctx->tmp_name at the atomic DDL
      transaction commit.  If the system crashes
      before this is completed, some orphan tables
      with ctx->tmp_name may be recovered. */
      row_merge_drop_table(trx, ctx->old_table);

      /* Rebuild the prebuilt object. */
      ctx->prebuilt =
          row_create_prebuilt(ctx->new_table, altered_table->s->reclength);
      if (update_own_prebuilt) {
        m_prebuilt = ctx->prebuilt;
      }
      user_trx->will_lock++;
      m_prebuilt->trx = user_trx;
    }
    DBUG_INJECT_CRASH("ib_commit_inplace_crash", crash_inject_count++);
  }

  row_mysql_unlock_data_dictionary(trx);

  if (altered_table->found_next_number_field != NULL) {
    dd_set_autoinc(new_dd_tab->se_private_data(), autoinc);
  }

  DBUG_EXECUTE_IF("ib_ddl_crash_before_update_stats", DBUG_SUICIDE(););

  /* TODO: The following code could be executed
  while allowing concurrent access to the table
  (MDL downgrade). */

  if (new_clustered) {
    for (inplace_alter_handler_ctx **pctx = ctx_array; *pctx; pctx++) {
      ha_innobase_inplace_ctx *ctx =
          static_cast<ha_innobase_inplace_ctx *>(*pctx);
      DBUG_ASSERT(ctx->need_rebuild());

      alter_stats_rebuild(ctx->new_table, table->s->table_name.str, m_user_thd);
      DBUG_INJECT_CRASH("ib_commit_inplace_crash", crash_inject_count++);
    }
  } else {
    for (inplace_alter_handler_ctx **pctx = ctx_array; *pctx; pctx++) {
      ha_innobase_inplace_ctx *ctx =
          static_cast<ha_innobase_inplace_ctx *>(*pctx);
      DBUG_ASSERT(!ctx->need_rebuild());

      alter_stats_norebuild(ha_alter_info, ctx, altered_table,
                            table->s->table_name.str, m_user_thd);
      DBUG_INJECT_CRASH("ib_commit_inplace_crash", crash_inject_count++);

      if (ctx->fts_drop_aux_vec != nullptr &&
          ctx->fts_drop_aux_vec->aux_name.size() > 0) {
        fts_drop_dd_tables(ctx->fts_drop_aux_vec,
                           dict_table_is_file_per_table(ctx->old_table));
      }
    }
  }

  /* We don't support compression for the system tablespace nor
  the temporary tablespace. Only because they are shared tablespaces.
  There is no other technical reason. */

  innobase_parse_hint_from_comment(m_user_thd, m_prebuilt->table,
                                   altered_table->s);

  /* TODO: Also perform DROP TABLE and DROP INDEX after
  the MDL downgrade. */

#ifdef UNIV_DEBUG
  dict_index_t *clust_index = ctx0->prebuilt->table->first_index();
  DBUG_ASSERT(!clust_index->online_log);
  DBUG_ASSERT(dict_index_get_online_status(clust_index) ==
              ONLINE_INDEX_COMPLETE);

  for (dict_index_t *index = clust_index; index; index = index->next()) {
    DBUG_ASSERT(!index->to_be_dropped);
  }
#endif /* UNIV_DEBUG */
  MONITOR_ATOMIC_DEC(MONITOR_PENDING_ALTER_TABLE);
  DBUG_RETURN(false);
}

/** Helper class for in-place alter partitioned table, see handler.h */
class ha_innopart_inplace_ctx : public inplace_alter_handler_ctx {
  /* Only used locally in this file, so have everything public for
  conveniance. */
 public:
  /** Total number of partitions. */
  uint m_tot_parts;
  /** Array of inplace contexts for all partitions. */
  inplace_alter_handler_ctx **ctx_array;
  /** Array of prebuilt for all partitions. */
  row_prebuilt_t **prebuilt_array;
  /** Array of old table information needed for writing back to DD */
  alter_table_old_info_t *m_old_info;

  ha_innopart_inplace_ctx(THD *thd, uint tot_parts)
      : inplace_alter_handler_ctx(),
        m_tot_parts(tot_parts),
        ctx_array(),
        prebuilt_array(),
        m_old_info() {}

  ~ha_innopart_inplace_ctx() {
    if (ctx_array) {
      for (uint i = 0; i < m_tot_parts; i++) {
        destroy(ctx_array[i]);
      }
      ut_free(ctx_array);
    }

    if (m_old_info != nullptr) {
      ut_free(m_old_info);
    }

    if (prebuilt_array) {
      /* First entry is the original prebuilt! */
      for (uint i = 1; i < m_tot_parts; i++) {
        /* Don't close the tables. */
        prebuilt_array[i]->table = nullptr;
        row_prebuilt_free(prebuilt_array[i], false);
      }
      ut_free(prebuilt_array);
    }
  }
};

/** Helper class for encapsulating new/altered partitions during
ADD(HASH/KEY)/COALESCE/REORGANIZE PARTITION. Here as many partition slots
as in new table would be created, it's OK for ADD/COALESCE PARTITION,
however more partition slots would probably be created for REORGANIZE PARTITION.
Considering that it's easy to get table in this way, it's still OK. */
class Altered_partitions {
 public:
  /** Constructor
  @param[in]	parts	total partitions */
  Altered_partitions(uint parts)
      : m_new_table_parts(),
        m_ins_nodes(),
        m_sql_stat_start(),
        m_trx_ids(),
        m_num_new_parts(parts) {}

  /** Destructor */
  ~Altered_partitions();

  /** Initialize the object.
  @return	false	on success
  @retval	true	on failure */
  bool initialize();

  /** Open and set currently used partition.
  @param[in]	new_part_id	Partition id to set.
  @param[in,out]	part		Internal table object to use. */
  void set_part(ulint new_part_id, dict_table_t *part) {
    ut_ad(m_new_table_parts[new_part_id] == nullptr);
    m_new_table_parts[new_part_id] = part;
    part->skip_alter_undo = true;
    m_sql_stat_start.set(new_part_id);
  }

  /** Get lower level internal table object for partition.
  @param[in]	part_id	 Partition id.
  @return Lower level internal table object for the partition id. */
  dict_table_t *part(uint part_id) {
    ut_ad(part_id < m_num_new_parts);
    return (m_new_table_parts[part_id]);
  }

  /** To write a row, set up prebuilt for using a specified partition.
  @param[in,out]	prebuilt	Prebuilt to update.
  @param[in]	new_part_id	Partition to use. */
  void prepare_write(row_prebuilt_t *prebuilt, uint new_part_id) const {
    ut_ad(m_new_table_parts[new_part_id]);
    prebuilt->table = m_new_table_parts[new_part_id];
    prebuilt->ins_node = m_ins_nodes[new_part_id];
    prebuilt->trx_id = m_trx_ids[new_part_id];
    prebuilt->sql_stat_start = m_sql_stat_start.test(new_part_id);
  }

  /** After a write, update cached values for a partition from prebuilt.
  @param[in,out]	prebuilt	Prebuilt to copy from.
  @param[in]	new_part_id	Partition id to copy. */
  void finish_write(row_prebuilt_t *prebuilt, uint new_part_id) {
    ut_ad(m_new_table_parts[new_part_id] == prebuilt->table);
    m_ins_nodes[new_part_id] = prebuilt->ins_node;
    m_trx_ids[new_part_id] = prebuilt->trx_id;
    if (!prebuilt->sql_stat_start) {
      m_sql_stat_start.set(new_part_id, 0);
    }
  }

 private:
  /** New partitions created during ADD(HASH/KEY)/COALESCE/REORGANIZE
  PARTITION. */
  dict_table_t **m_new_table_parts;

  /** Insert nodes per partition. */
  ins_node_t **m_ins_nodes;

  /** bytes for sql_stat_start bitset */
  byte *m_bitset;

  /** sql_stat_start per partition */
  Sql_stat_start_parts m_sql_stat_start;

  /** Trx id per partition. */
  trx_id_t *m_trx_ids;

  /** Number of new partitions. */
  size_t m_num_new_parts;
};

/** Destructor */
Altered_partitions::~Altered_partitions() {
  if (m_new_table_parts != nullptr) {
    for (ulint i = 0; i < m_num_new_parts; i++) {
      if (m_new_table_parts[i] != nullptr) {
        m_new_table_parts[i]->skip_alter_undo = false;
      }
    }

    ut_free(m_new_table_parts);
  }

  if (m_ins_nodes != nullptr) {
    for (ulint i = 0; i < m_num_new_parts; i++) {
      if (m_ins_nodes[i] != nullptr) {
        ins_node_t *ins = m_ins_nodes[i];
        ut_ad(ins->select == nullptr);
        que_graph_free_recursive(ins->select);
        ins->select = nullptr;
        if (ins->entry_sys_heap != nullptr) {
          mem_heap_free(ins->entry_sys_heap);
          ins->entry_sys_heap = nullptr;
        }
      }
    }

    ut_free(m_ins_nodes);
  }

  ut_free(m_bitset);
  ut_free(m_trx_ids);
}

/** Initialize the object.
@return false on success else true. */
bool Altered_partitions::initialize() {
  size_t alloc_size = sizeof(*m_new_table_parts) * m_num_new_parts;
  m_new_table_parts =
      static_cast<dict_table_t **>(ut_zalloc(alloc_size, mem_key_partitioning));

  alloc_size = sizeof(*m_ins_nodes) * m_num_new_parts;
  m_ins_nodes =
      static_cast<ins_node_t **>(ut_zalloc(alloc_size, mem_key_partitioning));

  alloc_size = sizeof(*m_bitset) * UT_BITS_IN_BYTES(m_num_new_parts);
  m_bitset = static_cast<byte *>(ut_zalloc(alloc_size, mem_key_partitioning));

  alloc_size = sizeof(*m_trx_ids) * m_num_new_parts;
  m_trx_ids =
      static_cast<trx_id_t *>(ut_zalloc(alloc_size, mem_key_partitioning));

  if (m_new_table_parts == nullptr || m_ins_nodes == nullptr ||
      m_bitset == nullptr || m_trx_ids == nullptr) {
    ut_free(m_new_table_parts);
    ut_free(m_ins_nodes);
    ut_free(m_bitset);
    ut_free(m_trx_ids);

    return (true);
  }

  m_sql_stat_start.init(m_bitset, UT_BITS_IN_BYTES(m_num_new_parts));

  return (false);
}

/** Class(interface) which manages the operations for partitions of states
in different categories during ALTER PARTITION. There are four categories
for now:
1. normal: mapping to PART_NORMAL, which means the partition is not changed
2. add: mapping to PART_TO_BE_ADDED
3. drop: mapping to PART_TO_BE_DROPPED, PART_TO_BE_REORGED
and PART_REORGED_DROPPED
4. change: mapping to PART_CHANGED */
class alter_part {
 public:
  /** Virtual destructor */
  virtual ~alter_part() {}

  /** Return the partition id */
  virtual uint part_id() const { return (m_part_id); }

  /** Return the partition state */
  virtual partition_state state() const { return (m_state); }

  /** Get the InnoDB table object for newly created partition
  if applicable
  @return the InnoDB table object or nullptr if not applicable */
  dict_table_t *new_table() { return (m_new); }

  /** Prepare
  @param[in,out]	altered_table	Table definition after the ALTER
  @param[in]	old_part	the stored old partition or nullptr
                                  if no corresponding one exists
  @param[in,out]	new_part	the stored new partition or nullptr
                                  if no corresponding one exists
  @return 0 or error number */
  virtual int prepare(TABLE *altered_table, const dd::Partition *old_part,
                      dd::Partition *new_part) {
    return (0);
  }

  /** Try to commit
  @param[in]	table		Table definition before the ALTER
  @param[in,out]	altered_table	Table definition after the ALTER
  @param[in]	old_part	the stored old partition or nullptr
                                  if no corresponding one exists
  @param[in,out]	new_part	the stored new partition or nullptr
                                  if no corresponding one exists
  @return 0 or error number */
  virtual int try_commit(const TABLE *table, TABLE *altered_table,
                         const dd::Partition *old_part,
                         dd::Partition *new_part) {
    return (0);
  }

  /** Rollback */
  virtual void rollback() { return; }

 protected:
  /** Constructor
  @param[in,out]	trx		InnoDB transaction, nullptr if not used
  @param[in]	part_id		Partition id in the table. This could
                                  be partition id for either old table
                                  or new table, callers should remember
                                  which one is applicable
  @param[in]	state		Partition state of the partition on
                                  which this class will do operations.
                                  If this is for one partition in new
                                  table, the partition state is the same
                                  for both the new partition and the
                                  corresponding old partition
  @param[in]	table_name	Partitioned table name, in the
                                  form of db/table, which considers
                                  the charset
  @param[in,out]	old		InnoDB table object for old partition,
                                  default is nullptr, which means there
                                  is no corresponding object */
  alter_part(trx_t *trx, uint part_id, partition_state state,
             const char *table_name, dict_table_t *old)
      : m_trx(trx),
        m_part_id(part_id),
        m_state(state),
        m_table_name(table_name),
        m_old(old),
        m_new(nullptr) {}

  /** Build the partition name for specified partition
  @param[in]	dd_part		dd::Partition
  @param[in]	temp		True if this is a temporary name
  @param[in,out]	name		Partition name buffer, which is of
                                  length FN_REFLEN */
  void build_partition_name(const dd::Partition *dd_part, bool temp,
                            char *name);

  /** Create a new partition
  @param[in]	part_name	Partition name, including db/table
  @param[in,out]	dd_part		dd::Partition
  @param[in]	table		Table format
  @param[in]	tablespace	Tablespace of this partition,
                                  if length is 0, it means no
                                  tablespace specified
  @param[in]	file_per_table	Current value of innodb_file_per_table
  @param[in]	autoinc		Next AUTOINC value to use
  @return 0 or error number */
  int create(const char *part_name, dd::Partition *dd_part, TABLE *table,
             const char *tablespace, bool file_per_table, ib_uint64_t autoinc);

 protected:
  /** InnoDB transaction, nullptr if not used */
  trx_t *const m_trx;

  /** Partition id in the table. This could be partition id for
  either old table or new table, callers should remember which one
  is applicable */
  uint m_part_id;

  /** Partition state of the partition on which this class will
  do operations. If this is for one partition in new table, the
  partition state is the same for both the new partition and the
  corresponding old partition */
  partition_state m_state;

  /** Partitioned table name, in form of ./db/table, which already
  considers the charset */
  const char *m_table_name;

  /** The InnoDB table object for old partition */
  dict_table_t *m_old;

  /** The InnoDB table object for newly created partition */
  dict_table_t *m_new;
};

/** Build the partition name for specified partition
@param[in]	dd_part		dd::Partition
@param[in]	temp		True if this is a temporary name
@param[in,out]	name		Partition name buffer, which is of
                                length FN_REFLEN */
void alter_part::build_partition_name(const dd::Partition *dd_part, bool temp,
                                      char *name) {
  size_t len = 0;
  const char *table_name;

  /* Just get the 'db/table' part. In embedded server, m_table_name
  could be a full path */
  table_name = strrchr(m_table_name, OS_PATH_SEPARATOR);
  ut_a(table_name != nullptr);
  while (*(--table_name) != OS_PATH_SEPARATOR)
    ;
  ++table_name;

  strcpy(name, table_name);
#if OS_PATH_SEPARATOR != '/'
  char *slash = strchr(name, OS_PATH_SEPARATOR);
  ut_a(slash != nullptr);
  *slash = '/';
#endif

  len += strlen(table_name);
  ut_ad(len < FN_REFLEN);

  size_t post_len = Ha_innopart_share::create_partition_postfix(
      name + len, FN_REFLEN - len, dd_part);

  len += post_len;
  ut_ad(len < FN_REFLEN);

  if (temp) {
    strcpy(name + len, TMP_POSTFIX);
    ut_ad(len + sizeof TMP_POSTFIX < FN_REFLEN);
  }
}

/** Create a new partition
@param[in]	part_name	Partition name, including db/table
@param[in,out]	dd_part		dd::Partition
@param[in]	table		Table format
@param[in]	tablespace	Tablespace of this partition, if length is 0,
                                it means no tablespace specified
@param[in]	file_per_table	Current value of innodb_file_per_table
@param[in]	autoinc		Next AUTOINC value to use
@return 0 or error number */
int alter_part::create(const char *part_name, dd::Partition *dd_part,
                       TABLE *table, const char *tablespace,
                       bool file_per_table, ib_uint64_t autoinc) {
  ut_ad(m_state == PART_TO_BE_ADDED || m_state == PART_CHANGED);

  dd::Table &dd_table = dd_part->table();
  dd::Properties &options = dd_table.options();
  uint32 key_block_size;
  ut_ad(options.exists("key_block_size"));
  options.get_uint32("key_block_size", &key_block_size);

  dd::Properties &part_options = dd_part->options();
  dd::String_type data_file_name;
  part_options.get(data_file_name_key, data_file_name);
  /* index_file_name is not allowed for now */
  char full_path[FN_REFLEN];
  if (!data_file_name.empty()) {
    /* Have to append the postfix table name, to make it work */
    const char *name = strrchr(part_name, '/');
    ut_ad(name != nullptr);
    size_t len = data_file_name.length();
    strcpy(full_path, data_file_name.c_str());
    full_path[len] = OS_PATH_SEPARATOR;
    strcpy(full_path + len + 1, name + 1);
  }

  HA_CREATE_INFO create_info;
  update_create_info_from_table(&create_info, table);
  create_info.auto_increment_value = autoinc;
  create_info.key_block_size = key_block_size;
  create_info.data_file_name = data_file_name.empty() ? nullptr : full_path;
  create_info.tablespace = tablespace[0] == '\0' ? nullptr : tablespace;

  /* The below check is the same as for CREATE TABLE, but since we are
  doing an alter here it will not trigger the check in
  create_option_tablespace_is_valid(). */
  if (tablespace_is_shared_space(&create_info) &&
      create_info.data_file_name != nullptr &&
      create_info.data_file_name[0] != '\0') {
    my_printf_error(ER_ILLEGAL_HA_CREATE_OPTION,
                    "InnoDB: DATA DIRECTORY cannot be used"
                    " with a TABLESPACE assignment.",
                    MYF(0));
    return (HA_WRONG_CREATE_OPTION);
  }

  return (innobase_basic_ddl::create_impl<dd::Partition>(
      current_thd, part_name, table, &create_info, dd_part, file_per_table,
      false, false, 0, 0));
}

typedef std::vector<alter_part *, ut_allocator<alter_part *>> alter_part_array;

/** Construct all necessary alter_part_* objects according to the given
partition states in both old and new tables */
class alter_part_factory {
 public:
  /** Constructor
  @param[in,out]	trx		Transaction
  @param[in]	ha_alter_info	ALTER Information
  @param[in,out]	part_share	Innopart share
  @param[in]	old_part_info	Partition info of the table before
                                  ALTER TABLE */
  alter_part_factory(trx_t *trx, const Alter_inplace_info *ha_alter_info,
                     Ha_innopart_share *part_share,
                     partition_info *old_part_info)
      : m_trx(trx),
        m_part_share(part_share),
        m_ha_alter_info(ha_alter_info),
        m_old_part_info(old_part_info),
        m_file_per_table(srv_file_per_table) {}

  /** Destructor */
  ~alter_part_factory() {}

  /** Create the alter_part_* objects according to the given
  partition states
  @param[in,out]	to_drop		To store the alter_part_* objects
                                  for partitions to be dropped
  @param[in,out]	all_news	To store the alter_part_* objects
                                  for partitions in table after
                                  ALTER TABLE
  @return	false	On success
  @retval	true	On failure */
  bool create(alter_part_array &to_drop, alter_part_array &all_news) {
    to_drop.clear();
    all_news.clear();

    if (!(m_ha_alter_info->handler_flags &
          Alter_inplace_info::REORGANIZE_PARTITION)) {
      return (create_for_non_reorg(to_drop, all_news));
    } else {
      return (create_for_reorg(to_drop, all_news));
    }
  }

 private:
  /** Create the alter_part_* objects when it's an operation like
  REORGANIZE PARTITION
  @param[in,out]	to_drop		To store the alter_part_* objects
                                  for partitions to be dropped
  @param[in,out]	all_news	To store the alter_part_* objects
                                  for partitions in table after
                                  ALTER TABLE
  @return false	On success
  @retval true	On failure */
  bool create_for_reorg(alter_part_array &to_drop, alter_part_array &all_news);

  /** Create the alter_part_* objects when it's NOT an operation like
  REORGANIZE PARTITION
  @param[in,out]	to_drop		To store the alter_part_* objects
                                  for partitions to be dropped
  @param[in,out]	all_news	To store the alter_part_* objects
                                  for partitions in table after
                                  ALTER TABLE
  @return	false	On success
  @retval	true	On Failure */
  bool create_for_non_reorg(alter_part_array &to_drop,
                            alter_part_array &all_news);

  /** Create alter_part_add object(s) along with checking if the
  partition (and its subpartitions) conflicts with any of the original
  ones.
  This is only for REORGANIZE PARTITION
  @param[in]	new_part	The new partition to check
  @param[in,out]	new_part_id	Partition id for both partition and
                                  subpartition, which would be increased
                                  by number of subpartitions per
                                  partition here
  @param[in,out]	all_news	To store the alter_part_add objects
  @retval	false	On success
  @retval	true	On failure */
  bool create_new_checking_conflict(partition_element *new_part,
                                    uint &new_part_id,
                                    alter_part_array &all_news);

  /** Create alter_part_drop object(s) along with checking if the
  partition (and its subpartitions) conflicts with any of the to
  be created ones.
  This is only for REORGANIZE PARTITION
  @param[in]	old_part	The old partition to check
  @param[in,out]	old_part_id	Partition id for this partition or
                                  the first subpartition, which would
                                  be increased by number of subpartitions
                                  per partition here
  @param[in,out]	to_drop		To store the alter_part_drop objects
  @retval	false	On success
  @retval	true	On failure */
  bool create_old_checking_conflict(partition_element *old_part,
                                    uint &old_part_id,
                                    alter_part_array &to_drop);

  /** Check if the two (sub)partitions conflict with each other.
  That is they have same name and both are innodb_file_per_table
  @param[in]	new_part	New partition to check
  @param[in]	old_part	Old partition to check
  @retval true	Conflict
  @retval	false	Not conflict */
  bool is_conflict(const partition_element *new_part,
                   const partition_element *old_part);

  /** Create alter_part_* object(s) for subpartitions of a partition,
  or the partition itself
  @param[in,out]	array		Where to store the new object(s)
  @param[in]	part		partition_element to handle
  @param[in,out]	part_id		Partition id for both partition and
                                  subpartition, which would be increased
                                  by number of object(s) created
  @param[in]	old_part_id	Start partition id of the table before
                                  ALTER TABLE
  @param[in]	state		Partition state
  @param[in]	conflict	Only valid when state is
                                  PART_TO_BE_ADDED. True if the new
                                  (sub)partition has the same name with
                                  an exist one and they are of
                                  innodb_file_per_table
  @retval	false	On success
  @retval	true	On failure */
  bool create_one(alter_part_array &array, partition_element *part,
                  uint &part_id, uint old_part_id, partition_state state,
                  bool conflict);

  /** Create the specified alter_part_* object
  @param[in]	part_id		Partition id for current partition
  @param[in]	old_part_id	Start partition id of the table before
                                  ALTER TABLE
  @param[in]	state		Partition state
  @param[in]	tablespace	Tablespace specified explicitly
  @param[in]	conflict	Only valid when state is
                                  PART_TO_BE_ADDED. True if the new
                                  (sub)partition has the same name with
                                  an exist one and they are of
                                  innodb_file_per_table
  @return alter_part_* object or nullptr */
  alter_part *create_one_low(uint &part_id, uint old_part_id,
                             partition_state state, const char *tablespace,
                             bool conflict);

 private:
  /** InnoDB transaction */
  trx_t *const m_trx;

  /** InnoDB partition specific Handler_share */
  Ha_innopart_share *const m_part_share;

  /** ALTER information */
  const Alter_inplace_info *const m_ha_alter_info;

  /** Partition info of the table before ALTER TABLE */
  partition_info *const m_old_part_info;

  /** Current innodb_file_per_table value */
  bool m_file_per_table;
};

/** Helper class for in-place alter partitions, see handler.h */
class alter_parts : public inplace_alter_handler_ctx {
 public:
  /** Constructor
  @param[in,out]	trx		InnoDB transaction
  @param[in,out]	part_share	Innopart share
  @param[in]	ha_alter_info	ALTER information
  @param[in]	old_part_info	Partition info of the table before
                                  ALTER TABLE
  @param[in,out]	new_partitions	Altered partition helper */
  alter_parts(trx_t *trx, Ha_innopart_share *part_share,
              const Alter_inplace_info *ha_alter_info,
              partition_info *old_part_info, Altered_partitions *new_partitions)
      : m_trx(trx),
        m_part_share(part_share),
        m_ha_alter_info(ha_alter_info),
        m_new_partitions(new_partitions),
        m_factory(trx, ha_alter_info, part_share, old_part_info),
        m_news(),
        m_to_drop() {}

  /** Destructor */
  ~alter_parts();

  /** Create the to be created partitions and update internal
  structures with concurrent writes blocked, while preparing
  ALTER TABLE.
  @param[in]	old_dd_tab	dd::Table before ALTER TABLE
  @param[in,out]	new_dd_tab	dd::Table after ALTER TABLE
  @param[in,out]	altered_table	Table definition after the ALTER
  @return 0 or error number, my_error() should be called by callers */
  int prepare(const dd::Table &old_dd_tab, dd::Table &new_dd_tab,
              TABLE *altered_table);

  /** Notify the storage engine that the changes made during
  prepare_inplace_alter_table() and inplace_alter_table()
  will be rolled back for all the partitions. */
  void rollback();

  /** Try to commit the changes made during prepare_inplace_alter_table()
  inside the storage engine. This is protected by MDL_EXCLUSIVE.
  @param[in]	old_dd_tab	dd::Table before ALTER TABLE
  @param[in,out]	new_dd_tab	dd::Table after ALTER TABLE
  @param[in]	table		Table definition before the ALTER
  @param[in,out]	altered_table	Table definition after the ALTER
  @return 0 or error number, my_error() should be called by callers */
  int try_commit(const dd::Table &old_dd_tab, dd::Table &new_dd_tab,
                 const TABLE *table, TABLE *altered_table);

  /** Determine if this is an ALTER TABLE ... PARTITION operation
  @param[in]	ha_alter_info	thd DDL operation
  @return whether it is a such kind of operation */
  static inline bool apply_to(const Alter_inplace_info *ha_alter_info) {
    return ((ha_alter_info->handler_flags & OPERATIONS) != 0);
  }

  /** Determine if copying data between partitions is necessary
  @param[in]	ha_alter_info	thd DDL operation
  @return whether it is necessary to copy data */
  static inline bool need_copy(const Alter_inplace_info *ha_alter_info) {
    ut_ad(apply_to(ha_alter_info));

    /* Basically, only DROP PARTITION, ADD PARTITION for RANGE/LIST
    partitions don't require copying data between partitions */
    if (ha_alter_info->handler_flags & Alter_inplace_info::ADD_PARTITION) {
      switch (ha_alter_info->modified_part_info->part_type) {
        case partition_type::RANGE:
        case partition_type::LIST:
          return (false);
        default:
          break;
      }
    }

    return (
        !(ha_alter_info->handler_flags & (Alter_inplace_info::DROP_PARTITION)));
  }

 private:
  /** Initialize the m_news and m_to_drop array here
  @param[in]	old_dd_tab	dd::Table before ALTER TABLE
  @param[in]	new_dd_tab	dd::Table after ALTER TABLE
  @retval true if success
  @retval false on failure */
  bool prepare_alter_part(const dd::Table &old_dd_tab, dd::Table &new_dd_tab);

  /** Prepare or commit for all the partitions in table after ALTER TABLE
  @param[in]	old_dd_tab	dd::Table before ALTER TABLE
  @param[in,out]	new_dd_tab	dd::Table after ALTER TABLE
  @param[in,out]	altered_table	Table definition after the ALTER
  @param[in]	prepare		true if it's in prepare phase,
                                  false if it's in commit phase
  @return 0 or error number */
  int prepare_or_commit_for_new(const dd::Table &old_dd_tab,
                                dd::Table &new_dd_tab, TABLE *altered_table,
                                bool prepare);

  /** Prepare or commit for all the partitions in table before ALTER TABLE
  @param[in]	old_dd_tab	dd::Table before ALTER TABLE
  @param[in,out]	altered_table	Table definition after the ALTER
  @param[in]	prepare		true if it's in prepare phase,
                                  false if it's in commit phase
  @return 0 or error number */
  int prepare_or_commit_for_old(const dd::Table &old_dd_tab,
                                TABLE *altered_table, bool prepare);

 public:
  /** Operations that the native partitioning can perform inplace */
  static constexpr Alter_inplace_info::HA_ALTER_FLAGS OPERATIONS =
      Alter_inplace_info::ADD_PARTITION | Alter_inplace_info::DROP_PARTITION |
      Alter_inplace_info::ALTER_REBUILD_PARTITION |
      Alter_inplace_info::COALESCE_PARTITION |
      Alter_inplace_info::REORGANIZE_PARTITION;

 private:
  /** InnoDB transaction */
  trx_t *const m_trx;

  /** InnoDB partition specific Handler_share */
  Ha_innopart_share *const m_part_share;

  /** Operation being performed */
  const Alter_inplace_info *const m_ha_alter_info;

  /** New partitions helper */
  Altered_partitions *const m_new_partitions;

  /** alter_part factory which creates all the necessary alter_part_* */
  alter_part_factory m_factory;

  /** The alter_part array for all the newly created partitions */
  alter_part_array m_news;

  /** The alter_part array for all the to be dropped partitions */
  alter_part_array m_to_drop;
};

/** Class which handles the partition of state PART_NORMAL.
See comments for alter_part_factory::create_for_reorg
and alter_part_factory::create_for_non_reorg. */
class alter_part_normal : public alter_part {
 public:
  /** Constructor
  @param[in]	part_id		Partition id in the table. This could
                                  be partition id for either old table
                                  or new table, callers should remember
                                  which one is applicable
  @param[in]	state		Partition state of the partition on
                                  which this class will do operations.
                                  If this is for one partition in new
                                  table, the partition state is the same
                                  for both the new partition and the
                                  corresponding old partition
  @param[in,out]	old		InnoDB table object for old partition,
                                  default is nullptr, which means there
                                  is no corresponding object */
  alter_part_normal(uint part_id, partition_state state, dict_table_t *old)
      : /* Table name is not used in this class, so pass a fake
        one */
        alter_part(nullptr, part_id, state, old->name.m_name, old) {}

  /** Destructor */
  ~alter_part_normal() {}

  /** Prepare
  @param[in,out]	altered_table	Table definition after the ALTER
  @param[in]	old_part	the stored old partition or nullptr
                                  if no corresponding one exists
  @param[in,out]	new_part	the stored new partition or nullptr
                                  if no corresponding one exists
  @return 0 or error number */
  int prepare(TABLE *altered_table, const dd::Partition *old_part,
              dd::Partition *new_part) {
    ut_ad(old_part->name() == new_part->name());

    dd_copy_private<dd::Partition>(*new_part, *old_part);

    return (0);
  }

  /** Try to commit
  @param[in]	table		Table definition before the ALTER
  @param[in,out]	altered_table	Table definition after the ALTER
  @param[in]	old_part	the stored old partition or nullptr
                                  if no corresponding one exists
  @param[in,out]	new_part	the stored new partition or nullptr
                                  if no corresponding one exists
  @return 0 or error number */
  int try_commit(const TABLE *table, TABLE *altered_table,
                 const dd::Partition *old_part, dd::Partition *new_part) {
    ut_ad(m_old != nullptr);

    btr_drop_ahi_for_table(m_old);

    mutex_enter(&dict_sys->mutex);
    dd_table_close(m_old, nullptr, nullptr, true);
    dict_table_remove_from_cache(m_old);
    mutex_exit(&dict_sys->mutex);
    return (0);
  }
};

/** Class which handles the partition of the state PART_TO_BE_ADDED.
See comments for alter_part_factory::create_for_reorg
and alter_part_factory::create_for_non_reorg. */
class alter_part_add : public alter_part {
 public:
  /** Constructor
  @param[in]	part_id		Partition id in the table. This could
                                  be partition id for either old table
                                  or new table, callers should remember
                                  which one is applicable
  @param[in]	state		Partition state of the partition on
                                  which this class will do operations.
                                  If this is for one partition in new
                                  table, the partition state is the same
                                  for both the new partition and the
                                  corresponding old partition
  @param[in]	table_name	Partitioned table name, in the form
                                  of db/table, which already considers
                                  the charset
  @param[in]	tablespace	Tablespace specified explicitly
  @param[in,out]	trx		InnoDB transaction
  @param[in]	ha_alter_info	ALTER information
  @param[in]	file_per_table	Current value of innodb_file_per_table
  @param[in]	autoinc		Next autoinc value to use
  @param[in]	conflict	True if there is already a partition
                                  table with the same name */
  alter_part_add(uint part_id, partition_state state, const char *table_name,
                 const char *tablespace, trx_t *trx,
                 const Alter_inplace_info *ha_alter_info, bool file_per_table,
                 ib_uint64_t autoinc, bool conflict)
      : alter_part(trx, part_id, state, table_name, nullptr),
        m_ha_alter_info(ha_alter_info),
        m_file_per_table(file_per_table),
        m_autoinc(autoinc),
        m_conflict(conflict) {
    if (tablespace == nullptr || tablespace[0] == '\0') {
      m_tablespace[0] = '\0';
    } else {
      strcpy(m_tablespace, tablespace);
    }
  }

  /** Destructor */
  ~alter_part_add() {}

  /** Prepare
  @param[in,out]	altered_table	Table definition after the ALTER
  @param[in]	old_part	the stored old partition or nullptr
                                  if no corresponding one exists
  @param[in,out]	new_part	the stored new partition or nullptr
                                  if no corresponding one exists
  @return 0 or error number */
  int prepare(TABLE *altered_table, const dd::Partition *old_part,
              dd::Partition *new_part) {
    ut_ad(old_part != nullptr);
    ut_ad(new_part != nullptr);
    char part_name[FN_REFLEN];

    if (is_shared_tablespace(m_tablespace)) {
      my_printf_error(ER_ILLEGAL_HA_CREATE_OPTION,
                      PARTITION_IN_SHARED_TABLESPACE, MYF(0));
      return (HA_ERR_INTERNAL_ERROR);
    }

    build_partition_name(new_part, need_rename(), part_name);

    int error = create(part_name, new_part, altered_table, m_tablespace,
                       m_file_per_table, m_autoinc);

    if (error == 0 && alter_parts::need_copy(m_ha_alter_info)) {
      mutex_enter(&dict_sys->mutex);
      m_new = dict_table_check_if_in_cache_low(part_name);
      ut_ad(m_new != nullptr);
      m_new->acquire();
      dict_table_ddl_release(m_new);
      mutex_exit(&dict_sys->mutex);

      return (m_new == nullptr ? DB_TABLE_NOT_FOUND : 0);
    }

    return (error);
  }

  /** Try to commit
  @param[in]	table		Table definition before the ALTER
  @param[in,out]	altered_table	Table definition after the ALTER
  @param[in]	old_part	the stored old partition or nullptr
                                  if no corresponding one exists
  @param[in,out]	new_part	the stored new partition or nullptr
                                  if no corresponding one exists
  @return 0 or error number */
  int try_commit(const TABLE *table, TABLE *altered_table,
                 const dd::Partition *old_part, dd::Partition *new_part) {
    int error = 0;

    if (need_rename()) {
      char old_name[FN_REFLEN];
      char new_name[FN_REFLEN];
      build_partition_name(new_part, true, old_name);
      build_partition_name(new_part, false, new_name);
      error = innobase_basic_ddl::rename_impl<dd::Partition>(
          m_trx->mysql_thd, old_name, new_name, new_part, new_part);
    }

    if (m_new != nullptr) {
      dd_table_close(m_new, m_trx->mysql_thd, nullptr, false);
      m_new = nullptr;
    }

    return (error);
  }

  /** Rollback */
  void rollback() {
    /* Release the new table so that in post DDL, this table can be
    rolled back. */
    if (m_new != nullptr) {
      dd_table_close(m_new, m_trx->mysql_thd, nullptr, false);
      m_new = nullptr;
    }
  }

 private:
  /** Check if the new partition file needs a temporary name and
  should be renamed at last */
  bool need_rename() const { return (m_conflict); }

 private:
  /** ALTER information */
  const Alter_inplace_info *m_ha_alter_info;

  /** Current value of innodb_file_per_table */
  const bool m_file_per_table;

  /** Next AUTOINC value to use */
  const ib_uint64_t m_autoinc;

  /** True if there is already a partition table with the same name */
  const bool m_conflict;

  /** Tablespace of this partition */
  char m_tablespace[FN_REFLEN + 1];
};

/** Class which handles the partition of states
PART_TO_BE_DROPPED, PART_TO_BE_REORGED and PART_REORGED_DROPPED.
See comments for alter_part_factory::create_for_reorg
and alter_part_factory::create_for_non_reorg. */
class alter_part_drop : public alter_part {
 public:
  /** Constructor
  @param[in]	part_id		Partition id in the table. This could
                                  be partition id for either old table
                                  or new table, callers should remember
                                  which one is applicable
  @param[in]	state		Partition state of the partition on
                                  which this class will do operations.
                                  If this is for one partition in new
                                  table, the partition state is the same
                                  for both the new partition and the
                                  corresponding old partition
  @param[in]	table_name	Partitioned table name, in the form
                                  of db/table, which already considers
                                  the charset
  @param[in,out]	trx		InnoDB transaction
  @param[in,out]	old		InnoDB table object for old partition,
                                  default is nullptr, which means there
                                  is no corresponding object
  @param[in]	conflict	True if there is already a partition
                                  table with the same name */
  alter_part_drop(uint part_id, partition_state state, const char *table_name,
                  trx_t *trx, dict_table_t *old, bool conflict)
      : alter_part(trx, part_id, state, table_name, old),
        m_conflict(conflict) {}

  /** Destructor */
  ~alter_part_drop() {}

  /** Try to commit
  @param[in]	table		Table definition before the ALTER
  @param[in,out]	altered_table	Table definition after the ALTER
  @param[in]	old_part	the stored old partition or nullptr
                                  if no corresponding one exists
  @param[in,out]	new_part	the stored new partition or nullptr
                                  if no corresponding one exists
  @return 0 or error number */
  int try_commit(const TABLE *table, TABLE *altered_table,
                 const dd::Partition *old_part, dd::Partition *new_part) {
    ut_ad(new_part == nullptr);

    mutex_enter(&dict_sys->mutex);
    dict_table_ddl_acquire(m_old);
    mutex_exit(&dict_sys->mutex);
    dd_table_close(m_old, nullptr, nullptr, false);

    int error;
    char part_name[FN_REFLEN];
    THD *thd = m_trx->mysql_thd;

    build_partition_name(old_part, false, part_name);

    if (!m_conflict) {
      error = innobase_basic_ddl::delete_impl<dd::Partition>(thd, part_name,
                                                             old_part);
    } else {
      /* Have to rename it to a temporary name to prevent
      name conflict, because later deleting table doesn't
      remove the data file at once. Also notice that don't
      use the #tmp name, because it could be already used
      by the corresponding new partition. */
      mem_heap_t *heap = mem_heap_create(FN_REFLEN);
      char db_buf[NAME_LEN + 1];
      char tbl_buf[NAME_LEN + 1];
      MDL_ticket *mdl_ticket = nullptr;

      char *temp_name = dict_mem_create_temporary_tablename(
          heap, m_old->name.m_name, m_old->id);

      /* Acquire mdl lock on the temporary table name. */
      dd_parse_tbl_name(temp_name, db_buf, tbl_buf, nullptr, nullptr, nullptr);

      if (dd::acquire_exclusive_table_mdl(thd, db_buf, tbl_buf, false,
                                          &mdl_ticket)) {
        mem_heap_free(heap);
        return (HA_ERR_GENERIC);
      }

      error = innobase_basic_ddl::rename_impl<dd::Partition>(
          thd, part_name, temp_name, old_part, old_part);
      if (error == 0) {
        error = innobase_basic_ddl::delete_impl<dd::Partition>(thd, temp_name,
                                                               old_part);
      }

      mem_heap_free(heap);
    }

    return (error);
  }

 private:
  /** True if there is already a partition table with the same name */
  const bool m_conflict;
};

/** Class which handles the partition of the state PART_CHANGED.
See comments for alter_part_factory::create_for_reorg
and alter_part_factory::create_for_non_reorg. */
class alter_part_change : public alter_part {
 public:
  /** Constructor
  @param[in]	part_id		Partition id in the table. This could
                                  be partition id for either old table
                                  or new table, callers should remember
                                  which one is applicable
  @param[in]	state		Partition state of the partition on
                                  which this class will do operations.
                                  If this is for one partition in new
                                  table, the partition state is the same
                                  for both the new partition and the
                                  corresponding old partition
  @param[in]	table_name	Partitioned table name, in the form
                                  of db/table, which already considers
                                  the chraset
  @param[in]	tablespace	Tablespace specified explicitly
  @param[in,out]	trx		InnoDB transaction
  @param[in,out]	old		InnoDB table object for old partition,
                                  default is nullptr, which means there
                                  is no corresponding object
  @param[in]	ha_alter_info	ALTER information
  @param[in]	file_per_table	Current value of innodb_file_per_table
  @param[in]	autoinc		Next AUTOINC value to use */
  alter_part_change(uint part_id, partition_state state, const char *table_name,
                    const char *tablespace, trx_t *trx, dict_table_t *old,
                    const Alter_inplace_info *ha_alter_info,
                    bool file_per_table, ib_uint64_t autoinc)
      : alter_part(trx, part_id, state, table_name, old),
        m_ha_alter_info(ha_alter_info),
        m_file_per_table(file_per_table),
        m_autoinc(autoinc) {
    if (tablespace == nullptr || tablespace[0] == '\0') {
      m_tablespace[0] = '\0';
    } else {
      strcpy(m_tablespace, tablespace);
    }
  }

  /** Destructor */
  ~alter_part_change() {}

  /** Prepare
  @param[in,out]	altered_table	Table definition after the ALTER
  @param[in]	old_part	the stored old partition or nullptr
                                  if no corresponding one exists
  @param[in,out]	new_part	the stored new partition or nullptr
                                  if no corresponding one exists
  @return 0 or error number */
  int prepare(TABLE *altered_table, const dd::Partition *old_part,
              dd::Partition *new_part);

  /** Try to commit
  @param[in]	table		Table definition before the ALTER
  @param[in,out]	altered_table	Table definition after the ALTER
  @param[in]	old_part	the stored old partition or nullptr
                                  if no corresponding one exists
  @param[in,out]	new_part	the stored new partition or nullptr
                                  if no corresponding one exists
  @return 0 or error number */
  int try_commit(const TABLE *table, TABLE *altered_table,
                 const dd::Partition *old_part, dd::Partition *new_part);

  /** Rollback */
  void rollback() {
    /* Release the new table so that in post DDL, this table can be
    rolled back. */
    if (m_new != nullptr) {
      dd_table_close(m_new, m_trx->mysql_thd, nullptr, false);
      m_new = nullptr;
    }
  }

 private:
  /** ALTER information */
  const Alter_inplace_info *m_ha_alter_info;

  /** Current value of innodb_file_per_table */
  const bool m_file_per_table;

  /** Next AUTOINC value to use */
  const ib_uint64_t m_autoinc;

  /** Tablespace of this partition */
  char m_tablespace[FN_REFLEN + 1];
};

/** Prepare
@param[in,out]	altered_table	Table definition after the ALTER
@param[in]	old_part	the stored old partition or nullptr
                                if no corresponding one exists
@param[in,out]	new_part	the stored new partition or nullptr
                                if no corresponding one exists
@return 0 or error number */
int alter_part_change::prepare(TABLE *altered_table,
                               const dd::Partition *old_part,
                               dd::Partition *new_part) {
  ut_ad(old_part != nullptr);
  ut_ad(new_part != nullptr);

  /* In some scenario, it could be unnecessary to create partition
  with temporary name, for example, old one is in innodb_system while
  new one is innodb_file_per_table. However, this would result in
  same table name for two tables, which is confusing. So the temporary
  name is used always and final rename is necessary too */
  char part_name[FN_REFLEN];
  build_partition_name(new_part, true, part_name);

  int error = create(part_name, new_part, altered_table, m_tablespace,
                     m_file_per_table, m_autoinc);

  if (error == 0) {
    mutex_enter(&dict_sys->mutex);
    m_new = dict_table_check_if_in_cache_low(part_name);
    ut_ad(m_new != nullptr);
    m_new->acquire();
    dict_table_ddl_release(m_new);
    mutex_exit(&dict_sys->mutex);

    return (m_new == nullptr);
  }

  return (error);
}

/** Try to commit
@param[in]	table		Table definition before the ALTER
@param[in,out]	altered_table	Table definition after the ALTER
@param[in]	old_part	the stored old partition or nullptr
                                if no corresponding one exists
@param[in,out]	new_part	the stored new partition or nullptr
                                if no corresponding one exists
@return 0 or error number */
int alter_part_change::try_commit(const TABLE *table, TABLE *altered_table,
                                  const dd::Partition *old_part,
                                  dd::Partition *new_part) {
  ut_ad(old_part != nullptr);
  ut_ad(new_part != nullptr);
  ut_ad(old_part->name() == new_part->name());

  THD *thd = m_trx->mysql_thd;
  char db_buf[NAME_LEN + 1];
  char tbl_buf[NAME_LEN + 1];
  char *temp_old_name = dict_mem_create_temporary_tablename(
      m_old->heap, m_old->name.m_name, m_old->id);

  mutex_enter(&dict_sys->mutex);
  dict_table_ddl_acquire(m_old);
  mutex_exit(&dict_sys->mutex);
  dd_table_close(m_old, nullptr, nullptr, false);

  /* Acquire mdl lock on the temporary table name. */
  dd_parse_tbl_name(temp_old_name, db_buf, tbl_buf, nullptr, nullptr, nullptr);

  MDL_ticket *mdl_ticket = nullptr;
  if (dd::acquire_exclusive_table_mdl(thd, db_buf, tbl_buf, false,
                                      &mdl_ticket)) {
    return (HA_ERR_GENERIC);
  }

  char old_name[FN_REFLEN];
  char temp_name[FN_REFLEN];
  build_partition_name(new_part, false, old_name);
  build_partition_name(new_part, true, temp_name);

  int error;

  error = innobase_basic_ddl::rename_impl<dd::Partition>(
      thd, old_name, temp_old_name, old_part, old_part);
  if (error == 0) {
    error = innobase_basic_ddl::rename_impl<dd::Partition>(
        thd, temp_name, old_name, new_part, new_part);
    if (error == 0) {
      error = innobase_basic_ddl::delete_impl<dd::Partition>(thd, temp_old_name,
                                                             old_part);
    }
  }

  if (m_new != nullptr) {
    dd_table_close(m_new, thd, nullptr, false);
    m_new = nullptr;
  }

  return (error);
}

/** Create alter_part_* object(s) for subpartitions of a partition,
or the partition itself
@param[in,out]	array		Where to store the new object(s)
@param[in]	part		partition_element to handle
@param[in,out]	part_id		Partition id for both partition and
                                subpartition, which would be increased
                                by number of object(s) created
@param[in]	old_part_id	Start partition id of the table before
                                ALTER TABLE
@param[in]	state		Partition state
@param[in]	conflict	Only valid when state is
                                PART_TO_BE_ADDED. True if the new
                                (sub)partition has the same name with
                                an exist one and they are of
                                innodb_file_per_table
@retval false	On success
@retval true	On failure */
bool alter_part_factory::create_one(alter_part_array &array,
                                    partition_element *part, uint &part_id,
                                    uint old_part_id, partition_state state,
                                    bool conflict) {
  if (part->subpartitions.elements > 0) {
    partition_element *sub_elem;
    List_iterator_fast<partition_element> new_sub_it(part->subpartitions);
    while ((sub_elem = new_sub_it++) != nullptr) {
      const char *tablespace = partition_get_tablespace(
          m_ha_alter_info->create_info->tablespace, part, sub_elem);
      alter_part *alter =
          create_one_low(part_id, old_part_id++, state, tablespace, conflict);
      if (alter == nullptr) {
        return (true);
      }

      ++part_id;
      array.push_back(alter);
    }
  } else {
    const char *tablespace = partition_get_tablespace(
        m_ha_alter_info->create_info->tablespace, part, nullptr);
    alter_part *alter =
        create_one_low(part_id, old_part_id++, state, tablespace, conflict);
    if (alter == nullptr) {
      return (true);
    }

    ++part_id;
    array.push_back(alter);
  }

  return (false);
}

/** Create the specified alter_part_* object
@param[in]	part_id		Partition id for current partition

@param[in]	old_part_id	Start partition id of the table before
                                ALTER TABLE
@param[in]	state		Partition state
@param[in]	tablespace	Tablespace specified explicitly
@param[in]	conflict	Only valid when state is
                                PART_TO_BE_ADDED. True if the new
                                (sub)partition has the same name with
                                an exist one and they are of
                                innodb_file_per_table
@return alter_part_* object or nullptr */
alter_part *alter_part_factory::create_one_low(uint &part_id, uint old_part_id,
                                               partition_state state,
                                               const char *tablespace,
                                               bool conflict) {
  alter_part *alter_part = nullptr;

  switch (state) {
    case PART_NORMAL:
      alter_part =
          UT_NEW(alter_part_normal(part_id, state,
                                   m_part_share->get_table_part(old_part_id)),
                 mem_key_partitioning);
      break;
    case PART_TO_BE_ADDED:
      alter_part = UT_NEW(
          alter_part_add(part_id, state,
                         m_part_share->get_table_share()->normalized_path.str,
                         tablespace, m_trx, m_ha_alter_info, m_file_per_table,
                         m_part_share->next_auto_inc_val, conflict),
          mem_key_partitioning);
      break;
    case PART_TO_BE_DROPPED:
    case PART_TO_BE_REORGED:
    case PART_REORGED_DROPPED:
      alter_part = UT_NEW(
          alter_part_drop(part_id, state,
                          m_part_share->get_table_share()->normalized_path.str,
                          m_trx, m_part_share->get_table_part(old_part_id),
                          conflict),
          mem_key_partitioning);
      break;
    case PART_CHANGED:
      alter_part = UT_NEW(
          alter_part_change(
              part_id, state,
              m_part_share->get_table_share()->normalized_path.str, tablespace,
              m_trx, m_part_share->get_table_part(old_part_id), m_ha_alter_info,
              m_file_per_table, m_part_share->next_auto_inc_val),
          mem_key_partitioning);
      break;
    default:
      ut_ad(0);
  }

  return (alter_part);
}

/** Create alter_part_add object(s) along with checking if the
partition (and its subpartitions) conflicts with any of the original ones
This is only for REORGANIZE PARTITION
@param[in]	new_part	The new partition to check
@param[in,out]	new_part_id	Partition id for both partition and
                                subpartition, which would be increased
                                by number of subpartitions per partition here
@param[in,out]	all_news	To store the alter_part_add objects here
@retval	false	On success
@retval	true	On failure */
bool alter_part_factory::create_new_checking_conflict(
    partition_element *new_part, uint &new_part_id,
    alter_part_array &all_news) {
  ut_ad((m_ha_alter_info->handler_flags &
         Alter_inplace_info::REORGANIZE_PARTITION) != 0);

  partition_info *part_info = m_ha_alter_info->modified_part_info;
  /* To compare with this partition list which contains all the
  to be reorganized partitions */
  List_iterator_fast<partition_element> tmp_part_it(part_info->temp_partitions);
  partition_element *tmp_part_elem;

  while ((tmp_part_elem = tmp_part_it++) != nullptr) {
    if (!is_conflict(new_part, tmp_part_elem)) {
      continue;
    }

    if (m_ha_alter_info->modified_part_info->is_sub_partitioned()) {
      List_iterator_fast<partition_element> tmp_sub_it(
          tmp_part_elem->subpartitions);
      partition_element *tmp_sub_elem;
      List_iterator_fast<partition_element> new_sub_it(new_part->subpartitions);
      partition_element *new_sub_elem;

      while ((new_sub_elem = new_sub_it++) != nullptr) {
        ut_ad(new_sub_elem->partition_name != nullptr);
        tmp_sub_elem = tmp_sub_it++;
        ut_ad(tmp_sub_elem != nullptr);
        ut_ad(tmp_sub_elem->partition_name != nullptr);

        bool conflict = is_conflict(new_sub_elem, tmp_sub_elem);
        if (create_one(all_news, new_sub_elem, new_part_id, 0, PART_TO_BE_ADDED,
                       conflict)) {
          return (true);
        }
      }
      ut_ad((tmp_sub_elem = tmp_sub_it++) == nullptr);
    } else {
      if (create_one(all_news, new_part, new_part_id, 0, PART_TO_BE_ADDED,
                     true)) {
        return (true);
      }
    }

    /* Once matched, all are done */
    return (false);
  }

  return (
      create_one(all_news, new_part, new_part_id, 0, PART_TO_BE_ADDED, false));
}

/** Create alter_part_drop object(s) along with checking if the
partition (and its subpartitions) conflicts with any of the to
be created ones.
This is only for REORGANIZE PARTITION
@param[in]	old_part	The old partition to check
@param[in,out]	old_part_id	Partition id for this partition or
                                the first subpartition, which would
                                be increased by number of subpartitions
                                per partition here
@param[in,out]	to_drop		To store the alter_part_drop objects
@retval	false	On success
@retval	true	On failure */
bool alter_part_factory::create_old_checking_conflict(
    partition_element *old_part, uint &old_part_id, alter_part_array &to_drop) {
  ut_ad((m_ha_alter_info->handler_flags &
         Alter_inplace_info::REORGANIZE_PARTITION) != 0);

  partition_info *part_info = m_ha_alter_info->modified_part_info;
  /* To compare with this partition list which contains all the
  new to be added partitions */
  List_iterator_fast<partition_element> part_it(part_info->partitions);
  partition_element *part_elem;

  while ((part_elem = part_it++) != nullptr) {
    if (!is_conflict(part_elem, old_part)) {
      continue;
    }

    if (m_ha_alter_info->modified_part_info->is_sub_partitioned()) {
      List_iterator_fast<partition_element> sub_it(part_elem->subpartitions);
      partition_element *sub_elem;
      List_iterator_fast<partition_element> old_sub_it(old_part->subpartitions);
      partition_element *old_sub_elem;

      while ((old_sub_elem = old_sub_it++) != nullptr) {
        ut_ad(old_sub_elem->partition_name != nullptr);
        sub_elem = sub_it++;
        ut_ad(sub_elem != nullptr);
        ut_ad(sub_elem->partition_name != nullptr);

        bool conflict = is_conflict(sub_elem, old_sub_elem);
        if (create_one(to_drop, old_sub_elem, old_part_id, old_part_id,
                       PART_TO_BE_REORGED, conflict)) {
          return (true);
        }
      }
      ut_ad((sub_elem = sub_it++) == nullptr);
    } else {
      if (create_one(to_drop, old_part, old_part_id, old_part_id,
                     PART_TO_BE_REORGED, true)) {
        return (true);
      }
    }

    /* Once matched, all are done */
    return (false);
  }

  return (create_one(to_drop, old_part, old_part_id, old_part_id,
                     PART_TO_BE_REORGED, false));
}

/** Check if the two (sub)partitions conflict with each other,
Which means they have same name.
@param[in]	new_part	New partition to check
@param[in]	old_part	Old partition to check
@retval true	Conflict
@retval false	Not conflict */
bool alter_part_factory::is_conflict(const partition_element *new_part,
                                     const partition_element *old_part) {
  if (my_strcasecmp(system_charset_info, new_part->partition_name,
                    old_part->partition_name) != 0) {
    return (false);
  }

  /* To prevent the conflict(same) names in table cache, not to
  check the innodb_file_per_table */
  return (true);
}

/** Suppose that there is a table with 4 range partitions: p0, p1, p2, p3,
and the p2 and p3 are going to be reorganized into p21, p22, p31, p33.

In modified_part_info->temp_partitions list, there are only p2 and p3
with the state PART_TO_BE_REORGED, while in modified_part_info->partitions
list, it contains
{PART_NORMAL, PART_NORMAL, PART_TO_BE_ADDED, PART_TO_BE_ADDED,
PART_TO_BE_ADDED, PART_TO_BE_ADDED}.

So finally, the to_drop array would contain
{alter_part_drop, alter_part_drop}, which are for p2, p3;
the all_news array would contains
{alter_part_normal, alter_part_normal, alter_part_add, alter_part_add,
alter_part_add, alter_part_add}.

Note that the scenario that reorganized and to be reorganized
partition/subpartition have the same name, would be checked here too */

/** Create the alter_part_* objects when it's an operation like
REORGANIZE PARTITION
@param[in,out]	to_drop		To store the alter_part_* objects
                                for partitions to be dropped
@param[in,out]	all_news	To store the alter_part_* objects
                                for partitions in table after ALTER TABLE
@return false	On success
@retval true	On failure */
bool alter_part_factory::create_for_reorg(alter_part_array &to_drop,
                                          alter_part_array &all_news) {
  ut_ad((m_ha_alter_info->handler_flags &
         Alter_inplace_info::REORGANIZE_PARTITION) != 0);
  ut_ad(m_ha_alter_info->modified_part_info->num_subparts ==
        m_old_part_info->num_subparts);

  partition_info *part_info = m_ha_alter_info->modified_part_info;
  /* This list contains only the to be reorganized partitions,
  the sequence is the same as the list of m_old_part_info,
  and they should be consecutive ones */
  List_iterator_fast<partition_element> tmp_part_it(part_info->temp_partitions);
  /* This list contains all the new partitions */
  List_iterator_fast<partition_element> part_it(part_info->partitions);
  /* This list contains all the old partitions */
  List_iterator_fast<partition_element> old_part_it(
      m_old_part_info->partitions);
  partition_element *part_elem;
  partition_element *tmp_part_elem;
  partition_element *old_part_elem;
  uint parts_per_part =
      part_info->is_sub_partitioned() ? part_info->num_subparts : 1;

  tmp_part_elem = tmp_part_it++;
  ut_ad(tmp_part_elem != nullptr);
  old_part_elem = old_part_it++;
  ut_ad(old_part_elem != nullptr);

  uint old_part_id = 0;
  uint new_part_id = 0;

  /* There are 3 steps here:
  1. Check if the old one is a to be reorganized one, if so, mark it
  and check next old one
  2. If not, check if the new one is a to be added one, if so, mark it
  and check next new one
  3. If not, the old one and the new one should point to the same
  partition */
  while ((part_elem = part_it++) != nullptr) {
    while (old_part_elem != nullptr && tmp_part_elem != nullptr &&
           strcmp(tmp_part_elem->partition_name,
                  old_part_elem->partition_name) == 0) {
      ut_ad(tmp_part_elem->part_state == PART_TO_BE_REORGED);

      if (create_old_checking_conflict(old_part_elem, old_part_id, to_drop)) {
        return (true);
      }

      old_part_elem = old_part_it++;
      tmp_part_elem = tmp_part_it++;
    }

    switch (part_elem->part_state) {
      case PART_TO_BE_ADDED:

        if (create_new_checking_conflict(part_elem, new_part_id, all_news)) {
          return (true);
        }

        break;

      case PART_NORMAL:

        ut_ad(strcmp(part_elem->partition_name,
                     old_part_elem->partition_name) == 0);

        if (create_one(all_news, part_elem, new_part_id, old_part_id,
                       PART_NORMAL, false)) {
          return (true);
        }

        old_part_elem = old_part_it++;
        old_part_id += parts_per_part;

        break;

      default:
        ut_ad(0);
    }
  }

  ut_ad(old_part_elem == nullptr);
  ut_ad(tmp_part_elem == nullptr);

  return (false);
}

/** Suppose that there is a table with 4 range partitions: p0, p1, p2, p3.

1. ADD PARTITION p4
modified_part_info->partitions list contains
{PART_NORMAL, PART_NORMAL, PART_NORMAL, PART_NORMAL, PART_TO_BE_ADDED}.

So finally, the to_drop array would contain
{}, which is empty;
the all_news array would contains
{alter_part_normal, alter_part_normal, alter_part_normal, alter_part_normal,
alter_part_add}.

2. DROP PARTITION p2
modified_part_info->partitions list contain
{PART_NORMAL, PART_NORMAL, PART_TO_BE_DROPPED, PART_NORMAL}.

So finally, the to_drop array would contain
{alter_part_drop}, which is for p2, so part_id is 2;
the all_news array would contains
{alter_part_normal, alter_part_normal, alter_part_normal}.


Suppose it's the same table with 4 partitions, but it's partitioned by HASH.

3. ADD PARTITION 2
modified_part_info->partitions list contains
{PART_CHANGED, PART_CHANGED, PART_CHANGED, PART_CHANGED, PART_TO_BE_ADDED,
PART_TO_BE_ADDED}.

So finally, the to_drop array would contain
{}, which is empty;
the all_news array would contains
{alter_part_change, alter_part_change, alter_part_change, alter_part_change,
alter_part_add, alter_part_add}.

4. COALESCE PARTITION 2
modified_part_info->partitions contains:
{PART_CHANGED, PART_CHANGED, PART_REORGED_DROPPED, PART_REORGED_DROPPED}.

So finally, the to_drop array would contain
{alter_part_drop, alter_part_drop}, which are for p2, p3, part_id are 2 and 3;
the all_news array would contains
{alter_part_change, alter_part_change}.

5. REBUILD PARTITION p0, p2
modified_part_info->partitions contains:
{PART_NORMAL, PART_CHANGED, PART_NORMAL, PART_CHANGED}.

So finally, the to_drop array would contain
{}, which is empty;
the all_news array would contains
{alter_part_normal, alter_part_change, alter_part_normal, alter_part_change}. */

/** Create the alter_part_* objects when it's NOT an operation like
REORGANIZE PARTITION
@param[in,out]	to_drop		To store the alter_part_* objects
                                for partitions to be dropped
@param[in,out]	all_news	To store the alter_part_* objects
                                for partitions in table after ALTER TABLE
@return	false	On success
@retval	true	On Failure */
bool alter_part_factory::create_for_non_reorg(alter_part_array &to_drop,
                                              alter_part_array &all_news) {
  ut_ad((m_ha_alter_info->handler_flags &
         Alter_inplace_info::REORGANIZE_PARTITION) == 0);

  partition_info *part_info = m_ha_alter_info->modified_part_info;
  uint parts_per_part =
      part_info->is_sub_partitioned() ? part_info->num_subparts : 1;
  List_iterator_fast<partition_element> part_it(part_info->partitions);
  partition_element *part_elem;
  uint old_part_id = 0;
  uint new_part_id = 0;

  while ((part_elem = part_it++) != nullptr) {
    partition_state state = part_elem->part_state;
    switch (state) {
      case PART_NORMAL:
      case PART_CHANGED:
        if (create_one(all_news, part_elem, new_part_id, old_part_id, state,
                       false)) {
          return (true);
        }

        old_part_id += parts_per_part;
        break;
      case PART_TO_BE_ADDED:
        if (create_one(all_news, part_elem, new_part_id, 0, state, false)) {
          return (true);
        }

        break;
      case PART_TO_BE_DROPPED:
      case PART_REORGED_DROPPED:
        if (create_one(to_drop, part_elem, old_part_id, old_part_id, state,
                       false)) {
          return (true);
        }

        break;
      default:
        ut_ad(0);
    }
  }

  return (false);
}

#ifndef DBUG_OFF
/** Check if the specified partition_state is of drop state
@param[in]	s	The state to be checked
@retval	true    if this is of a drop state
@retval	false   if not */
inline static bool is_drop_state(partition_state s) {
  return (s == PART_TO_BE_DROPPED || s == PART_REORGED_DROPPED ||
          s == PART_TO_BE_REORGED);
}
#endif

/** Check if the specified partition_state is of common state
@param[in]	s	The state to be checked
@retval	true	if this is of a common state
@retval	false	if not */
inline static bool is_common_state(partition_state s) {
  return (s == PART_NORMAL || s == PART_CHANGED);
}

/** Destructor */
alter_parts::~alter_parts() {
  for (alter_part *alter_part : m_news) {
    UT_DELETE(alter_part);
  }

  for (alter_part *alter_part : m_to_drop) {
    UT_DELETE(alter_part);
  }
}

/** Create the to be created partitions and update internal
structures with concurrent writes blocked, while preparing
ALTER TABLE.
@param[in]	old_dd_tab	dd::Table before ALTER TABLE
@param[in,out]	new_dd_tab	dd::Table after ALTER TABLE
@param[in,out]	altered_table	Table definition after the ALTER
@return 0 or error number, my_error() should be called by callers */
int alter_parts::prepare(const dd::Table &old_dd_tab, dd::Table &new_dd_tab,
                         TABLE *altered_table) {
  if (m_factory.create(m_to_drop, m_news)) {
    return (true);
  }

  if (m_part_share->get_table_share()->found_next_number_field) {
    dd_set_autoinc(new_dd_tab.se_private_data(),
                   m_ha_alter_info->create_info->auto_increment_value);
  }

  int error;
  error = prepare_or_commit_for_old(old_dd_tab, altered_table, true);
  if (error != 0) {
    return (error);
  }

  error =
      prepare_or_commit_for_new(old_dd_tab, new_dd_tab, altered_table, true);

  /* We don't have to prepare for the partitions that will be dropped. */

  return (error);
}

/** Notify the storage engine that the changes made during
prepare_inplace_alter_table() and inplace_alter_table()
will be rolled back for all the partitions. */
void alter_parts::rollback() {
  for (alter_part *alter_part : m_to_drop) {
    alter_part->rollback();
  }

  for (alter_part *alter_part : m_news) {
    alter_part->rollback();
  }
}

/** Try to commit the changes made during prepare_inplace_alter_table()
inside the storage engine.v This is protected by MDL_EXCLUSIVE.
@param[in]	old_dd_tab	dd::Table before ALTER TABLE
@param[in,out]	new_dd_tab	dd::Table after ALTER TABLE
@param[in]	table		Table definition before the ALTER
@param[in,out]	altered_table	Table definition after the ALTER
@return 0 or error number, my_error() should be called by callers */
int alter_parts::try_commit(const dd::Table &old_dd_tab, dd::Table &new_dd_tab,
                            const TABLE *table, TABLE *altered_table) {
  int error;
  /* Commit for the old ones first, to clear data files for new ones */
  error = prepare_or_commit_for_old(old_dd_tab, altered_table, false);
  if (error != 0) {
    return (error);
  }

  error =
      prepare_or_commit_for_new(old_dd_tab, new_dd_tab, altered_table, false);
  if (error != 0) {
    return (error);
  }

  return (0);
}

/** Prepare for all the partitions in table after ALTER TABLE
@param[in]	old_dd_tab	dd::Table before ALTER TABLE
@param[in,out]	new_dd_tab	dd::Table after ALTER TABLE
@param[in,out]	altered_table	Table definition after the ALTER
@param[in]	prepare		true if it's in prepare phase,
                                false if it's in commit phase
@return 0 or error number */
int alter_parts::prepare_or_commit_for_new(const dd::Table &old_dd_tab,
                                           dd::Table &new_dd_tab,
                                           TABLE *altered_table, bool prepare) {
  auto oldp = old_dd_tab.leaf_partitions().begin();
  uint new_part_id = 0;
  uint old_part_id = 0;
  uint drop_seq = 0;
  const dd::Partition *old_part = nullptr;
  int error = 0;

  for (auto new_part : *new_dd_tab.leaf_partitions()) {
    ut_ad(new_part_id < m_news.size());

    /* To add a new partition, there is no corresponding old one,
    otherwise, find the old one */
    partition_state s = m_news[new_part_id]->state();
    if (is_common_state(s)) {
      bool found = false;
      for (; oldp != old_dd_tab.leaf_partitions().end() && !found; ++oldp) {
        old_part = *oldp;

        ++old_part_id;
        if (drop_seq < m_to_drop.size() &&
            (old_part_id - 1 == m_to_drop[drop_seq]->part_id())) {
          ut_ad(is_drop_state(m_to_drop[drop_seq]->state()));
          ++drop_seq;
          continue;
        }

        found = true;
      }

      ut_ad(found);
      ut_ad(drop_seq <= m_to_drop.size());
      ut_ad(new_part->name() == old_part->name());
      ut_ad((new_part->parent() == nullptr) == (old_part->parent() == nullptr));
      ut_ad(new_part->parent() == nullptr ||
            new_part->parent()->name() == old_part->parent()->name());
    } else {
      ut_ad(s == PART_TO_BE_ADDED);
      /* Let's still set one to get the old table name */
      old_part = *(old_dd_tab.leaf_partitions().begin());
    }

    alter_part *alter_part = m_news[new_part_id];
    ut_ad(alter_part != nullptr);

    if (prepare) {
      error = alter_part->prepare(altered_table, old_part, new_part);
      if (error != 0) {
        return (error);
      }

      if (m_new_partitions != nullptr && alter_part->new_table() != nullptr) {
        m_new_partitions->set_part(new_part_id, alter_part->new_table());
      }
    } else {
      error =
          alter_part->try_commit(nullptr, altered_table, old_part, new_part);
      if (error != 0) {
        return (error);
      }
    }

    ++new_part_id;
  }

#ifdef UNIV_DEBUG
  ut_ad(drop_seq <= m_to_drop.size());
  for (uint i = drop_seq; i < m_to_drop.size(); ++i) {
    ut_ad(!is_common_state(m_to_drop[i]->state()));
  }
#endif /* UNIV_DEBUG */

  return (error);
}

/** Prepare or commit for all the partitions in table before ALTER TABLE
@param[in]	old_dd_tab	dd::Table before ALTER TABLE
@param[in,out]	altered_table	Table definition after the ALTER
@param[in]	prepare		true if it's in prepare phase,
                                false if it's in commit phase
@return 0 or error number */
int alter_parts::prepare_or_commit_for_old(const dd::Table &old_dd_tab,
                                           TABLE *altered_table, bool prepare) {
  uint old_part_id = 0;
  auto dd_part = old_dd_tab.leaf_partitions().begin();
  int error = 0;

  for (alter_part *alter_part : m_to_drop) {
    const dd::Partition *old_part = nullptr;

    for (; dd_part != old_dd_tab.leaf_partitions().end(); ++dd_part) {
      if (old_part_id++ < alter_part->part_id()) {
        continue;
      }

      old_part = *dd_part;
      ++dd_part;
      break;
    }
    ut_ad(old_part != nullptr);

    if (prepare) {
      error = alter_part->prepare(altered_table, old_part, nullptr);
    } else {
      error = alter_part->try_commit(nullptr, altered_table, old_part, nullptr);
    }

    if (error != 0) {
      return (error);
    }
  }

  return (error);
}

/** Determine if one ALTER TABLE can be done instantly on the partitioned table
@param[in]	ha_alter_info	the DDL operation
@param[in]	num_parts	number of partitions
@param[in]	part_share	the partitioned tables
@param[in]	old_table	old TABLE
@param[in]	altered_table	new TABLE
@return Instant_Type accordingly */
static inline Instant_Type innopart_support_instant(
    const Alter_inplace_info *ha_alter_info, uint16_t num_parts,
    const Ha_innopart_share *part_share, const TABLE *old_table,
    const TABLE *altered_table) {
  Instant_Type type = Instant_Type::INSTANT_IMPOSSIBLE;

  for (uint32_t i = 0; i < num_parts; ++i) {
    type = innobase_support_instant(
        ha_alter_info, part_share->get_table_part(i), old_table, altered_table);
    if (type == Instant_Type::INSTANT_IMPOSSIBLE) {
      return (type);
    }
  }

  return (type);
}

/** Check if supported inplace alter table.
@param[in]	altered_table	Altered MySQL table.
@param[in]	ha_alter_info	Information about inplace operations to do.
@return	Lock level, not supported or error */
enum_alter_inplace_result ha_innopart::check_if_supported_inplace_alter(
    TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
  DBUG_ENTER("ha_innopart::check_if_supported_inplace_alter");
  DBUG_ASSERT(ha_alter_info->handler_ctx == NULL);

  /* Not supporting these for partitioned tables yet! */

  /* FK not yet supported. */
  if (ha_alter_info->handler_flags & (Alter_inplace_info::ADD_FOREIGN_KEY |
                                      Alter_inplace_info::DROP_FOREIGN_KEY)) {
    ha_alter_info->unsupported_reason =
        innobase_get_err_msg(ER_FOREIGN_KEY_ON_PARTITIONED);
    DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
  }
  /* FTS not yet supported either. */
  if ((ha_alter_info->handler_flags & Alter_inplace_info::ADD_INDEX)) {
    for (uint i = 0; i < ha_alter_info->index_add_count; i++) {
      const KEY *key =
          &ha_alter_info->key_info_buffer[ha_alter_info->index_add_buffer[i]];
      if (key->flags & HA_FULLTEXT) {
        DBUG_ASSERT(!(key->flags & HA_KEYFLAG_MASK &
                      ~(HA_FULLTEXT | HA_PACK_KEY | HA_GENERATED_KEY |
                        HA_BINARY_PACK_KEY)));
        ha_alter_info->unsupported_reason =
            innobase_get_err_msg(ER_FULLTEXT_NOT_SUPPORTED_WITH_PARTITIONING);
        DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
      }
    }
  }
  /* We cannot allow INPLACE to change order of KEY partitioning fields! */
  if ((ha_alter_info->handler_flags &
       Alter_inplace_info::ALTER_STORED_COLUMN_ORDER) &&
      !m_part_info->same_key_column_order(
          &ha_alter_info->alter_info->create_list)) {
    DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
  }

  /* Cannot allow INPLACE for drop and create PRIMARY KEY if partition is
  on Primary Key - PARTITION BY KEY() */
  if ((ha_alter_info->handler_flags & (Alter_inplace_info::ADD_PK_INDEX |
                                       Alter_inplace_info::DROP_PK_INDEX))) {
    /* Check partition by key(). */
    if ((m_part_info->part_type == partition_type::HASH) &&
        m_part_info->list_of_part_fields &&
        m_part_info->part_field_list.is_empty()) {
      DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
    }

    /* Check sub-partition by key(). */
    if ((m_part_info->subpart_type == partition_type::HASH) &&
        m_part_info->list_of_subpart_fields &&
        m_part_info->subpart_field_list.is_empty()) {
      DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
    }
  }

  /* Check for ALTER TABLE ... PARTITION, following operations can
  be done inplace */
  if (alter_parts::apply_to(ha_alter_info)) {
    /* Two meanings here:
    1. ALTER TABLE .. PARTITION could not be combined with
    other ALTER TABLE operations;
    2. Only one operation of ALTER TABLE .. PARTITION can be
    done in single statement. Only exception is that
    'ALTER TABLE table REORGANIZE PARTITION' for HASH/KEY
    partitions. This will flag both COALESCE_PARTITION
    and ALTER_TABLE_REORG;
    The ALTER_ALL_PARTITION should be screened out, which could only
    be set along with the REBUILD PARTITION */
    ut_ad(is_single_bit(ha_alter_info->handler_flags &
                        ~Alter_inplace_info::ALTER_ALL_PARTITION) ||
          ha_alter_info->handler_flags ==
              (Alter_inplace_info::COALESCE_PARTITION |
               Alter_inplace_info::ALTER_TABLE_REORG));
    ut_ad(!(ha_alter_info->handler_flags &
            Alter_inplace_info::ALTER_ALL_PARTITION) ||
          (ha_alter_info->handler_flags &
           Alter_inplace_info::ALTER_REBUILD_PARTITION));

    if (alter_parts::need_copy(ha_alter_info)) {
      DBUG_RETURN(HA_ALTER_INPLACE_SHARED_LOCK_AFTER_PREPARE);
    } else {
      DBUG_RETURN(HA_ALTER_INPLACE_NO_LOCK_AFTER_PREPARE);
    }
  }

  Instant_Type instant_type = innopart_support_instant(
      ha_alter_info, m_tot_parts, m_part_share, this->table, altered_table);
  ha_alter_info->handler_trivial_ctx =
      instant_type_to_int(Instant_Type::INSTANT_IMPOSSIBLE);

  switch (instant_type) {
    case Instant_Type::INSTANT_IMPOSSIBLE:
      break;
    case Instant_Type::INSTANT_ADD_COLUMN:
      if (ha_alter_info->alter_info->requested_algorithm ==
          Alter_info::ALTER_TABLE_ALGORITHM_INPLACE) {
        break;
      } else if (ha_alter_info->error_if_not_empty) {
        /* In this case, it can't be instant because the table
        may not be empty. Have to fall back to INPLACE */
        break;
      }
      /* Fall through */
    case Instant_Type::INSTANT_NO_CHANGE:
    case Instant_Type::INSTANT_VIRTUAL_ONLY:
      ha_alter_info->handler_trivial_ctx = instant_type_to_int(instant_type);
      DBUG_RETURN(HA_ALTER_INPLACE_INSTANT);
  }

  /* Check for PK and UNIQUE should already be done when creating the
  new table metadata.
  (fix_partition_info/check_primary_key+check_unique_key) */

  set_partition(0);
  DBUG_RETURN(ha_innobase::check_if_supported_inplace_alter(altered_table,
                                                            ha_alter_info));
}

/** Prepare inplace alter table.
Allows InnoDB to update internal structures with concurrent
writes blocked (provided that check_if_supported_inplace_alter()
did not return HA_ALTER_INPLACE_NO_LOCK).
This will be invoked before inplace_alter_table().
@param[in]	altered_table	TABLE object for new version of table.
@param[in]	ha_alter_info	Structure describing changes to be done
                                by ALTER TABLE and holding data used during
                                in-place alter.
@param[in]	old_table_def	dd::Table object describing old version
                                of the table.
@param[in,out]	new_table_def	dd::Table object for the new version of
                                the table. Can be adjusted by this call.
                                Changes to the table definition will be
                                persisted in the data-dictionary at statement
                                commit time.
@retval true Failure.
@retval false Success. */
bool ha_innopart::prepare_inplace_alter_table(TABLE *altered_table,
                                              Alter_inplace_info *ha_alter_info,
                                              const dd::Table *old_table_def,
                                              dd::Table *new_table_def) {
  DBUG_ENTER("ha_innopart::prepare_inplace_alter_table");
  DBUG_ASSERT(ha_alter_info->handler_ctx == nullptr);

  if (tablespace_is_shared_space(ha_alter_info->create_info)) {
    my_printf_error(ER_ILLEGAL_HA_CREATE_OPTION, PARTITION_IN_SHARED_TABLESPACE,
                    MYF(0));
    DBUG_RETURN(true);
  }

  /* The row format in new table may differ from the old one,
  which is set by server earlier. So keep them the same */
  new_table_def->set_row_format(old_table_def->row_format());

  if (altered_table->found_next_number_field != nullptr) {
    dd_copy_autoinc(old_table_def->se_private_data(),
                    new_table_def->se_private_data());
  }

  if (alter_parts::apply_to(ha_alter_info)) {
    DBUG_RETURN(prepare_inplace_alter_partition(altered_table, ha_alter_info,
                                                old_table_def, new_table_def));
  }

  ha_innopart_inplace_ctx *ctx_parts;
  THD *thd = ha_thd();
  bool res = true;

  /* Clean up all ins/upd nodes. */
  clear_ins_upd_nodes();
  /*
  This object will be freed by server, so always use 'new'
  and there is no need to free on failure */
  ctx_parts = new (*THR_MALLOC) ha_innopart_inplace_ctx(thd, m_tot_parts);
  if (ctx_parts == nullptr) {
    DBUG_RETURN(HA_ALTER_ERROR);
  }

  ctx_parts->ctx_array =
      UT_NEW_ARRAY_NOKEY(inplace_alter_handler_ctx *, m_tot_parts + 1);
  if (ctx_parts->ctx_array == nullptr) {
    DBUG_RETURN(HA_ALTER_ERROR);
  }

  memset(ctx_parts->ctx_array, 0,
         sizeof(inplace_alter_handler_ctx *) * (m_tot_parts + 1));

  ctx_parts->m_old_info =
      UT_NEW_ARRAY_NOKEY(alter_table_old_info_t, m_tot_parts);
  if (ctx_parts->m_old_info == nullptr) {
    DBUG_RETURN(HA_ALTER_ERROR);
  }

  ctx_parts->prebuilt_array = UT_NEW_ARRAY_NOKEY(row_prebuilt_t *, m_tot_parts);
  if (ctx_parts->prebuilt_array == nullptr) {
    DBUG_RETURN(HA_ALTER_ERROR);
  }
  /* For the first partition use the current prebuilt. */
  ctx_parts->prebuilt_array[0] = m_prebuilt;
  /* Create new prebuilt for the rest of the partitions.
  It is needed for the current implementation of
  ha_innobase::commit_inplace_alter_table(). */
  for (uint i = 1; i < m_tot_parts; i++) {
    row_prebuilt_t *tmp_prebuilt;
    tmp_prebuilt = row_create_prebuilt(m_part_share->get_table_part(i),
                                       table_share->reclength);
    /* Use same trx as original prebuilt. */
    tmp_prebuilt->trx = m_prebuilt->trx;
    ctx_parts->prebuilt_array[i] = tmp_prebuilt;
  }

  if (altered_table->found_next_number_field != nullptr) {
    dd_set_autoinc(new_table_def->se_private_data(),
                   ha_alter_info->create_info->auto_increment_value);
  }

  const char *save_tablespace = ha_alter_info->create_info->tablespace;

  const char *save_data_file_name = ha_alter_info->create_info->data_file_name;

  auto oldp = old_table_def->leaf_partitions().begin();
  auto newp = new_table_def->leaf_partitions()->begin();

  for (uint i = 0; i < m_tot_parts; ++oldp, ++newp) {
    m_prebuilt = ctx_parts->prebuilt_array[i];
    set_partition(i);

    const dd::Partition *old_part = *oldp;
    dd::Partition *new_part = *newp;
    ut_ad(old_part != nullptr);
    ut_ad(new_part != nullptr);
    ut_ad(m_prebuilt->table->id == old_part->se_private_id());

    ha_alter_info->handler_ctx = nullptr;

    /* Set the tablespace and data_file_name value of the
    alter_info to the tablespace and data_file_name value
    that was existing for the partition originally, so that
    for ALTER TABLE the tablespace clause in create option
    is ignored for existing partitions, and later set it
    back to its old value */

    ha_alter_info->create_info->tablespace = m_prebuilt->table->tablespace;
    ha_alter_info->create_info->data_file_name =
        m_prebuilt->table->data_dir_path;

    res = prepare_inplace_alter_table_impl<dd::Partition>(
        altered_table, ha_alter_info, old_part, new_part);

    update_partition(i);
    ctx_parts->ctx_array[i] = ha_alter_info->handler_ctx;
    if (res) {
      break;
    }

    ha_innobase_inplace_ctx *ctx =
        static_cast<ha_innobase_inplace_ctx *>(ctx_parts->ctx_array[i]);
    if (ctx != nullptr) {
      ctx_parts->m_old_info[i].update(ctx->old_table, ctx->need_rebuild());
    }

    ++i;
  }

  m_prebuilt = ctx_parts->prebuilt_array[0];
  ha_alter_info->handler_ctx = ctx_parts;
  ha_alter_info->group_commit_ctx = ctx_parts->ctx_array;
  ha_alter_info->create_info->tablespace = save_tablespace;
  ha_alter_info->create_info->data_file_name = save_data_file_name;

  DBUG_RETURN(res);
}

/** Inplace alter table.
Alter the table structure in-place with operations
specified using Alter_inplace_info.
The level of concurrency allowed during this operation depends
on the return value from check_if_supported_inplace_alter().
@param[in]	altered_table	TABLE object for new version of table.
@param[in]	ha_alter_info	Structure describing changes to be done
                                by ALTER TABLE and holding data used during
                                in-place alter.
@param[in]	old_table_def	dd::Table object describing old version
                                of the table.
@param[in,out]	new_table_def	dd::Table object for the new version of
                                the table. Can be adjusted by this call.
                                Changes to the table definition will be
                                persisted in the data-dictionary at statement
                                commit time.
@retval true Failure.
@retval false Success. */
bool ha_innopart::inplace_alter_table(TABLE *altered_table,
                                      Alter_inplace_info *ha_alter_info,
                                      const dd::Table *old_table_def,
                                      dd::Table *new_table_def) {
  if (alter_parts::apply_to(ha_alter_info)) {
    return (inplace_alter_partition(altered_table, ha_alter_info, old_table_def,
                                    new_table_def));
  }

  bool res = true;
  ha_innopart_inplace_ctx *ctx_parts;

  ctx_parts =
      static_cast<ha_innopart_inplace_ctx *>(ha_alter_info->handler_ctx);

  /* It could be not allocated at all */
  if (ctx_parts == nullptr) {
    return (false);
  }

  auto oldp = old_table_def->leaf_partitions().begin();
  auto newp = new_table_def->leaf_partitions()->begin();

  for (uint i = 0; i < m_tot_parts; ++oldp, ++newp) {
    const dd::Partition *old_part = *oldp;
    dd::Partition *new_part = *newp;

    m_prebuilt = ctx_parts->prebuilt_array[i];
    ha_alter_info->handler_ctx = ctx_parts->ctx_array[i];
    set_partition(i);

    res = inplace_alter_table_impl<dd::Partition>(altered_table, ha_alter_info,
                                                  old_part, new_part);
    ut_ad(ctx_parts->ctx_array[i] == ha_alter_info->handler_ctx);
    ctx_parts->ctx_array[i] = ha_alter_info->handler_ctx;

    if (res) {
      break;
    }

    ++i;
  }
  m_prebuilt = ctx_parts->prebuilt_array[0];
  ha_alter_info->handler_ctx = ctx_parts;
  return (res);
}

/** Commit or rollback inplace alter table.
Commit or rollback the changes made during
prepare_inplace_alter_table() and inplace_alter_table() inside
the storage engine. Note that the allowed level of concurrency
during this operation will be the same as for
inplace_alter_table() and thus might be higher than during
prepare_inplace_alter_table(). (E.g concurrent writes were
blocked during prepare, but might not be during commit).
@param[in]	altered_table	TABLE object for new version of table.
@param[in]	ha_alter_info	Structure describing changes to be done
                                by ALTER TABLE and holding data used during
                                in-place alter.
@param[in]	commit		true => Commit, false => Rollback.
@param[in]	old_table_def	dd::Table object describing old version
                                of the table.
@param[in,out]	new_table_def	dd::Table object for the new version of
                                the table. Can be adjusted by this call.
                                Changes to the table definition will be
                                persisted in the data-dictionary at statement
                                commit time.
@retval true Failure.
@retval false Success. */
bool ha_innopart::commit_inplace_alter_table(TABLE *altered_table,
                                             Alter_inplace_info *ha_alter_info,
                                             bool commit,
                                             const dd::Table *old_table_def,
                                             dd::Table *new_table_def) {
  if (alter_parts::apply_to(ha_alter_info)) {
    return (commit_inplace_alter_partition(altered_table, ha_alter_info, commit,
                                           old_table_def, new_table_def));
  }

  ha_innopart_inplace_ctx *ctx_parts =
      static_cast<ha_innopart_inplace_ctx *>(ha_alter_info->handler_ctx);

  /* It could be not allocated at all */
  if (ctx_parts == nullptr) {
    return (false);
  }

  bool res = false;
  ut_ad(ctx_parts->ctx_array != nullptr);
  ut_ad(ctx_parts->prebuilt_array != nullptr);
  ut_ad(ctx_parts->prebuilt_array[0] == m_prebuilt);

  if (commit) {
    /* Commit is done through first partition (group commit). */
    ut_ad(ha_alter_info->group_commit_ctx == ctx_parts->ctx_array);
    ha_alter_info->handler_ctx = ctx_parts->ctx_array[0];
    set_partition(0);

    res = ha_innobase::commit_inplace_alter_table_impl<dd::Table>(
        altered_table, ha_alter_info, commit, old_table_def, new_table_def);
    ut_ad(res || !ha_alter_info->group_commit_ctx);

    goto end;
  }

  /* Rollback is done for each partition. */
  for (uint i = 0; i < m_tot_parts; i++) {
    m_prebuilt = ctx_parts->prebuilt_array[i];
    ha_alter_info->handler_ctx = ctx_parts->ctx_array[i];
    set_partition(i);
    if (ha_innobase::commit_inplace_alter_table_impl<dd::Table>(
            altered_table, ha_alter_info, commit, old_table_def,
            new_table_def)) {
      res = true;
    }
    ut_ad(ctx_parts->ctx_array[i] == ha_alter_info->handler_ctx);
    ctx_parts->ctx_array[i] = ha_alter_info->handler_ctx;
  }
end:
  /* All are done successfully, now write back metadata to DD */
  if (commit && !res) {
    ut_ad(!(is_instant(ha_alter_info) && ctx_parts->m_old_info[0].m_rebuild));

    auto oldp = old_table_def->leaf_partitions().begin();
    auto newp = new_table_def->leaf_partitions()->begin();
    bool inplace_instant = false;

    for (uint i = 0; i < m_tot_parts; ++oldp, ++newp) {
      const dd::Partition *old_part = *oldp;
      dd::Partition *new_part = *newp;
      ut_ad(old_part != nullptr);
      ut_ad(new_part != nullptr);

      ha_innobase_inplace_ctx *ctx =
          static_cast<ha_innobase_inplace_ctx *>(ctx_parts->ctx_array[i]);

      if (is_instant(ha_alter_info)) {
        dd_commit_inplace_instant(
            ha_alter_info, m_user_thd, m_prebuilt->trx,
            m_part_share->get_table_part(i), table, altered_table, old_part,
            new_part,
            altered_table->found_next_number_field != nullptr
                ? reinterpret_cast<uint64_t *>(&m_part_share->next_auto_inc_val)
                : nullptr);
      } else if (!(ha_alter_info->handler_flags & ~INNOBASE_INPLACE_IGNORE) ||
                 ctx == nullptr) {
        dd_commit_inplace_no_change(old_part, new_part, true);
      } else {
        inplace_instant = !ctx_parts->m_old_info[0].m_rebuild;
        dd_commit_inplace_alter_table(ctx_parts->m_old_info[i], ctx->new_table,
                                      old_part, new_part);
      }

      ++i;
    }

    if (inplace_instant) {
      dd_commit_inplace_update_partition_instant_meta(
          m_part_share, m_tot_parts, old_table_def, new_table_def);
    }

#ifdef UNIV_DEBUG
    if (!res) {
      if (dd_table_has_instant_cols(*old_table_def) &&
          !ctx_parts->m_old_info[0].m_rebuild) {
        ut_ad(dd_table_has_instant_cols(*new_table_def));
      }

      uint i = 0;
      for (auto part : *new_table_def->leaf_partitions()) {
        ha_innobase_inplace_ctx *ctx =
            static_cast<ha_innobase_inplace_ctx *>(ctx_parts->ctx_array[i++]);
        if (ctx != nullptr) {
          ut_ad(dd_table_match(ctx->new_table, part));
        }
      }
    }
#endif /* univ_debug */
  }

  /* Move the ownership of the new tables back to the m_part_share. */
  ha_innobase_inplace_ctx *ctx;
  for (uint i = 0; i < m_tot_parts; i++) {
    /* TODO: Fix to only use one prebuilt (i.e. make inplace
    alter partition aware instead of using multiple prebuilt
    copies... */
    ctx = static_cast<ha_innobase_inplace_ctx *>(ctx_parts->ctx_array[i]);
    if (ctx != nullptr) {
      m_part_share->set_table_part(i, ctx->prebuilt->table);
      ctx->prebuilt->table = nullptr;
      ctx_parts->prebuilt_array[i] = ctx->prebuilt;
    } else {
      break;
    }
  }
  /* The above juggling of prebuilt must be reset here. */
  m_prebuilt = ctx_parts->prebuilt_array[0];
  m_prebuilt->table = m_part_share->get_table_part(0);
  ha_alter_info->handler_ctx = ctx_parts;
  return (res);
}

/** Create the Altered_partitoins object
@param[in]	ha_alter_info	thd DDL operation
@retval	true	On failure
@retval	false	On success */
bool ha_innopart::prepare_for_copy_partitions(
    Alter_inplace_info *ha_alter_info) {
  ut_ad(m_new_partitions == nullptr);
  ut_ad(alter_parts::need_copy(ha_alter_info));

  uint num_parts = ha_alter_info->modified_part_info->num_parts;
  uint total_parts = num_parts;

  if (ha_alter_info->modified_part_info->is_sub_partitioned()) {
    total_parts *= ha_alter_info->modified_part_info->num_subparts;
  }

  m_new_partitions =
      UT_NEW(Altered_partitions(total_parts), mem_key_partitioning);

  if (m_new_partitions == nullptr) {
    return (true);
  } else if (m_new_partitions->initialize()) {
    UT_DELETE(m_new_partitions);
    m_new_partitions = nullptr;
    return (true);
  }

  return (false);
}

/** write row to new partition.
@param[in]	new_part	New partition to write to.
@return 0 for success else error code. */
int ha_innopart::write_row_in_new_part(uint new_part) {
  int result;
  DBUG_ENTER("ha_innopart::write_row_in_new_part");

  m_last_part = new_part;
  if (m_new_partitions->part(new_part) == nullptr) {
    /* Altered partition contains misplaced row. */
    m_err_rec = table->record[0];
    DBUG_RETURN(HA_ERR_ROW_IN_WRONG_PARTITION);
  }

  m_new_partitions->prepare_write(m_prebuilt, new_part);
  result = ha_innobase::write_row(table->record[0]);
  m_new_partitions->finish_write(m_prebuilt, new_part);
  DBUG_RETURN(result);
}

/** Allows InnoDB to update internal structures with concurrent
writes blocked (given that check_if_supported_inplace_alter()
did not return HA_ALTER_INPLACE_NO_LOCK).
This is for 'ALTER TABLE ... PARTITION' and a corresponding function
to inplace_alter_table().
This will be invoked before inplace_alter_partition().

@param[in,out]	altered_table	TABLE object for new version of table
@param[in,out]	ha_alter_info	Structure describing changes to be done
                                by ALTER TABLE and holding data used during
                                in-place alter.
@param[in]	old_dd_tab	Table definition before the ALTER
@param[in,out]	new_dd_tab	Table definition after the ALTER
@retval true	Failure
@retval false	Success */
bool ha_innopart::prepare_inplace_alter_partition(
    TABLE *altered_table, Alter_inplace_info *ha_alter_info,
    const dd::Table *old_dd_tab, dd::Table *new_dd_tab) {
  clear_ins_upd_nodes();

  trx_start_if_not_started_xa(m_prebuilt->trx, true);

  if (alter_parts::need_copy(ha_alter_info) &&
      prepare_for_copy_partitions(ha_alter_info)) {
    my_error(ER_OUT_OF_RESOURCES, MYF(0));
    return (true);
  }

  alter_parts *ctx =
      UT_NEW_NOKEY(alter_parts(m_prebuilt->trx, m_part_share, ha_alter_info,
                               m_part_info, m_new_partitions));

  if (ctx == nullptr) {
    my_error(ER_OUT_OF_RESOURCES, MYF(0));
    return (true);
  }

  ha_alter_info->handler_ctx = ctx;

  int error = ctx->prepare(*old_dd_tab, *new_dd_tab, altered_table);
  if (error != 0) {
    print_error(error, MYF(error != ER_OUTOFMEMORY ? 0 : ME_FATALERROR));
  }
  return (error);
}

/** Alter the table structure in-place with operations
specified using HA_ALTER_FLAGS and Alter_inplace_information.
This is for 'ALTER TABLE ... PARTITION' and a corresponding function
to inplace_alter_table().
The level of concurrency allowed during this operation depends
on the return value from check_if_supported_inplace_alter().

@param[in,out]	altered_table	TABLE object for new version of table
@param[in,out]	ha_alter_info	Structure describing changes to be done
                                by ALTER TABLE and holding data used during
                                in-place alter.
@param[in]	old_dd_tab	Table definition before the ALTER
@param[in,out]	new_dd_tab	Table definition after the ALTER
@retval true	Failure
@retval false	Success */
bool ha_innopart::inplace_alter_partition(TABLE *altered_table,
                                          Alter_inplace_info *ha_alter_info,
                                          const dd::Table *old_dd_tab,
                                          dd::Table *new_dd_tab) {
  if (!alter_parts::need_copy(ha_alter_info)) {
    return (false);
  }

  /* The lock type can be set as none, since in this step, the
  shared table lock is held, thus no other changes. This is to fix
  if the table was explicitly lock, then select_lock_type in the
  prebuilt here would not be LOCK_NONE, then row locks would be
  required; if we finally want to drop the original partitions,
  these row locks would lead to failure/crash. */
  ulint lock_type = m_prebuilt->select_lock_type;
  m_prebuilt->select_lock_type = LOCK_NONE;

  prepare_change_partitions();

  partition_info *old_part_info = table->part_info;

  set_part_info(ha_alter_info->modified_part_info, true);

  prepare_change_partitions();

  ulonglong deleted;
  int res;

  res = copy_partitions(&deleted);

  set_part_info(old_part_info, false);

  m_prebuilt->select_lock_type = lock_type;

  if (res > 0) {
    print_error(res, MYF(res != ER_OUTOFMEMORY ? 0 : ME_FATALERROR));
  }

  return (res);
}

/** Prepare to commit or roll back ALTER TABLE...ALGORITHM=INPLACE.
This is for 'ALTER TABLE ... PARTITION' and a corresponding function
to commit_inplace_alter_table().
@param[in,out]	altered_table	TABLE object for new version of table.
@param[in,out]	ha_alter_info	ALGORITHM=INPLACE metadata
@param[in]	commit		true=Commit, false=Rollback.
@param[in]	old_dd_tab	old table
@param[in,out]	new_dd_tab	new table
@retval true	on failure (my_error() will have been called)
@retval false	on success */
bool ha_innopart::commit_inplace_alter_partition(
    TABLE *altered_table, Alter_inplace_info *ha_alter_info, bool commit,
    const dd::Table *old_dd_tab, dd::Table *new_dd_tab) {
  alter_parts *ctx = static_cast<alter_parts *>(ha_alter_info->handler_ctx);
  if (ctx == nullptr) {
    ut_ad(!commit);
    return (false);
  }

  if (commit) {
    int error = ctx->try_commit(*old_dd_tab, *new_dd_tab, table, altered_table);
    if (!error) {
      m_prebuilt->table = nullptr;

      UT_DELETE(ctx);
      ha_alter_info->handler_ctx = nullptr;

      UT_DELETE(m_new_partitions);
      m_new_partitions = nullptr;

      if (altered_table->found_next_number_field) {
        dd_set_autoinc(new_dd_tab->se_private_data(),
                       m_part_share->next_auto_inc_val);
      }

      dd_copy_table(*new_dd_tab, *old_dd_tab);
      dd_part_adjust_table_id(new_dd_tab);
      if (!dd_table_part_has_instant_cols(*new_dd_tab) &&
          dd_table_has_instant_cols(*new_dd_tab)) {
        dd_clear_instant_table(*new_dd_tab);
      }
    }

    return (error != 0);
  }

  ctx->rollback();
  UT_DELETE(ctx);
  ha_alter_info->handler_ctx = nullptr;

  UT_DELETE(m_new_partitions);
  m_new_partitions = nullptr;

  return (false);
}

/** Check if the DATA DIRECTORY is specified (implicitly or explicitly)
@param[in]	dd_part		The dd::Partition to be checked
@retval true	the DATA DIRECTORY is specified (implicitly or explicitly)
@retval false	otherwise */
static bool dd_part_has_datadir(const dd::Partition *dd_part) {
  ut_ad(dd_part_is_stored(dd_part));

  return (dd_part->options().exists(data_file_name_key) ||
          (dd_part->parent() != nullptr &&
           dd_part->parent()->options().exists(data_file_name_key)) ||
          dd_part->table().se_private_data().exists(
              dd_table_key_strings[DD_TABLE_DATA_DIRECTORY]));
}

/** Exchange partition.
Low-level primitive which implementation is provided here.
@param[in]	part_table_path		data file path of the partitioned table
@param[in]	swap_table_path		data file path of the to be
swapped table
@param[in]	part_id			The id of the partition to be exchanged
@param[in]	part_table		partitioned table to be exchanged
@param[in]	swap_table		table to be exchanged
@return error number
@retval 0	on success */
int ha_innopart::exchange_partition_low(const char *part_table_path,
                                        const char *swap_table_path,
                                        uint part_id, dd::Table *part_table,
                                        dd::Table *swap_table) {
  DBUG_ENTER("ha_innopart::exchange_partition_low");

  ut_ad(part_table != nullptr);
  ut_ad(swap_table != nullptr);
  ut_ad(m_part_share != nullptr);
  ut_ad(dd_table_is_partitioned(*part_table));
  ut_ad(!dd_table_is_partitioned(*swap_table));
  ut_ad(innobase_strcasecmp(part_table->name().c_str(),
                            table_share->table_name.str) == 0);
  ut_ad(part_id < m_tot_parts);

  if (high_level_read_only) {
    my_error(ER_READ_ONLY_MODE, MYF(0));
    DBUG_RETURN(HA_ERR_TABLE_READONLY);
  }

  if (dd_table_has_instant_cols(*part_table) ||
      dd_table_has_instant_cols(*swap_table)) {
    my_error(ER_PARTITION_EXCHANGE_DIFFERENT_OPTION, MYF(0),
             "INSTANT COLUMN(s)");
    DBUG_RETURN(true);
  }

  /* Find the specified dd::Partition object */
  uint id = 0;
  dd::Partition *dd_part = nullptr;
  for (auto part : *part_table->leaf_partitions()) {
    ut_d(dict_table_t *table = m_part_share->get_table_part(id));
    ut_ad(table->n_ref_count == 1);
    ut_ad(!table->is_temporary());

    if (++id > part_id) {
      dd_part = part;
      break;
    }
  }
  ut_ad(dd_part != nullptr);

  /* According to current restriction, all options should be equal
  between partition and table. And DATA DIRECTORY and INDEX DIRECTORY
  should not be set */
  if (dd_part_has_datadir(dd_part) ||
      swap_table->options().exists(data_file_name_key)) {
    my_error(ER_PARTITION_EXCHANGE_DIFFERENT_OPTION, MYF(0), "DATA DIRECTORY");
    DBUG_RETURN(true);
  }
  if (dd_part->options().exists(index_file_name_key) ||
      swap_table->options().exists(index_file_name_key)) {
    ut_ad(0);
    my_error(ER_PARTITION_EXCHANGE_DIFFERENT_OPTION, MYF(0), "INDEX DIRECTORY");
    DBUG_RETURN(true);
  }

  /* Get the innodb table objects of part_table and swap_table */
  const table_id_t table_id = swap_table->se_private_id();
  dict_table_t *part = m_part_share->get_table_part(part_id);
  dict_table_t *swap;
  const ulint fold = ut_fold_ull(table_id);

  mutex_enter(&dict_sys->mutex);
  HASH_SEARCH(id_hash, dict_sys->table_id_hash, fold, dict_table_t *, swap,
              ut_ad(swap->cached), swap->id == table_id);
  mutex_exit(&dict_sys->mutex);
  ut_ad(swap != nullptr);
  ut_ad(swap->n_ref_count == 1);

  /* Declare earlier before 'goto' */
  auto swap_i = swap_table->indexes()->begin();
  ut_d(auto part_table_i = part_table->indexes()->begin());
  dd::Object_id p_se_id = dd_part->se_private_id();

  /* Try to rename files. Tablespace checking ensures that
  both partition and table are of implicit tablespace. The plan is:
  1. Rename the swap table to the intermediate file
  2. Rename the partition to the swap table file
  3. Rename the intermediate file of swap table to the partition file */
  THD *thd = m_prebuilt->trx->mysql_thd;
  char *swap_name = strdup(swap->name.m_name);
  char *part_name = strdup(part->name.m_name);

  /* Define the temporary table name, by appending TMP_POSTFIX */
  char temp_name[FN_REFLEN];
  snprintf(temp_name, sizeof temp_name, "%s%s", swap_name, TMP_POSTFIX);

  int error = 0;
  error = innobase_basic_ddl::rename_impl<dd::Table>(thd, swap_name, temp_name,
                                                     swap_table, swap_table);
  if (error != 0) {
    goto func_exit;
  }
  error = innobase_basic_ddl::rename_impl<dd::Partition>(
      thd, part_name, swap_name, dd_part, dd_part);
  if (error != 0) {
    goto func_exit;
  }
  error = innobase_basic_ddl::rename_impl<dd::Table>(thd, temp_name, part_name,
                                                     swap_table, swap_table);
  if (error != 0) {
    goto func_exit;
  }

  /* Swap the se_private_data and options between indexes.
  The se_private_data should be swapped between every index of
  dd_part and swap_table; however, options should be swapped(checked)
  between part_table and swap_table */
  for (auto part_index : *dd_part->indexes()) {
    ut_ad(swap_i != swap_table->indexes()->end());
    auto swap_index = *swap_i;
    ++swap_i;

    dd::Object_id p_tablespace_id = part_index->tablespace_id();
    part_index->set_tablespace_id(swap_index->tablespace_id());
    swap_index->set_tablespace_id(p_tablespace_id);

    ut_ad(part_index->se_private_data().empty() ==
          swap_index->se_private_data().empty());
    ut_ad(part_index->se_private_data().size() ==
          swap_index->se_private_data().size());

    if (!part_index->se_private_data().empty()) {
      dd::Properties_impl p_se_data;
      p_se_data.assign(part_index->se_private_data());
      part_index->se_private_data().clear();
      part_index->se_private_data().assign(swap_index->se_private_data());
      swap_index->se_private_data().clear();
      swap_index->se_private_data().assign(p_se_data);
    }

    ut_ad(part_table_i != part_table->indexes()->end());
    ut_d(auto part_table_index = *part_table_i);
    ut_d(++part_table_i);
    ut_ad(part_table_index->options().raw_string() ==
          swap_index->options().raw_string());
  }
  ut_ad(part_table_i == part_table->indexes()->end());
  ut_ad(swap_i == swap_table->indexes()->end());

  /* Swap the se_private_data and options of the two tables.
  Only the max autoinc should be set to both tables */
  if (m_part_share->get_table_share()->found_next_number_field) {
    uint64 part_autoinc = part->autoinc;
    uint64 swap_autoinc = swap->autoinc;
    uint64 max_autoinc = std::max(part_autoinc, swap_autoinc);

    dd_set_autoinc(swap_table->se_private_data(), max_autoinc);
    dd_set_autoinc(part_table->se_private_data(),
                   std::max(swap_autoinc, m_part_share->next_auto_inc_val));

    dict_table_autoinc_lock(part);
    dict_table_autoinc_initialize(part, max_autoinc);
    dict_table_autoinc_unlock(part);

    if (m_part_share->next_auto_inc_val < swap_autoinc) {
      lock_auto_increment();
      m_part_share->next_auto_inc_val = swap_autoinc;
      unlock_auto_increment();
    }
  }

  /* Swap the se_private_id between partition and table */
  dd_part->set_se_private_id(swap_table->se_private_id());
  swap_table->set_se_private_id(p_se_id);

func_exit:
  free(swap_name);
  free(part_name);

  DBUG_RETURN(error);
}

/**
@param thd the session
@param start_value the lower bound
@param max_value the upper bound (inclusive) */

ib_sequence_t::ib_sequence_t(THD *thd, ulonglong start_value,
                             ulonglong max_value)
    : m_max_value(max_value),
      m_increment(0),
      m_offset(0),
      m_next_value(start_value),
      m_eof(false) {
  if (thd != 0 && m_max_value > 0) {
    thd_get_autoinc(thd, &m_offset, &m_increment);

    if (m_increment > 1 || m_offset > 1) {
      /* If there is an offset or increment specified
      then we need to work out the exact next value. */

      m_next_value = innobase_next_autoinc(start_value, 1, m_increment,
                                           m_offset, m_max_value);

    } else if (start_value == 0) {
      /* The next value can never be 0. */
      m_next_value = 1;
    }
  } else {
    m_eof = true;
  }
}

/**
Postfix increment
@return the next value to insert */

ulonglong ib_sequence_t::operator++(int)UNIV_NOTHROW {
  ulonglong current = m_next_value;

  ut_ad(!m_eof);
  ut_ad(m_max_value > 0);

  m_next_value =
      innobase_next_autoinc(current, 1, m_increment, m_offset, m_max_value);

  if (m_next_value == m_max_value && current == m_next_value) {
    m_eof = true;
  }

  return (current);
}
