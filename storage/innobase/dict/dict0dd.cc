/*****************************************************************************

Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

/** @file dict/dict0dd.cc
Data dictionary interface */

#ifndef UNIV_HOTBACKUP
#include <auto_thd.h>
#include <current_thd.h>
#include <sql/thd_raii.h>
#include <sql_backup_lock.h>
#include <sql_class.h>
#include <sql_thd_internal_api.h>
#include "item.h"
#else /* !UNIV_HOTBACKUP */
#include <my_base.h>
#endif /* !UNIV_HOTBACKUP */

#include <dd/properties.h>
#include "dict0crea.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "dict0mem.h"
#include "dict0priv.h"
#include "sql/dd/impl/types/column_impl.h"
#include "sql/dd/types/column_type_element.h"
#ifndef UNIV_HOTBACKUP
#include "dict0stats.h"
#endif /* !UNIV_HOTBACKUP */
#include "data0type.h"
#include "dict0dict.h"
#include "fil0fil.h"
#include "mach0data.h"
#include "rem0rec.h"
#ifndef UNIV_HOTBACKUP
#include "fts0priv.h"
#include "gis/rtree_support.h"  // fetch_srs
#endif                          /* !UNIV_HOTBACKUP */
#include "srv0start.h"
#include "ut0crc32.h"
#ifndef UNIV_HOTBACKUP
#include "btr0sea.h"
#include "derror.h"
#include "fts0plugin.h"
#include "ha_innodb.h"
#include "ha_innopart.h"
#include "ha_prototypes.h"
#include "mysql/plugin.h"
#include "query_options.h"
#include "sql/create_field.h"
#include "sql/mysqld.h"  // lower_case_file_system
#include "sql_base.h"
#include "sql_table.h"
#endif /* !UNIV_HOTBACKUP */

const char *DD_instant_col_val_coder::encode(const byte *stream, size_t in_len,
                                             size_t *out_len) {
  cleanup();

  m_result = ut::new_arr_withkey<byte>(UT_NEW_THIS_FILE_PSI_KEY,
                                       ut::Count{in_len * 2});
  char *result = reinterpret_cast<char *>(m_result);

  for (size_t i = 0; i < in_len; ++i) {
    uint8_t v1 = ((stream[i] & 0xF0) >> 4);
    uint8_t v2 = (stream[i] & 0x0F);

    result[i * 2] = (v1 < 10 ? '0' + v1 : 'a' + v1 - 10);
    result[i * 2 + 1] = (v2 < 10 ? '0' + v2 : 'a' + v2 - 10);
  }

  *out_len = in_len * 2;

  return result;
}

const byte *DD_instant_col_val_coder::decode(const char *stream, size_t in_len,
                                             size_t *out_len) {
  ut_ad(in_len % 2 == 0);

  cleanup();

  m_result = ut::new_arr_withkey<byte>(UT_NEW_THIS_FILE_PSI_KEY,
                                       ut::Count{in_len / 2});

  for (size_t i = 0; i < in_len / 2; ++i) {
    char c1 = stream[i * 2];
    char c2 = stream[i * 2 + 1];

    ut_ad(isdigit(c1) || (c1 >= 'a' && c1 <= 'f'));
    ut_ad(isdigit(c2) || (c2 >= 'a' && c2 <= 'f'));

    m_result[i] = ((isdigit(c1) ? c1 - '0' : c1 - 'a' + 10) << 4) +
                  ((isdigit(c2) ? c2 - '0' : c2 - 'a' + 10));
  }

  *out_len = in_len / 2;

  return m_result;
}

#ifndef UNIV_HOTBACKUP
bool dd_is_valid_row_version(uint32_t version) {
  return (version != UINT32_UNDEFINED && version > 0 &&
          version <= (uint32_t)MAX_ROW_VERSION);
}

bool dd_column_is_added(const dd::Column *dd_col) {
  const char *s = dd_column_key_strings[DD_INSTANT_VERSION_ADDED];
  if (!dd_col->se_private_data().exists(s)) {
    return false;
  }

#ifdef UNIV_DEBUG
  uint32_t version = UINT32_UNDEFINED;
  dd_col->se_private_data().get(s, &version);
  ut_ad(dd_is_valid_row_version(version));
#endif

  return true;
}

bool dd_column_is_dropped(const dd::Column *dd_col) {
  const char *s = dd_column_key_strings[DD_INSTANT_VERSION_DROPPED];
  if (!dd_col->se_private_data().exists(s)) {
    return false;
  }

#ifdef UNIV_DEBUG
  uint32_t version = UINT32_UNDEFINED;
  dd_col->se_private_data().get(s, &version);
  ut_ad(dd_is_valid_row_version(version));
#endif

  return true;
}

uint32_t dd_column_get_version_added(const dd::Column *dd_col) {
  if (!dd_column_is_added(dd_col)) {
    return UINT32_UNDEFINED;
  }

  uint32_t version = UINT32_UNDEFINED;
  dd_col->se_private_data().get(dd_column_key_strings[DD_INSTANT_VERSION_ADDED],
                                &version);
  ut_a(dd_is_valid_row_version(version));
  return (version);
}

uint32_t dd_column_get_version_dropped(const dd::Column *dd_col) {
  if (!dd_column_is_dropped(dd_col)) {
    return UINT32_UNDEFINED;
  }

  uint32_t version = UINT32_UNDEFINED;
  dd_col->se_private_data().get(
      dd_column_key_strings[DD_INSTANT_VERSION_DROPPED], &version);
  ut_a(dd_is_valid_row_version(version));
  return (version);
}

/** Check if the InnoDB index is consistent with dd::Index
@param[in]      index           InnoDB index
@param[in]      dd_index        dd::Index or dd::Partition_index
@return true    if match
@retval false   if not match */
template <typename Index>
static bool dd_index_match(const dict_index_t *index, const Index *dd_index) {
  bool match = true;

  /* Don't check the name for primary index, since internal index
  name could be variant */
  if (my_strcasecmp(system_charset_info, index->name(),
                    dd_index->name().c_str()) != 0 &&
      strcmp(dd_index->name().c_str(), "PRIMARY") != 0) {
    ib::warn(ER_IB_MSG_162)
        << "Index name in InnoDB is " << index->name()
        << " while index name in global DD is " << dd_index->name();
    match = false;
  }

  const dd::Properties &p = dd_index->se_private_data();
  uint64_t id = 0;
  uint32_t root = 0;
  uint64_t trx_id = 0;
  ut_ad(p.exists(dd_index_key_strings[DD_INDEX_ID]));
  p.get(dd_index_key_strings[DD_INDEX_ID], &id);
  if (id != index->id) {
    ib::warn(ER_IB_MSG_163)
        << "Index id in InnoDB is " << index->id << " while index id in"
        << " global DD is " << id;
    match = false;
  }

  ut_ad(p.exists(dd_index_key_strings[DD_INDEX_ROOT]));
  p.get(dd_index_key_strings[DD_INDEX_ROOT], &root);
  if (root != index->page) {
    ib::warn(ER_IB_MSG_164)
        << "Index root in InnoDB is " << index->page << " while index root in"
        << " global DD is " << root;
    match = false;
  }

  ut_ad(p.exists(dd_index_key_strings[DD_INDEX_TRX_ID]));
  p.get(dd_index_key_strings[DD_INDEX_TRX_ID], &trx_id);
  /* For DD tables, the trx_id=0 is got from get_se_private_id().
  TODO: index->trx_id is not expected to be 0 once Bug#25730513 is fixed*/
  if (trx_id != 0 && index->trx_id != 0 && trx_id != index->trx_id) {
    ib::warn(ER_IB_MSG_165) << "Index transaction id in InnoDB is "
                            << index->trx_id << " while index transaction"
                            << " id in global DD is " << trx_id;
    match = false;
  }

  return match;
}

/** Check if the InnoDB table is consistent with dd::Table
@tparam         Table           dd::Table or dd::Partition
@param[in]      table                   InnoDB table
@param[in]      dd_table                dd::Table or dd::Partition
@return true    if match
@retval false   if not match */
template <typename Table>
bool dd_table_match(const dict_table_t *table, const Table *dd_table) {
  /* Temporary table has no metadata written */
  if (dd_table == nullptr || table->is_temporary()) {
    return true;
  }

  bool match = true;

  if (dd_table->se_private_id() != table->id) {
    ib::warn(ER_IB_MSG_166)
        << "Table id in InnoDB is " << table->id
        << " while the id in global DD is " << dd_table->se_private_id();
    match = false;
  }

  /* If tablespace is discarded, no need to check indexes */
  if (dict_table_is_discarded(table)) {
    return match;
  }

  for (const auto dd_index : dd_table->indexes()) {
    if (dd_table->tablespace_id() == dict_sys_t::s_dd_sys_space_id &&
        dd_index->tablespace_id() != dd_table->tablespace_id()) {
      ib::warn(ER_IB_MSG_167)
          << "Tablespace id in table is " << dd_table->tablespace_id()
          << ", while tablespace id in index " << dd_index->name() << " is "
          << dd_index->tablespace_id();
    }

    const dict_index_t *index = dd_find_index(table, dd_index);
    ut_ad(index != nullptr);

    if (!dd_index_match(index, dd_index)) {
      match = false;
    }
  }

  /* Tablespace and options can be checked here too */
  return match;
}

template bool dd_table_match<dd::Table>(const dict_table_t *,
                                        const dd::Table *);
template bool dd_table_match<dd::Partition>(const dict_table_t *,
                                            const dd::Partition *);

/** Release a metadata lock.
@param[in,out]  thd     current thread
@param[in,out]  mdl     metadata lock */
void dd_mdl_release(THD *thd, MDL_ticket **mdl) {
  if (*mdl == nullptr) {
    return;
  }

  dd::release_mdl(thd, *mdl);
  *mdl = nullptr;
}

THD *dd_thd_for_undo(const trx_t *trx) {
  return trx->mysql_thd == nullptr ? current_thd : trx->mysql_thd;
}

/** Check if current undo needs a MDL or not
@param[in]      trx     transaction
@return true if MDL is necessary, otherwise false */
bool dd_mdl_for_undo(const trx_t *trx) {
  /* Try best to find a valid THD for checking, in case in background
  rollback thread, trx doesn't hold a mysql_thd */
  THD *thd = dd_thd_for_undo(trx);

  /* There are four cases for the undo to check here:
  1. In recovery phase, binlog recover, there is no concurrent
  user queries, so MDL is no necessary. In this case, thd is NULL.
  2. In background rollback thread, there could be concurrent
  user queries, so MDL is needed. In this case, thd is not NULL
  3. In runtime transaction rollback, no need for MDL.
  THD::transaction_rollback_request would be set.
  4. In runtime asynchronous rollback, no need for MDL.
  Check TRX_FORCE_ROLLBACK. */
  return (thd != nullptr && !thd->transaction_rollback_request &&
          ((trx->in_innodb & TRX_FORCE_ROLLBACK) == 0));
}

int acquire_uncached_table(THD *thd, dd::cache::Dictionary_client *client,
                           const dd::Table *dd_table, const char *name,
                           TABLE_SHARE *ts, TABLE *td) {
  int error = 0;
  dd::Schema *schema;
  const char *table_cache_key;
  size_t table_cache_key_len;

  if (name != nullptr) {
    schema = nullptr;
    table_cache_key = name;
    table_cache_key_len = dict_get_db_name_len(name);
  } else {
    error =
        client->acquire_uncached<dd::Schema>(dd_table->schema_id(), &schema);
    if (error != 0) {
      return (error);
    }
    table_cache_key = schema->name().c_str();
    table_cache_key_len = schema->name().size();
  }

  init_tmp_table_share(thd, ts, table_cache_key, table_cache_key_len,
                       dd_table->name().c_str(), "" /* file name */, nullptr);

  error = open_table_def_suppress_invalid_meta_data(thd, ts, dd_table->table());

  if (error == 0) {
    error = open_table_from_share(thd, ts, (dd_table->table()).name().c_str(),
                                  0, SKIP_NEW_HANDLER, 0, td, false, dd_table);
  }
  if (error != 0) {
    free_table_share(ts);
  }
  return error;
}

void release_uncached_table(TABLE_SHARE *ts, TABLE *td) {
  closefrm(td, false);
  free_table_share(ts);
}

int dd_table_open_on_dd_obj(THD *thd, dd::cache::Dictionary_client *client,
                            const dd::Table &dd_table,
                            const dd::Partition *dd_part, const char *tbl_name,
                            dict_table_t *&table, const TABLE *td) {
#ifdef UNIV_DEBUG
  if (dd_part != nullptr) {
    ut_ad(&dd_part->table() == &dd_table);
    ut_ad(dd_table.se_private_id() == dd::INVALID_OBJECT_ID);
    ut_ad(dd_table_is_partitioned(dd_table));

    ut_ad(dd_part->parent_partition_id() == dd::INVALID_OBJECT_ID ||
          dd_part->parent() != nullptr);

    ut_ad(((dd_part->table().subpartition_type() != dd::Table::ST_NONE) ==
           (dd_part->parent() != nullptr)));
  }
#endif /* UNIV_DEBUG */

  int error = 0;
  const table_id_t table_id =
      dd_part == nullptr ? dd_table.se_private_id() : dd_part->se_private_id();
  const auto hash_value = ut::hash_uint64(table_id);

  ut_ad(table_id != dd::INVALID_OBJECT_ID);

  dict_sys_mutex_enter();

  HASH_SEARCH(id_hash, dict_sys->table_id_hash, hash_value, dict_table_t *,
              table, ut_ad(table->cached), table->id == table_id);

  if (table != nullptr) {
    table->acquire();
  }

  dict_sys_mutex_exit();

  if (table != nullptr) {
    return 0;
  }

#ifdef UNIV_DEBUG
  /* If this is a internal temporary table, it's impossible
  to verify the MDL against the table name, because both the
  database name and table name may be invalid for MDL */
  if (tbl_name && !row_is_mysql_tmp_table_name(tbl_name)) {
    std::string db_str;
    std::string tbl_str;
    dict_name::get_table(tbl_name, db_str, tbl_str);

    ut_ad(innobase_strcasecmp(dd_table.name().c_str(), tbl_str.c_str()) == 0);
  }
#endif /* UNIV_DEBUG */

  if (td != nullptr) {
    ut_ad(tbl_name != nullptr);

    if (dd_part) {
      table = dd_open_table(client, td, tbl_name, dd_part, thd);
    } else {
      table = dd_open_table(client, td, tbl_name, &dd_table, thd);
    }
    return 0;
  }

  TABLE_SHARE ts;
  TABLE table_def;
  dd::Schema *schema;

  error =
      acquire_uncached_table(thd, client, &dd_table, tbl_name, &ts, &table_def);
  if (error != 0) {
    return (error);
  }

  char tmp_name[MAX_FULL_NAME_LEN + 1];
  const char *tab_namep;
  if (tbl_name) {
    tab_namep = tbl_name;
  } else {
    char tmp_schema[MAX_DATABASE_NAME_LEN + 1];
    char tmp_tablename[MAX_TABLE_NAME_LEN + 1];
    error = client->acquire_uncached<dd::Schema>(dd_table.schema_id(), &schema);
    if (error != 0) {
      return error;
    }
    tablename_to_filename(schema->name().c_str(), tmp_schema,
                          MAX_DATABASE_NAME_LEN + 1);
    tablename_to_filename(dd_table.name().c_str(), tmp_tablename,
                          MAX_TABLE_NAME_LEN + 1);
    snprintf(tmp_name, sizeof tmp_name, "%s/%s", tmp_schema, tmp_tablename);
    tab_namep = tmp_name;
  }
  if (dd_part == nullptr) {
    table = dd_open_table(client, &table_def, tab_namep, &dd_table, thd);
    if (table == nullptr) {
      error = HA_ERR_GENERIC;
    }
  } else {
    table = dd_open_table(client, &table_def, tab_namep, dd_part, thd);
  }
  release_uncached_table(&ts, &table_def);
  return error;
}

/** Load an InnoDB table definition by InnoDB table ID.
@param[in,out]  thd             current thread
@param[in,out]  mdl             metadata lock;
nullptr if we are resurrecting table IX locks in recovery
@param[in]      table_id        InnoDB table or partition ID
@return InnoDB table
@retval nullptr if the table is not found, or there was an error */
static dict_table_t *dd_table_open_on_id_low(THD *thd, MDL_ticket **mdl,
                                             table_id_t table_id) {
  std::string part_table;
  const char *name_to_open = nullptr;

  ut_ad(thd == nullptr || thd == current_thd);
#ifdef UNIV_DEBUG
  btrsea_sync_check check(false);
  ut_ad(!sync_check_iterate(check));
#endif
  ut_ad(srv_shutdown_state.load() < SRV_SHUTDOWN_DD);

  if (thd == nullptr) {
    ut_ad(mdl == nullptr);
    thd = current_thd;
  }

  /* During server startup, while recovering XA transaction we don't have THD.
  The table should have been already in innodb cache if present in DD while
  resurrecting transaction. We assume the table is not in DD and return. We
  cannot continue anyway here with NULL THD. */
  if (thd == nullptr) {
    return nullptr;
  }

  dict_table_t *ib_table = nullptr;

  {
    const dd::Table *dd_table;
    const dd::Partition *dd_part = nullptr;
    dd::cache::Dictionary_client *dc = dd::get_dd_client(thd);
    dd::cache::Dictionary_client::Auto_releaser releaser(dc);

    /* Since we start with table se_private_id, and we do not have
    table name, so we cannot MDL lock the table(name). So we will
    try to get the table name without MDL protection, and verify later,
    after we got the table name and MDL lock it. Thus a loop is needed
    in case the verification failed, and another attempt is made until
    all things matches */
    for (;;) {
      dd::String_type schema;
      dd::String_type tablename;
      if (dc->get_table_name_by_se_private_id(handler_name, table_id, &schema,
                                              &tablename)) {
        return nullptr;
      }

      const bool not_table = schema.empty();

      if (not_table) {
        if (dc->get_table_name_by_partition_se_private_id(
                handler_name, table_id, &schema, &tablename) ||
            schema.empty()) {
          return nullptr;
        }
      }

      /* Now we have tablename, and MDL locked it if necessary. */
      if (mdl != nullptr) {
        if (*mdl == nullptr &&
            dd_mdl_acquire(thd, mdl, schema.c_str(), tablename.c_str())) {
          return nullptr;
        }

        ut_ad(*mdl != nullptr);
      }

      if (dc->acquire(schema, tablename, &dd_table) || dd_table == nullptr) {
        if (mdl != nullptr) {
          dd_mdl_release(thd, mdl);
        }
        return nullptr;
      }

      const bool is_part = dd_table_is_partitioned(*dd_table);

      /* Verify facts between dd_table and facts we know
      1) Partition table or not
      2) Table ID matches or not
      3) Table in InnoDB */
      bool same_name = not_table == is_part &&
                       (not_table || dd_table->se_private_id() == table_id) &&
                       dd_table->engine() == handler_name;

      /* Do more verification for partition table */
      if (same_name && is_part) {
        auto end = dd_table->leaf_partitions().end();
        auto i =
            std::search_n(dd_table->leaf_partitions().begin(), end, 1, table_id,
                          [](const dd::Partition *p, table_id_t id) {
                            return (p->se_private_id() == id);
                          });

        if (i == end) {
          same_name = false;
        } else {
          dd_part = *i;
          ut_ad(dd_part_is_stored(dd_part));

          std::string partition;
          /* Build the partition name. */
          dict_name::build_partition(dd_part, partition);

          /* Build the partitioned table name. */
          dict_name::build_table(schema.c_str(), tablename.c_str(), partition,
                                 false, true, part_table);
          name_to_open = part_table.c_str();
        }
      }

      /* facts do not match, retry */
      if (!same_name) {
        if (mdl != nullptr) {
          dd_mdl_release(thd, mdl);
        }
        continue;
      }

      ut_ad(same_name);
      break;
    }

    ut_ad(dd_part != nullptr || dd_table->se_private_id() == table_id);
    ut_ad(dd_part == nullptr || dd_table == &dd_part->table());
    ut_ad(dd_part == nullptr || dd_part->se_private_id() == table_id);

    dd_table_open_on_dd_obj(thd, dc, *dd_table, dd_part, name_to_open, ib_table,
                            nullptr);
  }

  if (mdl && ib_table == nullptr) {
    dd_mdl_release(thd, mdl);
  }

  return ib_table;
}
#endif /* !UNIV_HOTBACKUP */

/** Check if access to a table should be refused.
@param[in,out]  table   InnoDB table or partition
@return error code
@retval 0 on success (DD_SUVCCESS) */
[[nodiscard]] static int dd_check_corrupted(dict_table_t *&table) {
  if (table->is_corrupted()) {
    if (dict_table_is_sdi(table->id)
#ifndef UNIV_HOTBACKUP
        || dict_table_is_system(table->id)
#endif /* !UNIV_HOTBACKUP */
    ) {
#ifndef UNIV_HOTBACKUP
      my_error(ER_TABLE_CORRUPT, MYF(0), "", table->name.m_name);
#else  /* !UNIV_HOTBACKUP */
      ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_168)
          << "table is corrupt: " << table->name.m_name;
#endif /* !UNIV_HOTBACKUP */
    } else {
#ifndef UNIV_HOTBACKUP
      std::string db_str;
      std::string tbl_str;
      dict_name::get_table(table->name.m_name, db_str, tbl_str);

      my_error(ER_TABLE_CORRUPT, MYF(0), db_str.c_str(), tbl_str.c_str());
#else  /* !UNIV_HOTBACKUP */
      ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_169)
          << "table is corrupt: " << table->name.m_name;
#endif /* !UNIV_HOTBACKUP */
    }
    table = nullptr;
    return HA_ERR_TABLE_CORRUPT;
  }

  dict_index_t *index = table->first_index();
  if (!dict_table_is_sdi(table->id) && fil_space_get(index->space) == nullptr) {
#ifndef UNIV_HOTBACKUP
    if (!dict_table_is_discarded(table)) {
      my_error(ER_TABLESPACE_MISSING, MYF(0), table->name.m_name);
    }
#else  /* !UNIV_HOTBACKUP */
    ib::fatal(UT_LOCATION_HERE, ER_IB_MSG_170)
        << "table space is missing: " << table->name.m_name;
#endif /* !UNIV_HOTBACKUP */
    table = nullptr;
    return HA_ERR_TABLESPACE_MISSING;
  }

  /* Ignore missing tablespaces for secondary indexes. */
  for (;;) {
    index = index->next();
    if (!index) break;
    if (!index->is_corrupted() && fil_space_get(index->space) == nullptr) {
      dict_set_corrupted(index);
    }
  }

  return 0;
}

/** Open a persistent InnoDB table based on InnoDB table id, and
hold Shared MDL lock on it.
@param[in]      table_id                table identifier
@param[in,out]  thd                     current MySQL connection (for mdl)
@param[in,out]  mdl                     metadata lock (*mdl set if
table_id was found) mdl=NULL if we are resurrecting table IX locks in recovery
@param[in]      dict_locked             dict_sys mutex is held
@param[in]      check_corruption        check if the table is corrupted or not.
@return table
@retval NULL if the table does not exist or cannot be opened */
dict_table_t *dd_table_open_on_id(table_id_t table_id, THD *thd,
                                  MDL_ticket **mdl, bool dict_locked,
                                  bool check_corruption) {
  dict_table_t *ib_table;
  const auto hash_value = ut::hash_uint64(table_id);
  char full_name[MAX_FULL_NAME_LEN + 1];

  if (!dict_locked) {
    dict_sys_mutex_enter();
  }

  HASH_SEARCH(id_hash, dict_sys->table_id_hash, hash_value, dict_table_t *,
              ib_table, ut_ad(ib_table->cached), ib_table->id == table_id);

reopen:
  if (ib_table == nullptr) {
#ifndef UNIV_HOTBACKUP
    if (dict_table_is_sdi(table_id)) {
      /* The table is SDI table */
      space_id_t space_id = dict_sdi_get_space_id(table_id);

      /* Create in-memory table object for SDI table */
      dict_index_t *sdi_index =
          dict_sdi_create_idx_in_mem(space_id, false, 0, false);

      if (sdi_index == nullptr) {
        if (!dict_locked) {
          dict_sys_mutex_exit();
        }
        return nullptr;
      }

      ib_table = sdi_index->table;

      ut_ad(ib_table != nullptr);
      ib_table->acquire();

      if (!dict_locked) {
        dict_sys_mutex_exit();
      }
    } else {
      dict_sys_mutex_exit();

      ib_table = dd_table_open_on_id_low(thd, mdl, table_id);

      if (dict_locked) {
        dict_sys_mutex_enter();
      }
    }
#else  /* !UNIV_HOTBACKUP */
    /* PRELIMINARY TEMPORARY WORKAROUND: is this ever used? */
    bool not_hotbackup = false;
    ut_a(not_hotbackup);
#endif /* !UNIV_HOTBACKUP */
  } else if (mdl == nullptr || ib_table->is_temporary() ||
             dict_table_is_sdi(ib_table->id)) {
    if (dd_check_corrupted(ib_table)) {
      ut_ad(ib_table == nullptr);
    } else {
      ib_table->acquire();
    }

    if (!dict_locked) {
      dict_sys_mutex_exit();
    }
  } else {
    for (;;) {
#ifndef UNIV_HOTBACKUP
      std::string db_str;
      std::string tbl_str;
      dict_name::get_table(ib_table->name.m_name, db_str, tbl_str);
#endif /* !UNIV_HOTBACKUP */

      memset(full_name, 0, MAX_FULL_NAME_LEN + 1);

      strcpy(full_name, ib_table->name.m_name);

      ut_ad(!ib_table->is_temporary());

      dict_sys_mutex_exit();

#ifndef UNIV_HOTBACKUP
      if (db_str.empty() || tbl_str.empty()) {
        if (dict_locked) {
          dict_sys_mutex_enter();
        }
        return nullptr;
      }

      if (dd_mdl_acquire(thd, mdl, db_str.c_str(), tbl_str.c_str())) {
        if (dict_locked) {
          dict_sys_mutex_enter();
        }
        return nullptr;
      }
#endif /* !UNIV_HOTBACKUP */

      /* Re-lookup the table after acquiring MDL. */
      dict_sys_mutex_enter();

      HASH_SEARCH(id_hash, dict_sys->table_id_hash, hash_value, dict_table_t *,
                  ib_table, ut_ad(ib_table->cached), ib_table->id == table_id);

      if (ib_table != nullptr) {
        ulint namelen = strlen(ib_table->name.m_name);

        /* The table could have been renamed. After
        we release dict mutex before the old table
        name is MDL locked. So we need to go back
        to  MDL lock the new name. */
        if (namelen != strlen(full_name) ||
            memcmp(ib_table->name.m_name, full_name, namelen)) {
#ifndef UNIV_HOTBACKUP
          dd_mdl_release(thd, mdl);
#endif /* !UNIV_HOTBACKUP */
          continue;
        } else if (check_corruption && dd_check_corrupted(ib_table)) {
          ut_ad(ib_table == nullptr);
        } else if (ib_table->discard_after_ddl) {
#ifndef UNIV_HOTBACKUP
          btr_drop_ahi_for_table(ib_table);
          dict_table_remove_from_cache(ib_table);
#endif /* !UNIV_HOTBACKUP */
          ib_table = nullptr;
#ifndef UNIV_HOTBACKUP
          dd_mdl_release(thd, mdl);
#endif /* !UNIV_HOTBACKUP */
          goto reopen;
        } else {
          ib_table->acquire_with_lock();
        }
      }

      dict_sys_mutex_exit();
      break;
    }

#ifndef UNIV_HOTBACKUP
    ut_ad(*mdl != nullptr);

    /* Now the table can't be found, release MDL,
    let dd_table_open_on_id_low() do the lock, as table
    name could be changed */
    if (ib_table == nullptr) {
      dd_mdl_release(thd, mdl);
      ib_table = dd_table_open_on_id_low(thd, mdl, table_id);

      if (ib_table == nullptr && *mdl != nullptr) {
        dd_mdl_release(thd, mdl);
      }
    }
#else  /* !UNIV_HOTBACKUP */
    /* PRELIMINARY TEMPORARY WORKAROUND: is this ever used? */
    bool not_hotbackup = false;
    ut_a(not_hotbackup);
#endif /* !UNIV_HOTBACKUP */

    if (dict_locked) {
      dict_sys_mutex_enter();
    }
  }

  ut_ad(dict_locked == dict_sys_mutex_own());

  return ib_table;
}

#ifndef UNIV_HOTBACKUP
/** Set the discard flag for a non-partitioned dd table.
@param[in,out]  thd             current thread
@param[in]      table           InnoDB table
@param[in,out]  table_def       MySQL dd::Table to update
@param[in]      discard         discard flag
@return true    if success
@retval false if fail. */
bool dd_table_discard_tablespace(THD *thd, const dict_table_t *table,
                                 dd::Table *table_def, bool discard) {
  bool ret = false;

  DBUG_TRACE;

  ut_ad(thd == current_thd);
#ifdef UNIV_DEBUG
  btrsea_sync_check check(false);
  ut_ad(!sync_check_iterate(check));
#endif /* UNIV_DEBUG */

  ut_ad(srv_shutdown_state.load() < SRV_SHUTDOWN_DD);

  if (table_def->se_private_id() != dd::INVALID_OBJECT_ID) {
    ut_ad(table_def->table().leaf_partitions()->empty());

    /* For discarding, we need to set new private
    id to dd_table */
    if (discard) {
      /* Set the new private id to dd_table object. */
      table_def->set_se_private_id(table->id);
    } else {
      ut_ad(table_def->se_private_id() == table->id);
    }

    /* Set index root page. */
    for (auto dd_index : *table_def->indexes()) {
      const dict_index_t *index = dd_find_index(table, dd_index);
      ut_ad(index != nullptr);

      dd::Properties &p = dd_index->se_private_data();
      p.set(dd_index_key_strings[DD_INDEX_ROOT], index->page);
    }

    /* Set new table id for dd columns */
    for (auto dd_column : *table_def->columns()) {
      dd_column->se_private_data().set(dd_index_key_strings[DD_TABLE_ID],
                                       table->id);
    }

    /* Set 'discard' attribute in dd::Table::se_private_data. */
    dd_set_discarded(*table_def, discard);

    /* Set the 'state' key value in dd::Tablespace::se_private_data*/
    dd::Object_id dd_space_id =
        (*table_def->indexes()->begin())->tablespace_id();
    std::string space_name(table->name.m_name);
    dict_name::convert_to_space(space_name);
    dd_space_states dd_state =
        (discard ? DD_SPACE_STATE_DISCARDED : DD_SPACE_STATE_NORMAL);
    dd_tablespace_set_state(thd, dd_space_id, space_name, dd_state);

    ret = true;
  } else {
    ret = false;
  }

  return ret;
}

/** Open an internal handle to a persistent InnoDB table by name.
@param[in,out]  thd             current thread
@param[out]     mdl             metadata lock
@param[in]      name            InnoDB table name
@param[in]      dict_locked     has dict_sys mutex locked
@param[in]      ignore_err      whether to ignore err
@param[out]     error           pointer to error
@return handle to non-partitioned table
@retval NULL if the table does not exist */
dict_table_t *dd_table_open_on_name(THD *thd, MDL_ticket **mdl,
                                    const char *name, bool dict_locked,
                                    ulint ignore_err, int *error) {
  DBUG_TRACE;

#ifdef UNIV_DEBUG
  btrsea_sync_check check(false);
  ut_ad(!sync_check_iterate(check));
#endif
  ut_ad(srv_shutdown_state.load() < SRV_SHUTDOWN_DD);

  dict_table_t *table = nullptr;

  /* Get pointer to a table object in InnoDB dictionary cache.
  For intrinsic table, get it from session private data */
  if (thd) {
    table = thd_to_innodb_session(thd)->lookup_table_handler(name);
  }

  if (table != nullptr) {
    table->acquire();
    return table;
  }

  std::string db_name;
  std::string tbl_name;
  dict_name::get_table(name, db_name, tbl_name);

  if (db_name.empty() || tbl_name.empty()) {
    return nullptr;
  }

  bool skip_mdl = !(thd && mdl);

  if (!skip_mdl) {
    if (dict_locked) {
      /* We cannot acquire MDL while holding dict_sys->mutex. The reason that
      the caller has already locked this mutex is so that the dict_table_t
      that we will find and return to it will not be dropped while the caller
      is using it. So it is safe to exit, get the mdl and enter again before
      finding this dict_table_t. */
      dict_sys_mutex_exit();
    }

    bool got_mdl = dd_mdl_acquire(thd, mdl, db_name.c_str(), tbl_name.c_str());

    if (dict_locked) {
      dict_sys_mutex_enter();
    }

    if (got_mdl) {
      return nullptr;
    }
  }

  if (!dict_locked) {
    dict_sys_mutex_enter();
  }

  table = dict_table_check_if_in_cache_low(name);

  if (table != nullptr) {
    table->acquire_with_lock();
    if (!dict_locked) {
      dict_sys_mutex_exit();
    }
    return table;
  }

  dict_sys_mutex_exit();

  const dd::Table *dd_table = nullptr;
  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  if (client->acquire(db_name.c_str(), tbl_name.c_str(), &dd_table) ||
      dd_table == nullptr || dd_table->engine() != innobase_hton_name) {
    /* The checking for engine should be only useful(valid)
    for getting table statistics for IS. Two relevant API
    functions are:
    1. innobase_get_table_statistics
    2. innobase_get_index_column_cardinality */
    table = nullptr;
  } else {
    if (dd_table->se_private_id() == dd::INVALID_OBJECT_ID) {
      ut_ad(!dd_table->leaf_partitions().empty());

      if (dict_name::is_partition(name)) {
        const dd::Partition *dd_part = nullptr;

        for (auto part : dd_table->leaf_partitions()) {
          if (dict_name::match_partition(name, part)) {
            dd_part = part;
            break;
          }
        }

        /* Safe check for release mode. */
        if (dd_part == nullptr) {
          table = nullptr;
          ut_d(ut_error);
        } else {
          dd_table_open_on_dd_obj(thd, client, *dd_table, dd_part, name, table,
                                  nullptr);
        }

      } else {
        /* FIXME: Once FK functions will not open
        partitioned table in current improper way,
        just assert this false */
        table = nullptr;
      }
    } else {
      ut_ad(dd_table->leaf_partitions().empty());
      int err = dd_table_open_on_dd_obj(thd, client, *dd_table, nullptr, name,
                                        table, nullptr);
      if (error) {
        *error = err;
      }
    }
  }

  if (table && table->is_corrupted() &&
      !(ignore_err & DICT_ERR_IGNORE_CORRUPT)) {
    dict_sys_mutex_enter();
    table->release();
    dict_table_remove_from_cache(table);
    table = nullptr;
    dict_sys_mutex_exit();
  }

  if (table == nullptr && mdl) {
    dd_mdl_release(thd, mdl);
    *mdl = nullptr;
  }

  if (dict_locked) {
    dict_sys_mutex_enter();
  }

  return table;
}
#endif /* !UNIV_HOTBACKUP */

/** Close an internal InnoDB table handle.
@param[in,out]  table           InnoDB table handle
@param[in,out]  thd             current MySQL connection (for mdl)
@param[in,out]  mdl             metadata lock (will be set NULL)
@param[in]      dict_locked     whether we hold dict_sys mutex */
void dd_table_close(dict_table_t *table, THD *thd, MDL_ticket **mdl,
                    bool dict_locked) {
  dict_table_close(table, dict_locked, false);

#ifndef UNIV_HOTBACKUP
  if (mdl != nullptr && *mdl != nullptr) {
    ut_ad(!table->is_temporary());
    dd_mdl_release(thd, mdl);
  }
#endif /* !UNIV_HOTBACKUP */
}

#ifndef UNIV_HOTBACKUP
/** Replace the tablespace name in the file name.
@param[in]  dd_file  the tablespace file object.
@param[in]  new_space_name  new table space name to be updated in file name.
                            It must have already been converted to the
                            filename_charset such that
                             `d1/d2\d3`.`t3\t4/t5`
                            should look like:
                            d1@002fd2@005cd3/t3@005ct4@002ft5
                            both on Windows and on Linux. */
static void replace_space_name_in_file_name(dd::Tablespace_file *dd_file,
                                            dd::String_type new_space_name) {
  ut_ad(std::count(new_space_name.begin(), new_space_name.end(),
                   Fil_path::DB_SEPARATOR) == 1);

  /* Obtain the old tablespace file name. */
  dd::String_type old_file_name = dd_file->filename();

  /* We assume that old_file_name ends with:
  OS_PATH_SEPARATOR + db_name + OS_PATH_SEPARATOR + table_name + dot_ext[IBD],
  so on Windows it can look like:
  .\d1@002fd2@005cd3\t1@002ft2@005ct3.ibd
  and on Linux it could be:
  ./d1@002fd2@005cd3/t1@002ft2@005ct3.ibd */
  ut_ad(std::count(old_file_name.begin(), old_file_name.end(),
                   OS_PATH_SEPARATOR) >= 2);
  ut_ad(old_file_name.rfind(dot_ext[IBD]) ==
        old_file_name.length() - strlen(dot_ext[IBD]));

  /* Strip the last two components of the path (keep the slash) */
  auto last_separator_pos = old_file_name.find_last_of(OS_PATH_SEPARATOR);
  auto previous_separator_pos =
      old_file_name.find_last_of(OS_PATH_SEPARATOR, last_separator_pos - 1);
  old_file_name.resize(previous_separator_pos + 1);

  /* Take care of path separators */
  std::replace(new_space_name.begin(), new_space_name.end(),
               Fil_path::DB_SEPARATOR, OS_PATH_SEPARATOR);

  old_file_name += new_space_name + dot_ext[IBD];

  /* Update the file name path */
  dd_file->set_filename(old_file_name);
}

dberr_t dd_tablespace_rename(dd::Object_id dd_space_id, bool is_system_cs,
                             const char *new_space_name, const char *new_path) {
  THD *thd = current_thd;

  DBUG_TRACE;
#ifdef UNIV_DEBUG
  btrsea_sync_check check(false);
  ut_ad(!sync_check_iterate(check));
#endif /* UNIV_DEBUG */
  ut_ad(srv_shutdown_state.load() < SRV_SHUTDOWN_DD);

  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  dd::Tablespace *dd_space = nullptr;

  /* Get the dd tablespace */
  if (client->acquire_uncached_uncommitted<dd::Tablespace>(dd_space_id,
                                                           &dd_space) ||
      dd_space == nullptr) {
    ut_d(ut_error);
    ut_o(return DB_ERROR);
  }

  MDL_ticket *src_ticket = nullptr;
  if (dd_tablespace_get_mdl(dd_space->name().c_str(), &src_ticket)) {
    ut_d(ut_error);
    ut_o(return DB_ERROR);
  }

  std::string tablespace_name(new_space_name);
  /* Convert if not in system character set. */
  if (!is_system_cs) {
    dict_name::convert_to_space(tablespace_name);
  }

  MDL_ticket *dst_ticket = nullptr;
  if (dd_tablespace_get_mdl(tablespace_name.c_str(), &dst_ticket)) {
    ut_d(ut_error);
    ut_o(return DB_ERROR);
  }

  dd::Tablespace *new_space = nullptr;

  /* Acquire the new dd tablespace for modification */
  if (client->acquire_for_modification<dd::Tablespace>(dd_space_id,
                                                       &new_space)) {
    ut_d(ut_error);
    ut_o(return DB_ERROR);
  }

  ut_ad(new_space->files().size() == 1);

  dd::String_type old_space_name = new_space->name();

  new_space->set_name(tablespace_name.c_str());

  dd::Tablespace_file *dd_file =
      const_cast<dd::Tablespace_file *>(*(new_space->files().begin()));

  if (new_path != nullptr) {
    dd_file->set_filename(new_path);

  } else {
    replace_space_name_in_file_name(dd_file, new_space_name);
    ut_ad(dd_tablespace_get_state_enum(dd_space) == DD_SPACE_STATE_DISCARDED);
  }

  bool fail = client->update(new_space);
  ut_ad(!fail);
  dd::rename_tablespace_mdl_hook(thd, src_ticket, dst_ticket);

  return fail ? DB_ERROR : DB_SUCCESS;
}

/** Validate the table format options.
@param[in]      thd             THD instance
@param[in]      form            MySQL table definition
@param[in]      real_type       real row type if it's not ROW_TYPE_NOT_USED
@param[in]      zip_allowed     whether ROW_FORMAT=COMPRESSED is OK
@param[in]      strict          whether innodb_strict_mode=ON
@param[out]     is_redundant    whether ROW_FORMAT=REDUNDANT
@param[out]     blob_prefix     whether ROW_FORMAT=DYNAMIC
                                or ROW_FORMAT=COMPRESSED
@param[out]     zip_ssize       log2(compressed page size),
                                or 0 if not ROW_FORMAT=COMPRESSED
@param[out]     is_implicit     if tablespace is implicit
@retval true if invalid (my_error will have been called)
@retval false if valid */
static bool format_validate(THD *thd, const TABLE *form, row_type real_type,
                            bool zip_allowed, bool strict, bool *is_redundant,
                            bool *blob_prefix, ulint *zip_ssize,
                            bool is_implicit) {
  bool is_temporary = false;
  ut_ad(thd != nullptr);
  ut_ad(!zip_allowed || srv_page_size <= UNIV_ZIP_SIZE_MAX);

  /* 1+log2(compressed_page_size), or 0 if not compressed */
  *zip_ssize = 0;
  const ulint zip_ssize_max =
      std::min((ulint)UNIV_PAGE_SSIZE_MAX, (ulint)PAGE_ZIP_SSIZE_MAX);
  const char *zip_refused = zip_allowed ? nullptr
                                        : srv_page_size <= UNIV_ZIP_SIZE_MAX
                                              ? "innodb_file_per_table=OFF"
                                              : "innodb_page_size>16k";
  bool invalid = false;

  if (real_type == ROW_TYPE_NOT_USED) {
    real_type = form->s->real_row_type;
  }

  if (auto key_block_size = form->s->key_block_size) {
    unsigned valid_zssize = 0;
    char kbs[MY_INT32_NUM_DECIMAL_DIGITS + sizeof "KEY_BLOCK_SIZE=" + 1];
    snprintf(kbs, sizeof kbs, "KEY_BLOCK_SIZE=%u", key_block_size);
    for (unsigned kbsize = 1, zssize = 1; zssize <= zip_ssize_max;
         zssize++, kbsize <<= 1) {
      if (kbsize == key_block_size) {
        valid_zssize = zssize;
        break;
      }
    }

    if (valid_zssize == 0) {
      if (strict) {
        my_error(ER_WRONG_VALUE, MYF(0), "KEY_BLOCK_SIZE",
                 kbs + sizeof "KEY_BLOCK_SIZE");
        invalid = true;
      } else {
        push_warning_printf(thd, Sql_condition::SL_WARNING, ER_WRONG_VALUE,
                            ER_DEFAULT(ER_WRONG_VALUE), "KEY_BLOCK_SIZE",
                            kbs + sizeof "KEY_BLOCK_SIZE");
      }
    } else if (!zip_allowed) {
      int error = is_temporary ? ER_UNSUPPORT_COMPRESSED_TEMPORARY_TABLE
                               : ER_ILLEGAL_HA_CREATE_OPTION;

      if (strict) {
        my_error(error, MYF(0), innobase_hton_name, kbs, zip_refused);
        invalid = true;
      } else {
        push_warning_printf(thd, Sql_condition::SL_WARNING, error,
                            ER_DEFAULT_NONCONST(error), innobase_hton_name, kbs,
                            zip_refused);
      }
    } else if (real_type != ROW_TYPE_COMPRESSED) {
      /* This could happen when
      1. There was an ALTER TABLE ... COPY to move
      the table from COMPRESSED into DYNAMIC, etc.
      2. For partitioned table, some partitions of which
      could be of different row format from the specified
      one */
    } else if (form->s->row_type == ROW_TYPE_DEFAULT ||
               form->s->row_type == ROW_TYPE_COMPRESSED) {
      ut_ad(real_type == ROW_TYPE_COMPRESSED);
      *zip_ssize = valid_zssize;
    } else {
      int error = is_temporary ? ER_UNSUPPORT_COMPRESSED_TEMPORARY_TABLE
                               : ER_ILLEGAL_HA_CREATE_OPTION;
      const char *conflict = get_row_format_name(form->s->row_type);

      if (strict) {
        my_error(error, MYF(0), innobase_hton_name, kbs, conflict);
        invalid = true;
      } else {
        push_warning_printf(thd, Sql_condition::SL_WARNING, error,
                            ER_DEFAULT_NONCONST(error), innobase_hton_name, kbs,
                            conflict);
      }
    }
  } else if (form->s->row_type != ROW_TYPE_COMPRESSED || !is_temporary) {
    /* not ROW_FORMAT=COMPRESSED (nor KEY_BLOCK_SIZE),
    or not TEMPORARY TABLE */
  } else if (strict) {
    my_error(ER_UNSUPPORT_COMPRESSED_TEMPORARY_TABLE, MYF(0));
    invalid = true;
  } else {
    push_warning(thd, Sql_condition::SL_WARNING,
                 ER_UNSUPPORT_COMPRESSED_TEMPORARY_TABLE,
                 ER_THD(thd, ER_UNSUPPORT_COMPRESSED_TEMPORARY_TABLE));
  }

  /* Check for a valid InnoDB ROW_FORMAT specifier and
  other incompatibilities. */
  rec_format_t innodb_row_format = REC_FORMAT_DYNAMIC;

  switch (form->s->row_type) {
    case ROW_TYPE_DYNAMIC:
      ut_ad(*zip_ssize == 0);
      /* If non strict_mode, row type can be converted between
      COMPRESSED and DYNAMIC */
      ut_ad(real_type == ROW_TYPE_DYNAMIC || real_type == ROW_TYPE_COMPRESSED);
      break;
    case ROW_TYPE_COMPACT:
      ut_ad(*zip_ssize == 0);
      ut_ad(real_type == ROW_TYPE_COMPACT);
      innodb_row_format = REC_FORMAT_COMPACT;
      break;
    case ROW_TYPE_REDUNDANT:
      ut_ad(*zip_ssize == 0);
      ut_ad(real_type == ROW_TYPE_REDUNDANT);
      innodb_row_format = REC_FORMAT_REDUNDANT;
      break;
    case ROW_TYPE_FIXED:
    case ROW_TYPE_PAGED:
    case ROW_TYPE_NOT_USED: {
      const char *name = get_row_format_name(form->s->row_type);
      if (strict) {
        my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0), innobase_hton_name, name);
        invalid = true;
      } else {
        push_warning_printf(
            thd, Sql_condition::SL_WARNING, ER_ILLEGAL_HA_CREATE_OPTION,
            ER_DEFAULT(ER_ILLEGAL_HA_CREATE_OPTION), innobase_hton_name, name);
      }
    }
      [[fallthrough]];
    case ROW_TYPE_DEFAULT:
      switch (real_type) {
        case ROW_TYPE_FIXED:
        case ROW_TYPE_PAGED:
        case ROW_TYPE_NOT_USED:
        case ROW_TYPE_DEFAULT:
          /* get_real_row_type() should not return these */
          ut_d(ut_error);
          [[fallthrough]];
        case ROW_TYPE_DYNAMIC:
          ut_ad(*zip_ssize == 0);
          break;
        case ROW_TYPE_COMPACT:
          ut_ad(*zip_ssize == 0);
          innodb_row_format = REC_FORMAT_COMPACT;
          break;
        case ROW_TYPE_REDUNDANT:
          ut_ad(*zip_ssize == 0);
          innodb_row_format = REC_FORMAT_REDUNDANT;
          break;
        case ROW_TYPE_COMPRESSED:
          innodb_row_format = REC_FORMAT_COMPRESSED;
          break;
      }

      if (*zip_ssize == 0) {
        /* No valid KEY_BLOCK_SIZE was specified,
        so do not imply ROW_FORMAT=COMPRESSED. */
        if (innodb_row_format == REC_FORMAT_COMPRESSED) {
          innodb_row_format = REC_FORMAT_DYNAMIC;
        }
        break;
      }
      [[fallthrough]];
    case ROW_TYPE_COMPRESSED:
      if (is_temporary) {
        if (strict) {
          invalid = true;
        }
        /* ER_UNSUPPORT_COMPRESSED_TEMPORARY_TABLE
        was already reported. */
        ut_ad(real_type == ROW_TYPE_DYNAMIC);
        break;
      } else if (zip_allowed && real_type == ROW_TYPE_COMPRESSED) {
        /* ROW_FORMAT=COMPRESSED without KEY_BLOCK_SIZE
        implies half the maximum compressed page size. */
        if (*zip_ssize == 0) {
          *zip_ssize = zip_ssize_max - 1;
        }
        innodb_row_format = REC_FORMAT_COMPRESSED;
        break;
      }

      if (strict) {
        my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0), innobase_hton_name,
                 "ROW_FORMAT=COMPRESSED", zip_refused);
        invalid = true;
      }
  }

  if (const char *algorithm =
          form->s->compress.length > 0 ? form->s->compress.str : nullptr) {
    Compression compression;
    dberr_t err = Compression::check(algorithm, &compression);

    if (err == DB_UNSUPPORTED) {
      if (strict) {
        my_error(ER_WRONG_VALUE, MYF(0), "COMPRESSION", algorithm);
        invalid = true;
      } else {
        push_warning_printf(thd, Sql_condition::SL_WARNING, ER_WRONG_VALUE,
                            ER_DEFAULT(ER_WRONG_VALUE), "COMPRESSION",
                            algorithm);
      }
    } else if (compression.m_type != Compression::NONE) {
      if (*zip_ssize != 0) {
        if (strict) {
          my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0), innobase_hton_name,
                   "COMPRESSION",
                   form->s->key_block_size ? "KEY_BLOCK_SIZE"
                                           : "ROW_FORMAT=COMPRESSED");
          invalid = true;
        }
      }

      if (is_temporary) {
        my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0), innobase_hton_name,
                 "COMPRESSION", "TEMPORARY");
        invalid = true;
      } else if (!is_implicit && strict) {
        my_error(ER_ILLEGAL_HA_CREATE_OPTION, MYF(0), innobase_hton_name,
                 "COMPRESSION", "TABLESPACE");
        invalid = true;
      }
    }
  }

  /* Check if there are any FTS indexes defined on this table. */
  for (uint i = 0; i < form->s->keys; i++) {
    const KEY *key = &form->key_info[i];

    if ((key->flags & HA_FULLTEXT) && is_temporary) {
      /* We don't support FTS indexes in temporary
      tables. */
      my_error(ER_INNODB_NO_FT_TEMP_TABLE, MYF(0));
      return true;
    }
  }

  ut_ad((*zip_ssize == 0) == (innodb_row_format != REC_FORMAT_COMPRESSED));

  *is_redundant = false;
  *blob_prefix = false;

  switch (innodb_row_format) {
    case REC_FORMAT_REDUNDANT:
      *is_redundant = true;
      *blob_prefix = true;
      break;
    case REC_FORMAT_COMPACT:
      *blob_prefix = true;
      break;
    case REC_FORMAT_COMPRESSED:
      ut_ad(!is_temporary);
      break;
    case REC_FORMAT_DYNAMIC:
      break;
  }

  return invalid;
}

/** Set the AUTO_INCREMENT attribute.
@param[in,out]  se_private_data         dd::Table::se_private_data
@param[in]      autoinc                 the auto-increment value */
void dd_set_autoinc(dd::Properties &se_private_data, uint64_t autoinc) {
  /* The value of "autoinc" here is the AUTO_INCREMENT attribute
  specified at table creation. AUTO_INCREMENT=0 will silently
  be treated as AUTO_INCREMENT=1. Likewise, if no AUTO_INCREMENT
  attribute was specified, the value would be 0. */

  if (autoinc > 0) {
    /* InnoDB persists the "previous" AUTO_INCREMENT value. */
    autoinc--;
  }

  uint64_t version = 0;

  if (se_private_data.exists(dd_table_key_strings[DD_TABLE_AUTOINC])) {
    /* Increment the dynamic metadata version, so that
    any previously buffered persistent dynamic metadata
    will be ignored after this transaction commits. */

    if (!se_private_data.get(dd_table_key_strings[DD_TABLE_VERSION],
                             &version)) {
      version++;
    } else {
      /* incomplete se_private_data */
      ut_d(ut_error);
    }
  }

  se_private_data.set(dd_table_key_strings[DD_TABLE_VERSION], version);
  se_private_data.set(dd_table_key_strings[DD_TABLE_AUTOINC], autoinc);
}

/** Copy the AUTO_INCREMENT and version attribute if exist.
@param[in]      src     dd::Table::se_private_data to copy from
@param[out]     dest    dd::Table::se_private_data to copy to */
void dd_copy_autoinc(const dd::Properties &src, dd::Properties &dest) {
  uint64_t autoinc = 0;
  uint64_t version = 0;

  if (!src.exists(dd_table_key_strings[DD_TABLE_AUTOINC])) {
    return;
  }

  if (src.get(dd_table_key_strings[DD_TABLE_AUTOINC],
              reinterpret_cast<uint64_t *>(&autoinc)) ||
      src.get(dd_table_key_strings[DD_TABLE_VERSION],
              reinterpret_cast<uint64_t *>(&version))) {
    ut_d(ut_error);
    ut_o(return );
  }

  dest.set(dd_table_key_strings[DD_TABLE_VERSION], version);
  dest.set(dd_table_key_strings[DD_TABLE_AUTOINC], autoinc);
}

/** Copy the metadata of a table definition if there was an instant
ADD COLUMN happened. This should be done when it's not an ALTER TABLE
with rebuild.
@param[in,out]  new_table       New table definition
@param[in]      old_table       Old table definition */
void dd_copy_instant_n_cols(dd::Table &new_table, const dd::Table &old_table) {
  ut_ad(dd_table_is_upgraded_instant(old_table));

  if (!dd_table_is_upgraded_instant(new_table)) {
    uint32_t cols;
    old_table.se_private_data().get(dd_table_key_strings[DD_TABLE_INSTANT_COLS],
                                    &cols);
    new_table.se_private_data().set(dd_table_key_strings[DD_TABLE_INSTANT_COLS],
                                    cols);
  }
#ifdef UNIV_DEBUG
  else {
    uint32_t old_cols, new_cols;
    old_table.se_private_data().get(dd_table_key_strings[DD_TABLE_INSTANT_COLS],
                                    &old_cols);
    new_table.se_private_data().get(dd_table_key_strings[DD_TABLE_INSTANT_COLS],
                                    &new_cols);
    ut_ad(old_cols == new_cols);
  }
#endif /* UNIV_DEBUG */
}

template <typename Table>
void dd_copy_private(Table &new_table, const Table &old_table) {
  uint64_t autoinc = 0;
  uint64_t version = 0;
  bool reset = false;
  dd::Properties &se_private_data = new_table.se_private_data();

  /* AUTOINC metadata could be set at the beginning for
  non-partitioned tables. So already set metadata should be kept */
  if (se_private_data.exists(dd_table_key_strings[DD_TABLE_AUTOINC])) {
    se_private_data.get(dd_table_key_strings[DD_TABLE_AUTOINC], &autoinc);
    se_private_data.get(dd_table_key_strings[DD_TABLE_VERSION], &version);
    reset = true;
  }

  new_table.se_private_data().clear();

  new_table.set_se_private_id(old_table.se_private_id());
  new_table.set_se_private_data(old_table.se_private_data());

  if (!dd_table_is_partitioned(new_table.table()) ||
      dd_part_is_first(reinterpret_cast<dd::Partition *>(&new_table))) {
    /* copy table se-private data for first partition */
    new_table.table().se_private_data().clear();
    new_table.table().set_se_private_data(old_table.table().se_private_data());
  }

  if (reset) {
    se_private_data.set(dd_table_key_strings[DD_TABLE_VERSION], version);
    se_private_data.set(dd_table_key_strings[DD_TABLE_AUTOINC], autoinc);
  }

  ut_ad(new_table.indexes()->size() == old_table.indexes().size());

  /* Note that server could provide old and new dd::Table with
  different index order in this case, so always do a double loop */
  for (const auto old_index : old_table.indexes()) {
    auto idx = new_table.indexes()->begin();
    for (; (*idx)->name() != old_index->name(); ++idx)
      ;
    ut_ad(idx != new_table.indexes()->end());

    auto new_index = *idx;
    ut_ad(!old_index->se_private_data().empty());
    ut_ad(new_index != nullptr);
    ut_ad(new_index->se_private_data().empty());
    ut_ad(new_index->name() == old_index->name());

    new_index->set_se_private_data(old_index->se_private_data());
    new_index->set_tablespace_id(old_index->tablespace_id());
  }

  new_table.table().set_row_format(old_table.table().row_format());
}

template void dd_copy_private<dd::Table>(dd::Table &, const dd::Table &);
template void dd_copy_private<dd::Partition>(dd::Partition &,
                                             const dd::Partition &);

bool is_renamed(const Alter_inplace_info *ha_alter_info, const char *old_name,
                std::string &new_name) {
  List_iterator_fast<Create_field> cf_it(
      ha_alter_info->alter_info->create_list);
  cf_it.rewind();
  Create_field *cf;
  while ((cf = cf_it++) != nullptr) {
    if (cf->field && cf->field->is_flag_set(FIELD_IS_RENAMED) &&
        !my_strcasecmp(system_charset_info, old_name, cf->change)) {
      /* This column is being renamed */
      new_name = cf->field_name;
      return true;
    }
  }

  return false;
}

bool is_dropped(const Alter_inplace_info *ha_alter_info,
                const char *column_name) {
  for (const Alter_drop *drop : ha_alter_info->alter_info->drop_list) {
    if (drop->type != Alter_drop::COLUMN) continue;

    if (!my_strcasecmp(system_charset_info, column_name, drop->name)) {
      return true;
    }
  }

  return false;
}

void dd_copy_table_columns(const Alter_inplace_info *ha_alter_info,
                           dd::Table &new_table, const dd::Table &old_table,
                           dict_table_t *old_dict_table) {
  bool first_row_version = false;
  if (old_dict_table && !old_dict_table->has_row_versions()) {
    first_row_version = true;
  }

  /* Columns in new table maybe more than old tables, when this is
  called for adding instant columns. Also adding and dropping
  virtual columns instantly is another case. */

  for (const auto old_col : old_table.columns()) {
    if (old_col->is_se_hidden() && !is_system_column(old_col->name().c_str()) &&
        (strcmp(old_col->name().c_str(), FTS_DOC_ID_COL_NAME) != 0)) {
      /* Must be an already dropped column. */
      ut_ad(dd_column_is_dropped(old_col));
      continue;
    }

    dd::Column *new_col = nullptr;
    std::string new_name;

    /* Skip the dropped column */
    if (is_dropped(ha_alter_info, old_col->name().c_str())) {
      continue;
    } else if (is_renamed(ha_alter_info, old_col->name().c_str(), new_name)) {
      new_col = const_cast<dd::Column *>(
          dd_find_column(&new_table, new_name.c_str()));
    } else {
      new_col = const_cast<dd::Column *>(
          dd_find_column(&new_table, old_col->name().c_str()));
    }

    ut_a(new_col);

    const char *s = dd_column_key_strings[DD_INSTANT_PHYSICAL_POS];
    if (!old_col->se_private_data().empty()) {
      if (!new_col->se_private_data().empty())
        new_col->se_private_data().clear();
      new_col->set_se_private_data(old_col->se_private_data());
    }

    /* If this is first time table is getting row version, add physical pos */
    if (old_dict_table && !new_col->is_virtual() && first_row_version) {
      /* Even the renamed column would have same phy_pos as old column */
      dict_col_t *col =
          old_dict_table->get_col_by_name(old_col->name().c_str());
      ut_a(col != nullptr);
      new_col->se_private_data().set(s, col->get_phy_pos());
    }
  }
}

void dd_part_adjust_table_id(dd::Table *new_table) {
  ut_ad(dd_table_is_partitioned(*new_table));

  auto part = new_table->leaf_partitions()->begin();
  table_id_t table_id = (*part)->se_private_id();

  for (auto dd_column : *new_table->table().columns()) {
    dd_column->se_private_data().set(dd_index_key_strings[DD_TABLE_ID],
                                     table_id);
  }
}

/** Clear the instant ADD COLUMN information of a table
@param[in,out]  dd_table        dd::Table
@param[in]      clear_version   true if version metadata is to be cleared
@return DB_SUCCESS or error code */
dberr_t dd_clear_instant_table(dd::Table &dd_table, bool clear_version) {
  dberr_t err = DB_SUCCESS;
  dd_table.se_private_data().remove(
      dd_table_key_strings[DD_TABLE_INSTANT_COLS]);

  std::vector<std::string> cols_to_drop;

  for (auto col : *dd_table.columns()) {
    auto fn = [&](const char *s) {
      if (col->se_private_data().exists(s)) {
        col->se_private_data().remove(s);
      }
    };

    if (!clear_version) {
      bool is_versioned = dd_column_is_dropped(col) || dd_column_is_added(col);
      if (is_versioned) {
        continue;
      }

      /* Possibly an INSTANT ADD column */
      fn(dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT_NULL]);
      fn(dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT]);
    } else {
      /* Possibly an INSTANT ADD/DROP column with a version */
      if (dd_column_is_dropped(col)) {
        cols_to_drop.push_back(col->name().c_str());
        continue;
      }
      fn(dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT_NULL]);
      fn(dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT]);
      fn(dd_column_key_strings[DD_INSTANT_VERSION_ADDED]);
      fn(dd_column_key_strings[DD_INSTANT_VERSION_DROPPED]);
      fn(dd_column_key_strings[DD_INSTANT_PHYSICAL_POS]);
    }
  }

  if (!cols_to_drop.empty()) {
    for (auto col_name : cols_to_drop) {
      if (!dd_drop_hidden_column(&dd_table, col_name.c_str())) {
        ib::error(ER_IB_MSG_CLEAR_INSTANT_DROP_COLUMN_METADATA)
            << dd_table.name().c_str();
        my_error(
            ER_INTERNAL_ERROR, MYF(0),
            "Failed to truncate table. You may drop and re-create this table.");
        ut_ad(0);
        err = DB_ERROR;
      }
    }
  }
  cols_to_drop.clear();

  return err;
}

/** Clear the instant ADD COLUMN information of a partition, to make it
as a normal partition
@param[in,out]  dd_part         dd::Partition */
void dd_clear_instant_part(dd::Partition &dd_part) {
  ut_ad(dd_part_has_instant_cols(dd_part));

  dd_part.se_private_data().remove(
      dd_partition_key_strings[DD_PARTITION_INSTANT_COLS]);
}

#ifdef UNIV_DEBUG
bool dd_instant_columns_consistent(const dd::Table &dd_table) {
  bool found = false;
  size_t n_non_instant_cols = 0;
  size_t n_version_add_cols [[maybe_unused]] = 0;
  size_t n_instant_add_cols = 0;
  size_t n_version_drop_cols = 0;
  for (auto column : dd_table.columns()) {
    if (column->is_virtual() || is_system_column(column->name().c_str())) {
      continue;
    }

    if (column->se_private_data().exists(
            dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT_NULL]) ||
        column->se_private_data().exists(
            dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT])) {
      found = true;
      if (dd_column_is_added(column)) {
        n_version_add_cols++;
      } else {
        /* In upgraded table, Instant ADD column with no v_added */
        ut_ad(dd_table_is_upgraded_instant(dd_table));
        n_instant_add_cols++;
      }

      continue;
    }

    if (dd_column_is_dropped(column)) {
      n_version_drop_cols++;
      continue;
    }

    ++n_non_instant_cols;
  }

  if (!dd_table_is_upgraded_instant(dd_table)) {
    ut_ad(dd_table_has_row_versions(dd_table));
    ut_ad(n_instant_add_cols == 0);
    return true;
  }

  /* If we reach here, table is in v1 instant format */
  bool exp = false;
  const char *s = dd_table_key_strings[DD_TABLE_INSTANT_COLS];
  uint32_t n_inst_cols = 0;
  dd_table.se_private_data().get(s, &n_inst_cols);

  /* Note that n_inst_cols could be 0 if the table only had some virtual
  columns before instant ADD COLUMN. So below check should be sufficient.

  Moreover, existing columns before first INSTANT ADD could have been dropped.
  So n_non_instant_cols could be less then n_inst_cols provided it is accounted
  in n_version_drop_cols. */
  exp = (n_non_instant_cols == n_inst_cols) ||
        (n_non_instant_cols < n_inst_cols &&
         (n_version_drop_cols >= (n_inst_cols - n_non_instant_cols)));

  ut_ad(exp);

  /* found will be false iff after upgrade INSTANT ADD column was INSTANT
  DROP. */
  bool exp2 = found || dd_table_has_row_versions(dd_table);
  ut_ad(exp2);

  return (exp && exp2);
}
#endif /* UNIV_DEBUG */

static void instant_update_table_cols_count(dict_table_t *dict_table,
                                            uint32_t n_added_column,
                                            uint32_t n_dropped_column) {
  dict_table->current_col_count += n_added_column;
  dict_table->current_col_count -= n_dropped_column;
  dict_table->total_col_count += n_added_column;

  ut_ad(dict_table->total_col_count >= dict_table->current_col_count);
}

bool copy_dropped_columns(const dd::Table *old_dd_table,
                          dd::Table *new_dd_table,
                          uint32_t current_row_version) {
  ut_d(bool is_instant_v1 = false);

  for (const auto column : old_dd_table->columns()) {
    const char *col_name = column->name().c_str();

    /* Copy physical pos of SYSTEM columns */
    if (is_system_column(col_name)) {
      uint32_t phy_pos = UINT32_UNDEFINED;

      const char *s = dd_column_key_strings[DD_INSTANT_PHYSICAL_POS];

      /* Following is possible if table is upgraded */
      if (!column->se_private_data().exists(s)) {
        ut_d(is_instant_v1 = true);
        continue;
      }

      column->se_private_data().get(s, &phy_pos);
      ut_ad(phy_pos != UINT32_UNDEFINED);

      dd::Column *new_table_col =
          const_cast<dd::Column *>(dd_find_column(new_dd_table, col_name));
      ut_ad(new_table_col != nullptr);
      new_table_col->se_private_data().set(s, phy_pos);
      continue;
    }

    if (!column->is_se_hidden() ||
        innobase_strcasecmp(col_name, FTS_DOC_ID_COL_NAME) == 0) {
      continue;
    }

    /* In V1, we can't have INSTANT DROP columns */
    ut_ad(!is_instant_v1);

    const dd::Column *searchedColumn = dd_find_column(new_dd_table, col_name);
    if (searchedColumn != nullptr) {
      if (!dd_column_is_dropped(searchedColumn)) {
        /* User is trying to add column with name same as existing hidden
         * dropped column name. */
        ib::info(ER_IB_HIDDEN_NAME_CONFLICT, searchedColumn->name().c_str(),
                 col_name);
        my_error(ER_WRONG_COLUMN_NAME, MYF(0), searchedColumn->name().c_str());
        return true;
      }
      /* Column is already present in new table. It is either already dropped
      column in previous statements or is being dropped in same statement. In
      both the cases, continue. */
#ifdef UNIV_DEBUG
      ut_ad(dd_column_is_dropped(column));
      uint32_t v_dropped = dd_column_get_version_dropped(column);
      ut_ad(current_row_version >= v_dropped);
#endif
      continue;
    }

    /* Add this column as an SE_HIDDEN column in new table def */
    dd::Column *new_column = dd_add_hidden_column(
        new_dd_table, col_name, column->char_length(), column->type());
    ut_ad(new_column != nullptr);

    /* Copy se private data */
    ut_ad(!column->se_private_data().empty());
    new_column->se_private_data().clear();
    new_column->set_se_private_data(column->se_private_data());

    new_column->set_nullable(column->is_nullable());
    new_column->set_char_length(column->char_length());
    new_column->set_numeric_scale(column->numeric_scale());
    new_column->set_unsigned(column->is_unsigned());
    new_column->set_collation_id(column->collation_id());
    new_column->set_type(column->type());
    /* Elements for enum columns */
    if (column->type() == dd::enum_column_types::ENUM ||
        column->type() == dd::enum_column_types::SET) {
      for (const auto *source_elem : column->elements()) {
        auto *elem_obj = new_column->add_element();
        elem_obj->set_name(source_elem->name());
      }
    }

    ut_ad(dd_find_column(new_dd_table, col_name) != nullptr);
  }
  return false;
}

static void set_dropped_column_name(std::string &name, uint32_t version,
                                    uint32_t phy_pos) {
  std::ostringstream new_name;
  new_name << INSTANT_DROP_PREFIX_8_0_32 << "v" << version << "_p" << phy_pos
           << "_" << name;
  name = new_name.str();
  name.resize(std::min<size_t>(name.size(), NAME_CHAR_LEN));
}

bool dd_drop_instant_columns(
    const dd::Table *old_dd_table, dd::Table *new_dd_table,
    dict_table_t *new_dict_table,
    const Columns &cols_to_drop IF_DEBUG(, const Columns &cols_to_add,
                                         Alter_inplace_info *ha_alter_info)) {
  if (dd_table_has_instant_drop_cols(*old_dd_table)) {
    /* Copy metadata of already dropped columns */
    if (copy_dropped_columns(old_dd_table, new_dd_table,
                             new_dict_table->current_row_version))
      return true;
  }

#ifdef UNIV_DEBUG
  auto validate_column = [&](Field *column) {
    /* Valid cases are :
    1. Column is not present in the new table definition
    2. Column is present but it is a virtual column being added
    2. Column is present but it is a stored column being added
    3. Column is present and is not being added, it is a renamed column */

    auto dd_col = dd_find_column(new_dd_table, column->field_name);
    /* Virtual columns are not part of cols_to_add so they are checked here. */
    if (dd_col == nullptr || dd_col->is_virtual()) {
      return true;
    }

    for (const auto &field : cols_to_add) {
      if (strcmp(field->field_name, column->field_name) == 0) {
        return true;
      }
    }

    for (const auto col : old_dd_table->columns()) {
      std::string new_name;
      if (is_renamed(ha_alter_info, col->name().c_str(), new_name)) {
        if (strcmp(new_name.c_str(), column->field_name) == 0) {
          return true;
        }
      }
    }

    return false;
  };
#endif

  for (const auto &column : cols_to_drop) {
    ut_ad(!innobase_is_v_fld(column));

    /* Get column to be dropped from old table def */
    dd::Column *col_to_drop = const_cast<dd::Column *>(
        dd_find_column(old_dd_table, column->field_name));
    ut_ad(col_to_drop != nullptr);

    /* This column shouldn't be present in the new table and if it does,
    it must be being added/renamed in the same command. */
    ut_ad(validate_column(column));

    dd::Properties &private_data = col_to_drop->se_private_data();

    uint32_t phy_pos = UINT32_UNDEFINED;
    const char *s = dd_column_key_strings[DD_INSTANT_PHYSICAL_POS];
    if (!private_data.exists(s)) {
      ut_ad(!dd_table_has_row_versions(*old_dd_table));
      ut_ad(!new_dict_table->has_row_versions());
      const dict_col_t *col =
          new_dict_table->get_col_by_name(column->field_name);
      phy_pos = col->get_phy_pos();
    } else {
      private_data.get(s, &phy_pos);
    }

    ut_ad(phy_pos != UINT32_UNDEFINED);

    std::string dropped_col_name(col_to_drop->name().c_str());
    set_dropped_column_name(dropped_col_name,
                            new_dict_table->current_row_version + 1, phy_pos);

    /* Add this column as an SE_HIDDEN column in new table def. NOTE: This call
    will update the DD_INSTANT_VERSION_DROPPED for the column as well. */
    dd::Column *dropped_col =
        dd_add_hidden_column(new_dd_table, dropped_col_name.c_str(),
                             col_to_drop->char_length(), col_to_drop->type());
    if (dropped_col == nullptr) {
      /* Table already has column with name same as dropped_col_name */
      ib::info(ER_IB_HIDDEN_NAME_CONFLICT, dropped_col_name.c_str(),
               dropped_col_name.c_str())
          << "If you have any conflicting user column please rename it.";
      return true;
    }
    ut_ad(dropped_col != nullptr);

    {
      /* Set metadata of dropped column */
      dd::Properties &private_data = dropped_col->se_private_data();
      if (dd_column_is_added(col_to_drop)) {
        uint32_t v_added = dd_column_get_version_added(col_to_drop);
        private_data.set(dd_column_key_strings[DD_INSTANT_VERSION_ADDED],
                         v_added);
      }
      private_data.set(dd_column_key_strings[DD_INSTANT_VERSION_DROPPED],
                       new_dict_table->current_row_version + 1);
      private_data.set(dd_column_key_strings[DD_INSTANT_PHYSICAL_POS], phy_pos);

      dropped_col->set_nullable(col_to_drop->is_nullable());
      dropped_col->set_char_length(col_to_drop->char_length());
      dropped_col->set_numeric_scale(col_to_drop->numeric_scale());
      dropped_col->set_unsigned(col_to_drop->is_unsigned());
      dropped_col->set_collation_id(col_to_drop->collation_id());
      dropped_col->set_type(col_to_drop->type());
      /* Elements for enum columns */
      if (col_to_drop->type() == dd::enum_column_types::ENUM ||
          col_to_drop->type() == dd::enum_column_types::SET) {
        for (const auto *source_elem : col_to_drop->elements()) {
          auto *elem_obj = dropped_col->add_element();
          elem_obj->set_name(source_elem->name());
        }
      }
    }

    ut_ad(dd_find_column(new_dd_table, dropped_col_name.c_str()) != nullptr);
  }

  instant_update_table_cols_count(new_dict_table, 0, cols_to_drop.size());

  return false;
}

bool dd_add_instant_columns(const dd::Table *old_dd_table,
                            dd::Table *new_dd_table,
                            dict_table_t *new_dict_table,
                            const Columns &cols_to_add) {
  if (dd_table_has_instant_drop_cols(*old_dd_table)) {
    /* Copy metadata of already dropped columns */
    if (copy_dropped_columns(old_dd_table, new_dd_table,
                             new_dict_table->current_row_version))
      return true;
  }

  auto set_col_default = [&](Field *field, dd::Properties &se_private) {
    /* Get the mtype and prtype of this field. Keep this same
    with the code in dd_fill_dict_table(), except FTS check */
    ulint prtype = 0;
    unsigned col_len = field->pack_length();
    ulint nulls_allowed;
    ulint unsigned_type;
    ulint binary_type;
    ulint long_true_varchar;
    ulint charset_no;
    ulint mtype = get_innobase_type_from_mysql_type(&unsigned_type, field);

    nulls_allowed = field->is_nullable() ? 0 : DATA_NOT_NULL;

    binary_type = field->binary() ? DATA_BINARY_TYPE : 0;

    charset_no = 0;
    if (dtype_is_string_type(mtype)) {
      charset_no = static_cast<ulint>(field->charset()->number);
    }

    long_true_varchar = 0;
    if (field->type() == MYSQL_TYPE_VARCHAR) {
      col_len -= field->get_length_bytes();

      if (field->get_length_bytes() == 2) {
        long_true_varchar = DATA_LONG_TRUE_VARCHAR;
      }
    }

    prtype =
        dtype_form_prtype((ulint)field->type() | nulls_allowed | unsigned_type |
                              binary_type | long_true_varchar,
                          charset_no);

    dict_col_t col;
    /* Set a fake col_pos, since this should be useless */
    dict_mem_fill_column_struct(&col, 0, mtype, prtype, col_len, true,
                                UINT32_UNDEFINED,
                                new_dict_table->current_row_version, 0);
    dfield_t dfield;
    col.copy_type(dfield_get_type(&dfield));

    ulint size = field->pack_length();
    uint64_t buf;
    const byte *mysql_data = field->field_ptr();

    row_mysql_store_col_in_innobase_format(
        &dfield, reinterpret_cast<byte *>(&buf), true, mysql_data, size,
        dict_table_is_comp(new_dict_table));

    DD_instant_col_val_coder coder;
    size_t length = 0;
    const char *value = coder.encode(reinterpret_cast<byte *>(dfield.data),
                                     dfield.len, &length);

    dd::String_type default_value;
    default_value.assign(dd::String_type(value, length));
    se_private.set(dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT],
                   default_value);
  };

  uint32_t total_cols =
      new_dict_table->total_col_count + new_dict_table->get_n_sys_cols();
  uint32_t next_phy_pos = 0;
  for (size_t i = 0; i < total_cols; i++) {
    dict_col_t *col = new_dict_table->get_col(i);

    if (col->get_phy_pos() == UINT32_UNDEFINED) {
      ut_ad(col == new_dict_table->get_sys_col(DATA_ROW_ID));
      continue;
    }

    if (col->has_prefix_phy_pos()) {
      /* Column prefix part of clustered index. It appears twice. */
      next_phy_pos += 2;
      continue;
    }

    next_phy_pos++;
  }

  uint32_t cols_added = 0;
  /* For each new column populate se_private_data */
  for (const auto new_column : cols_to_add) {
    Field *field = new_column;

    ut_ad(!innobase_is_v_fld(field));

    /* The MySQL type code has to fit in 8 bits
    in the metadata stored in the InnoDB change buffer. */
    ut_ad(field->charset() == nullptr ||
          field->charset()->number <= MAX_CHAR_COLL_NUM);
    ut_ad(field->charset() == nullptr || field->charset()->number > 0);

    dd::Column *column = const_cast<dd::Column *>(
        dd_find_column(new_dd_table, field->field_name));
    ut_ad(column != nullptr);
    dd::Properties &se_private = column->se_private_data();

    /* Set Table Id */
    se_private.set(dd_index_key_strings[DD_TABLE_ID], new_dict_table->id);

    /* Set Version Added */
    se_private.set(dd_column_key_strings[DD_INSTANT_VERSION_ADDED],
                   new_dict_table->current_row_version + 1);

    /* Set physical position on row */
    se_private.set(dd_column_key_strings[DD_INSTANT_PHYSICAL_POS],
                   next_phy_pos + cols_added++);

    /* Set Default NULL */
    if (field->is_real_null()) {
      se_private.set(dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT_NULL],
                     true);
      continue;
    }

    /* Set Default value */
    set_col_default(field, se_private);
  }

  instant_update_table_cols_count(new_dict_table, cols_to_add.size(), 0);

  ut_ad(cols_added > 0);
  return false;
}

/** Compare the default values between imported column and column defined
in the server. Note that it's absolutely OK if there is no default value
in the column defined in server, since it can be filled in later.
@param[in]      dd_col  dd::Column
@param[in]      col     InnoDB column object
@return true    The default values match
@retval false   Not match */
bool dd_match_default_value(const dd::Column *dd_col, const dict_col_t *col) {
  ut_ad(col->instant_default != nullptr);

  const dd::Properties &private_data = dd_col->se_private_data();

  if (private_data.exists(dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT])) {
    dd::String_type value;
    const byte *default_value;
    size_t len;
    bool match;
    DD_instant_col_val_coder coder;

    private_data.get(dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT], &value);
    default_value = coder.decode(value.c_str(), value.length(), &len);

    match = col->instant_default->len == len &&
            memcmp(col->instant_default->value, default_value, len) == 0;

    return match;

  } else if (private_data.exists(
                 dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT_NULL])) {
    return (col->instant_default->len == UNIV_SQL_NULL);
  }

  return true;
}

/** Write default value of a column to dd::Column
@param[in]      col     default value of this column to write
@param[in,out]  dd_col  where to store the default value */
void dd_write_default_value(const dict_col_t *col, dd::Column *dd_col) {
  if (col->instant_default->len == UNIV_SQL_NULL) {
    dd_col->se_private_data().set(
        dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT_NULL], true);
  } else {
    dd::String_type default_value;
    size_t length = 0;
    DD_instant_col_val_coder coder;
    const char *value = coder.encode(col->instant_default->value,
                                     col->instant_default->len, &length);

    default_value.assign(dd::String_type(value, length));
    dd_col->se_private_data().set(
        dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT], default_value);
  }
}

/** Parse the default value from dd::Column::se_private to dict_col_t
@param[in]      se_private_data dd::Column::se_private
@param[in,out]  col             InnoDB column object
@param[in,out]  heap            Heap to store the default value */
void dd_parse_default_value(const dd::Properties &se_private_data,
                            dict_col_t *col, mem_heap_t *heap) {
  if (se_private_data.exists(
          dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT_NULL])) {
    col->set_default(nullptr, UNIV_SQL_NULL, heap);
  } else if (se_private_data.exists(
                 dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT])) {
    const byte *default_value;
    size_t len;
    dd::String_type value;
    DD_instant_col_val_coder coder;

    se_private_data.get(dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT],
                        &value);

    default_value = coder.decode(value.c_str(), value.length(), &len);

    col->set_default(default_value, len, heap);
  }
}

#ifdef UNIV_DEBUG
void static inline validate_dropped_col_metadata(const dd::Table *dd_table,
                                                 const dict_table_t *table) {
  if (!table->has_instant_drop_cols()) {
    return;
  }

  for (size_t i = table->get_n_user_cols(); i < table->get_total_cols(); ++i) {
    if (is_system_column(table->get_col_name(i))) {
      continue;
    }

    dict_col_t *col = table->get_col(i);

    const dd::Column *dd_col = dd_find_column(dd_table, table->get_col_name(i));
    ut_ad(dd_col != nullptr);

    /* Check phy_pos */
    uint32_t value;
    const char *s = dd_column_key_strings[DD_INSTANT_PHYSICAL_POS];
    dd_col->se_private_data().get(s, &value);
    ut_ad(value == col->get_phy_pos());

    /* Check version_added */
    if (dd_column_is_added(dd_col)) {
      ut_ad(col->is_instant_added());
      ut_ad(dd_column_get_version_added(dd_col) == col->get_version_added());
    } else {
      ut_ad(!col->is_instant_added());
    }

    /* Check version_dropped */
    ut_ad(dd_column_is_dropped(dd_col));
    ut_ad(col->is_instant_dropped());
    ut_ad(dd_column_get_version_dropped(dd_col) == col->get_version_dropped());
  }
}
#endif

/** Import all metadata which is related to instant ADD COLUMN of a table
to dd::Table. This is used for IMPORT.
@param[in]      table           InnoDB table object
@param[in,out]  dd_table        dd::Table */
void dd_import_instant_add_columns(const dict_table_t *table,
                                   dd::Table *dd_table) {
  ut_ad(table->has_instant_cols() || table->has_row_versions());
  ut_ad(dict_table_is_partition(table) == dd_table_is_partitioned(*dd_table));

  if (table->has_instant_cols()) {
    ut_ad(table->is_upgraded_instant());
    if (!dd_table_is_partitioned(*dd_table)) {
      dd_table->se_private_data().set(
          dd_table_key_strings[DD_TABLE_INSTANT_COLS],
          table->get_instant_cols());
    } else { /* Partitioned table */
      uint32_t n_inst_cols = std::numeric_limits<uint32_t>::max();

      if (dd_table->se_private_data().exists(
              dd_table_key_strings[DD_TABLE_INSTANT_COLS])) {
        dd_table->se_private_data().get(
            dd_table_key_strings[DD_TABLE_INSTANT_COLS], &n_inst_cols);
      }

      if (n_inst_cols > table->get_instant_cols()) {
        dd_table->se_private_data().set(
            dd_table_key_strings[DD_TABLE_INSTANT_COLS],
            table->get_instant_cols());
      }

      dd::Partition *partition = nullptr;
      for (const auto dd_part : *dd_table->leaf_partitions()) {
        if (dict_name::match_partition(table->name.m_name, dd_part)) {
          partition = dd_part;
          break;
        }
      }

      ut_ad(partition != nullptr);

      partition->se_private_data().set(
          dd_partition_key_strings[DD_PARTITION_INSTANT_COLS],
          table->get_instant_cols());
    }
  }

  /* Copy all default values if necessary */
  for (uint16_t i = 0; i < table->get_n_user_cols(); ++i) {
    dict_col_t *col = table->get_col(i);

    dd::Column *dd_col = const_cast<dd::Column *>(
        dd_find_column(dd_table, table->get_col_name(i)));
    ut_ad(dd_col != nullptr);
    dd::Properties &private_data = dd_col->se_private_data();

    if (col->instant_default == nullptr) {
      ut_ad(!col->is_instant_added() && !col->is_instant_dropped());
    } else {
      /* Default values mismatch should have been done. So only write default
      value when it's not ever recorded */
      if (!private_data.exists(
              dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT_NULL]) &&
          !private_data.exists(
              dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT])) {
        dd_write_default_value(col, dd_col);
      }
    }

    if (table->has_row_versions()) {
      /* Set phy_pos */
      uint32_t value = col->get_phy_pos();
      const char *s = dd_column_key_strings[DD_INSTANT_PHYSICAL_POS];
      private_data.set(s, value);

      if (col->is_instant_added()) {
        /* Set version_added */
        value = col->get_version_added();
        s = dd_column_key_strings[DD_INSTANT_VERSION_ADDED];
        private_data.set(s, value);
      }

      if (col->is_instant_dropped()) {
        /* Set version_dropped */
        value = col->get_version_dropped();
        s = dd_column_key_strings[DD_INSTANT_VERSION_DROPPED];
        private_data.set(s, value);
      }
    }
  }

  /* Add phy_pos for SYSTEM COLUMNS */
  if (table->has_row_versions()) {
    auto fn = [&](uint32_t sys_col, const char *name) {
      dd::Column *dd_col =
          const_cast<dd::Column *>(dd_find_column(dd_table, name));
      ut_ad(dd_col != nullptr || sys_col == DATA_ROW_ID);
      if (!dd_col) return;
      dd::Properties &private_data = dd_col->se_private_data();

      dict_col_t *dict_col = table->get_sys_col(sys_col);
      ut_ad(dict_col->get_phy_pos() != UINT32_UNDEFINED);

      const char *s = dd_column_key_strings[DD_INSTANT_PHYSICAL_POS];
      private_data.set(s, dict_col->get_phy_pos());
    };

    fn(DATA_ROW_ID, "DB_ROW_ID");
    fn(DATA_TRX_ID, "DB_TRX_ID");
    fn(DATA_ROLL_PTR, "DB_ROLL_PTR");
  }

  ut_d(validate_dropped_col_metadata(dd_table, table));
}

/** Write metadata of a index to dd::Index
@param[in]      dd_space_id     Tablespace id, which server allocates
@param[in,out]  dd_index        dd::Index
@param[in]      index           InnoDB index object */
template <typename Index>
static void dd_write_index(dd::Object_id dd_space_id, Index *dd_index,
                           const dict_index_t *index) {
  ut_ad(index->id != 0);
  ut_ad(index->page >= FSP_FIRST_INODE_PAGE_NO);

  dd_index->set_tablespace_id(dd_space_id);

  dd::Properties &p = dd_index->se_private_data();
  p.set(dd_index_key_strings[DD_INDEX_ID], index->id);
  p.set(dd_index_key_strings[DD_INDEX_SPACE_ID], index->space);
  p.set(dd_index_key_strings[DD_TABLE_ID], index->table->id);
  p.set(dd_index_key_strings[DD_INDEX_ROOT], index->page);
  p.set(dd_index_key_strings[DD_INDEX_TRX_ID], index->trx_id);
}

template void dd_write_index<dd::Index>(dd::Object_id, dd::Index *,
                                        const dict_index_t *);
template void dd_write_index<dd::Partition_index>(dd::Object_id,
                                                  dd::Partition_index *,
                                                  const dict_index_t *);

template <typename Table>
void dd_write_table(dd::Object_id dd_space_id, Table *dd_table,
                    const dict_table_t *table) {
  /* Only set the tablespace id for tables in innodb_system tablespace */
  if (dd_space_id == dict_sys_t::s_dd_sys_space_id) {
    dd_table->set_tablespace_id(dd_space_id);
  }

  dd_table->set_se_private_id(table->id);

  if (DICT_TF_HAS_DATA_DIR(table->flags)) {
    ut_ad(dict_table_is_file_per_table(table));
    dd_table->se_private_data().set(
        dd_table_key_strings[DD_TABLE_DATA_DIRECTORY], true);
  }

  for (auto dd_index : *dd_table->indexes()) {
    /* Don't assume the index orders are the same, even on
    CREATE TABLE. This could be called from TRUNCATE path,
    which would do some adjustment on FULLTEXT index, thus
    the out-of-sync order */
    const dict_index_t *index = dd_find_index(table, dd_index);
    ut_ad(index != nullptr);
    dd_write_index(dd_space_id, dd_index, index);
  }

  bool has_row_versions = table->has_row_versions();
  ut_ad(!has_row_versions || !table->is_fts_aux());

  if (!dd_table_is_partitioned(dd_table->table()) ||
      dd_part_is_first(reinterpret_cast<dd::Partition *>(dd_table))) {
    std::vector<dd::Column *> cols_to_remove;
    const char *s = nullptr;
    for (auto dd_column : *dd_table->table().columns()) {
      dd_column->se_private_data().set(dd_index_key_strings[DD_TABLE_ID],
                                       table->id);

      /* Write physical post only for tables having row versions */
      if (!has_row_versions || dd_column->is_virtual()) {
        continue;
      }

      /* Write physical pos for non-virtual columns */
      dict_col_t *col = table->get_col_by_name(dd_column->name().c_str());
      if (col == nullptr) {
        /* It's possible during TRUNCATE of table with INSTANT DROP column. */
        ut_a(dd_table_has_instant_cols(dd_table->table()));
        ut_a(table->current_row_version == 0);
        ut_a(dd_column_is_dropped(dd_column));

        cols_to_remove.push_back(dd_column);
        continue;
      }

      s = dd_column_key_strings[DD_INSTANT_PHYSICAL_POS];
      if (table->has_row_versions()) {
        /* In case of partitioned table, when a new partition is created,
        column metadata may not be set and needs to be set now. */
        if (dd_table_is_partitioned(dd_table->table())) {
          dd_column->se_private_data().set(s, col->get_phy_pos());

          if (col->is_instant_added()) {
            s = dd_column_key_strings[DD_INSTANT_VERSION_ADDED];
            dd_column->se_private_data().set(
                s, (uint32_t)col->get_version_added());
          }

          if (col->is_instant_dropped()) {
            s = dd_column_key_strings[DD_INSTANT_VERSION_DROPPED];
            dd_column->se_private_data().set(
                s, (uint32_t)col->get_version_dropped());
          }
        } else {
          /* Table has instant col added/dropped. Each column shall have
          physical pos updated. */
          ut_ad(dd_column->se_private_data().exists(s));
        }
      } else {
        /* No instant add/drop col */
        dd_column->se_private_data().set(s, col->get_phy_pos());
      }
    }

    if (cols_to_remove.size() > 0) {
      dd::Abstract_table::Column_collection *col_collection =
          dd_table->table().columns();
      for (auto col : cols_to_remove) {
        ut_ad(std::find(col_collection->begin(), col_collection->end(), col) !=
              col_collection->end());
        col_collection->remove(dynamic_cast<dd::Column_impl *>(col));
      }
    }
  }
}

template void dd_write_table<dd::Table>(dd::Object_id, dd::Table *,
                                        const dict_table_t *);
template void dd_write_table<dd::Partition>(dd::Object_id, dd::Partition *,
                                            const dict_table_t *);

template <typename Table>
void dd_set_table_options(Table *dd_table, const dict_table_t *table) {
  dd::Table *dd_table_def = &(dd_table->table());
  enum row_type type = ROW_TYPE_DEFAULT;
  dd::Table::enum_row_format format = dd::Table::RF_DYNAMIC;
  dd::Properties &options = dd_table_def->options();

  switch (dict_tf_get_rec_format(table->flags)) {
    case REC_FORMAT_REDUNDANT:
      format = dd::Table::RF_REDUNDANT;
      type = ROW_TYPE_REDUNDANT;
      break;
    case REC_FORMAT_COMPACT:
      format = dd::Table::RF_COMPACT;
      type = ROW_TYPE_COMPACT;
      break;
    case REC_FORMAT_COMPRESSED:
      format = dd::Table::RF_COMPRESSED;
      type = ROW_TYPE_COMPRESSED;
      break;
    case REC_FORMAT_DYNAMIC:
      format = dd::Table::RF_DYNAMIC;
      type = ROW_TYPE_DYNAMIC;
      break;
    default:
      ut_error;
  }

  if (!dd_table_is_partitioned(*dd_table_def)) {
    if (auto zip_ssize = DICT_TF_GET_ZIP_SSIZE(table->flags)) {
      uint32_t old_size;
      if (!options.get("key_block_size", &old_size) && old_size != 0) {
        options.set("key_block_size", 1 << (zip_ssize - 1));
      }
    } else {
      options.set("key_block_size", 0);
      /* It's possible that InnoDB ignores the specified
      key_block_size, so check the block_size for every index.
      Server assumes if block_size = 0, there should be no
      option found, so remove it when found */
      for (auto dd_index : *dd_table_def->indexes()) {
        if (dd_index->options().exists("block_size")) {
          dd_index->options().remove("block_size");
        }
      }
    }

    dd_table_def->set_row_format(format);
    if (options.exists("row_type")) {
      options.set("row_type", type);
    }
  } else if (dd_table_def->row_format() != format) {
    dd_table->se_private_data().set(
        dd_partition_key_strings[DD_PARTITION_ROW_FORMAT], format);
  }
}

template void dd_set_table_options<dd::Table>(dd::Table *,
                                              const dict_table_t *);
template void dd_set_table_options<dd::Partition>(dd::Partition *,
                                                  const dict_table_t *);

void dd_update_v_cols(dd::Table *dd_table, table_id_t id) {
  for (auto dd_column : *dd_table->columns()) {
#ifdef UNIV_DEBUG
    if (dd_column->se_private_data().exists(
            dd_index_key_strings[DD_TABLE_ID])) {
      table_id_t table_id;
      dd_column->se_private_data().get(dd_index_key_strings[DD_TABLE_ID],
                                       reinterpret_cast<uint64_t *>(&table_id));
      ut_ad(table_id == id);
    }
#endif /* UNIV_DEBUG */

    if (!dd_column->is_virtual()) {
      continue;
    }

    dd::Properties &p = dd_column->se_private_data();

    if (!p.exists(dd_index_key_strings[DD_TABLE_ID])) {
      p.set(dd_index_key_strings[DD_TABLE_ID], id);
    }
  }
}

/** Write metadata of a tablespace to dd::Tablespace
@param[in,out]  dd_space        dd::Tablespace
@param[in]      space_id        InnoDB tablespace ID
@param[in]      fsp_flags       InnoDB tablespace flags
@param[in]      state           InnoDB tablespace state */
void dd_write_tablespace(dd::Tablespace *dd_space, space_id_t space_id,
                         uint32_t fsp_flags, dd_space_states state) {
  dd::Properties &p = dd_space->se_private_data();
  p.set(dd_space_key_strings[DD_SPACE_ID], space_id);
  p.set(dd_space_key_strings[DD_SPACE_FLAGS], static_cast<uint32>(fsp_flags));
  p.set(dd_space_key_strings[DD_SPACE_SERVER_VERSION],
        DD_SPACE_CURRENT_SRV_VERSION);
  p.set(dd_space_key_strings[DD_SPACE_VERSION], DD_SPACE_CURRENT_SPACE_VERSION);
  p.set(dd_space_key_strings[DD_SPACE_STATE], dd_space_state_values[state]);
}

/** Add fts doc id column and index to new table
when old table has hidden fts doc id without fulltext index
@param[in,out]  new_table       New dd table
@param[in]      old_table       Old dd table */
void dd_add_fts_doc_id_index(dd::Table &new_table, const dd::Table &old_table) {
  if (new_table.columns()->size() == old_table.columns().size()) {
    ut_ad(new_table.indexes()->size() == old_table.indexes().size());
    return;
  }

  ut_ad(new_table.columns()->size() + 1 == old_table.columns().size());
  ut_ad(new_table.indexes()->size() + 1 == old_table.indexes().size());

  /* Add hidden FTS_DOC_ID column */
  dd::Column *col = new_table.add_column();
  col->set_hidden(dd::Column::enum_hidden_type::HT_HIDDEN_SE);
  col->set_name(FTS_DOC_ID_COL_NAME);
  col->set_type(dd::enum_column_types::LONGLONG);
  col->set_nullable(false);
  col->set_unsigned(true);
  col->set_collation_id(1);

  /* Add hidden FTS_DOC_ID index */
  dd_set_hidden_unique_index(new_table.add_index(), FTS_DOC_ID_INDEX_NAME, col);

  return;
}

/** Find the specified dd::Index or dd::Partition_index in an InnoDB table
@tparam         Index                   dd::Index or dd::Partition_index
@param[in]      table                   InnoDB table object
@param[in]      dd_index                Index to search
@return the dict_index_t object related to the index */
template <typename Index>
const dict_index_t *dd_find_index(const dict_table_t *table, Index *dd_index) {
  /* If the name is PRIMARY, return the first index directly,
  because the internal index name could be 'GEN_CLUST_INDEX'.
  It could be possible that the primary key name is not PRIMARY,
  because it's an implicitly upgraded unique index. We have to
  search all the indexes */
  if (dd_index->name() == "PRIMARY") {
    return table->first_index();
  }

  /* The order could be different because all unique dd::Index(es)
  would be in front of other indexes. */
  const dict_index_t *index;
  for (index = table->first_index();
       (index != nullptr &&
        (dd_index->name() != index->name() || !index->is_committed()));
       index = index->next()) {
  }

  ut_ad(index != nullptr);

#ifdef UNIV_DEBUG
  /* Never find another index with the same name */
  const dict_index_t *next_index = index->next();
  for (; (next_index != nullptr && (dd_index->name() != next_index->name() ||
                                    !next_index->is_committed()));
       next_index = next_index->next()) {
  }
  ut_ad(next_index == nullptr);
#endif /* UNIV_DEBUG */

  return index;
}

template const dict_index_t *dd_find_index<dd::Index>(const dict_table_t *,
                                                      dd::Index *);
template const dict_index_t *dd_find_index<dd::Partition_index>(
    const dict_table_t *, dd::Partition_index *);

/** Create an index.
@param[in]      dd_index        DD Index
@param[in,out]  table           InnoDB table
@param[in]      form            MySQL table structure
@param[in]      key_num         key_info[] offset
@return         error code
@retval         0 on success
@retval         HA_ERR_INDEX_COL_TOO_LONG if a column is too long
@retval         HA_ERR_TOO_BIG_ROW if the record is too long */
[[nodiscard]] static int dd_fill_one_dict_index(const dd::Index *dd_index,
                                                dict_table_t *table,
                                                const TABLE_SHARE *form,
                                                uint key_num) {
  const KEY &key = form->key_info[key_num];
  ulint type = 0;
  unsigned n_fields = key.user_defined_key_parts;
  unsigned n_uniq = n_fields;

  ut_ad(!dict_sys_mutex_own());
  /* This name cannot be used for a non-primary index */
  ut_ad(key_num == form->primary_key ||
        my_strcasecmp(system_charset_info, key.name, primary_key_name) != 0);
  /* PARSER is only valid for FULLTEXT INDEX */
  ut_ad((key.flags & (HA_FULLTEXT | HA_USES_PARSER)) != HA_USES_PARSER);
  ut_ad(form->fields > 0);
  ut_ad(n_fields > 0);

  if (key.flags & HA_SPATIAL) {
    ut_ad(!table->is_intrinsic());
    type = DICT_SPATIAL;
    ut_ad(n_fields == 1);
  } else if (key.flags & HA_FULLTEXT) {
    ut_ad(!table->is_intrinsic());
    type = DICT_FTS;
    n_uniq = 0;
  } else if (key_num == form->primary_key) {
    ut_ad(key.flags & HA_NOSAME);
    ut_ad(n_uniq > 0);
    type = DICT_CLUSTERED | DICT_UNIQUE;
  } else {
    type = (key.flags & HA_NOSAME) ? DICT_UNIQUE : 0;
  }

  ut_ad(!!(type & DICT_FTS) == (n_uniq == 0));

  dict_index_t *index =
      dict_mem_index_create(table->name.m_name, key.name, 0, type, n_fields);

  index->n_uniq = n_uniq;

  const ulint max_len = DICT_MAX_FIELD_LEN_BY_FORMAT(table);
  DBUG_EXECUTE_IF("ib_create_table_fail_at_create_index",
                  dict_mem_index_free(index);
                  my_error(ER_INDEX_COLUMN_TOO_LONG, MYF(0), max_len);
                  return HA_ERR_TOO_BIG_ROW;);

  for (unsigned i = 0; i < key.user_defined_key_parts; i++) {
    const KEY_PART_INFO *key_part = &key.key_part[i];
    unsigned prefix_len = 0;
    const Field *field = key_part->field;
    ut_ad(field == form->field[key_part->fieldnr - 1]);
    ut_ad(field == form->field[field->field_index()]);

    if (field->is_virtual_gcol()) {
      index->type |= DICT_VIRTUAL;

      /* Whether it is a multi-value index */
      if ((field->gcol_info->expr_item &&
           field->gcol_info->expr_item->returns_array()) ||
          field->is_array()) {
        index->type |= DICT_MULTI_VALUE;
      }
    }

    bool is_asc = true;

    if (key_part->key_part_flag & HA_REVERSE_SORT) {
      is_asc = false;
    }

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
      ut_ad(key.flags & (HA_SPATIAL | HA_FULLTEXT) ||
            (!is_blob(field->real_type()) &&
             field->real_type() != MYSQL_TYPE_GEOMETRY) ||
            key_part->length >= (field->type() == MYSQL_TYPE_VARCHAR
                                     ? field->key_length()
                                     : field->pack_length()));
      prefix_len = 0;
    }

    if ((key_part->length > max_len || prefix_len > max_len) &&
        !(key.flags & (HA_FULLTEXT))) {
      dict_mem_index_free(index);
      my_error(ER_INDEX_COLUMN_TOO_LONG, MYF(0), max_len);
      return HA_ERR_INDEX_COL_TOO_LONG;
    }

    dict_col_t *col = nullptr;

    if (innobase_is_v_fld(field)) {
      dict_v_col_t *v_col =
          dict_table_get_nth_v_col_mysql(table, field->field_index());
      col = reinterpret_cast<dict_col_t *>(v_col);
    } else {
      ulint t_num_v = 0;
      for (ulint z = 0; z < field->field_index(); z++) {
        if (innobase_is_v_fld(form->field[z])) {
          t_num_v++;
        }
      }

      col = &table->cols[field->field_index() - t_num_v];
    }

    col->is_visible = !field->is_hidden_by_system();
    dict_index_add_col(index, table, col, prefix_len, is_asc);
  }

  ut_ad(((key.flags & HA_FULLTEXT) == HA_FULLTEXT) ==
        !!(index->type & DICT_FTS));

  index->n_user_defined_cols = key.user_defined_key_parts;

  if (dict_index_add_to_cache(table, index, 0, false) != DB_SUCCESS) {
    ut_d(ut_error);
    ut_o(return HA_ERR_GENERIC);
  }

  index = UT_LIST_GET_LAST(table->indexes);

  if (index->type & DICT_FTS) {
    ut_ad((key.flags & HA_FULLTEXT) == HA_FULLTEXT);
    ut_ad(index->n_uniq == 0);
    ut_ad(n_uniq == 0);

    if (table->fts->cache == nullptr) {
      DICT_TF2_FLAG_SET(table, DICT_TF2_FTS);
      table->fts->cache = fts_cache_create(table);

      rw_lock_x_lock(&table->fts->cache->init_lock, UT_LOCATION_HERE);
      /* Notify the FTS cache about this index. */
      fts_cache_index_cache_create(table, index);
      rw_lock_x_unlock(&table->fts->cache->init_lock);
    }
  }

  if (strcmp(index->name, FTS_DOC_ID_INDEX_NAME) == 0) {
    ut_ad(table->fts_doc_id_index == nullptr);
    table->fts_doc_id_index = index;
  }

  if (dict_index_is_spatial(index)) {
    ut_ad(dd_index->name() == key.name);
    size_t geom_col_idx;
    for (geom_col_idx = 0; geom_col_idx < dd_index->elements().size();
         ++geom_col_idx) {
      if (!dd_index->elements()[geom_col_idx]->column().is_se_hidden()) break;
    }
    const dd::Column &col = dd_index->elements()[geom_col_idx]->column();
    bool srid_has_value = col.srs_id().has_value();
    index->fill_srid_value(srid_has_value ? col.srs_id().value() : 0,
                           srid_has_value);
  }

  return 0;
}

/** Parse MERGE_THRESHOLD value from a comment string.
@param[in]      thd     connection
@param[in]      str     string which might include 'MERGE_THRESHOLD='
@return value parsed
@retval dict_index_t::MERGE_THRESHOLD_DEFAULT for missing or invalid value. */
static ulint dd_parse_merge_threshold(THD *thd, const char *str) {
  static constexpr char label[] = "MERGE_THRESHOLD=";
  const char *pos = strstr(str, label);

  if (pos != nullptr) {
    pos += (sizeof label) - 1;

    int ret = atoi(pos);

    if (ret > 0 && unsigned(ret) <= DICT_INDEX_MERGE_THRESHOLD_DEFAULT) {
      return static_cast<ulint>(ret);
    }

    push_warning_printf(thd, Sql_condition::SL_WARNING, WARN_OPTION_IGNORED,
                        ER_DEFAULT(WARN_OPTION_IGNORED), "MERGE_THRESHOLD");
  }

  return DICT_INDEX_MERGE_THRESHOLD_DEFAULT;
}

/** Copy attributes from MySQL TABLE_SHARE into an InnoDB table object.
@param[in,out]  thd             thread context
@param[in,out]  table           InnoDB table
@param[in]      table_share     TABLE_SHARE */
inline void dd_copy_from_table_share(THD *thd, dict_table_t *table,
                                     const TABLE_SHARE *table_share) {
  if (table->is_temporary()) {
    dict_stats_set_persistent(table, false, true);
  } else {
    switch (table_share->db_create_options &
            (HA_OPTION_STATS_PERSISTENT | HA_OPTION_NO_STATS_PERSISTENT)) {
      default:
        /* If a CREATE or ALTER statement contains
        STATS_PERSISTENT=0 STATS_PERSISTENT=1,
        it will be interpreted as STATS_PERSISTENT=1. */
      case HA_OPTION_STATS_PERSISTENT:
        dict_stats_set_persistent(table, true, false);
        break;
      case HA_OPTION_NO_STATS_PERSISTENT:
        dict_stats_set_persistent(table, false, true);
        break;
      case 0:
        break;
    }
  }

  dict_stats_auto_recalc_set(
      table, table_share->stats_auto_recalc == HA_STATS_AUTO_RECALC_ON,
      table_share->stats_auto_recalc == HA_STATS_AUTO_RECALC_OFF);

  table->stats_sample_pages = table_share->stats_sample_pages;

  const ulint merge_threshold_table =
      table_share->comment.str
          ? dd_parse_merge_threshold(thd, table_share->comment.str)
          : DICT_INDEX_MERGE_THRESHOLD_DEFAULT;
  dict_index_t *index = table->first_index();

  index->merge_threshold = merge_threshold_table;

  if (dict_index_is_auto_gen_clust(index)) {
    index = index->next();
  }

  for (uint i = 0; i < table_share->keys; i++) {
    const KEY *key_info = &table_share->key_info[i];

    ut_ad(index != nullptr);

    if (key_info->flags & HA_USES_COMMENT && key_info->comment.str != nullptr) {
      index->merge_threshold =
          dd_parse_merge_threshold(thd, key_info->comment.str);
    } else {
      index->merge_threshold = merge_threshold_table;
    }

    index = index->next();

    /* Skip hidden FTS_DOC_ID index */
    if (index != nullptr && index->hidden) {
      ut_ad(strcmp(index->name, FTS_DOC_ID_INDEX_NAME) == 0);
      index = index->next();
    }
  }

#ifdef UNIV_DEBUG
  if (index != nullptr) {
    ut_ad(table_share->keys == 0);
    ut_ad(index->hidden);
    ut_ad(strcmp(index->name, FTS_DOC_ID_INDEX_NAME) == 0);
  }
#endif
}

/** Instantiate index related metadata
@param[in,out]  dd_table        Global DD table metadata
@param[in]      m_form          MySQL table definition
@param[in,out]  m_table         InnoDB table definition
@param[in]      m_thd           THD instance
@return 0 if successful, otherwise error number */
inline int dd_fill_dict_index(const dd::Table &dd_table, const TABLE *m_form,
                              dict_table_t *m_table, THD *m_thd) {
  int error = 0;

  ut_ad(!dict_sys_mutex_own());

  /* Create the keys */
  if (m_form->s->keys == 0 || m_form->s->primary_key == MAX_KEY) {
    /* Create an index which is used as the clustered index;
    order the rows by the hidden InnoDB column DB_ROW_ID. */
    dict_index_t *index = dict_mem_index_create(
        m_table->name.m_name, "GEN_CLUST_INDEX", 0, DICT_CLUSTERED, 0);
    index->n_uniq = 0;

    dberr_t new_err =
        dict_index_add_to_cache(m_table, index, index->page, false);
    if (new_err != DB_SUCCESS) {
      error = HA_ERR_GENERIC;
      goto dd_error;
    }
  } else {
    /* In InnoDB, the clustered index must always be
    created first. */
    error = dd_fill_one_dict_index(dd_table.indexes()[m_form->s->primary_key],
                                   m_table, m_form->s, m_form->s->primary_key);
    if (error != 0) {
      goto dd_error;
    }
  }

  for (uint i = !m_form->s->primary_key; i < m_form->s->keys; i++) {
    ulint dd_index_num = i + ((m_form->s->primary_key == MAX_KEY) ? 1 : 0);

    error = dd_fill_one_dict_index(dd_table.indexes()[dd_index_num], m_table,
                                   m_form->s, i);
    if (error != 0) {
      goto dd_error;
    }
  }

  if (dict_table_has_fts_index(m_table)) {
    ut_ad(DICT_TF2_FLAG_IS_SET(m_table, DICT_TF2_FTS));
  }

  /* Create the ancillary tables that are common to all FTS indexes on
  this table. */
  if (DICT_TF2_FLAG_IS_SET(m_table, DICT_TF2_FTS_HAS_DOC_ID) ||
      DICT_TF2_FLAG_IS_SET(m_table, DICT_TF2_FTS)) {
    fts_doc_id_index_enum ret;

    ut_ad(!m_table->is_intrinsic());
    /* Check whether there already exists FTS_DOC_ID_INDEX */
    ret = innobase_fts_check_doc_id_index_in_def(m_form->s->keys,
                                                 m_form->key_info);

    switch (ret) {
      case FTS_INCORRECT_DOC_ID_INDEX:
        push_warning_printf(m_thd, Sql_condition::SL_WARNING,
                            ER_WRONG_NAME_FOR_INDEX,
                            " InnoDB: Index name %s is reserved"
                            " for the unique index on"
                            " FTS_DOC_ID column for FTS"
                            " Document ID indexing"
                            " on table %s. Please check"
                            " the index definition to"
                            " make sure it is of correct"
                            " type\n",
                            FTS_DOC_ID_INDEX_NAME, m_table->name.m_name);

        if (m_table->fts) {
          fts_free(m_table);
        }

        my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0), FTS_DOC_ID_INDEX_NAME);
        return HA_ERR_GENERIC;
      case FTS_EXIST_DOC_ID_INDEX:
        break;
      case FTS_NOT_EXIST_DOC_ID_INDEX:
        dict_index_t *doc_id_index;
        doc_id_index = dict_mem_index_create(
            m_table->name.m_name, FTS_DOC_ID_INDEX_NAME, 0, DICT_UNIQUE, 1);
        doc_id_index->add_field(FTS_DOC_ID_COL_NAME, 0, true);

        dberr_t new_err = dict_index_add_to_cache(m_table, doc_id_index,
                                                  doc_id_index->page, false);
        if (new_err != DB_SUCCESS) {
          error = HA_ERR_GENERIC;
          goto dd_error;
        }

        doc_id_index = UT_LIST_GET_LAST(m_table->indexes);
        doc_id_index->hidden = true;
    }

    /* Cache all the FTS indexes on this table in the FTS
    specific structure. They are used for FTS indexed
    column update handling. */
    if (dict_table_has_fts_index(m_table)) {
      fts_t *fts = m_table->fts;
      ut_a(fts != nullptr);

      dict_table_get_all_fts_indexes(m_table, m_table->fts->indexes);
    }

    ulint fts_doc_id_col = ULINT_UNDEFINED;

    ret = innobase_fts_check_doc_id_index(m_table, nullptr, &fts_doc_id_col);

    if (ret != FTS_INCORRECT_DOC_ID_INDEX) {
      ut_ad(m_table->fts->doc_col == ULINT_UNDEFINED);
      m_table->fts->doc_col = fts_doc_id_col;
      ut_ad(m_table->fts->doc_col != ULINT_UNDEFINED);

      m_table->fts_doc_id_index =
          dict_table_get_index_on_name(m_table, FTS_DOC_ID_INDEX_NAME);
    }
  }

  if (error == 0) {
    dd_copy_from_table_share(m_thd, m_table, m_form->s);
    ut_ad(!m_table->is_temporary() ||
          !dict_table_page_size(m_table).is_compressed());
    if (!m_table->is_temporary()) {
      dict_table_stats_latch_create(m_table, true);
    }
  } else {
  dd_error:
    dict_sys_mutex_enter();

    for (dict_index_t *f_index = UT_LIST_GET_LAST(m_table->indexes);
         f_index != nullptr; f_index = UT_LIST_GET_LAST(m_table->indexes)) {
      dict_index_remove_from_cache(m_table, f_index);
    }

    dict_sys_mutex_exit();

    dict_mem_table_free(m_table);
  }

  return error;
}

/** Determine if a table contains a fulltext index.
@param[in]      table           dd::Table
@return whether the table contains any fulltext index */
inline bool dd_table_contains_fulltext(const dd::Table &table) {
  for (const dd::Index *index : table.indexes()) {
    if (index->type() == dd::Index::IT_FULLTEXT) {
      return true;
    }
  }
  return false;
}

/** Read the metadata of default values for all columns added instantly
@param[in]      dd_table        dd::Table
@param[in,out]  table           InnoDB table object */
static void dd_fill_instant_columns_default(const dd::Table &dd_table,
                                            dict_table_t *table) {
  ut_ad(table->has_instant_cols() || table->has_row_versions());
  ut_ad(dd_table_has_instant_cols(dd_table));

#ifdef UNIV_DEBUG
  for (uint16_t i = 0; i < table->get_n_cols(); ++i) {
    ut_ad(table->get_col(i)->instant_default == nullptr);
  }
#endif /* UNIV_DEBUG */

  uint32_t skip = 0;
  if (dd_table_is_partitioned(dd_table)) {
    if (dd_table_is_upgraded_instant(dd_table)) {
      /* In instant v1, when a partition is added into table, it won't have any
      instant columns eg :
      - t1 (c1, c2) with partition p0, p1.
      - INSTANT ADD c3
      - For p0 and p1, n_instant_cols = 2;
      - ADD NEW Partition p2.
      - p2 would have c1, c2 and c3 as normal column i.e. n_instant_cols = 0.
      - INSTANT ADD c4.
      - For p2, n_instant_cols = 3 now.
      - So for p0 and p1, we have to populate instant default for c3 and c4. But
        for p3, we need to set only for c4. We need to skip for c3.
      - So skip count = 3 - 2 = 1.
      - So loop has to try 2 times to set the default value for each partition
        but for skip 1st time (for c3) for this new partition p2. */
      const char *s = dd_table_key_strings[DD_TABLE_INSTANT_COLS];
      ut_ad(dd_table.se_private_data().exists(s));

      uint32_t cols;
      dd_table.se_private_data().get(s, &cols);
      ut_ad(cols <= table->get_instant_cols());
      skip = table->get_instant_cols() - cols;
    }
  }

#ifdef UNIV_DEBUG
  auto verify_name = [&](const dd::Column *col, uint32_t pos) {
    const char *name = table->col_names;
    for (uint32_t i = 0; i < pos - 1; ++i) {
      name += strlen(name) + 1;
    }
    ut_ad(col->name() == name);
  };
#endif /* UNIV_DEBUG */

  uint32_t innodb_pos = 0;
  for (const auto col : dd_table.columns()) {
    if (col->is_virtual() || is_system_column(col->name().c_str())) {
      continue;
    }

    const dd::Properties &private_data = col->se_private_data();

    /* Skip the dropped columns */
    if (dd_column_is_dropped(col)) {
      continue;
    }

    dict_col_t *column = table->get_col(innodb_pos++);
    ut_ad(!column->is_virtual());

    ut_d(verify_name(col, innodb_pos));

    if (!private_data.exists(
            dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT_NULL]) &&
        !private_data.exists(
            dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT])) {
      /* This is not INSTANT ADD column */
      ut_ad(!dd_column_is_added(col));
      continue;
    }

    /* Skip only if it is instant added in v1. */
    if (skip > 0 && !dd_column_is_added(col)) {
      --skip;
      continue;
    }

    /* Note that it's before dict_table_add_to_cache(),
    don't worry about the dict_sys->size. */
    dd_parse_default_value(private_data, column, table->heap);
  }

#ifdef UNIV_DEBUG
  if (!table->has_row_versions()) {
    uint16_t n_default = 0;
    for (uint16_t i = 0; i < table->get_n_user_cols(); ++i) {
      if (table->get_col(i)->instant_default != nullptr) {
        ++n_default;
      }
    }

    /* Instant add columns + columns before first instant == total columns in
    table now. */
    ut_ad(n_default + table->get_instant_cols() == table->get_n_user_cols());
  }
#endif /* UNIV_DEBUG */
}

static void fill_dict_dropped_column(const dd::Column *column,
                                     dict_table_t *dict_table,
                                     IF_DEBUG(uint32_t &crv, )
                                         mem_heap_t *heap) {
  ut_ad(!column->is_virtual());
  ut_ad(column->is_se_hidden());
  ut_ad(!is_system_column(column->name().c_str()));

  /* Get version added */
  uint32_t v_added = dd_column_get_version_added(column);

  /* Get version dropped */
  ut_a(dd_column_is_dropped(column));
  uint32_t v_dropped = dd_column_get_version_dropped(column);

#ifdef UNIV_DEBUG
  crv = std::max(crv, v_dropped);
#endif

  /* Get physical position */
  uint32_t phy_pos = UINT32_UNDEFINED;
  const char *s = dd_column_key_strings[DD_INSTANT_PHYSICAL_POS];
  ut_ad(column->se_private_data().exists(s));
  column->se_private_data().get(s, &phy_pos);
  ut_ad(phy_pos != UINT32_UNDEFINED);

  /* Get column mtype */
  ulint unsigned_type;
  ulint binary_type;
  ulint charset_no;
  const CHARSET_INFO *charset = dd_get_mysql_charset(column->collation_id());
  ulint mtype = get_innobase_type_from_mysql_dd_type(
      &unsigned_type, &binary_type, &charset_no, column->type(), charset,
      column->is_unsigned());

  /* Get column length */
  ulint col_len = calc_pack_length(
      column->type(), column->char_length(), column->elements_count(),
      /* InnoDB always treats BIT as char. */
      true, column->numeric_scale(), column->is_unsigned());

  ulint long_true_varchar = 0;
  if (column->type() == dd::enum_column_types::VARCHAR) {
    size_t length_bytes = column->char_length() > 255 ? 2 : 1;
    col_len -= length_bytes;

    if (length_bytes == 2) {
      long_true_varchar = DATA_LONG_TRUE_VARCHAR;
    }
  }

  /* Get column prtype */
  ulint nulls_allowed = column->is_nullable() ? 0 : DATA_NOT_NULL;
  ulint prtype = dtype_form_prtype(
      (ulint)dd_get_old_field_type(column->type()) | unsigned_type |
          binary_type | nulls_allowed | long_true_varchar,
      charset_no);

  /* Add column to InnoDB dictionary cache */
  dict_mem_table_add_col(dict_table, heap, column->name().c_str(), mtype,
                         prtype, col_len, false, phy_pos, v_added, v_dropped);
}

void get_field_types(const dd::Table *dd_tab, const dict_table_t *m_table,
                     const Field *field, unsigned &col_len, ulint &mtype,
                     ulint &prtype) {
  /* The MySQL type code has to fit in 8 bits in the metadata stored in the
  InnoDB change buffer. */
  ut_ad(field->charset() == nullptr ||
        field->charset()->number <= MAX_CHAR_COLL_NUM);
  ut_ad(field->charset() == nullptr || field->charset()->number > 0);

  ulint long_true_varchar = 0;
  ulint binary_type;
  ulint nulls_allowed;
  ulint unsigned_type;
  ulint charset_no = 0;

  mtype = get_innobase_type_from_mysql_type(&unsigned_type, field);

  nulls_allowed = field->is_nullable() ? 0 : DATA_NOT_NULL;

  /* Convert non nullable fields in FTS AUX tables as nullable.
  This is because in 5.7, we created FTS AUX tables clustered
  index with nullable field, although NULLS are not inserted.
  When fields are nullable, the record layout is dependent on
  that. When registering FTS AUX Tables with new DD, we cannot
  register nullable fields as part of Primary Key. Hence we register
  them as non-nullabe in DD but treat as nullable in InnoDB.
  This way the compatibility with 5.7 FTS AUX tables is also
  maintained. */
  if (dd_tab && m_table->is_fts_aux()) {
    const dd::Table &dd_table = dd_tab->table();
    const dd::Column *dd_col = dd_find_column(&dd_table, field->field_name);
    const dd::Properties &p = dd_col->se_private_data();
    if (p.exists("nullable")) {
      bool nullable;
      p.get("nullable", &nullable);
      nulls_allowed = nullable ? 0 : DATA_NOT_NULL;
    }
  }

  binary_type = field->binary() ? DATA_BINARY_TYPE : 0;

  if (dtype_is_string_type(mtype)) {
    charset_no = static_cast<ulint>(field->charset()->number);
  }

  col_len = field->pack_length();
  if (field->type() == MYSQL_TYPE_VARCHAR) {
    col_len -= field->get_length_bytes();

    if (field->get_length_bytes() == 2) {
      long_true_varchar = DATA_LONG_TRUE_VARCHAR;
    }
  }

  ulint is_virtual = (innobase_is_v_fld(field)) ? DATA_VIRTUAL : 0;

  ulint is_multi_val =
      innobase_is_multi_value_fld(field) ? DATA_MULTI_VALUE : 0;

  if (is_multi_val) {
    col_len = field->key_length();
  }

  if (!is_virtual) {
    prtype =
        dtype_form_prtype((ulint)field->type() | nulls_allowed | unsigned_type |
                              binary_type | long_true_varchar,
                          charset_no);
  } else {
    prtype = dtype_form_prtype(
        (ulint)field->type() | nulls_allowed | unsigned_type | binary_type |
            long_true_varchar | is_virtual | is_multi_val,
        charset_no);
  }
}

template <typename Table>
static inline void fill_dict_existing_column(
    const Table *dd_tab, const TABLE *m_form, dict_table_t *m_table,
    IF_DEBUG(uint32_t &crv, ) mem_heap_t *heap, const uint32_t pos,
    bool has_row_versions) {
  const Field *field = m_form->field[pos];
  unsigned col_len;
  ulint mtype;
  ulint prtype;
  get_field_types(&dd_tab->table(), m_table, field, col_len, mtype, prtype);

  ulint is_virtual = (innobase_is_v_fld(field)) ? DATA_VIRTUAL : 0;

  if (!is_virtual) {
    const dd::Column *column =
        dd_find_column(&dd_tab->table(), field->field_name);
    ut_ad(column != nullptr);

    /* Get version added */
    uint32_t v_added = dd_column_get_version_added(column);
#ifdef UNIV_DEBUG
    if (dd_is_valid_row_version(v_added)) {
      crv = std::max(crv, v_added);
    }
#endif

    /* This column must be present */
    ut_ad(!dd_column_is_dropped(column));

    /* Get physical pos */
    uint32_t phy_pos = UINT32_UNDEFINED;
    if (has_row_versions) {
      ut_ad(!m_table->is_system_table && !m_table->is_fts_aux());
      const char *s = dd_column_key_strings[DD_INSTANT_PHYSICAL_POS];

      ut_ad(column->se_private_data().exists(s));
      if (column->se_private_data().exists(s)) {
        column->se_private_data().get(s, &phy_pos);
        ut_ad(phy_pos != UINT32_UNDEFINED);
      }
    }

    dict_mem_table_add_col(m_table, heap, field->field_name, mtype, prtype,
                           col_len, !field->is_hidden_by_system(), phy_pos,
                           (uint8_t)v_added, UINT8_UNDEFINED);
  } else {
    dict_mem_table_add_v_col(m_table, heap, field->field_name, mtype, prtype,
                             col_len, pos,
                             field->gcol_info->non_virtual_base_columns(),
                             !field->is_hidden_by_system());
  }

  bool is_stored = innobase_is_s_fld(field);
  if (is_stored) {
    ut_ad(!is_virtual);
    /* Added stored column in m_s_cols list. */
    dict_mem_table_add_s_col(m_table,
                             field->gcol_info->non_virtual_base_columns());
  }
}

void fill_dict_dropped_columns(const dd::Table *dd_table,
                               dict_table_t *dict_table,
                               IF_DEBUG(uint32_t &crv, ) mem_heap_t *heap) {
  ut_ad(!dict_table->is_system_table);

  /* Fill column which has(d) been dropped instantly from the table */
  ut_d(uint32_t dropped_col_count = 0);
  for (auto column : dd_table->columns()) {
    if (is_system_column(column->name().c_str())) {
      continue;
    }

    if (dd_column_is_dropped(column)) {
      fill_dict_dropped_column(column, dict_table, IF_DEBUG(crv, ) heap);
      ut_d(dropped_col_count++;);
    }
  }

  ut_ad(dict_table->get_n_instant_drop_cols() == dropped_col_count);
}

template <typename Table>
static inline void fill_dict_columns(const Table *dd_table, const TABLE *m_form,
                                     dict_table_t *dict_table,
                                     const unsigned n_mysql_cols,
                                     mem_heap_t *heap, bool add_doc_id) {
  IF_DEBUG(uint32_t crv = 0;)

  /* Add existing columns metadata information. */
  bool has_row_versions = dd_table_has_row_versions(dd_table->table());
  for (unsigned i = 0; i < n_mysql_cols; i++) {
    fill_dict_existing_column(dd_table, m_form, dict_table,
                              IF_DEBUG(crv, ) heap, i, has_row_versions);
  }

  if (add_doc_id) {
    /* Add the hidden FTS_DOC_ID column. */
    fts_add_doc_id_column(dict_table, heap);
  }

  /* Add system columns to make adding index work */
  dict_table_add_system_columns(dict_table, heap);

  if (dict_table->has_row_versions()) {
    /* Read physical pos for system columns. */

    auto fn = [&](uint32_t sys_col, const char *name) {
      const dd::Column *dd_col = dd_find_column(&dd_table->table(), name);

      uint32_t phy_pos = UINT32_UNDEFINED;
      const char *s = dd_column_key_strings[DD_INSTANT_PHYSICAL_POS];
      if (dd_col && dd_col->se_private_data().exists(s)) {
        dd_col->se_private_data().get(s, &phy_pos);
      }

      dict_col_t *dict_col = dict_table->get_sys_col(sys_col);
      dict_col->set_phy_pos(phy_pos);
    };

    fn(DATA_ROW_ID, "DB_ROW_ID");
    fn(DATA_TRX_ID, "DB_TRX_ID");
    fn(DATA_ROLL_PTR, "DB_ROLL_PTR");
  }

  /* If table has INSTANT DROP columns, add them now. */
  if (dict_table->has_instant_drop_cols()) {
    fill_dict_dropped_columns(&dd_table->table(), dict_table,
                              IF_DEBUG(crv, ) heap);
  }

  ut_ad(dict_table->current_row_version == crv);

  /* For each virtual column, we will need to set up its base column info */
  if (dict_table->n_v_cols > 0) {
    ulint j = 0;
    for (unsigned i = 0; i < n_mysql_cols; i++) {
      dict_v_col_t *v_col;

      Field *field = m_form->field[i];

      if (!innobase_is_v_fld(field)) {
        continue;
      }

      v_col = dict_table_get_nth_v_col(dict_table, j);

      j++;

      innodb_base_col_setup(dict_table, field, v_col);
    }
  }
}

/** Instantiate in-memory InnoDB table metadata (dict_table_t),
without any indexes.
@tparam         Table           dd::Table or dd::Partition
@param[in]      dd_tab          Global Data Dictionary metadata,
                                or NULL for internal temporary table
@param[in]      m_form          MySQL TABLE for current table
@param[in]      norm_name       normalized table name
@param[in]      create_info     create info
@param[in]      zip_allowed     whether ROW_FORMAT=COMPRESSED is OK
@param[in]      strict          whether to use innodb_strict_mode=ON
@param[in]      m_thd           thread THD
@param[in]      is_implicit     if it is an implicit tablespace
@return created dict_table_t on success or nullptr */
template <typename Table>
static inline dict_table_t *dd_fill_dict_table(const Table *dd_tab,
                                               const TABLE *m_form,
                                               const char *norm_name,
                                               HA_CREATE_INFO *create_info,
                                               bool zip_allowed, bool strict,
                                               THD *m_thd, bool is_implicit) {
  ut_ad(dd_tab != nullptr);
  ut_ad(m_thd != nullptr);
  ut_ad(norm_name != nullptr);
  ut_ad(create_info == nullptr || m_form->s->row_type == create_info->row_type);
  ut_ad(create_info == nullptr ||
        m_form->s->key_block_size == create_info->key_block_size);
  ut_ad(dd_tab != nullptr);

  if (m_form->s->fields > REC_MAX_N_USER_FIELDS) {
    my_error(ER_TOO_MANY_FIELDS, MYF(0));
    return nullptr;
  }

  /* Fetch se private data for table from DD object. */
  const dd::Properties &table_se_private = dd_tab->table().se_private_data();

  /* Set encryption option for file-per-table tablespace. */
  bool is_encrypted = false;
  dd::String_type encrypt;
  if (dd_tab->table().options().exists("encrypt_type")) {
    dd_tab->table().options().get("encrypt_type", &encrypt);
    if (!Encryption::is_none(encrypt.c_str())) {
      ut_ad(innobase_strcasecmp(encrypt.c_str(), "y") == 0);
      is_encrypted = true;
    }
  }

  /* Check discard flag. */
  bool is_discard = false;
  is_discard = dd_is_discarded(*dd_tab);

  const unsigned n_mysql_cols = m_form->s->fields;

  /* First check if dd::Table contains the right hidden column as FTS_DOC_ID */
  bool has_doc_id = false;
  const dd::Column *doc_col;
  doc_col = dd_find_column(&dd_tab->table(), FTS_DOC_ID_COL_NAME);

  /* Check weather this is a proper typed FTS_DOC_ID */
  if (doc_col && doc_col->type() == dd::enum_column_types::LONGLONG &&
      !doc_col->is_nullable()) {
    has_doc_id = true;
  }

  const bool fulltext =
      dd_tab != nullptr && dd_table_contains_fulltext(dd_tab->table());

#ifdef UNIV_DEBUG
  /* If there is a fulltext index, then it must have a FTS_DOC_ID */
  if (fulltext) {
    ut_ad(has_doc_id);
  }
#endif

  /* Need to add FTS_DOC_ID column if it is not defined by user,
  since TABLE_SHARE::fields does not contain it if it is a hidden col */
  bool add_doc_id = false;
  if (has_doc_id && doc_col->is_se_hidden()) {
#ifdef UNIV_DEBUG
    ulint doc_id_col;
    ut_ad(!create_table_check_doc_id_col(m_thd, m_form, &doc_id_col));
#endif
    add_doc_id = true;
  }

  const unsigned n_cols = n_mysql_cols + (add_doc_id ? 1 : 0);

  row_type real_type = ROW_TYPE_NOT_USED;

  if (dd_table_is_partitioned(dd_tab->table())) {
    const dd::Properties &part_p = dd_tab->se_private_data();
    if (part_p.exists(dd_partition_key_strings[DD_PARTITION_ROW_FORMAT])) {
      dd::Table::enum_row_format format;
      part_p.get(dd_partition_key_strings[DD_PARTITION_ROW_FORMAT],
                 reinterpret_cast<uint32_t *>(&format));
      switch (format) {
        case dd::Table::RF_REDUNDANT:
          real_type = ROW_TYPE_REDUNDANT;
          break;
        case dd::Table::RF_COMPACT:
          real_type = ROW_TYPE_COMPACT;
          break;
        case dd::Table::RF_COMPRESSED:
          real_type = ROW_TYPE_COMPRESSED;
          break;
        case dd::Table::RF_DYNAMIC:
          real_type = ROW_TYPE_DYNAMIC;
          break;
        default:
          ut_d(ut_error);
      }
    }
  }

  bool is_redundant;
  bool blob_prefix;
  ulint zip_ssize;
  /* Validate the table format options */
  if (format_validate(m_thd, m_form, real_type, zip_allowed, strict,
                      &is_redundant, &blob_prefix, &zip_ssize, is_implicit)) {
    return nullptr;
  }

  ulint n_v_cols = 0;
  ulint n_m_v_cols = 0;

  /* Find out the number of virtual columns */
  for (ulint i = 0; i < m_form->s->fields; i++) {
    Field *field = m_form->field[i];

    ut_ad(!(!innobase_is_v_fld(field) && innobase_is_multi_value_fld(field)));

    if (innobase_is_v_fld(field)) {
      n_v_cols++;

      if (innobase_is_multi_value_fld(field)) {
        n_m_v_cols++;
      }
    }
  }

  ut_ad(n_v_cols <= n_cols);

  uint32_t i_c = 0;
  uint32_t c_c = 0;
  uint32_t t_c = 0;
  uint32_t c_r_v = 0;

  dd_table_get_column_counters(dd_tab->table(), i_c, c_c, t_c, c_r_v);
  /* Create the dict_table_t */
  dict_table_t *m_table = dict_mem_table_create(norm_name, 0, n_cols, n_v_cols,
                                                n_m_v_cols, 0, 0, t_c - c_c);

  /* Setup column counters and current row version for table */
  m_table->initial_col_count = i_c;
  m_table->current_col_count = c_c;
  m_table->total_col_count = t_c;
  m_table->current_row_version = c_r_v;

  /* Set up the field in the newly allocated dict_table_t */
  m_table->id = dd_tab->se_private_id();

  if (dd_tab->se_private_data().exists(
          dd_table_key_strings[DD_TABLE_DATA_DIRECTORY])) {
    m_table->flags |= DICT_TF_MASK_DATA_DIR;
  }

  /* Note : In V2, we don't need number of columns in table when first INSTANT
  ADD was done. */

  /* For upgraded table having INSTANT ADD added columns in V1, it's necessary
  to read the number of instant columns for normal table (from dd::Table) or
  for partitioned table (from dd::Partition). One partition may have no instant
  columns, which is fine. */
  if (dd_table_is_upgraded_instant(dd_tab->table())) {
    auto fn = [&](const dd::Properties &p, const char *s) {
      uint32_t n_inst_cols;
      ut_a(p.exists(s));
      p.get(s, &n_inst_cols);
      m_table->set_instant_cols(n_inst_cols);
      m_table->set_upgraded_instant();
      ut_ad(m_table->has_instant_cols());
    };

    if (!dd_table_is_partitioned(dd_tab->table())) {
      fn(table_se_private, dd_table_key_strings[DD_TABLE_INSTANT_COLS]);
    } else if (dd_part_has_instant_cols(
                   *reinterpret_cast<const dd::Partition *>(dd_tab))) {
      fn(dd_tab->se_private_data(),
         dd_partition_key_strings[DD_PARTITION_INSTANT_COLS]);
    }
  }

  /* Check if this table is FTS AUX table, if so, set DICT_TF2_AUX flag */
  fts_aux_table_t aux_table;
  if (fts_is_aux_table_name(&aux_table, norm_name, strlen(norm_name))) {
    DICT_TF2_FLAG_SET(m_table, DICT_TF2_AUX);
    m_table->parent_id = aux_table.parent_id;
  }

  if (is_discard) {
    m_table->ibd_file_missing = true;
    m_table->flags2 |= DICT_TF2_DISCARDED;
  }

  if (!is_redundant) {
    m_table->flags |= DICT_TF_COMPACT;
  }

  if (is_implicit) {
    m_table->flags2 |= DICT_TF2_USE_FILE_PER_TABLE;
  } else {
    m_table->flags |= (1 << DICT_TF_POS_SHARED_SPACE);
  }

  if (!blob_prefix) {
    m_table->flags |= (1 << DICT_TF_POS_ATOMIC_BLOBS);
  }

  if (zip_ssize != 0) {
    m_table->flags |= (zip_ssize << DICT_TF_POS_ZIP_SSIZE);
  }

  m_table->fts = nullptr;
  if (has_doc_id) {
    if (fulltext) {
      DICT_TF2_FLAG_SET(m_table, DICT_TF2_FTS);
    }

    if (add_doc_id) {
      DICT_TF2_FLAG_SET(m_table, DICT_TF2_FTS_HAS_DOC_ID);
    }

    if (fulltext || add_doc_id) {
      m_table->fts = fts_create(m_table);
      m_table->fts->cache = fts_cache_create(m_table);
    }
  }

  bool is_temp = (m_form->s->tmp_table_def != nullptr);
  if (is_temp) {
    m_table->flags2 |= DICT_TF2_TEMPORARY;
  }

  if (is_encrypted) {
    /* We don't support encrypt intrinsic and temporary table.  */
    ut_ad(!m_table->is_intrinsic() && !m_table->is_temporary());
    /* This flag will be used to set file-per-table tablespace
    encryption flag */
    DICT_TF2_FLAG_SET(m_table, DICT_TF2_ENCRYPTION_FILE_PER_TABLE);
  }

  mem_heap_t *heap = mem_heap_create(1000, UT_LOCATION_HERE);

  /* Fill out each column info */
  fill_dict_columns(dd_tab, m_form, m_table, n_mysql_cols, heap, add_doc_id);

#ifdef UNIV_DEBUG
  if (m_table->is_upgraded_instant()) {
    ut_ad(m_table->has_instant_cols());
  }
#endif

  if (m_table->has_instant_cols() || m_table->has_row_versions()) {
    dd_fill_instant_columns_default(dd_tab->table(), m_table);
  }

  mem_heap_free(heap);

  return m_table;
}

bool dd_create_tablespace(dd::cache::Dictionary_client *dd_client,
                          const char *dd_space_name, space_id_t space_id,
                          uint32_t flags, const char *filename, bool discarded,
                          dd::Object_id &dd_space_id) {
  /* Get the autoextend_size attribute for the tablespace */
  fil_space_t *space = fil_space_get(space_id);
  ut_ad(space != nullptr);

  std::unique_ptr<dd::Tablespace> dd_space(dd::create_object<dd::Tablespace>());

  if (dd_space_name != nullptr) {
    dd_space->set_name(dd_space_name);
  }

  if (dd_tablespace_get_mdl(dd_space->name().c_str())) {
    return true;
  }

  dd_space->set_engine(innobase_hton_name);
  dd::Properties &p = dd_space->se_private_data();
  p.set(dd_space_key_strings[DD_SPACE_ID], static_cast<uint32>(space_id));
  p.set(dd_space_key_strings[DD_SPACE_FLAGS], static_cast<uint32>(flags));
  p.set(dd_space_key_strings[DD_SPACE_SERVER_VERSION],
        DD_SPACE_CURRENT_SRV_VERSION);
  p.set(dd_space_key_strings[DD_SPACE_VERSION], DD_SPACE_CURRENT_SPACE_VERSION);

  dd_space_states state =
      (fsp_is_undo_tablespace(space_id)
           ? DD_SPACE_STATE_ACTIVE
           : (discarded ? DD_SPACE_STATE_DISCARDED : DD_SPACE_STATE_NORMAL));
  p.set(dd_space_key_strings[DD_SPACE_STATE], dd_space_state_values[state]);

  dd::Tablespace_file *dd_file = dd_space->add_file();
  dd_file->set_filename(filename);
  dd_file->se_private_data().set(dd_space_key_strings[DD_SPACE_ID],
                                 static_cast<uint32>(space_id));

  dd::Properties &toptions = dd_space->options();
  if (!FSP_FLAGS_GET_ENCRYPTION(flags)) {
    /* Update DD Option value, for Unencryption */
    toptions.set("encryption", "N");

  } else {
    /* Update DD Option value, for Encryption */
    toptions.set("encryption", "Y");
  }

  toptions.set(autoextend_size_str, space->autoextend_size_in_bytes);

  if (dd_client->store(dd_space.get())) {
    return true;
  }

  dd_space_id = dd_space.get()->id();

  return false;
}

bool dd_create_implicit_tablespace(dd::cache::Dictionary_client *dd_client,
                                   space_id_t space_id, const char *space_name,
                                   const char *filename, bool discarded,
                                   dd::Object_id &dd_space_id) {
  fil_space_t *space = fil_space_get(space_id);
  uint32_t flags = space->flags;

  std::string tsn(space_name);
  dict_name::convert_to_space(tsn);

  bool fail = dd_create_tablespace(dd_client, tsn.c_str(), space_id, flags,
                                   filename, discarded, dd_space_id);

  return fail;
}

bool dd_drop_tablespace(dd::cache::Dictionary_client *dd_client,
                        dd::Object_id dd_space_id) {
  std::unique_ptr<dd::Tablespace> dd_space;

  if (dd_client->acquire_uncached_uncommitted(dd_space_id, &dd_space) ||
      dd_space == nullptr) {
    my_error(ER_INTERNAL_ERROR, MYF(0),
             " InnoDB can't get tablespace object"
             " for space ",
             dd_space_id);

    return true;
  }

  ut_a(dd_space != nullptr);

  if (dd_tablespace_get_mdl(dd_space->name().c_str())) {
    my_error(ER_INTERNAL_ERROR, MYF(0),
             " InnoDB can't set exclusive MDL on"
             " tablespace ",
             dd_space->name().c_str());

    return true;
  }

  bool error = dd_client->drop(dd_space.get());
  DBUG_EXECUTE_IF("fail_while_dropping_dd_object", error = true;);

  if (error) {
    my_error(ER_INTERNAL_ERROR, MYF(0), " InnoDB can't drop tablespace object",
             dd_space->name().c_str());
  }

  return error;
}

/** Determine if a tablespace is implicit.
@param[in]      dd_space        DD space object
@param[out]     implicit        whether the tablespace is implicit tablespace
@retval false   on success
@retval true    on failure (corrupt tablespace object). */
bool dd_tablespace_is_implicit(const dd::Tablespace *dd_space, bool *implicit) {
  space_id_t id = 0;
  uint32_t flags;

  if (dd_space->se_private_data().get(dd_space_key_strings[DD_SPACE_ID], &id)) {
    return true;
  }

  dd_space->se_private_data().get(dd_space_key_strings[DD_SPACE_FLAGS], &flags);
  *implicit = fsp_is_file_per_table(id, flags);

  return false;
}

bool dd_get_tablespace_size_option(dd::cache::Dictionary_client *dd_client,
                                   const dd::Object_id dd_space_id,
                                   uint64_t *autoextend_size) {
  /* Get the tablespace object. */
  dd::Tablespace *dd_space = nullptr;

  if (dd_client->acquire_uncached_uncommitted<dd::Tablespace>(dd_space_id,
                                                              &dd_space)) {
    /* purecov: begin inspected */
    my_error(ER_INTERNAL_ERROR, MYF(0),
             " InnoDB: Can't get tablespace object for space ", dd_space_id);
    return true;
    /* purecov: end */
  }

  ut_a(dd_space != nullptr);

  const dd::Properties &p = dd_space->options();

  if (p.exists(autoextend_size_str)) {
    p.get(autoextend_size_str, autoextend_size);
  } else {
    *autoextend_size = 0;
  }

  return false;
}

bool dd_implicit_alter_tablespace(dd::cache::Dictionary_client *dd_client,
                                  dd::Object_id dd_space_id,
                                  HA_CREATE_INFO *create_info) {
  ut_a(create_info->m_implicit_tablespace_autoextend_size_change);

  dd::Tablespace *dd_space = nullptr;
  bool is_implicit{};

  if (dd_client->acquire_uncached_uncommitted<dd::Tablespace>(dd_space_id,
                                                              &dd_space) ||
      dd_space == nullptr ||
      dd_tablespace_is_implicit(dd_space, &is_implicit) || !is_implicit) {
    /* purecov: begin inspected */
    my_error(ER_INTERNAL_ERROR, MYF(0),
             " InnoDB: Can't get tablespace object for space ", dd_space_id);
    return true;
    /* purecov: end */
  }

  ut_a(dd_space != nullptr);

  if (dd_tablespace_get_mdl(dd_space->name().c_str())) {
    /* purecov: begin inspected */
    my_error(ER_INTERNAL_ERROR, MYF(0),
             " InnoDB can't set exclusive MDL on"
             " tablespace ",
             dd_space->name().c_str());
    return true;
    /* purecov: end */
  }

  /* Get the space id from the tablespace properties. */
  const dd::Properties &pd = dd_space->se_private_data();
  uint32_t id;
  pd.get(dd_space_key_strings[DD_SPACE_ID], &id);

  /* Get the tablespace options. */
  dd::Properties &p = dd_space->options();

  /* Find out if the tablespace is discarded. */
  bool is_discarded = dd_tablespace_is_discarded(dd_space);

  ut_ad(fil_space_get(id) != nullptr || is_discarded);

  if (create_info->m_implicit_tablespace_autoextend_size_change &&
      create_info->m_implicit_tablespace_autoextend_size > 0 &&
      validate_autoextend_size_value(
          create_info->m_implicit_tablespace_autoextend_size) != DB_SUCCESS) {
    return true;
  }

  /* Set the autoextend_size attribute if changed. */
  if (create_info->m_implicit_tablespace_autoextend_size_change) {
    p.set(autoextend_size_str,
          create_info->m_implicit_tablespace_autoextend_size);
  }

  if (dd_client->update(dd_space)) {
    return true;
  }

  /* Set the autoextend_size value in the cached space object. */

  /* Space could be invalid in case of a discarded tablespaces. The
  autoextend_size attribute will be set in the fil_space_t when it
  is re-initialized during import. */
  if (!is_discarded) {
    if (create_info->m_implicit_tablespace_autoextend_size_change) {
      fil_set_autoextend_size(
          id, create_info->m_implicit_tablespace_autoextend_size);
    }
  }

  return false;
}

bool dd_set_tablespace_compression(dd::cache::Dictionary_client *client,
                                   const char *algorithm,
                                   dd::Object_id dd_space_id) {
  dd::Tablespace *dd_space{};
  auto fail = client->acquire_uncached<dd::Tablespace>(dd_space_id, &dd_space);

  if (fail || dd_space == nullptr) {
    return true;
  }

  space_id_t space_id = 0;

  dd_space->se_private_data().get(dd_space_key_strings[DD_SPACE_ID], &space_id);

  auto err = fil_set_compression(space_id, algorithm);

  return err != DB_SUCCESS;
}

/** Load foreign key constraint info for the dd::Table object.
@param[out]     m_table         InnoDB table handle
@param[in]      dd_table        Global DD table
@param[in]      col_names       column names, or NULL
@param[in]      ignore_err      DICT_ERR_IGNORE_FK_NOKEY or DICT_ERR_IGNORE_NONE
@param[in]      dict_locked     True if dict_sys->mutex is already held,
                                otherwise false
@return DB_SUCCESS      if successfully load FK constraint */
dberr_t dd_table_load_fk_from_dd(dict_table_t *m_table,
                                 const dd::Table *dd_table,
                                 const char **col_names,
                                 dict_err_ignore_t ignore_err,
                                 bool dict_locked) {
  dberr_t err = DB_SUCCESS;

  /* Now fill in the foreign key info */
  for (const dd::Foreign_key *key : dd_table->foreign_keys()) {
    char buf[MAX_FULL_NAME_LEN + 1];

    if (*(key->name().c_str()) == '#' && *(key->name().c_str() + 1) == 'f') {
      continue;
    }

    dd::String_type db_name = key->referenced_table_schema_name();
    dd::String_type tb_name = key->referenced_table_name();

    bool truncated;
    build_table_filename(buf, sizeof(buf), db_name.c_str(), tb_name.c_str(),
                         nullptr, 0, &truncated);

    char norm_name[FN_REFLEN * 2];

    if (truncated || !normalize_table_name(norm_name, buf)) {
      /* purecov: begin inspected */
      ut_d(ut_error);
      ut_o(return DB_TOO_LONG_PATH);
      /* purecov: end */
    }

    dict_foreign_t *foreign = dict_mem_foreign_create();
    foreign->foreign_table_name =
        mem_heap_strdup(foreign->heap, m_table->name.m_name);

    dict_mem_foreign_table_name_lookup_set(foreign, true);

    if (innobase_get_lower_case_table_names() == 2) {
      innobase_casedn_str(norm_name);
    } else {
#ifndef _WIN32
      if (innobase_get_lower_case_table_names() == 1) {
        innobase_casedn_str(norm_name);
      }
#endif /* !_WIN32 */
    }

    foreign->referenced_table_name = mem_heap_strdup(foreign->heap, norm_name);
    dict_mem_referenced_table_name_lookup_set(foreign, true);
    ulint db_len = dict_get_db_name_len(m_table->name.m_name);

    ut_ad(db_len > 0);

    memcpy(buf, m_table->name.m_name, db_len);

    buf[db_len] = '\0';

    snprintf(norm_name, sizeof norm_name, "%s/%s", buf, key->name().c_str());

    foreign->id = mem_heap_strdup(foreign->heap, norm_name);

    switch (key->update_rule()) {
      case dd::Foreign_key::RULE_NO_ACTION:
        /*
          Since SET DEFAULT clause is not supported, ignore it by converting
          into the value DICT_FOREIGN_ON_UPDATE_NO_ACTION
        */
      case dd::Foreign_key::RULE_SET_DEFAULT:
        foreign->type = DICT_FOREIGN_ON_UPDATE_NO_ACTION;
        break;
      case dd::Foreign_key::RULE_RESTRICT:
        foreign->type = 0;
        break;
      case dd::Foreign_key::RULE_CASCADE:
        foreign->type = DICT_FOREIGN_ON_UPDATE_CASCADE;
        break;
      case dd::Foreign_key::RULE_SET_NULL:
        foreign->type = DICT_FOREIGN_ON_UPDATE_SET_NULL;
        break;
      default:
        ut_d(ut_error);
    }

    switch (key->delete_rule()) {
      case dd::Foreign_key::RULE_NO_ACTION:
        /*
          Since SET DEFAULT clause is not supported, ignore it by converting
          into the value DICT_FOREIGN_ON_UPDATE_NO_ACTION
        */
      case dd::Foreign_key::RULE_SET_DEFAULT:
        foreign->type |= DICT_FOREIGN_ON_DELETE_NO_ACTION;
      case dd::Foreign_key::RULE_RESTRICT:
        break;
      case dd::Foreign_key::RULE_CASCADE:
        foreign->type |= DICT_FOREIGN_ON_DELETE_CASCADE;
        break;
      case dd::Foreign_key::RULE_SET_NULL:
        foreign->type |= DICT_FOREIGN_ON_DELETE_SET_NULL;
        break;
      default:
        ut_d(ut_error);
    }

    foreign->n_fields = key->elements().size();

    foreign->foreign_col_names = static_cast<const char **>(
        mem_heap_alloc(foreign->heap, foreign->n_fields * sizeof(void *)));

    foreign->referenced_col_names = static_cast<const char **>(
        mem_heap_alloc(foreign->heap, foreign->n_fields * sizeof(void *)));

    ulint num_ref = 0;

    for (const dd::Foreign_key_element *key_e : key->elements()) {
      dd::String_type ref_col_name = key_e->referenced_column_name();

      foreign->referenced_col_names[num_ref] =
          mem_heap_strdup(foreign->heap, ref_col_name.c_str());
      ut_ad(ref_col_name.c_str());

      const dd::Column *f_col = &key_e->column();
      foreign->foreign_col_names[num_ref] =
          mem_heap_strdup(foreign->heap, f_col->name().c_str());
      num_ref++;
    }

    if (!dict_locked) {
      dict_sys_mutex_enter();
    }
#ifdef UNIV_DEBUG
    dict_table_t *for_table;

    for_table =
        dict_table_check_if_in_cache_low(foreign->foreign_table_name_lookup);

    ut_ad(for_table);
#endif
    /* Fill in foreign->foreign_table and index, then add to
    dict_table_t */
    err =
        dict_foreign_add_to_cache(foreign, col_names, false, true, ignore_err);
    if (!dict_locked) {
      dict_sys_mutex_exit();
    }

    if (err != DB_SUCCESS) {
      break;
    }

    /* Set up the FK virtual column info */
    dict_mem_table_free_foreign_vcol_set(m_table);
    dict_mem_table_fill_foreign_vcol_set(m_table);
  }
  return err;
}

/** Load foreign key constraint for the table. Note, it could also open
the foreign table, if this table is referenced by the foreign table
@param[in,out]  client          data dictionary client
@param[in]      tbl_name        Table Name
@param[in]      col_names       column names, or NULL
@param[out]     m_table         InnoDB table handle
@param[in]      dd_table        Global DD table
@param[in]      thd             thread THD
@param[in]      dict_locked     True if dict_sys->mutex is already held,
                                otherwise false
@param[in]      check_charsets  whether to check charset compatibility
@param[in,out]  fk_tables       name list for tables that refer to this table
@return DB_SUCCESS      if successfully load FK constraint */
dberr_t dd_table_load_fk(dd::cache::Dictionary_client *client,
                         const char *tbl_name, const char **col_names,
                         dict_table_t *m_table, const dd::Table *dd_table,
                         THD *thd, bool dict_locked, bool check_charsets,
                         dict_names_t *fk_tables) {
  dberr_t err = DB_SUCCESS;
  dict_err_ignore_t ignore_err = DICT_ERR_IGNORE_NONE;

  /* Check whether FOREIGN_KEY_CHECKS is set to 0. If so, the table
  can be opened even if some FK indexes are missing. If not, the table
  can't be opened in the same situation */
  if (thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS)) {
    ignore_err = DICT_ERR_IGNORE_FK_NOKEY;
  }

  err = dd_table_load_fk_from_dd(m_table, dd_table, col_names, ignore_err,
                                 dict_locked);

  if (err != DB_SUCCESS) {
    return err;
  }

  if (dict_locked) {
    dict_sys_mutex_exit();
  }

  DBUG_EXECUTE_IF("enable_stack_overrun_post_alter_commit",
                  { DBUG_SET("+d,simulate_stack_overrun"); });
  err = dd_table_check_for_child(client, tbl_name, col_names, m_table,
                                 check_charsets, ignore_err, fk_tables);
  DBUG_EXECUTE_IF("enable_stack_overrun_post_alter_commit",
                  { DBUG_SET("-d,simulate_stack_overrun"); });

  if (dict_locked) {
    dict_sys_mutex_enter();
  }

  return err;
}

dberr_t dd_table_check_for_child(dd::cache::Dictionary_client *client,
                                 const char *tbl_name, const char **col_names,
                                 dict_table_t *m_table, bool check_charsets,
                                 dict_err_ignore_t ignore_err,
                                 dict_names_t *fk_tables) {
  dberr_t err = DB_SUCCESS;

  /* TODO: NewDD: Temporary ignore DD system table until
  WL#6049 inplace */
  if (!dict_sys_t::is_dd_table_id(m_table->id) && fk_tables != nullptr) {
    std::vector<dd::String_type> child_schema;
    std::vector<dd::String_type> child_name;
    std::string db_str;
    std::string tbl_str;

    dict_name::get_table(m_table->name.m_name, db_str, tbl_str);

    if (client->fetch_fk_children_uncached(db_str.c_str(), tbl_str.c_str(),
                                           "InnoDB", false, &child_schema,
                                           &child_name)) {
      return DB_ERROR;
    }

    std::vector<dd::String_type>::iterator it = child_name.begin();
    for (auto &db_name : child_schema) {
      dd::String_type tb_name = *it;

      char buf[2 * NAME_CHAR_LEN * 5 + 2 + 1];
      bool truncated;
      build_table_filename(buf, sizeof(buf), db_name.c_str(), tb_name.c_str(),
                           nullptr, 0, &truncated);

      char full_name[FN_REFLEN];

      if (truncated || !normalize_table_name(full_name, buf)) {
        /* purecov: begin inspected */
        ut_d(ut_error);
        ut_o(return DB_TOO_LONG_PATH);
        /* purecov: end */
      }

      if (innobase_get_lower_case_table_names() == 2) {
        innobase_casedn_str(full_name);
      } else {
#ifndef _WIN32
        if (innobase_get_lower_case_table_names() == 1) {
          innobase_casedn_str(full_name);
        }
#endif /* !_WIN32 */
      }

      dict_sys_mutex_enter();

      /* Load the foreign table first */
      dict_table_t *foreign_table =
          dd_table_open_on_name_in_mem(full_name, true);

      if (foreign_table) {
        for (auto &fk : foreign_table->foreign_set) {
          if (strcmp(fk->referenced_table_name, tbl_name) != 0) {
            continue;
          }

          if (fk->referenced_table) {
            ut_ad(fk->referenced_table == m_table);
          } else {
            err = dict_foreign_add_to_cache(fk, col_names, check_charsets,
                                            false, ignore_err);
            if (err != DB_SUCCESS) {
              foreign_table->release();
              dict_sys_mutex_exit();
              return err;
            }
          }
        }
        foreign_table->release();
      } else {
        /* To avoid recursively loading the tables
        related through the foreign key constraints,
        the child table name is saved here. The child
        table will be loaded later, along with its
        foreign key constraint. */
        lint old_size = mem_heap_get_size(m_table->heap);

        fk_tables->push_back(
            mem_heap_strdupl(m_table->heap, full_name, strlen(full_name)));

        lint new_size = mem_heap_get_size(m_table->heap);
        dict_sys->size += new_size - old_size;
      }

      dict_sys_mutex_exit();

      ut_ad(it != child_name.end());
      ++it;
    }
  }

  return err;
}

/** Get tablespace name of dd::Table
@tparam         Table           dd::Table or dd::Partition
@param[in]      dd_table        dd table object
@return the tablespace name or nullptr if failed */
template <typename Table>
const char *dd_table_get_space_name(const Table *dd_table) {
  dd::Tablespace *dd_space = nullptr;
  THD *thd = current_thd;
  const char *space_name;

  DBUG_TRACE;
  ut_ad(srv_shutdown_state.load() < SRV_SHUTDOWN_DD);

  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  dd::Object_id dd_space_id = (*dd_table->indexes().begin())->tablespace_id();

  if (client->acquire_uncached_uncommitted<dd::Tablespace>(dd_space_id,
                                                           &dd_space) ||
      dd_space == nullptr) {
    ut_d(ut_error);
    ut_o(return nullptr);
  }

  space_name = dd_space->name().c_str();

  return space_name;
}

/** Get the first filepath from mysql.tablespace_datafiles
for a given space_id.
@tparam         Table           dd::Table or dd::Partition
@param[in,out]  heap            heap for store file name.
@param[in]      table           dict table
@param[in]      dd_table        dd table obj
@return First filepath (caller must invoke ut::free() on it)
@retval nullptr if no mysql.tablespace_datafiles entry was found. */
template <typename Table>
char *dd_get_first_path(mem_heap_t *heap, dict_table_t *table,
                        Table *dd_table) {
  char *filepath = nullptr;
  dd::Tablespace *dd_space = nullptr;
  THD *thd = current_thd;
  MDL_ticket *mdl = nullptr;
  dd::Object_id dd_space_id;

  ut_ad(srv_shutdown_state.load() < SRV_SHUTDOWN_DD);
  ut_ad(!dict_sys_mutex_own());

  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  if (dd_table == nullptr) {
    std::string db_str;
    std::string tbl_str;
    dict_name::get_table(table->name.m_name, db_str, tbl_str);

    if (db_str.empty() || tbl_str.empty() ||
        dd_mdl_acquire(thd, &mdl, db_str.c_str(), tbl_str.c_str())) {
      return nullptr;
    }

    const dd::Table *table_def = nullptr;
    if (client->acquire(db_str.c_str(), tbl_str.c_str(), &table_def) ||
        table_def == nullptr) {
      dd_mdl_release(thd, &mdl);
      return nullptr;
    }

    dd_space_id = dd_first_index(table_def)->tablespace_id();

    dd_mdl_release(thd, &mdl);
  } else {
    dd_space_id = dd_first_index(dd_table)->tablespace_id();
  }

  if (client->acquire_uncached_uncommitted<dd::Tablespace>(dd_space_id,
                                                           &dd_space)) {
    ut_d(ut_error);
    ut_o(return nullptr);
  }

  if (dd_space != nullptr) {
    dd::Tablespace_file *dd_file =
        const_cast<dd::Tablespace_file *>(*(dd_space->files().begin()));

    filepath = mem_heap_strdup(heap, dd_file->filename().c_str());

    return filepath;
  }

  return nullptr;
}

template <typename Table>
void dd_get_and_save_data_dir_path(dict_table_t *table, const Table *dd_table,
                                   bool dict_mutex_own) {
  mem_heap_t *heap = nullptr;

  if (!DICT_TF_HAS_DATA_DIR(table->flags) || table->data_dir_path != nullptr) {
    return;
  }

  char *path = fil_space_get_first_path(table->space);

  if (path == nullptr) {
    heap = mem_heap_create(100, UT_LOCATION_HERE);
    if (dict_mutex_own) {
      dict_mutex_exit_for_mysql();
    }
    path = dd_get_first_path(heap, table, dd_table);
    if (dict_mutex_own) {
      dict_mutex_enter_for_mysql();
    }
  }

  if (!dict_mutex_own) {
    dict_mutex_enter_for_mysql();
  }

  if (path != nullptr) {
    dict_save_data_dir_path(table, path);
  }

  if (!dict_mutex_own) {
    dict_mutex_exit_for_mysql();
  }

  if (heap != nullptr) {
    mem_heap_free(heap);
  } else {
    ut::free(path);
  }
}

template void dd_get_and_save_data_dir_path<dd::Table>(dict_table_t *,
                                                       const dd::Table *, bool);

template void dd_get_and_save_data_dir_path<dd::Partition>(
    dict_table_t *, const dd::Partition *, bool);

/** Get the meta-data filename from the table name for a
single-table tablespace.
@param[in,out]  table           table object
@param[in]      dd_table        DD table object
@param[out]     filename        filename
@param[in]      max_len         filename max length */
void dd_get_meta_data_filename(dict_table_t *table, dd::Table *dd_table,
                               char *filename, ulint max_len) {
  /* Make sure the data_dir_path is set. */
  dd_get_and_save_data_dir_path(table, dd_table, false);

  std::string path = dict_table_get_datadir(table);

  auto filepath = Fil_path::make(path, table->name.m_name, CFG, true);

  ut_a(max_len >= strlen(filepath) + 1);

  strcpy(filename, filepath);

  ut::free(filepath);
}

/** Opens a tablespace for dd_load_table_one()
@tparam         Table                   dd::Table or dd::Partition
@param[in,out]  dd_table                dd table
@param[in,out]  table                   A table that refers to the tablespace to
                                        open
@param[in,out]  heap                    A memory heap
@param[in]      expected_fsp_flags      expected flags of tablespace to be
                                        loaded
@param[in]      ignore_err              Whether to ignore an error. */
template <typename Table>
void dd_load_tablespace(const Table *dd_table, dict_table_t *table,
                        mem_heap_t *heap, dict_err_ignore_t ignore_err,
                        uint32_t expected_fsp_flags) {
  ut_ad(!table->is_temporary());
  ut_ad(dict_sys_mutex_own());

  /* The system and temporary tablespaces are preloaded and
  always available. */
  if (fsp_is_system_or_temp_tablespace(table->space)) {
    return;
  }

  if (dict_table_is_discarded(table)) {
    /* If doing an IMPORT, don't report this warning. This is expected */
    if (thd_tablespace_op(current_thd) != Alter_info::ALTER_IMPORT_TABLESPACE) {
      ib::warn(ER_IB_MSG_171)
          << "Tablespace for table " << table->name << " is set as discarded.";
    }

    table->ibd_file_missing = true;
    return;
  }

  /* A general tablespace name is not the same as the table name.
  Use the general tablespace name if it can be read from the
  dictionary, if not use 'innodb_general_##. */
  char *shared_space_name = nullptr;
  const char *space_name;
  std::string tablespace_name;
  const char *tbl_name;

  if (DICT_TF_HAS_SHARED_SPACE(table->flags)) {
    if (table->space == dict_sys_t::s_dict_space_id) {
      shared_space_name = mem_strdup(dict_sys_t::s_dd_space_name);
    } else if (srv_sys_tablespaces_open) {
      /* For avoiding deadlock, we need to exit
      dict_sys->mutex. */
      dict_sys_mutex_exit();
      shared_space_name = mem_strdup(dd_table_get_space_name(dd_table));
      dict_sys_mutex_enter();
    } else {
      /* Make the temporary tablespace name. */
      shared_space_name = static_cast<char *>(ut::malloc_withkey(
          UT_NEW_THIS_FILE_PSI_KEY, strlen(general_space_name) + 20));

      sprintf(shared_space_name, "%s_" ULINTPF, general_space_name,
              static_cast<ulint>(table->space));
    }

    space_name = shared_space_name;
    tbl_name = space_name;
  } else {
    tbl_name = table->name.m_name;

    tablespace_name.assign(tbl_name);
    dict_name::convert_to_space(tablespace_name);
    space_name = tablespace_name.c_str();
  }

  auto is_already_opened = [&]() {
    if (fil_space_exists_in_mem(table->space, space_name, false, true)) {
      dd_get_and_save_data_dir_path(table, dd_table, true);
      ut::free(shared_space_name);
      return true;
    }
    return false;
  };

  /* The tablespace may already be open. */
  if (is_already_opened()) return;

  if (!(ignore_err & DICT_ERR_IGNORE_RECOVER_LOCK)) {
    ib::error(ER_IB_MSG_172)
        << "Failed to find tablespace for table " << table->name
        << " in the cache. Attempting"
           " to load the tablespace with space id "
        << table->space;
  }

  /* Try to get the filepath if this space_id is already open.
  If the filepath is not found, fil_ibd_open() will make a default
  filepath from the tablespace name */
  char *filepath = fil_space_get_first_path(table->space);

  if (filepath != nullptr) {
    /* If space id is already open with a different space name, then skip
    loading the space. It can happen because DDL log recovery might not
    have happened yet. */
    table->ibd_file_missing = true;

    ut::free(shared_space_name);
    ut::free(filepath);
    return;
  }

  ut_ad(filepath == nullptr);

  /* If the space is not open yet, then try to open by dd path. If the file
  path is changed then boot_tablespaces() would always open the tablespace. */
  dict_sys_mutex_exit();
  filepath = dd_get_first_path(heap, table, dd_table);
  DEBUG_SYNC_C("innodb_dd_load_tablespace_no_dict_mutex");
  dict_sys_mutex_enter();

  if (filepath == nullptr) {
    ib::warn(ER_IB_MSG_173) << "Could not find the filepath"
                            << " for table " << table->name << ", space ID "
                            << table->space << " in the data dictionary.";
  }

  /* The tablespace may have been opened while we released the dict_sys mutex.
  We need to check again if it was not opened in meantime, as fil_ibd_open
  will try to close it forcefully, even if some IO is underway, leading to
  crash. */
  if (is_already_opened()) return;

  /* Try to open the tablespace.  We set the 2nd param (fix_dict) to
  false because we do not have an x-lock on dict_operation_lock */
  dberr_t err =
      fil_ibd_open(true, FIL_TYPE_TABLESPACE, table->space, expected_fsp_flags,
                   space_name, filepath, true, false);

  if (err == DB_SUCCESS) {
    /* This will set the DATA DIRECTORY for SHOW CREATE TABLE. */
    dd_get_and_save_data_dir_path(table, dd_table, true);
  } else {
    /* We failed to find a sensible tablespace file */
    table->ibd_file_missing = true;
  }

  ut::free(shared_space_name);
}

/** Get the space name from mysql.tablespaces for a given space_id.
@tparam         Table           dd::Table or dd::Partition
@param[in,out]  heap            heap for store file name.
@param[in]      table           dict table
@param[in]      dd_table        dd table obj
@return First filepath (caller must invoke ut::free() on it)
@retval nullptr if no mysql.tablespace_datafiles entry was found. */
template <typename Table>
char *dd_space_get_name(mem_heap_t *heap, dict_table_t *table,
                        Table *dd_table) {
  dd::Object_id dd_space_id;
  THD *thd = current_thd;
  dd::Tablespace *dd_space = nullptr;

  ut_ad(srv_shutdown_state.load() < SRV_SHUTDOWN_DD);
  ut_ad(!dict_sys_mutex_own());

  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  if (dd_table == nullptr) {
    std::string db_str;
    std::string tbl_str;
    dict_name::get_table(table->name.m_name, db_str, tbl_str);

    MDL_ticket *mdl = nullptr;
    if (db_str.empty() || tbl_str.empty() ||
        dd_mdl_acquire(thd, &mdl, db_str.c_str(), tbl_str.c_str())) {
      return nullptr;
    }

    const dd::Table *table_def = nullptr;
    if (client->acquire(db_str.c_str(), tbl_str.c_str(), &table_def) ||
        table_def == nullptr) {
      dd_mdl_release(thd, &mdl);
      return nullptr;
    }

    dd_space_id = dd_first_index(table_def)->tablespace_id();

    dd_mdl_release(thd, &mdl);
  } else {
    dd_space_id = dd_first_index(dd_table)->tablespace_id();
  }

  if (client->acquire_uncached_uncommitted<dd::Tablespace>(dd_space_id,
                                                           &dd_space) ||
      dd_space == nullptr) {
    ut_d(ut_error);
    ut_o(return nullptr);
  }

  return mem_heap_strdup(heap, dd_space->name().c_str());
}

/** Make sure the tablespace name is saved in dict_table_t if the table
uses a general tablespace.
Try to read it from the fil_system_t first, then from DD.
@param[in]      table           Table object
@param[in]      dd_table        Global DD table or partition object
@param[in]      dict_mutex_own  true if dict_sys->mutex is owned already */
template <typename Table>
void dd_get_and_save_space_name(dict_table_t *table, const Table *dd_table,
                                bool dict_mutex_own) {
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

  /* Read it from the dictionary. */
  if (srv_sys_tablespaces_open) {
    if (dict_mutex_own) {
      dict_mutex_exit_for_mysql();
    }

    table->tablespace = dd_space_get_name(table->heap, table, dd_table);

    if (dict_mutex_own) {
      dict_mutex_enter_for_mysql();
    }
  }
}

template void dd_get_and_save_space_name<dd::Table>(dict_table_t *,
                                                    const dd::Table *, bool);

template void dd_get_and_save_space_name<dd::Partition>(dict_table_t *,
                                                        const dd::Partition *,
                                                        bool);

/** Open or load a table definition based on a Global DD object.
@tparam         Table           dd::Table or dd::Partition
@param[in,out]  client          data dictionary client
@param[in]      table           MySQL table definition
@param[in]      norm_name       Table Name
@param[in]      dd_table        Global DD table or partition object
@param[in]      thd             thread THD
@param[in,out]  fk_list         stack of table names which need to load
@return ptr to dict_table_t filled, otherwise, nullptr */
template <typename Table>
dict_table_t *dd_open_table_one(dd::cache::Dictionary_client *client,
                                const TABLE *table, const char *norm_name,
                                const Table *dd_table, THD *thd,
                                dict_names_t &fk_list) {
  ut_ad(dd_table != nullptr);

  bool implicit;
  std::unique_ptr<dd::Tablespace> dd_space;

  if (dd_table->tablespace_id() == dict_sys_t::s_dd_dict_space_id) {
    /* DD tables are in shared DD tablespace */
    implicit = false;
  } else {
    if (client->acquire_uncached_uncommitted<dd::Tablespace>(
            dd_first_index(dd_table)->tablespace_id(), &dd_space) ||
        dd_space == nullptr) {
      /* Tablespace no longer exist, it could be already dropped */
      return nullptr;
    }

    if (dd_tablespace_is_implicit(dd_space.get(), &implicit)) {
      /* Corrupt tablespace info. */
      return nullptr;
    }
  }

  const bool zip_allowed = srv_page_size <= UNIV_ZIP_SIZE_MAX;
  const bool strict = false;
  bool first_index = true;

  /* Create dict_table_t for the table */
  dict_table_t *m_table = dd_fill_dict_table(
      dd_table, table, norm_name, nullptr, zip_allowed, strict, thd, implicit);

  if (m_table == nullptr) {
    return nullptr;
  }

  /* Create dict_index_t for the table */
  int ret;
  ret = dd_fill_dict_index(dd_table->table(), table, m_table, thd);

  if (ret != 0) {
    return nullptr;
  }

  if (dd_space && !implicit) {
    const char *name = dd_space->name().c_str();
    if (name) {
      m_table->tablespace = mem_heap_strdupl(m_table->heap, name, strlen(name));
    }
  }

  if (Field **autoinc_col = table->s->found_next_number_field) {
    const dd::Properties &p = dd_table->table().se_private_data();
    dict_table_autoinc_set_col_pos(m_table, (*autoinc_col)->field_index());
    uint64_t version, autoinc = 0;
    if (p.get(dd_table_key_strings[DD_TABLE_VERSION], &version) ||
        p.get(dd_table_key_strings[DD_TABLE_AUTOINC], &autoinc)) {
      ut_ad(!"problem setting AUTO_INCREMENT");
      return nullptr;
    }

    m_table->version = version;
    dict_table_autoinc_lock(m_table);
    dict_table_autoinc_initialize(m_table, autoinc + 1);
    dict_table_autoinc_unlock(m_table);
    m_table->autoinc_persisted = autoinc;
  }

  mem_heap_t *heap = mem_heap_create(100, UT_LOCATION_HERE);
  bool fail = false;

  /* Now fill the space ID and Root page number for each index */
  dict_index_t *index = m_table->first_index();
  for (const auto dd_index : dd_table->indexes()) {
    ut_ad(index != nullptr);

    const dd::Properties &se_private_data = dd_index->se_private_data();
    uint64_t id = 0;
    uint32_t root = 0;
    uint32_t sid = 0;
    uint64_t trx_id = 0;
    dd::Object_id index_space_id = dd_index->tablespace_id();

    if (dd_table->tablespace_id() == dict_sys_t::s_dd_dict_space_id) {
      sid = dict_sys_t::s_dict_space_id;
    } else if (dd_table->tablespace_id() == dict_sys_t::s_dd_temp_space_id) {
      sid = dict_sys_t::s_temp_space_id;
    } else {
      std::unique_ptr<dd::Tablespace> index_space;
      if (client->acquire_uncached_uncommitted<dd::Tablespace>(index_space_id,
                                                               &index_space) ||
          index_space == nullptr) {
        my_error(ER_TABLESPACE_MISSING, MYF(0), m_table->name.m_name);
        fail = true;
        break;
      }

      if (index_space->se_private_data().get(dd_space_key_strings[DD_SPACE_ID],
                                             &sid)) {
        fail = true;
        break;
      }
    }

    if (first_index) {
      ut_ad(m_table->space == 0);
      m_table->space = sid;
      ut_ad(dd_table->tablespace_id() == dd::INVALID_OBJECT_ID ||
            dd_table->tablespace_id() == index_space_id);
      m_table->dd_space_id = index_space_id;

      uint32_t dd_fsp_flags;
      if (dd_table->tablespace_id() == dict_sys_t::s_dd_dict_space_id) {
        dd_fsp_flags = dict_tf_to_fsp_flags(m_table->flags);
      } else {
        ut_ad(dd_space != nullptr);
        dd_space->se_private_data().get(dd_space_key_strings[DD_SPACE_FLAGS],
                                        &dd_fsp_flags);
      }

      /* Make sure the data_dir_path is set in the dict_table_t. */
      dd_get_and_save_data_dir_path(m_table, dd_table, false);

      dict_sys_mutex_enter();
      dd_load_tablespace(dd_table, m_table, heap, DICT_ERR_IGNORE_RECOVER_LOCK,
                         dd_fsp_flags);

      DEBUG_SYNC_C("innodb_dd_load_tablespace_done");

      if (dd_space && m_table->space != TRX_SYS_SPACE &&
          fil_space_get(m_table->space) != nullptr) {
        /* Get the autoextend_size property from the tablespace
        and set the fil_space_t::autoextend_size attribute. */
        const dd::Properties &o = dd_space->options();
        uint64_t autoextend_size{};

        if (o.exists(autoextend_size_str)) {
          o.get(autoextend_size_str, &autoextend_size);
        }

        ut_d(dberr_t ret =)
            fil_set_autoextend_size(m_table->space, autoextend_size);
        ut_ad(ret == DB_SUCCESS);
      }

      dict_sys_mutex_exit();
      first_index = false;
    }

    if (se_private_data.get(dd_index_key_strings[DD_INDEX_ID], &id) ||
        se_private_data.get(dd_index_key_strings[DD_INDEX_ROOT], &root) ||
        se_private_data.get(dd_index_key_strings[DD_INDEX_TRX_ID], &trx_id)) {
      fail = true;
      break;
    }

    ut_ad(root > 1);
    ut_ad(index->type & DICT_FTS || root != FIL_NULL ||
          dict_table_is_discarded(m_table));
    ut_ad(id != 0);
    index->page = root;
    index->space = sid;
    index->id = id;
    index->trx_id = trx_id;

    /** Look up the spatial reference system in the
    dictionary. Since this may cause a table open to read the
    dictionary tables, it must be done while not holding
    &dict_sys->mutex. */
    if (dict_index_is_spatial(index))
      index->rtr_srs.reset(fetch_srs(index->srid));

    index = index->next();
  }

  if (!implicit) {
    dd_get_and_save_space_name(m_table, dd_table, false);
  }

  dict_sys_mutex_enter();

  if (fail) {
    for (dict_index_t *index = UT_LIST_GET_LAST(m_table->indexes);
         index != nullptr; index = UT_LIST_GET_LAST(m_table->indexes)) {
      dict_index_remove_from_cache(m_table, index);
    }
    dict_mem_table_free(m_table);
    dict_sys_mutex_exit();
    mem_heap_free(heap);

    return nullptr;
  }

  /* Re-check if the table has been opened/added by a concurrent
  thread */
  dict_table_t *exist = dict_table_check_if_in_cache_low(norm_name);
  if (exist != nullptr) {
    for (dict_index_t *index = UT_LIST_GET_LAST(m_table->indexes);
         index != nullptr; index = UT_LIST_GET_LAST(m_table->indexes)) {
      dict_index_remove_from_cache(m_table, index);
    }
    dict_mem_table_free(m_table);

    m_table = exist;
  } else {
    dict_table_add_to_cache(m_table, true);

    if (m_table->fts && dict_table_has_fts_index(m_table)) {
      fts_optimize_add_table(m_table);
    }

    if (dict_sys->dynamic_metadata != nullptr) {
      dict_table_load_dynamic_metadata(m_table);
    }
  }

  m_table->acquire();

  dict_sys_mutex_exit();

  /* Check if this is a DD system table */
  if (m_table != nullptr) {
    std::string db_str;
    std::string tbl_str;
    dict_name::get_table(m_table->name.m_name, db_str, tbl_str);

    m_table->is_dd_table =
        dd::get_dictionary()->is_dd_table_name(db_str.c_str(), tbl_str.c_str());
  }

  /* Load foreign key info. It could also register child table(s) that
  refers to current table */
  if (exist == nullptr) {
    dberr_t error =
        dd_table_load_fk(client, norm_name, nullptr, m_table,
                         &dd_table->table(), thd, false, true, &fk_list);
    if (error != DB_SUCCESS) {
      dict_table_close(m_table, false, false);
      m_table = nullptr;
    }
  }
  mem_heap_free(heap);

  return m_table;
}

/** Open single table with name
@param[in]      name            table name
@param[in]      dict_locked     dict_sys mutex is held or not
@param[in,out]  fk_list         foreign key name list
@param[in]      thd             thread THD */
static void dd_open_table_one_on_name(const char *name, bool dict_locked,
                                      dict_names_t &fk_list, THD *thd) {
  dict_table_t *table = nullptr;
  const dd::Table *dd_table = nullptr;
  MDL_ticket *mdl = nullptr;

  if (!dict_locked) {
    dict_sys_mutex_enter();
  }

  table = dict_table_check_if_in_cache_low(name);

  if (table != nullptr) {
    /* If the table is in cached already, do nothing. */
    if (!dict_locked) {
      dict_sys_mutex_exit();
    }

    return;
  } else {
    /* Otherwise, open it by dd obj. */

    /* Exit sys mutex to access server info */
    dict_sys_mutex_exit();

    std::string db_str;
    std::string tbl_str;
    dict_name::get_table(name, db_str, tbl_str);

    if (db_str.empty() || tbl_str.empty() ||
        dd_mdl_acquire(thd, &mdl, db_str.c_str(), tbl_str.c_str())) {
      goto func_exit;
    }

    dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
    dd::cache::Dictionary_client::Auto_releaser releaser(client);

    if (client->acquire(db_str.c_str(), tbl_str.c_str(), &dd_table) ||
        dd_table == nullptr) {
      goto func_exit;
    }

    ut_ad(dd_table->se_private_id() != dd::INVALID_OBJECT_ID);

    TABLE_SHARE ts;

    init_tmp_table_share(thd, &ts, db_str.c_str(), db_str.length(),
                         dd_table->name().c_str(), "" /* file name */, nullptr);

    ulint error =
        open_table_def_suppress_invalid_meta_data(thd, &ts, *dd_table);

    if (error != 0) {
      goto func_exit;
    }

    TABLE td;

    error = open_table_from_share(thd, &ts, dd_table->name().c_str(), 0,
                                  SKIP_NEW_HANDLER, 0, &td, false, dd_table);

    if (error != 0) {
      free_table_share(&ts);
      goto func_exit;
    }

    table = dd_open_table_one(client, &td, name, dd_table, thd, fk_list);

    closefrm(&td, false);
    free_table_share(&ts);
  }

func_exit:
  if (table != nullptr) {
    dd_table_close(table, thd, &mdl, false);
  } else {
    dd_mdl_release(thd, &mdl);
  }

  if (dict_locked) {
    dict_sys_mutex_enter();
  }
}

/** Open foreign tables reference a table.
@param[in]      fk_list         foreign key name list
@param[in]      dict_locked     dict_sys mutex is locked or not
@param[in]      thd             thread THD */
void dd_open_fk_tables(dict_names_t &fk_list, bool dict_locked, THD *thd) {
  while (!fk_list.empty()) {
    char *name = const_cast<char *>(fk_list.front());

    if (innobase_get_lower_case_table_names() == 2) {
      innobase_casedn_str(name);
    } else {
#ifndef _WIN32
      if (innobase_get_lower_case_table_names() == 1) {
        innobase_casedn_str(name);
      }
#endif /* !_WIN32 */
    }

    dd_open_table_one_on_name(name, dict_locked, fk_list, thd);

    fk_list.pop_front();
  }
}

/** Open or load a table definition based on a Global DD object.
@tparam         Table           dd::Table or dd::Partition
@param[in,out]  client          data dictionary client
@param[in]      table           MySQL table definition
@param[in]      norm_name       Table Name
@param[in]      dd_table        Global DD table or partition object
@param[in]      thd             thread THD
@return ptr to dict_table_t filled, otherwise, nullptr */
template <typename Table>
dict_table_t *dd_open_table(dd::cache::Dictionary_client *client,
                            const TABLE *table, const char *norm_name,
                            const Table *dd_table, THD *thd) {
  dict_table_t *m_table = nullptr;
  dict_names_t fk_list;

  m_table = dd_open_table_one(client, table, norm_name, dd_table, thd, fk_list);

  /* If there is foreign table references to this table, we will
  try to open them */
  if (m_table != nullptr && !fk_list.empty()) {
    dd_open_fk_tables(fk_list, false, thd);
  }

  return m_table;
}

template dict_table_t *dd_open_table<dd::Table>(dd::cache::Dictionary_client *,
                                                const TABLE *, const char *,
                                                const dd::Table *, THD *);

template dict_table_t *dd_open_table<dd::Partition>(
    dd::cache::Dictionary_client *, const TABLE *, const char *,
    const dd::Partition *, THD *);

/** Get next record from a new dd system table, like mysql.tables...
@param[in,out] pcur            Persistent cursor
@param[in]     mtr             Mini-transaction
@return the next rec of the dd system table */
static const rec_t *dd_getnext_system_low(btr_pcur_t *pcur, mtr_t *mtr) {
  rec_t *rec = nullptr;
  bool is_comp = dict_table_is_comp(pcur->index()->table);

  while (!rec || rec_get_deleted_flag(rec, is_comp)) {
    pcur->move_to_next_user_rec(mtr);

    rec = pcur->get_rec();

    if (!pcur->is_on_user_rec()) {
      /* end of index */
      pcur->close();

      return nullptr;
    }
  }

  /* Get a record, let's save the position */
  pcur->store_position(mtr);

  return rec;
}

/** Get next record of new DD system tables
@param[in,out]  pcur            Persistent cursor
@param[in]      mtr             Mini-transaction
@retval next record */
const rec_t *dd_getnext_system_rec(btr_pcur_t *pcur, mtr_t *mtr) {
  /* Restore the position */
  pcur->restore_position(BTR_SEARCH_LEAF, mtr, UT_LOCATION_HERE);

  return dd_getnext_system_low(pcur, mtr);
}

/** Scan a new dd system table, like mysql.tables...
@param[in]      thd             THD
@param[in,out]  mdl             MDL lock
@param[in,out]  pcur            Persistent cursor
@param[in,out]  mtr             Mini-transaction
@param[in]      system_table_name       Which dd system table to open
@param[in,out]  table           dict_table_t obj of dd system table
@retval the first rec of the dd system table */
const rec_t *dd_startscan_system(THD *thd, MDL_ticket **mdl, btr_pcur_t *pcur,
                                 mtr_t *mtr, const char *system_table_name,
                                 dict_table_t **table) {
  dict_index_t *clust_index;
  const rec_t *rec = nullptr;

  *table = dd_table_open_on_name(thd, mdl, system_table_name, true, false);
  mtr_commit(mtr);

  clust_index = UT_LIST_GET_FIRST((*table)->indexes);

  mtr_start(mtr);
  pcur->open_at_side(true, clust_index, BTR_SEARCH_LEAF, true, 0, mtr);

  rec = dd_getnext_system_low(pcur, mtr);

  return rec;
}

/**
  All DD tables would contain DB_TRX_ID and DB_ROLL_PTR fields
  before other fields. This offset indicates the position at
  which the first DD column is located.
*/
static const int DD_FIELD_OFFSET = 2;

/** Process one mysql.tables record and get the dict_table_t
@param[in]      heap            Temp memory heap
@param[in,out]  rec             mysql.tables record
@param[in,out]  table           dict_table_t to fill
@param[in]      dd_tables       dict_table_t obj of dd system table
@param[in]      mdl             MDL on the table
@param[in]      mtr             Mini-transaction
@retval error message, or NULL on success */
const char *dd_process_dd_tables_rec_and_mtr_commit(
    mem_heap_t *heap, const rec_t *rec, dict_table_t **table,
    dict_table_t *dd_tables, MDL_ticket **mdl, mtr_t *mtr) {
  ulint len;
  const byte *field;
  const char *err_msg = nullptr;
  ulint table_id;

  ut_ad(!rec_get_deleted_flag(rec, dict_table_is_comp(dd_tables)));
  ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));

  ulint *offsets = rec_get_offsets(rec, dd_tables->first_index(), nullptr,
                                   ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);

  const dd::Object_table &dd_object_table = dd::get_dd_table<dd::Table>();

  field = rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_ENGINE") + DD_FIELD_OFFSET, &len);

  /* If "engine" field is not "innodb", return. */
  if (strncmp((const char *)field, "InnoDB", 6) != 0) {
    *table = nullptr;
    mtr_commit(mtr);
    return err_msg;
  }

  /* Get the se_private_id field. */
  field = (const byte *)rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_SE_PRIVATE_ID") + DD_FIELD_OFFSET,
      &len);

  if (len != 8) {
    *table = nullptr;
    mtr_commit(mtr);
    return err_msg;
  }

  /* Get the table id */
  table_id = mach_read_from_8(field);

  /* Skip mysql.* tables. */
  if (dict_sys_t::is_dd_table_id(table_id)) {
    *table = nullptr;
    mtr_commit(mtr);
    return err_msg;
  }

  /* Commit before load the table again */
  mtr_commit(mtr);
  THD *thd = current_thd;

  *table = dd_table_open_on_id(table_id, thd, mdl, true, false);

  if (!(*table)) {
    err_msg = "Table not found";
  }

  return err_msg;
}

/** Process one mysql.table_partitions record and get the dict_table_t
@param[in]      heap            Temp memory heap
@param[in,out]  rec             mysql.table_partitions record
@param[in,out]  table           dict_table_t to fill
@param[in]      dd_tables       dict_table_t obj of dd partition table
@param[in]      mdl             MDL on the table
@param[in]      mtr             Mini-transaction
@retval error message, or NULL on success */
const char *dd_process_dd_partitions_rec_and_mtr_commit(
    mem_heap_t *heap, const rec_t *rec, dict_table_t **table,
    dict_table_t *dd_tables, MDL_ticket **mdl, mtr_t *mtr) {
  ulint len;
  const byte *field;
  const char *err_msg = nullptr;
  ulint table_id;

  ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));

  ut_ad(!rec_get_deleted_flag(rec, dict_table_is_comp(dd_tables)));

  ulint *offsets = rec_get_offsets(rec, dd_tables->first_index(), nullptr,
                                   ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);

  const dd::Object_table &dd_object_table = dd::get_dd_table<dd::Partition>();

  /* Get the engine field. */
  field = rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_ENGINE") + DD_FIELD_OFFSET, &len);

  /* If "engine" field is not "innodb", return. */
  if (strncmp((const char *)field, "InnoDB", 6) != 0) {
    *table = nullptr;
    mtr_commit(mtr);
    return err_msg;
  }

  /* Get the se_private_id field. */
  field = (const byte *)rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_SE_PRIVATE_ID") + DD_FIELD_OFFSET,
      &len);
  /* When table is partitioned table, the se_private_id is null. */
  if (len != 8) {
    *table = nullptr;
    mtr_commit(mtr);
    return err_msg;
  }

  /* Get the table id */
  table_id = mach_read_from_8(field);

  /* Skip mysql.* tables. */
  if (dict_sys_t::is_dd_table_id(table_id)) {
    *table = nullptr;
    mtr_commit(mtr);
    return err_msg;
  }

  /* Commit before load the table again */
  mtr_commit(mtr);
  THD *thd = current_thd;

  *table = dd_table_open_on_id(table_id, thd, mdl, true, false);

  if (!(*table)) {
    err_msg = "Table not found";
  }

  return err_msg;
}

/** Process one mysql.columns record and get info to dict_col_t
@param[in,out]  heap            Temp memory heap
@param[in]      rec             mysql.columns record
@param[in,out]  col             dict_col_t to fill
@param[in,out]  table_id        Table id
@param[in,out]  col_name        Column name
@param[in,out]  nth_v_col       Nth v column
@param[in]      dd_columns      dict_table_t obj of mysql.columns
@param[in,out]  mtr             Mini-transaction
@retval true if column is filled */
bool dd_process_dd_columns_rec(mem_heap_t *heap, const rec_t *rec,
                               dict_col_t *col, table_id_t *table_id,
                               char **col_name, ulint *nth_v_col,
                               const dict_table_t *dd_columns, mtr_t *mtr) {
  ulint len;
  const byte *field;
  dict_col_t *t_col;
  ulint pos;
  ulint v_pos = 0;
  dd::Column::enum_hidden_type hidden;
  bool is_virtual;
  dict_v_col_t *vcol = nullptr;

  ut_ad(!rec_get_deleted_flag(rec, dict_table_is_comp(dd_columns)));

  ulint *offsets = rec_get_offsets(rec, dd_columns->first_index(), nullptr,
                                   ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);

  const dd::Object_table &dd_object_table = dd::get_dd_table<dd::Column>();

  /* Get the hidden attribute, and skip if it's a hidden column. */
  field = (const byte *)rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_HIDDEN") + DD_FIELD_OFFSET, &len);
  hidden = static_cast<dd::Column::enum_hidden_type>(mach_read_from_1(field));
  if (hidden == dd::Column::enum_hidden_type::HT_HIDDEN_SE ||
      hidden == dd::Column::enum_hidden_type::HT_HIDDEN_SQL) {
    mtr_commit(mtr);
    return false;
  }

  /* Get the column name. */
  field = (const byte *)rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_NAME") + DD_FIELD_OFFSET, &len);
  *col_name = mem_heap_strdupl(heap, (const char *)field, len);

  /* Get the position. */
  field = (const byte *)rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_ORDINAL_POSITION") + DD_FIELD_OFFSET,
      &len);
  pos = mach_read_from_4(field) - 1;

  /* Get the is_virtual attribute. */
  field = (const byte *)rec_get_nth_field(nullptr, rec, offsets, 21, &len);
  is_virtual = mach_read_from_1(field) & 0x01;

  /* Get the se_private_data field. */
  field = (const byte *)rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_SE_PRIVATE_DATA") + DD_FIELD_OFFSET,
      &len);

  if (len == 0 || len == UNIV_SQL_NULL) {
    mtr_commit(mtr);
    return false;
  }

  char *p_ptr = (char *)mem_heap_strdupl(heap, (const char *)field, len);
  dd::String_type prop((char *)p_ptr);
  dd::Properties *p = dd::Properties::parse_properties(prop);

  /* Load the table and get the col. */
  if (!p || !p->exists(dd_index_key_strings[DD_TABLE_ID])) {
    if (p) {
      delete p;
    }
    mtr_commit(mtr);
    return false;
  }

  if (!p->get(dd_index_key_strings[DD_TABLE_ID], (uint64_t *)table_id)) {
    THD *thd = current_thd;
    dict_table_t *table;
    MDL_ticket *mdl = nullptr;

    /* Commit before we try to load the table. */
    mtr_commit(mtr);
    table = dd_table_open_on_id(*table_id, thd, &mdl, true, true);

    if (!table) {
      delete p;
      return false;
    }

    if (is_virtual) {
      vcol = dict_table_get_nth_v_col_mysql(table, pos);

      if (vcol == nullptr) {
        dd_table_close(table, thd, &mdl, true);
        delete p;
        return false;
      }

      /* Copy info. */
      col->ind = vcol->m_col.ind;
      col->mtype = vcol->m_col.mtype;
      col->prtype = vcol->m_col.prtype;
      col->len = vcol->m_col.len;

      v_pos = dict_create_v_col_pos(vcol->v_pos, vcol->m_col.ind);
    } else {
      if (table->n_v_cols == 0) {
        t_col = table->get_col(pos);
      } else {
        ulint col_nr;

        col_nr = dict_table_has_column(table, *col_name, pos);
        t_col = table->get_col(col_nr);
        ut_ad(t_col);
      }

      /* Copy info. */
      col->ind = t_col->ind;
      col->mtype = t_col->mtype;
      col->prtype = t_col->prtype;
      col->len = t_col->len;
    }

    if (p->exists(dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT_NULL]) ||
        p->exists(dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT])) {
      dd_parse_default_value(*p, col, heap);
    }

    dd_table_close(table, thd, &mdl, true);
    delete p;
  } else {
    delete p;
    mtr_commit(mtr);
    return false;
  }

  /* Report the virtual column number */
  if (col->prtype & DATA_VIRTUAL) {
    ut_ad(vcol != nullptr);
    ut_ad(v_pos != 0);
    ut_ad(is_virtual);

    *nth_v_col = dict_get_v_col_pos(v_pos);
  } else {
    *nth_v_col = ULINT_UNDEFINED;
  }

  return true;
}

/** Process one mysql.columns record for virtual columns
@param[in]      heap            temp memory heap
@param[in,out]  rec             mysql.columns record
@param[in,out]  table_id        Table id
@param[in,out]  pos             Position
@param[in,out]  base_pos        Base column position
@param[in,out]  n_row           Number of rows
@param[in]      dd_columns      dict_table_t obj of mysql.columns
@param[in]      mtr             Mini-transaction
@retval true if virtual info is filled */
bool dd_process_dd_virtual_columns_rec(mem_heap_t *heap, const rec_t *rec,
                                       table_id_t *table_id, ulint **pos,
                                       ulint **base_pos, ulint *n_row,
                                       dict_table_t *dd_columns, mtr_t *mtr) {
  ulint len;
  const byte *field;
  ulint origin_pos;
  dd::Column::enum_hidden_type hidden;
  bool is_virtual;

  ut_ad(!rec_get_deleted_flag(rec, dict_table_is_comp(dd_columns)));

  ulint *offsets = rec_get_offsets(rec, dd_columns->first_index(), nullptr,
                                   ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);

  const dd::Object_table &dd_object_table = dd::get_dd_table<dd::Column>();

  /* Get the is_virtual attribute, and skip if it's not a virtual column. */
  field = (const byte *)rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_IS_VIRTUAL") + DD_FIELD_OFFSET, &len);
  is_virtual = mach_read_from_1(field) & 0x01;
  if (!is_virtual) {
    mtr_commit(mtr);
    return false;
  }

  /* Get the hidden attribute, and skip if it's a hidden column. */
  field = (const byte *)rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_HIDDEN") + DD_FIELD_OFFSET, &len);
  hidden = static_cast<dd::Column::enum_hidden_type>(mach_read_from_1(field));
  if (hidden == dd::Column::enum_hidden_type::HT_HIDDEN_SE) {
    mtr_commit(mtr);
    return false;
  }

  /* Get the position. */
  field = (const byte *)rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_ORDINAL_POSITION") + DD_FIELD_OFFSET,
      &len);
  origin_pos = mach_read_from_4(field) - 1;

  /* Get the se_private_data field. */
  field = (const byte *)rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_SE_PRIVATE_DATA") + DD_FIELD_OFFSET,
      &len);

  if (len == 0 || len == UNIV_SQL_NULL) {
    mtr_commit(mtr);
    return false;
  }

  char *p_ptr = (char *)mem_heap_strdupl(heap, (const char *)field, len);
  dd::String_type prop((char *)p_ptr);
  dd::Properties *p = dd::Properties::parse_properties(prop);

  /* Load the table and get the col. */
  if (!p || !p->exists(dd_index_key_strings[DD_TABLE_ID])) {
    if (p) {
      delete p;
    }
    mtr_commit(mtr);
    return false;
  }

  if (!p->get(dd_index_key_strings[DD_TABLE_ID], (uint64_t *)table_id)) {
    THD *thd = current_thd;
    dict_table_t *table;
    MDL_ticket *mdl = nullptr;
    dict_v_col_t *vcol = nullptr;

    /* Commit before we try to load the table. */
    mtr_commit(mtr);
    table = dd_table_open_on_id(*table_id, thd, &mdl, true, true);

    if (!table) {
      delete p;
      return false;
    }

    vcol = dict_table_get_nth_v_col_mysql(table, origin_pos);

    if (vcol == nullptr || vcol->num_base == 0) {
      dd_table_close(table, thd, &mdl, true);
      delete p;
      return false;
    }

    *pos = static_cast<ulint *>(
        mem_heap_alloc(heap, vcol->num_base * sizeof(ulint)));
    *base_pos = static_cast<ulint *>(
        mem_heap_alloc(heap, vcol->num_base * sizeof(ulint)));
    *n_row = vcol->num_base;
    for (ulint i = 0; i < *n_row; i++) {
      (*pos)[i] = dict_create_v_col_pos(vcol->v_pos, vcol->m_col.ind);
      (*base_pos)[i] = vcol->base_col[i]->ind;
    }

    dd_table_close(table, thd, &mdl, true);
    delete p;
  } else {
    delete p;
    mtr_commit(mtr);
    return false;
  }

  return true;
}

/** Process one mysql.indexes record and get the dict_index_t
@param[in]      heap            Temp memory heap
@param[in,out]  rec             mysql.indexes record
@param[in,out]  index           dict_index_t to fill
@param[in]      mdl             MDL on index->table
@param[in,out]  parent          Parent table if it's fts aux table.
@param[in,out]  parent_mdl      MDL on parent if it's fts aux table.
@param[in]      dd_indexes      dict_table_t obj of mysql.indexes
@param[in]      mtr             Mini-transaction
@retval true if index is filled */
bool dd_process_dd_indexes_rec(mem_heap_t *heap, const rec_t *rec,
                               const dict_index_t **index, MDL_ticket **mdl,
                               dict_table_t **parent, MDL_ticket **parent_mdl,
                               dict_table_t *dd_indexes, mtr_t *mtr) {
  ulint len;
  const byte *field;
  uint32_t index_id;
  uint32_t space_id;
  uint64_t table_id;

  *index = nullptr;

  ut_ad(!rec_get_deleted_flag(rec, dict_table_is_comp(dd_indexes)));

  ulint *offsets = rec_get_offsets(rec, dd_indexes->first_index(), nullptr,
                                   ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);

  const dd::Object_table &dd_object_table = dd::get_dd_table<dd::Index>();

  field = rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_ENGINE") + DD_FIELD_OFFSET, &len);

  /* If "engine" field is not "innodb", return. */
  if (strncmp((const char *)field, "InnoDB", 6) != 0) {
    mtr_commit(mtr);
    return false;
  }

  /* Get the se_private_data field. */
  field = (const byte *)rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_SE_PRIVATE_DATA") + DD_FIELD_OFFSET,
      &len);

  if (len == 0 || len == UNIV_SQL_NULL) {
    mtr_commit(mtr);
    return false;
  }

  /* Get index id. */
  dd::String_type prop((char *)field);
  dd::Properties *p = dd::Properties::parse_properties(prop);

  if (!p || !p->exists(dd_index_key_strings[DD_INDEX_ID]) ||
      !p->exists(dd_index_key_strings[DD_INDEX_SPACE_ID])) {
    if (p) {
      delete p;
    }
    mtr_commit(mtr);
    return false;
  }

  if (p->get(dd_index_key_strings[DD_INDEX_ID], &index_id)) {
    delete p;
    mtr_commit(mtr);
    return false;
  }

  /* Get the tablespace id. */
  if (p->get(dd_index_key_strings[DD_INDEX_SPACE_ID], &space_id)) {
    delete p;
    mtr_commit(mtr);
    return false;
  }

  /* Skip mysql.* indexes. */
  if (space_id == dict_sys->s_dict_space_id) {
    delete p;
    mtr_commit(mtr);
    return false;
  }

  /* Load the table and get the index. */
  if (!p->exists(dd_index_key_strings[DD_TABLE_ID])) {
    delete p;
    mtr_commit(mtr);
    return false;
  }

  if (!p->get(dd_index_key_strings[DD_TABLE_ID], &table_id)) {
    THD *thd = current_thd;
    dict_table_t *table;

    /* Commit before load the table */
    mtr_commit(mtr);
    table = dd_table_open_on_id(table_id, thd, mdl, true, true);

    if (!table) {
      delete p;
      return false;
    }

    /* For fts aux table, we need to acquire mdl lock on parent. */
    if (table->is_fts_aux()) {
      fts_aux_table_t fts_table;

      /* Find the parent ID. */
      ut_d(bool is_fts =) fts_is_aux_table_name(&fts_table, table->name.m_name,
                                                strlen(table->name.m_name));
      ut_ad(is_fts);

      table_id_t parent_id = fts_table.parent_id;

      dd_table_close(table, thd, mdl, true);

      *parent = dd_table_open_on_id(parent_id, thd, parent_mdl, true, true);

      if (*parent == nullptr) {
        delete p;
        return false;
      }

      table = dd_table_open_on_id(table_id, thd, mdl, true, true);

      if (!table) {
        dd_table_close(*parent, thd, parent_mdl, true);
        delete p;
        return false;
      }
    }

    for (const dict_index_t *t_index = table->first_index(); t_index != nullptr;
         t_index = t_index->next()) {
      if (t_index->space == space_id && t_index->id == index_id) {
        *index = t_index;
      }
    }

    if (*index == nullptr) {
      dd_table_close(table, thd, mdl, true);
      if (table->is_fts_aux() && *parent) {
        dd_table_close(*parent, thd, parent_mdl, true);
      }
      delete p;
      return false;
    }

    delete p;
  } else {
    delete p;
    mtr_commit(mtr);
    return false;
  }

  return true;
}

/** Process one mysql.indexes record and get brief info to dict_index_t
@param[in]      heap            temp memory heap
@param[in,out]  rec             mysql.indexes record
@param[in,out]  index_id        index id
@param[in,out]  space_id        space id
@param[in]      dd_indexes      dict_table_t obj of mysql.indexes
@retval true if index is filled */
bool dd_process_dd_indexes_rec_simple(mem_heap_t *heap, const rec_t *rec,
                                      space_index_t *index_id,
                                      space_id_t *space_id,
                                      dict_table_t *dd_indexes) {
  ulint len;
  const byte *field;
  uint32_t idx_id;

  ut_ad(!rec_get_deleted_flag(rec, dict_table_is_comp(dd_indexes)));

  ulint *offsets = rec_get_offsets(rec, dd_indexes->first_index(), nullptr,
                                   ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);

  const dd::Object_table &dd_object_table = dd::get_dd_table<dd::Index>();

  field = rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_ENGINE") + DD_FIELD_OFFSET, &len);

  /* If "engine" field is not "innodb", return. */
  if (strncmp((const char *)field, "InnoDB", 6) != 0) {
    return false;
  }

  /* Get the se_private_data field. */
  field = (const byte *)rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_SE_PRIVATE_DATA") + DD_FIELD_OFFSET,
      &len);

  if (len == 0 || len == UNIV_SQL_NULL) {
    return false;
  }

  /* Get index id. */
  dd::String_type prop((char *)field);
  dd::Properties *p = dd::Properties::parse_properties(prop);

  if (!p || !p->exists(dd_index_key_strings[DD_INDEX_ID]) ||
      !p->exists(dd_index_key_strings[DD_INDEX_SPACE_ID])) {
    if (p) {
      delete p;
    }
    return false;
  }

  if (p->get(dd_index_key_strings[DD_INDEX_ID], &idx_id)) {
    delete p;
    return false;
  }
  *index_id = idx_id;

  /* Get the tablespace_id. */
  if (p->get(dd_index_key_strings[DD_INDEX_SPACE_ID], space_id)) {
    delete p;
    return false;
  }

  delete p;

  return true;
}

bool dd_process_dd_tablespaces_rec(mem_heap_t *heap, const rec_t *rec,
                                   space_id_t *space_id, char **name,
                                   uint32_t *flags, uint32_t *server_version,
                                   uint32_t *space_version, bool *is_encrypted,
                                   uint64_t *autoextend_size,
                                   dd::String_type *state,
                                   dict_table_t *dd_spaces) {
  ulint len;
  const byte *field;
  char *prop_str;
  char *opt_str;

  ut_ad(!rec_get_deleted_flag(rec, dict_table_is_comp(dd_spaces)));

  ulint *offsets = rec_get_offsets(rec, dd_spaces->first_index(), nullptr,
                                   ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);

  const dd::Object_table &dd_object_table = dd::get_dd_table<dd::Tablespace>();

  field = rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_ENGINE") + DD_FIELD_OFFSET, &len);

  /* If "engine" field is not "innodb", return. */
  if (strncmp((const char *)field, "InnoDB", 6) != 0) {
    return false;
  }

  /* Get name field. */
  field = rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_NAME") + DD_FIELD_OFFSET, &len);
  *name = reinterpret_cast<char *>(mem_heap_zalloc(heap, len + 1));
  memcpy(*name, field, len);

  /* Get the options string. */
  field = (const byte *)rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_OPTIONS") + DD_FIELD_OFFSET, &len);

  if (len == 0 || len == UNIV_SQL_NULL) {
    return false; /* purecov: inspected */
  }

  opt_str = static_cast<char *>(mem_heap_zalloc(heap, len + 1));
  memcpy(opt_str, field, len);
  dd::String_type opt(opt_str);
  const dd::Properties *o = dd::Properties::parse_properties(opt);

  if (!o) {
    return false; /* purecov: inspected */
  }

  /* Get encrypted. */
  *is_encrypted = false;
  dd::String_type encrypt;
  if (o->exists("encryption") && o->get("encryption", &encrypt)) {
    /* purecov: begin inspected */
    delete o;
    return false;
    /* purecov: end */
  }

  if (!Encryption::is_none(encrypt.c_str())) {
    *is_encrypted = true;
  }

  /* Get autoextend_size. */
  *autoextend_size = 0;
  if (o->exists(autoextend_size_str) &&
      o->get(autoextend_size_str, autoextend_size)) {
    /* purecov: begin inspected */
    delete o;
    return false;
    /* purecov: end */
  }

  delete o;

  /* Get the se_private_data field. */
  field = (const byte *)rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_SE_PRIVATE_DATA") + DD_FIELD_OFFSET,
      &len);

  if (len == 0 || len == UNIV_SQL_NULL) {
    return false;
  }

  prop_str = static_cast<char *>(mem_heap_zalloc(heap, len + 1));
  memcpy(prop_str, field, len);
  dd::String_type prop(prop_str);
  const dd::Properties *p = dd::Properties::parse_properties(prop);

  if (!p || !p->exists(dd_space_key_strings[DD_SPACE_ID]) ||
      !p->exists(dd_index_key_strings[DD_SPACE_FLAGS])) {
    if (p) {
      delete p;
    }
    return false;
  }

  /* Get space id. */
  if (p->get(dd_space_key_strings[DD_SPACE_ID], space_id)) {
    delete p;
    return false;
  }

  /* Get space flags. */
  if (p->get(dd_space_key_strings[DD_SPACE_FLAGS], flags)) {
    delete p;
    return false;
  }

  /* Get server version. */
  if (p->get(dd_space_key_strings[DD_SPACE_SERVER_VERSION], server_version)) {
    delete p;
    return false;
  }

  /* Get space version. */
  if (p->get(dd_space_key_strings[DD_SPACE_VERSION], space_version)) {
    delete p;
    return false;
  }

  /* Get tablespace state. */
  dd_tablespace_get_state(p, state, *space_id);

  /* For UNDO tablespaces, encryption is governed by srv_undo_log_encrypt
  variable and DD flags are not updated for encryption changes. Following
  is a workaround until UNDO tablespace encryption change is done by a DDL. */
  if (fsp_is_undo_tablespace(*space_id)) {
    *is_encrypted = srv_undo_log_encrypt;
  } else if (FSP_FLAGS_GET_ENCRYPTION(*flags)) {
    /* Get Encryption. */
    *is_encrypted = true;
  }

  delete p;

  return true;
}

/** Get dd tablespace id for fts table
@param[in]      parent_table    parent table of fts table
@param[in]      table           fts table
@param[in,out]  dd_space_id     dd table space id
@return true on success, false on failure. */
static bool dd_get_or_assign_fts_tablespace_id(const dict_table_t *parent_table,
                                               const dict_table_t *table,
                                               dd::Object_id &dd_space_id) {
  THD *thd = current_thd;
  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  dd::Object_id space_id = parent_table->dd_space_id;
  ut_ad(space_id != dd::INVALID_OBJECT_ID);

  dd_space_id = dd::INVALID_OBJECT_ID;

  if (dict_table_is_file_per_table(table)) {
    /* This means user table and file_per_table */
    bool ret;
    char *filename = fil_space_get_first_path(table->space);

    ret = dd_create_implicit_tablespace(
        client, table->space, table->name.m_name, filename, false, dd_space_id);

    ut::free(filename);
    if (ret) {
      return false;
    }

  } else if (table->space != TRX_SYS_SPACE &&
             table->space != srv_tmp_space.space_id()) {
    /* This is a user table that resides in shared tablespace */
    ut_ad(!dict_table_is_file_per_table(parent_table));
    ut_ad(!dict_table_is_file_per_table(table));
    ut_ad(DICT_TF_HAS_SHARED_SPACE(table->flags));

    /* Currently the tablespace id is hard coded as 0 */
    dd_space_id = space_id;

    const dd::Tablespace *index_space = nullptr;
    if (client->acquire<dd::Tablespace>(space_id, &index_space)) {
      return false;
    }

    uint32_t id;
    if (index_space == nullptr) {
      return false;
    } else if (index_space->se_private_data().get(
                   dd_space_key_strings[DD_SPACE_ID], &id) ||
               id != table->space) {
      ut_ad(!"missing or incorrect tablespace id");
      return false;
    }
  } else if (table->space == TRX_SYS_SPACE) {
    /* This is a user table that resides in innodb_system
    tablespace */
    ut_ad(!dict_table_is_file_per_table(table));
    dd_space_id = dict_sys_t::s_dd_sys_space_id;
  }

  return true;
}

/** Set table options for fts dd tables according to dict table
@param[in,out]  dd_table        dd table instance
@param[in]      table           dict table instance */
void dd_set_fts_table_options(dd::Table *dd_table, const dict_table_t *table) {
  dd_table->set_engine(innobase_hton_name);
  dd_table->set_hidden(dd::Abstract_table::HT_HIDDEN_SE);
  dd_table->set_collation_id(my_charset_bin.number);

  dd::Table::enum_row_format row_format = dd::Table::RF_DYNAMIC;
  switch (dict_tf_get_rec_format(table->flags)) {
    case REC_FORMAT_REDUNDANT:
      row_format = dd::Table::RF_REDUNDANT;
      break;
    case REC_FORMAT_COMPACT:
      row_format = dd::Table::RF_COMPACT;
      break;
    case REC_FORMAT_COMPRESSED:
      row_format = dd::Table::RF_COMPRESSED;
      break;
    case REC_FORMAT_DYNAMIC:
      row_format = dd::Table::RF_DYNAMIC;
      break;
    default:
      ut_error;
  }

  dd_table->set_row_format(row_format);

  /* FTS AUX tables are always not encrypted/compressed
  as it is designed now. So both "compress" and "encrypt_type"
  option are not set */

  dd::Properties *table_options = &dd_table->options();
  table_options->set("pack_record", true);
  table_options->set("checksum", false);
  table_options->set("delay_key_write", false);
  table_options->set("avg_row_length", 0);
  table_options->set("stats_sample_pages", 0);
  table_options->set("stats_auto_recalc", HA_STATS_AUTO_RECALC_DEFAULT);

  if (auto zip_ssize = DICT_TF_GET_ZIP_SSIZE(table->flags)) {
    table_options->set("key_block_size", 1 << (zip_ssize - 1));
  } else {
    table_options->set("key_block_size", 0);
  }
}

/** Add nullability info to column se_private_data
@param[in,out]  dd_col  DD table column
@param[in]      col     InnoDB table column */
static void dd_set_fts_nullability(dd::Column *dd_col, const dict_col_t *col) {
  bool is_nullable = !(col->prtype & DATA_NOT_NULL);
  dd::Properties &p = dd_col->se_private_data();
  p.set("nullable", is_nullable);
}

/** Create dd table for fts aux index table
@param[in]      parent_table    parent table of fts table
@param[in,out]  table           fts table
@param[in]      charset         fts index charset
@return true on success, false on failure */
bool dd_create_fts_index_table(const dict_table_t *parent_table,
                               dict_table_t *table,
                               const CHARSET_INFO *charset) {
  ut_ad(charset != nullptr);

  std::string db_name;
  std::string table_name;
  dict_name::get_table(table->name.m_name, db_name, table_name);

  /* Create dd::Table object */
  THD *thd = current_thd;
  dd::Schema_MDL_locker mdl_locker(thd);
  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  const dd::Schema *schema = nullptr;
  if (mdl_locker.ensure_locked(db_name.c_str()) ||
      client->acquire<dd::Schema>(db_name.c_str(), &schema)) {
    return false;
  }

  /* Check if schema is nullptr? */
  if (schema == nullptr) {
    my_error(ER_BAD_DB_ERROR, MYF(0), db_name.c_str());
    return false;
  }

  std::unique_ptr<dd::Table> dd_table_obj(schema->create_table(thd));
  dd::Table *dd_table = dd_table_obj.get();

  dd_table->set_name(table_name.c_str());
  dd_table->set_schema_id(schema->id());

  dd_set_fts_table_options(dd_table, table);

  /* FTS AUX tables are always not encrypted/compressed
  as it is designed now. So both "compress" and "encrypt_type"
  option are not set */

  /* Fill columns */
  /* 1st column: word */
  const char *col_name = nullptr;
  dd::Column *col = dd_table->add_column();
  col_name = "word";
  col->set_name(col_name);
  col->set_type(dd::enum_column_types::VARCHAR);
  col->set_char_length(FTS_INDEX_WORD_LEN);
  col->set_nullable(false);
  col->set_collation_id(charset->number);
  ut_ad(strcmp(col_name, table->get_col_name(0)) == 0);
  dd_set_fts_nullability(col, table->get_col(0));

  dd::Column *key_col1 = col;

  /* 2nd column: first_doc_id */
  col = dd_table->add_column();
  col->set_name("first_doc_id");
  col->set_type(dd::enum_column_types::LONGLONG);
  col->set_char_length(20);
  col->set_numeric_scale(0);
  col->set_nullable(false);
  col->set_unsigned(true);
  col->set_collation_id(charset->number);

  dd::Column *key_col2 = col;

  /* 3rd column: last_doc_id */
  col = dd_table->add_column();
  col->set_name("last_doc_id");
  col->set_type(dd::enum_column_types::LONGLONG);
  col->set_char_length(20);
  col->set_numeric_scale(0);
  col->set_nullable(false);
  col->set_unsigned(true);
  col->set_collation_id(charset->number);

  /* 4th column: doc_count */
  col = dd_table->add_column();
  col->set_name("doc_count");
  col->set_type(dd::enum_column_types::LONG);
  col->set_char_length(4);
  col->set_numeric_scale(0);
  col->set_nullable(false);
  col->set_unsigned(true);
  col->set_collation_id(charset->number);

  /* 5th column: ilist */
  col = dd_table->add_column();
  col->set_name("ilist");
  col->set_type(dd::enum_column_types::BLOB);
  col->set_char_length(8);
  col->set_nullable(false);
  col->set_collation_id(my_charset_bin.number);

  /* Fill index */
  dd::Index *index = dd_table->add_index();
  index->set_name("FTS_INDEX_TABLE_IND");
  index->set_algorithm(dd::Index::IA_BTREE);
  index->set_algorithm_explicit(false);
  index->set_visible(true);
  index->set_type(dd::Index::IT_PRIMARY);
  index->set_ordinal_position(1);
  index->set_generated(false);
  index->set_engine(dd_table->engine());

  index->options().set("flags", 32);

  dd::Index_element *index_elem;
  index_elem = index->add_element(key_col1);
  index_elem->set_length(FTS_INDEX_WORD_LEN);

  index_elem = index->add_element(key_col2);
  index_elem->set_length(FTS_INDEX_FIRST_DOC_ID_LEN);

  /* Fill table space info, etc */
  dd::Object_id dd_space_id;
  if (!dd_get_or_assign_fts_tablespace_id(parent_table, table, dd_space_id)) {
    return false;
  }

  table->dd_space_id = dd_space_id;

  dd_write_table(dd_space_id, dd_table, table);

  MDL_ticket *mdl_ticket = nullptr;
  if (dd::acquire_exclusive_table_mdl(thd, db_name.c_str(), table_name.c_str(),
                                      false, &mdl_ticket)) {
    ut_d(ut_error);
    ut_o(return false);
  }

  /* Store table to dd */
  bool fail = client->store(dd_table);
  if (fail) {
    ut_d(ut_error);
    ut_o(return false);
  }

  return true;
}

/** Create dd table for fts aux common table
@param[in]      parent_table    parent table of fts table
@param[in,out]  table           fts table
@param[in]      is_config       flag whether it's fts aux configure table
@return true on success, false on failure */
bool dd_create_fts_common_table(const dict_table_t *parent_table,
                                dict_table_t *table, bool is_config) {
  std::string db_name;
  std::string table_name;

  dict_name::get_table(table->name.m_name, db_name, table_name);

  /* Create dd::Table object */
  THD *thd = current_thd;
  dd::Schema_MDL_locker mdl_locker(thd);
  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  const dd::Schema *schema = nullptr;
  if (mdl_locker.ensure_locked(db_name.c_str()) ||
      client->acquire<dd::Schema>(db_name.c_str(), &schema)) {
    return false;
  }

  /* Check if schema is nullptr */
  if (schema == nullptr) {
    my_error(ER_BAD_DB_ERROR, MYF(0), db_name.c_str());
    return false;
  }

  std::unique_ptr<dd::Table> dd_table_obj(schema->create_table(thd));
  dd::Table *dd_table = dd_table_obj.get();

  dd_table->set_name(table_name.c_str());
  dd_table->set_schema_id(schema->id());

  dd_set_fts_table_options(dd_table, table);
  const char *col_name = nullptr;

  /* Fill columns */
  if (!is_config) {
    /* 1st column: doc_id */
    dd::Column *col = dd_table->add_column();
    col_name = "doc_id";
    col->set_name(col_name);
    col->set_type(dd::enum_column_types::LONGLONG);
    col->set_char_length(20);
    col->set_numeric_scale(0);
    col->set_nullable(false);
    col->set_unsigned(true);
    col->set_collation_id(my_charset_bin.number);
    ut_ad(strcmp(col_name, table->get_col_name(0)) == 0);
    dd_set_fts_nullability(col, table->get_col(0));

    dd::Column *key_col1 = col;

    /* Fill index */
    dd::Index *index = dd_table->add_index();
    index->set_name("FTS_COMMON_TABLE_IND");
    index->set_algorithm(dd::Index::IA_BTREE);
    index->set_algorithm_explicit(false);
    index->set_visible(true);
    index->set_type(dd::Index::IT_PRIMARY);
    index->set_ordinal_position(1);
    index->set_generated(false);
    index->set_engine(dd_table->engine());

    index->options().set("flags", 32);

    dd::Index_element *index_elem;
    index_elem = index->add_element(key_col1);
    index_elem->set_length(FTS_INDEX_FIRST_DOC_ID_LEN);
  } else {
    /* Fill columns */
    /* 1st column: key */
    dd::Column *col = dd_table->add_column();
    col_name = "key";
    col->set_name(col_name);
    col->set_type(dd::enum_column_types::VARCHAR);
    col->set_char_length(FTS_CONFIG_TABLE_KEY_COL_LEN);
    col->set_nullable(false);
    col->set_collation_id(my_charset_latin1.number);
    ut_ad(strcmp(col_name, table->get_col_name(0)) == 0);
    dd_set_fts_nullability(col, table->get_col(0));

    dd::Column *key_col1 = col;

    /* 2nd column: value */
    col = dd_table->add_column();
    col->set_name("value");
    col->set_type(dd::enum_column_types::VARCHAR);
    col->set_char_length(FTS_CONFIG_TABLE_VALUE_COL_LEN);
    col->set_nullable(false);
    col->set_collation_id(my_charset_latin1.number);

    /* Fill index */
    dd::Index *index = dd_table->add_index();
    index->set_name("FTS_COMMON_TABLE_IND");
    index->set_algorithm(dd::Index::IA_BTREE);
    index->set_algorithm_explicit(false);
    index->set_visible(true);
    index->set_type(dd::Index::IT_PRIMARY);
    index->set_ordinal_position(1);
    index->set_generated(false);
    index->set_engine(dd_table->engine());

    index->options().set("flags", 32);

    dd::Index_element *index_elem;
    index_elem = index->add_element(key_col1);
    index_elem->set_length(FTS_CONFIG_TABLE_KEY_COL_LEN);
  }

  /* Fill table space info, etc */
  dd::Object_id dd_space_id;
  if (!dd_get_or_assign_fts_tablespace_id(parent_table, table, dd_space_id)) {
    ut_d(ut_error);
    ut_o(return false);
  }

  table->dd_space_id = dd_space_id;

  dd_write_table(dd_space_id, dd_table, table);

  MDL_ticket *mdl_ticket = nullptr;
  if (dd::acquire_exclusive_table_mdl(thd, db_name.c_str(), table_name.c_str(),
                                      false, &mdl_ticket)) {
    return false;
  }

  /* Store table to dd */
  bool fail = client->store(dd_table);
  if (fail) {
    ut_d(ut_error);
    ut_o(return false);
  }

  return true;
}

/** Drop dd table & tablespace for fts aux table
@param[in]      name            table name
@param[in]      file_per_table  flag whether use file per table
@return true on success, false on failure. */
bool dd_drop_fts_table(const char *name, bool file_per_table) {
  std::string db_name;
  std::string table_name;

  dict_name::get_table(name, db_name, table_name);

  /* Create dd::Table object */
  THD *thd = current_thd;
  dd::Schema_MDL_locker mdl_locker(thd);
  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  MDL_ticket *mdl_ticket = nullptr;
  if (dd::acquire_exclusive_table_mdl(thd, db_name.c_str(), table_name.c_str(),
                                      false, &mdl_ticket)) {
    return false;
  }

  const dd::Table *dd_table = nullptr;
  if (client->acquire<dd::Table>(db_name.c_str(), table_name.c_str(),
                                 &dd_table)) {
    return false;
  }

  if (dd_table == nullptr) {
    return false;
  }

  if (file_per_table) {
    dd::Object_id dd_space_id = (*dd_table->indexes().begin())->tablespace_id();
    bool error;
    error = dd_drop_tablespace(client, dd_space_id);
    ut_a(!error);
  }

  if (client->drop(dd_table)) {
    return false;
  }

  return true;
}

/** Rename dd table & tablespace files for fts aux table
@param[in]      table           dict table
@param[in]      old_name        old innodb table name
@return true on success, false on failure. */
bool dd_rename_fts_table(const dict_table_t *table, const char *old_name) {
  std::string new_db;
  std::string new_table;
  dict_name::get_table(table->name.m_name, new_db, new_table);

  std::string old_db;
  std::string old_table;
  dict_name::get_table(old_name, old_db, old_table);

  ut_ad(new_db.compare(old_db) != 0);
  ut_ad(new_table.compare(old_table) == 0);

  /* Create dd::Table object */
  THD *thd = current_thd;
  dd::Schema_MDL_locker mdl_locker(thd);
  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  const dd::Schema *to_sch = nullptr;
  if (client->acquire<dd::Schema>(new_db.c_str(), &to_sch)) {
    return false;
  }

  MDL_ticket *mdl_ticket = nullptr;
  if (dd::acquire_exclusive_table_mdl(thd, old_db.c_str(), old_table.c_str(),
                                      false, &mdl_ticket)) {
    return false;
  }

  MDL_ticket *mdl_ticket2 = nullptr;
  if (dd::acquire_exclusive_table_mdl(thd, new_db.c_str(), new_table.c_str(),
                                      false, &mdl_ticket2)) {
    return false;
  }

  dd::Table *dd_table = nullptr;
  if (client->acquire_for_modification<dd::Table>(
          old_db.c_str(), old_table.c_str(), &dd_table)) {
    return false;
  }

  // Set schema id
  dd_table->set_schema_id(to_sch->id());

  /* Rename dd tablespace file */
  if (dict_table_is_file_per_table(table)) {
    char *new_path = fil_space_get_first_path(table->space);

    if (dd_tablespace_rename(table->dd_space_id, false, table->name.m_name,
                             new_path) != DB_SUCCESS) {
      ut_error;
    }

    ut::free(new_path);
  }

  if (client->update(dd_table)) {
    ut_d(ut_error);
    ut_o(return false);
  }

  return true;
}

/** Set the space_id attribute in se_private_data of tablespace
@param[in,out]  dd_space  dd::Tablespace object
@param[in]      space_id  tablespace ID */
void dd_tablespace_set_space_id(dd::Tablespace *dd_space, space_id_t space_id) {
  dd::Properties &p = dd_space->se_private_data();

  p.set(dd_space_key_strings[DD_SPACE_ID], space_id);
}

void dd_tablespace_set_state(THD *thd, dd::Object_id dd_space_id,
                             std::string space_name, dd_space_states dd_state) {
  using Client = dd::cache::Dictionary_client;
  using Releaser = dd::cache::Dictionary_client::Auto_releaser;

  /* Get Tablespace object */
  dd::Tablespace *dd_space = nullptr;
  Client *client = dd::get_dd_client(thd);
  Releaser releaser{client};

  if (dd_tablespace_get_mdl(space_name.c_str())) {
    ut_error;
  }

  if (client->acquire_for_modification(dd_space_id, &dd_space) ||
      dd_space == nullptr) {
    ut_error;
  }

  ut_a(dd_space != nullptr);

  dd_tablespace_set_state(dd_space, dd_state);

  if (client->update(dd_space)) {
    ut_d(ut_error);
  }
}

void dd_tablespace_set_state(dd::Tablespace *dd_space, dd_space_states state) {
  dd::Properties &p = dd_space->se_private_data();

  p.set(dd_space_key_strings[DD_SPACE_STATE], dd_space_state_values[state]);
}

bool dd_tablespace_set_id_and_state(const char *space_name, space_id_t space_id,
                                    dd_space_states state) {
  THD *thd = current_thd;
  dd::Tablespace *dd_space;

  dd::cache::Dictionary_client *dc = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser{dc};
  dd::String_type tsn{space_name};

  bool dd_result = dc->acquire_for_modification(tsn, &dd_space);
  if (dd_space == nullptr) {
    return DD_FAILURE;
  }

  dd_tablespace_set_space_id(dd_space, space_id);

  dd_tablespace_set_state(dd_space, state);

  return dd::commit_or_rollback_tablespace_change(thd, dd_space, dd_result);
}

void dd_set_discarded(dd::Table &table, bool discard) {
  ut_ad(!dd_table_is_partitioned(table));

  dd::Properties &p = table.se_private_data();
  p.set(dd_table_key_strings[DD_TABLE_DISCARD], discard);
}

void dd_set_discarded(dd::Partition &partition, bool discard) {
#ifdef UNIV_DEBUG
  bool is_leaf = false;
  for (const dd::Partition *part : *partition.table().leaf_partitions()) {
    if (part == &partition) {
      is_leaf = true;
      break;
    }
  }
  ut_ad(is_leaf);
#endif

  dd::Properties &p = partition.se_private_data();
  p.set(dd_partition_key_strings[DD_PARTITION_DISCARD], discard);
}

void dd_tablespace_get_state(const dd::Tablespace *dd_space,
                             dd::String_type *state, space_id_t space_id) {
  const dd::Properties &p = dd_space->se_private_data();

  dd_tablespace_get_state(&p, state, space_id);
}

void dd_tablespace_get_state(const dd::Properties *p, dd::String_type *state,
                             space_id_t space_id) {
  if (p->exists(dd_space_key_strings[DD_SPACE_STATE])) {
    p->get(dd_space_key_strings[DD_SPACE_STATE], state);
  } else {
    /* If this k/v pair is missing then the database may have been created
    by an earlier version. So calculate the state. */
    dd_space_states state_enum =
        dd_tablespace_get_state_enum_legacy(p, space_id);
    *state = dd_space_state_values[state_enum];
  }
}

dd_space_states dd_tablespace_get_state_enum(const dd::Tablespace *dd_space,
                                             space_id_t space_id) {
  const dd::Properties &p = dd_space->se_private_data();

  return dd_tablespace_get_state_enum(&p, space_id);
}

dd_space_states dd_tablespace_get_state_enum(const dd::Properties *p,
                                             space_id_t space_id) {
  dd_space_states state_enum = DD_SPACE_STATE__LAST;

  /* Look for the 'state' key and read its value from the DD. */
  if (p->exists(dd_space_key_strings[DD_SPACE_STATE])) {
    dd::String_type state;
    p->get(dd_space_key_strings[DD_SPACE_STATE], &state);

    /* Convert this string to a number. */
    for (int s = DD_SPACE_STATE_NORMAL; s < DD_SPACE_STATE__LAST; s++) {
      if (state == dd_space_state_values[s]) {
        state_enum = (dd_space_states)s;
        return state_enum;
      }
    }
  }

  return dd_tablespace_get_state_enum_legacy(p, space_id);
}

dd_space_states dd_tablespace_get_state_enum_legacy(const dd::Properties *p,
                                                    space_id_t space_id) {
  dd_space_states state_enum = DD_SPACE_STATE__LAST;

  /* This is called when the 'state' key is missing from the
  dd::Tablespace::se_private_data field. The database may have been
  created by an earlier version. So calculate the state another way.
  First, make sure we have the space_id. */
  if (space_id == SPACE_UNKNOWN) {
    if (p->exists(dd_space_key_strings[DD_SPACE_ID])) {
      p->get(dd_space_key_strings[DD_SPACE_ID], &space_id);
    } else {
      return DD_SPACE_STATE__LAST;
    }
  }
  ut_ad(space_id != SPACE_UNKNOWN);

  /* Undo tablespaces have the state recorded in undo::spaces. */
  if (fsp_is_undo_tablespace(space_id)) {
    undo::spaces->s_lock();
    undo::Tablespace *undo_space = undo::spaces->find(undo::id2num(space_id));

    if (undo_space->is_active()) {
      state_enum = DD_SPACE_STATE_ACTIVE;
    } else if (undo_space->is_empty()) {
      state_enum = DD_SPACE_STATE_EMPTY;
    } else {
      state_enum = DD_SPACE_STATE_INACTIVE;
    }
    undo::spaces->s_unlock();

    return state_enum;
  }

  /* This is an IBD tablespace without the 'state' key value. It might have
  been created with a MySQL version before the 'state' field was introduced.
  Look for the 'discarded' key value. */
  bool is_discarded = false;
  if (p->exists(dd_space_key_strings[DD_SPACE_DISCARD])) {
    p->get(dd_space_key_strings[DD_SPACE_DISCARD], &is_discarded);
  }

  state_enum = is_discarded ? DD_SPACE_STATE_DISCARDED : DD_SPACE_STATE_NORMAL;

  return state_enum;
}

/** Get the discarded state from se_private_data of tablespace
@param[in]      dd_space        dd::Tablespace object */
bool dd_tablespace_is_discarded(const dd::Tablespace *dd_space) {
  dd::String_type dd_state;

  dd_tablespace_get_state(dd_space, &dd_state);

  if (dd_state == dd_space_state_values[DD_SPACE_STATE_DISCARDED]) {
    return true;
  }

  return false;
}

bool dd_tablespace_get_mdl(const char *space_name, MDL_ticket **mdl_ticket,
                           bool foreground) {
  THD *thd = current_thd;
  /* Safeguard in release mode if background thread doesn't have THD. */
  if (thd == nullptr) {
    ut_d(ut_error);
    ut_o(return true);
  }
  /* Explicit duration for background threads. */
  bool trx_duration = foreground;

  /* Background thread should not block on MDL lock. */
  ulong timeout = foreground ? thd->variables.lock_wait_timeout : 0;
  bool result = acquire_shared_backup_lock(thd, timeout, trx_duration);

  if (!result) {
    result = dd::acquire_exclusive_tablespace_mdl(thd, space_name, false,
                                                  mdl_ticket, trx_duration);
    if (result) {
      release_backup_lock(thd);
    }
  }

  /* For background thread, clear timeout error. */
  if (result && !foreground && thd->is_error()) {
    thd->clear_error();
  }
  return result;
}

/** Release the MDL held by the given ticket.
@param[in]  mdl_ticket  tablespace MDL ticket */
void dd_release_mdl(MDL_ticket *mdl_ticket) {
  dd::release_mdl(current_thd, mdl_ticket);
  release_backup_lock(current_thd);
}

#ifdef UNIV_DEBUG
/** @return total number of indexes of all DD tables */
uint32_t dd_get_total_indexes_num() {
  uint32_t indexes_count = 0;
  for (uint32_t idx = 0; idx < innodb_dd_table_size; idx++) {
    indexes_count += innodb_dd_table[idx].n_indexes;
  }
  return indexes_count;
}
#endif /* UNIV_DEBUG */

/** Open a table from its database and table name, this is currently used by
foreign constraint parser to get the referenced table.
@param[in]      name                    foreign key table name
@param[in]      database_name           table db name
@param[in]      database_name_len       db name length
@param[in]      table_name              table db name
@param[in]      table_name_len          table name length
@param[in,out]  table                   table object or NULL
@param[in,out]  mdl                     mdl on table
@param[in,out]  heap                    heap memory
@return complete table name with database and table name, allocated from
heap memory passed in */
char *dd_get_referenced_table(const char *name, const char *database_name,
                              ulint database_name_len, const char *table_name,
                              ulint table_name_len, dict_table_t **table,
                              MDL_ticket **mdl, mem_heap_t *heap) {
  char *ref;
  const char *db_name;

  bool is_part = dict_name::is_partition(name);

  *table = nullptr;

  if (!database_name) {
    /* Use the database name of the foreign key table */

    db_name = name;
    database_name_len = dict_get_db_name_len(name);
  } else {
    db_name = database_name;
  }

  /* Copy database_name, '/', table_name, '\0' */
  ref = static_cast<char *>(
      mem_heap_alloc(heap, database_name_len + table_name_len + 2));

  memcpy(ref, db_name, database_name_len);
  ref[database_name_len] = '/';
  memcpy(ref + database_name_len + 1, table_name, table_name_len + 1);

  /* Values;  0 = Store and compare as given; case sensitive
              1 = Store and compare in lower; case insensitive
              2 = Store as given, compare in lower; case semi-sensitive */
  if (innobase_get_lower_case_table_names() == 2) {
    innobase_casedn_str(ref);
    if (!is_part) {
      *table = dd_table_open_on_name(current_thd, mdl, ref, true,
                                     DICT_ERR_IGNORE_NONE);
    }
    memcpy(ref, db_name, database_name_len);
    ref[database_name_len] = '/';
    memcpy(ref + database_name_len + 1, table_name, table_name_len + 1);

  } else {
#ifndef _WIN32
    if (innobase_get_lower_case_table_names() == 1) {
      innobase_casedn_str(ref);
    }
#else
    innobase_casedn_str(ref);
#endif /* !_WIN32 */
    if (!is_part) {
      *table = dd_table_open_on_name(current_thd, mdl, ref, true,
                                     DICT_ERR_IGNORE_NONE);
    }
  }

  return ref;
}

/** Update all InnoDB tablespace cache objects. This step is done post
dictionary trx rollback, binlog recovery and DDL_LOG apply. So DD is
consistent. Update the cached tablespace objects, if they differ from
the dictionary.
@param[in,out]  thd     thread handle
@retval true    on error
@retval false   on success */
bool dd_tablespace_update_cache(THD *thd) {
  /* If there are no prepared trxs, then DD reads would have been
  already consistent. No need to update cache */
  if (!trx_sys->found_prepared_trx) {
    return false;
  }

  dd::cache::Dictionary_client *dc = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(dc);
  std::vector<const dd::Tablespace *> tablespaces;

  space_id_t max_id = 0;

  if (dc->fetch_global_components(&tablespaces)) {
    return true;
  }

  bool fail = false;

  for (const dd::Tablespace *t : tablespaces) {
    ut_ad(!fail);

    if (t->engine() != innobase_hton_name) {
      continue;
    }

    const dd::Properties &p = t->se_private_data();
    uint32_t id;
    uint32_t flags = 0;

    /* There should be exactly one file name associated
    with each InnoDB tablespace, except innodb_system */
    fail = p.get(dd_space_key_strings[DD_SPACE_ID], &id) ||
           p.get(dd_space_key_strings[DD_SPACE_FLAGS], &flags) ||
           (t->files().size() != 1 &&
            strcmp(t->name().c_str(), dict_sys_t::s_sys_space_name) != 0);

    if (fail) {
      break;
    }

    /* Undo tablespaces may be deleted and re-created at
    startup and not registered in DD. So exempt undo tablespaces
    from verification */
    if (fsp_is_undo_tablespace(id)) {
      continue;
    }

    if (!dict_sys_t::is_reserved(id) && id > max_id) {
      /* Currently try to find the max one only, it should
      be able to reuse the deleted smaller ones later */
      max_id = id;
    }

    const dd::Tablespace_file *f = *t->files().begin();
    fail = f == nullptr;
    if (fail) {
      break;
    }

    const char *space_name = t->name().c_str();
    fil_space_t *space = fil_space_get(id);

    if (space != nullptr) {
      /* If the tablespace is already in cache, verify that
      the tablespace name matches the name in dictionary.
      If it doesn't match, use the name from dictionary. */

      /* Exclude Encryption flag as (un)encryption operation might be
      rolling forward in background thread. */
      ut_ad(!((space->flags ^ flags) & ~(FSP_FLAGS_MASK_ENCRYPTION)));

      fil_space_update_name(space, space_name);

    } else {
      fil_type_t purpose = fsp_is_system_temporary(id) ? FIL_TYPE_TEMPORARY
                                                       : FIL_TYPE_TABLESPACE;

      const char *filename = f->filename().c_str();

      /* If the user tablespace is not in cache, load the
      tablespace now, with the name from dictionary */

      /* It's safe to pass space_name in tablename charset
      because filename is already in filename charset. */
      dberr_t err = fil_ibd_open(false, purpose, id, flags, space_name,
                                 filename, false, false);
      switch (err) {
        case DB_SUCCESS:
          break;
        case DB_CANNOT_OPEN_FILE:
          break;
        default:
          ib::info(ER_IB_MSG_174)
              << "Unable to open tablespace " << id << " (flags=" << flags
              << ", filename=" << filename << ")."
              << " Have you deleted/moved the .IBD";
          ut_strerr(err);
      }
    }
    if (id != TRX_SYS_SPACE && fil_space_get(id) != nullptr) {
      /* Get the autoextend_size property from the tablespace
      and set the fil_space_t::autoextend_size attribute. */
      const dd::Properties &o = t->options();
      uint64_t autoextend_size{};

      if (o.exists(autoextend_size_str)) {
        o.get(autoextend_size_str, &autoextend_size);
      }

      ut_d(dberr_t ret =) fil_set_autoextend_size(id, autoextend_size);
      ut_ad(ret == DB_SUCCESS);
    }
  }

  fil_set_max_space_id_if_bigger(max_id);
  return fail;
}

/* Check if the table belongs to an encrypted tablespace.
@param[in]      table   table for which check is to be done
@return true if it does. */
bool dd_is_table_in_encrypted_tablespace(const dict_table_t *table) {
  fil_space_t *space = fil_space_get(table->space);
  if (space != nullptr) {
    return FSP_FLAGS_GET_ENCRYPTION(space->flags);
  } else {
    /* Its possible that tablespace flag is missing (for ex: after
    discard tablespace). In that case get tablespace flags from Data
    Dictionary/ */
    THD *thd = current_thd;
    dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
    dd::cache::Dictionary_client::Auto_releaser releaser(client);
    dd::Tablespace *dd_space = nullptr;

    if (!client->acquire_uncached_uncommitted<dd::Tablespace>(
            table->dd_space_id, &dd_space) &&
        dd_space != nullptr) {
      uint32_t flags;
      dd_space->se_private_data().get(dd_space_key_strings[DD_SPACE_FLAGS],
                                      &flags);

      return FSP_FLAGS_GET_ENCRYPTION(flags);
    }
    /* We should not reach here */
    ut_d(ut_error);
    ut_o(return false);
  }
}

void dict_table_t::get_table_name(std::string &schema,
                                  std::string &table) const {
  std::string dict_table_name(name.m_name);
  dict_name::get_table(dict_table_name, schema, table);
}
#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP
namespace dict_name {
void file_to_table(std::string &name, bool quiet) {
  ut_ad(name.length() < FN_REFLEN);
  char conv_name[FN_REFLEN + 1];

  /* Convert to system character set from file name character set. */
  filename_to_tablename(name.c_str(), conv_name, FN_REFLEN, quiet);
  name.assign(conv_name);
}

void table_to_file(std::string &name) {
  ut_ad(name.length() < FN_REFLEN);
  char conv_name[FN_REFLEN + 1];

  /* Convert to system character set from file name character set. */
  static_cast<void>(tablename_to_filename(name.c_str(), conv_name, FN_REFLEN));
  name.assign(conv_name);
}

/** Get partition and sub-partition separator strings.
@param[in]      is_57           true, if 5.7 style separator is needed
@param[out]     part_sep        partition separator
@param[out]     sub_part_sep    sub-partition separator */
static void get_partition_separators(bool is_57, std::string &part_sep,
                                     std::string &sub_part_sep) {
  if (!is_57) {
    part_sep.assign(PART_SEPARATOR);
    sub_part_sep.assign(SUB_PART_SEPARATOR);
    return;
  }
  /* 5.7 style partition separators. */
#ifdef _WIN32
  part_sep.assign(PART_SEPARATOR);
  sub_part_sep.assign(SUB_PART_SEPARATOR);
#else
  part_sep.assign(ALT_PART_SEPARATOR);
  sub_part_sep.assign(ALT_SUB_PART_SEPARATOR);
#endif /* _WIN32 */
}

/** Check for partition and sub partition.
@param[in]      dict_name       name from innodb dictionary
@param[in]      sub_part        true, if checking sub partition
@param[out]     position        position of partition in string
@return true, iff partition/sub-partition exists. */
static bool check_partition(const std::string &dict_name, bool sub_part,
                            size_t &position) {
  std::string part_sep = sub_part ? SUB_PART_SEPARATOR : PART_SEPARATOR;

  /* Check for partition separator string. */
  position = dict_name.find(part_sep);

  if (position != std::string::npos) {
    return true;
  }

  std::string alt_sep = sub_part ? ALT_SUB_PART_SEPARATOR : ALT_PART_SEPARATOR;

  /* Check for alternative partition separator. It is safe check for
  release build server and for upgrade. */
  position = dict_name.find(alt_sep);

  if (position != std::string::npos) {
    return true;
  }

  return false;
}

/** Check for TMP extension name.
@param[in]      dict_name       name from innodb dictionary
@param[out]     position        position of TMP extension in string
@return true, iff TMP extension exists. */
static bool check_tmp(const std::string &dict_name, size_t &position) {
  std::string check_name(dict_name);
  position = std::string::npos;

  /* For partitioned or sub partitioned table we need to search the
  temp postfix within the partition, sub-partition string. The temp
  extension looks as follows in different cases.

  1. Non partitioned table : table_name#TMP
  2. Table Partition       : table_name#p#part_name#TMP
  3. Table Sub Partition   : table_name#p#part_name#sp#sub_part_name#TMP

  The issue with checking only #TMP at the end of string is that the partition
  or sub partition could be named as 'TMP' and in following cases we could
  wrongly classify it as name with temporary extension.

  1. Table Partition       : table_name#p#TMP
  3. Table Sub Partition   : table_name#p#part_name#sp#TMP */

  size_t part_begin = std::string::npos;

  if (check_partition(dict_name, false, part_begin)) {
    part_begin += PART_SEPARATOR_LEN;
    auto part_string = check_name.substr(part_begin);
    /* Modify the name to start from the beginning of partition string
    excluding the partition separator '#p#'. */
    check_name.assign(part_string);

    size_t sub_part_begin = std::string::npos;

    if (check_partition(part_string, true, sub_part_begin)) {
      sub_part_begin += SUB_PART_SEPARATOR_LEN;
      auto sub_part_string = check_name.substr(sub_part_begin);
      /* Modify the name to start from the beginning of sub-partition string
      excluding the sub-partition separator '#sp#'. */
      check_name.assign(sub_part_string);
    }
  }

  auto length = check_name.size();

  if (length < TMP_POSTFIX_LEN) {
    return false;
  }

  auto postfix_pos = length - TMP_POSTFIX_LEN;
  auto ret = check_name.compare(postfix_pos, TMP_POSTFIX_LEN, TMP_POSTFIX);

  if (ret == 0) {
    auto length = dict_name.size();
    ut_a(length >= TMP_POSTFIX_LEN);

    position = length - TMP_POSTFIX_LEN;
    ut_ad(0 == dict_name.compare(position, TMP_POSTFIX_LEN, TMP_POSTFIX));
    return true;
  }
  return false;
}

bool is_partition(const std::string &dict_name) {
  size_t position;
  return check_partition(dict_name, false, position);
}

void get_table(const std::string &dict_name, std::string &schema,
               std::string &table) {
  bool is_tmp;
  std::string partition;
  get_table(dict_name, true, schema, table, partition, is_tmp);
}

void get_table(const std::string &dict_name, bool convert, std::string &schema,
               std::string &table, std::string &partition, bool &is_tmp) {
  size_t table_begin = dict_name.find(SCHEMA_SEPARATOR);

  /* Check if schema is specified. */
  if (table_begin == std::string::npos) {
    table_begin = 0;
    schema.clear();
  } else {
    schema.assign(dict_name.substr(0, table_begin));
    if (convert) {
      /* Perform conversion if requested. Allow invalid conversion
      in schema name. For temp table server passes directory name
      instead of schema name which might contain "." resulting in
      conversion to "?". For temp table this schema name is never used. */
      file_to_table(schema, true);
    }
    ++table_begin;
  }

  table.assign(dict_name.substr(table_begin));
  partition.clear();

  /* Check if partitioned table. */
  size_t part_begin = std::string::npos;
  bool is_part = check_partition(table, false, part_begin);

  /* Check if temp extension. */
  size_t tmp_begin = std::string::npos;
  is_tmp = check_tmp(table, tmp_begin);

  if (is_part) {
    ut_ad(part_begin > 0);
    size_t part_len = std::string::npos;

    if (is_tmp && tmp_begin > part_begin) {
      part_len = tmp_begin - part_begin;
    } else if (is_tmp) {
      /* TMP extension must follow partition. */
      ut_d(ut_error);
    }
    partition.assign(table.substr(part_begin, part_len));
    table.assign(table.substr(0, part_begin));

  } else if (is_tmp) {
    ut_ad(tmp_begin > 0);
    table.assign(table.substr(0, tmp_begin));
  }

  /* Perform conversion if requested. */
  if (convert) {
    file_to_table(table, false);
  }
}

void get_partition(const std::string &partition, bool convert,
                   std::string &part, std::string &sub_part) {
  ut_ad(is_partition(partition));

  /* Check if sub-partition exists. */
  size_t sub_pos = std::string::npos;
  bool is_sub = check_partition(partition, true, sub_pos);

  /* Assign partition name. */
  size_t part_begin = PART_SEPARATOR_LEN;
  size_t part_len = std::string::npos;

  if (is_sub) {
    ut_ad(sub_pos > part_begin);
    part_len = sub_pos - part_begin;
  }

  part.assign(partition.substr(part_begin, part_len));
  if (convert) {
    file_to_table(part, false);
  }

  /* Assign sub-partition name. */
  sub_part.clear();
  if (!is_sub) {
    return;
  }

  auto sub_begin = sub_pos + SUB_PART_SEPARATOR_LEN;
  auto sub_len = std::string::npos;

  sub_part.assign(partition.substr(sub_begin, sub_len));

  if (convert) {
    file_to_table(sub_part, false);
  }
}

void build_table(const std::string &schema, const std::string &table,
                 const std::string &partition, bool is_tmp, bool convert,
                 std::string &dict_name) {
  dict_name.clear();
  std::string conv_str;

  /* Check and append schema name. */
  if (!schema.empty()) {
    conv_str.assign(schema);
    /* Convert schema name. */
    if (convert) {
      table_to_file(conv_str);
    }
    dict_name.append(conv_str);
    dict_name.append(SCHEMA_SEPARATOR);
  }

  conv_str.assign(table);
  /* Convert table name. */
  if (convert) {
    table_to_file(conv_str);
  }
  dict_name.append(conv_str);

  /* Check and assign partition string. Any conversion for partition
  and sub-partition is already done while building partition string. */
  if (!partition.empty()) {
    dict_name.append(partition);
  }

  /* Check and append temporary extension. */
  if (is_tmp) {
    dict_name.append(TMP_POSTFIX);
  }
}

/** Build partition string from partition and sub-partition name
@param[in]      part            partition name
@param[in]      sub_part        sub-partition name
@param[in]      conv            callback to convert partition/sub-partition name
@param[in]      is_57           if 5.7 style partition name is needed
@param[out]     partition       partition string for dictionary table name */
static void build_partition_low(const std::string part,
                                const std::string sub_part, Convert_Func conv,
                                bool is_57, std::string &partition) {
  partition.clear();
  std::string conv_str;

  if (part.empty()) {
    ut_d(ut_error);
    ut_o(return );
  }

  /* Get partition separator strings */
  std::string part_sep;
  std::string sub_part_sep;

  get_partition_separators(is_57, part_sep, sub_part_sep);

  /* Append separator and partition. */
  partition.append(part_sep);

  conv_str.assign(part);
  if (conv) {
    conv(conv_str);
  }
  partition.append(conv_str);

  if (sub_part.empty()) {
    return;
  }

  /* Append separator and sub-partition. */
  partition.append(sub_part_sep);

  conv_str.assign(sub_part);
  if (conv) {
    conv(conv_str);
  }
  partition.append(conv_str);
}

/** Convert string to lower case.
@param[in,out]  name    name to convert */
static void to_lower(std::string &name) {
  /* Skip empty string. */
  if (name.empty()) {
    return;
  }
  ut_ad(name.length() < FN_REFLEN);
  char conv_name[FN_REFLEN];
  auto len = name.copy(&conv_name[0], FN_REFLEN - 1);
  conv_name[len] = '\0';

  innobase_casedn_str(&conv_name[0]);
  name.assign(&conv_name[0]);
}

/** Get partition and sub-partition name from DD. We convert the names to
lower case.
@param[in]      dd_part         partition object from DD
@param[in]      lower_case      convert to lower case name
@param[out]     part_name       partition name
@param[out]     sub_name        sub-partition name */
static void get_part_from_dd(const dd::Partition *dd_part, bool lower_case,
                             std::string &part_name, std::string &sub_name) {
  /* Assume sub-partition and get the parent partition. */
  auto sub_part = dd_part;
  auto part = sub_part->parent();

  /* If parent is null then there is no sub-partition. */
  if (part == nullptr) {
    part = dd_part;
    sub_part = nullptr;
  }

  ut_ad(part->name().length() < FN_REFLEN);

  part_name.assign(part->name().c_str());
  /* Convert partition name to lower case. */
  if (lower_case) {
    to_lower(part_name);
  }

  sub_name.clear();
  if (sub_part != nullptr) {
    ut_ad(sub_part->name().length() < FN_REFLEN);

    sub_name.assign(sub_part->name().c_str());
    /* Convert sub-partition name to lower case. */
    if (lower_case) {
      to_lower(sub_name);
    }
  }
}

void build_partition(const dd::Partition *dd_part, std::string &partition) {
  std::string part_name;
  std::string sub_name;

  /* Extract partition and sub-partition name from DD. */
  get_part_from_dd(dd_part, true, part_name, sub_name);

  /* Build partition string after converting names. */
  build_partition_low(part_name, sub_name, table_to_file, false, partition);
}

void build_57_partition(const dd::Partition *dd_part, std::string &partition) {
  std::string part_name;
  std::string sub_name;

  /* Extract partition and sub-partition name from DD. In 5.7, partition and
  sub-partition names are kept in same letter case as given by user. */
  bool lower_case = false;

  /* On windows, 5.7 partition sub-partition names are in lower case always. */
#ifdef _WIN32
  lower_case = true;
#endif /* _WIN32 */

  get_part_from_dd(dd_part, lower_case, part_name, sub_name);

  /* Build partition string after converting names. */
  build_partition_low(part_name, sub_name, table_to_file, true, partition);
}

bool match_partition(const std::string &dict_name,
                     const dd::Partition *dd_part) {
  std::string dd_partition;

  /* Extract partition and sub-partition name from DD. */
  build_partition(dd_part, dd_partition);

  std::string schema;
  std::string table;
  bool is_tmp;
  std::string partition;

  /* Extract schema, table and partition string without conversion. */
  get_table(dict_name, false, schema, table, partition, is_tmp);

#ifdef UNIV_DEBUG
  /* Innodb dictionary name should already be in lower case. */
  ut_ad(partition.length() < FN_REFLEN);

  char partition_string[FN_REFLEN];
  auto part_len = partition.copy(&partition_string[0], FN_REFLEN - 1);
  partition_string[part_len] = '\0';

  innobase_casedn_path(&partition_string[0]);
  std::string lower_case_str(&partition_string[0]);

  ut_ad(partition.compare(lower_case_str) == 0);
#endif /* UNIV_DEBUG */

  /* Match the string from DD and innodb dictionary. */
  return (dd_partition.compare(partition) == 0);
}

/** Get table and partition string in system cs from dictionary name.
@param[in]      dict_name       table name in dictionary
@param[out]     schema          schema name
@param[out]     table           table name
@param[out]     partition       partition string
@param[out]     is_tmp          true, if temporary table created by DDL */
static void get_table_parts(const std::string &dict_name, std::string &schema,
                            std::string &table, std::string &partition,
                            bool &is_tmp) {
  /* Extract schema, table and partition string converting to system cs. */
  get_table(dict_name, true, schema, table, partition, is_tmp);

  if (!partition.empty()) {
    std::string part;
    std::string sub_part;

    /* Extract partition details converting to system cs. */
    get_partition(partition, true, part, sub_part);

    /* During upgrade from 5.7 it is possible to have upper case
    names from SYS tables. */
    if (srv_is_upgrade_mode) {
      to_lower(part);
      to_lower(sub_part);
    }

#ifdef UNIV_DEBUG
    /* Validate that the names are in lower case. */
    std::string save_part(part);
    to_lower(part);
    ut_ad(save_part.compare(part) == 0);

    std::string save_sub_part(sub_part);
    to_lower(sub_part);
    ut_ad(save_sub_part.compare(sub_part) == 0);
#endif  // UNIV_DEBUG

    /* Build partition string. No conversion required. */
    partition.clear();
    build_partition_low(part, sub_part, nullptr, false, partition);
  }
}

void convert_to_space(std::string &dict_name) {
  std::string schema;
  std::string table;
  std::string partition;
  bool is_tmp = false;

  /* Get all table parts converted to system cs. */
  get_table_parts(dict_name, schema, table, partition, is_tmp);

  /* For lower case file systems, schema and table name are converted
  to lower case before generating tablespace name. Skip for general
  table space i.e. schema is empty. */
  if (lower_case_file_system && !schema.empty()) {
    ut_ad(lower_case_table_names != 0);
    to_lower(schema);
    to_lower(table);
  }

  /* Build the space name. No conversion required. */
  dict_name.clear();
  build_table(schema, table, partition, is_tmp, false, dict_name);

  ut_ad(dict_name.length() < dict_name::MAX_SPACE_NAME_LEN);
}

void rebuild_space(const std::string &dict_name, std::string &space_name) {
  std::string schema;
  std::string table;
  std::string partition;
  bool is_tmp = false;

  /* Get all table parts converted to system cs. */
  get_table_parts(dict_name, schema, table, partition, is_tmp);

  if (is_tmp) {
    partition.append(TMP_POSTFIX);
  }

  auto part_len = partition.length();
  auto space_len = space_name.length();

  ut_ad(space_len > part_len);

  if (space_len > part_len) {
    auto part_pos = space_len - part_len;

    std::string space_part = space_name.substr(part_pos);
    if (space_part.compare(partition) == 0) {
      return;
    }
    space_name.replace(part_pos, std::string::npos, partition);
  }
}

void rebuild(std::string &dict_name) {
  std::string schema;
  std::string table;
  std::string partition;
  bool is_tmp = false;

  /* Conversion is needed only for partitioned table. */
  if (!is_partition(dict_name)) {
    return;
  }

  /* Extract schema, table and partition string without conversion. */
  get_table(dict_name, false, schema, table, partition, is_tmp);

  if (!partition.empty()) {
    std::string part;
    std::string sub_part;

    /* Extract partition details converting to system cs. */
    get_partition(partition, true, part, sub_part);

    /* Convert partition names to lower case. */
    to_lower(part);
    to_lower(sub_part);

    /* Build partition string converting to file cs. */
    partition.clear();
    build_partition_low(part, sub_part, table_to_file, false, partition);
  }

  /* Re-build the table name. No cs conversion required. */
  dict_name.clear();
  build_table(schema, table, partition, is_tmp, false, dict_name);
}

}  // namespace dict_name
#endif /* !UNIV_HOTBACKUP */
