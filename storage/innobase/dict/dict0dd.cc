/*****************************************************************************

Copyright (c) 2017, 2018, Oracle and/or its affiliates. All Rights Reserved.

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
#include <sql_backup_lock.h>
#include <sql_class.h>
#include <sql_thd_internal_api.h>
#else /* !UNIV_HOTBACKUP */
#include <my_base.h>
#endif /* !UNIV_HOTBACKUP */

#include <dd/properties.h>
#include "dict0crea.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "dict0mem.h"
#include "dict0priv.h"
#ifndef UNIV_HOTBACKUP
#include "dict0stats.h"
#endif /* !UNIV_HOTBACKUP */
#include "data0type.h"
#include "dict0dict.h"
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
#include "query_options.h"
#include "sql_base.h"
#include "sql_table.h"
#endif /* !UNIV_HOTBACKUP */

const char *DD_instant_col_val_coder::encode(const byte *stream, size_t in_len,
                                             size_t *out_len) {
  cleanup();

  m_result = UT_NEW_ARRAY_NOKEY(byte, in_len * 2);
  char *result = reinterpret_cast<char *>(m_result);

  for (size_t i = 0; i < in_len; ++i) {
    uint8_t v1 = ((stream[i] & 0xF0) >> 4);
    uint8_t v2 = (stream[i] & 0x0F);

    result[i * 2] = (v1 < 10 ? '0' + v1 : 'a' + v1 - 10);
    result[i * 2 + 1] = (v2 < 10 ? '0' + v2 : 'a' + v2 - 10);
  }

  *out_len = in_len * 2;

  return (result);
}

const byte *DD_instant_col_val_coder::decode(const char *stream, size_t in_len,
                                             size_t *out_len) {
  ut_ad(in_len % 2 == 0);

  cleanup();

  m_result = UT_NEW_ARRAY_NOKEY(byte, in_len / 2);

  for (size_t i = 0; i < in_len / 2; ++i) {
    char c1 = stream[i * 2];
    char c2 = stream[i * 2 + 1];

    ut_ad(isdigit(c1) || (c1 >= 'a' && c1 <= 'f'));
    ut_ad(isdigit(c2) || (c2 >= 'a' && c2 <= 'f'));

    m_result[i] = ((isdigit(c1) ? c1 - '0' : c1 - 'a' + 10) << 4) +
                  ((isdigit(c2) ? c2 - '0' : c2 - 'a' + 10));
  }

  *out_len = in_len / 2;

  return (m_result);
}

#ifndef UNIV_HOTBACKUP
/** Check if the InnoDB index is consistent with dd::Index
@param[in]	index		InnoDB index
@param[in]	dd_index	dd::Index or dd::Partition_index
@return	true	if match
@retval	false	if not match */
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
  uint64 id;
  uint32 root;
  uint64 trx_id;
  ut_ad(p.exists(dd_index_key_strings[DD_INDEX_ID]));
  p.get_uint64(dd_index_key_strings[DD_INDEX_ID], &id);
  if (id != index->id) {
    ib::warn(ER_IB_MSG_163)
        << "Index id in InnoDB is " << index->id << " while index id in"
        << " global DD is " << id;
    match = false;
  }

  ut_ad(p.exists(dd_index_key_strings[DD_INDEX_ROOT]));
  p.get_uint32(dd_index_key_strings[DD_INDEX_ROOT], &root);
  if (root != index->page) {
    ib::warn(ER_IB_MSG_164)
        << "Index root in InnoDB is " << index->page << " while index root in"
        << " global DD is " << root;
    match = false;
  }

  ut_ad(p.exists(dd_index_key_strings[DD_INDEX_TRX_ID]));
  p.get_uint64(dd_index_key_strings[DD_INDEX_TRX_ID], &trx_id);
  /* For DD tables, the trx_id=0 is got from get_se_private_id().
  TODO: index->trx_id is not expected to be 0 once Bug#25730513 is fixed*/
  if (trx_id != 0 && index->trx_id != 0 && trx_id != index->trx_id) {
    ib::warn(ER_IB_MSG_165) << "Index transaction id in InnoDB is "
                            << index->trx_id << " while index transaction"
                            << " id in global DD is " << trx_id;
    match = false;
  }

  return (match);
}

/** Check if the InnoDB table is consistent with dd::Table
@param[in]	table			InnoDB table
@param[in]	dd_table		dd::Table or dd::Partition
@return	true	if match
@retval	false	if not match */
template <typename Table>
bool dd_table_match(const dict_table_t *table, const Table *dd_table) {
  /* Temporary table has no metadata written */
  if (dd_table == nullptr || table->is_temporary()) {
    return (true);
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
    return (match);
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
  return (match);
}

template bool dd_table_match<dd::Table>(const dict_table_t *,
                                        const dd::Table *);
template bool dd_table_match<dd::Partition>(const dict_table_t *,
                                            const dd::Partition *);

/** Release a metadata lock.
@param[in,out]	thd	current thread
@param[in,out]	mdl	metadata lock */
void dd_mdl_release(THD *thd, MDL_ticket **mdl) {
  if (*mdl == nullptr) {
    return;
  }

  dd::release_mdl(thd, *mdl);
  *mdl = nullptr;
}

/** Check if current undo needs a MDL or not
@param[in]	trx	transaction
@return true if MDL is necessary, otherwise false */
bool dd_mdl_for_undo(const trx_t *trx) {
  /* Try best to find a valid THD for checking, in case in background
  rollback thread, trx doens't hold a mysql_thd */
  THD *thd = trx->mysql_thd == nullptr ? current_thd : trx->mysql_thd;

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

/** Instantiate an InnoDB in-memory table metadata (dict_table_t)
based on a Global DD object.
@param[in,out]	client		data dictionary client
@param[in]	dd_table	Global DD table object
@param[in]	dd_part		Global DD partition or subpartition, or NULL
@param[in]	tbl_name	table name, or NULL if not known
@param[out]	table		InnoDB table (NULL if not found or loadable)
@param[in]	thd		Thread THD
@return	error code
@retval	0	on success */
int dd_table_open_on_dd_obj(dd::cache::Dictionary_client *client,
                            const dd::Table &dd_table,
                            const dd::Partition *dd_part, const char *tbl_name,
                            dict_table_t *&table, THD *thd) {
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
  const ulint fold = ut_fold_ull(table_id);

  ut_ad(table_id != dd::INVALID_OBJECT_ID);

  mutex_enter(&dict_sys->mutex);

  HASH_SEARCH(id_hash, dict_sys->table_id_hash, fold, dict_table_t *, table,
              ut_ad(table->cached), table->id == table_id);

  if (table != nullptr) {
    table->acquire();
  }

  mutex_exit(&dict_sys->mutex);

  if (table != nullptr) {
    return (0);
  }

#ifdef UNIV_DEBUG
  /* If this is a internal temporary table, it's impossible
  to verify the MDL against the table name, because both the
  database name and table name may be invalid for MDL */
  if (tbl_name && !row_is_mysql_tmp_table_name(tbl_name)) {
    char db_buf[MAX_DATABASE_NAME_LEN + 1];
    char tbl_buf[MAX_TABLE_NAME_LEN + 1];

    dd_parse_tbl_name(tbl_name, db_buf, tbl_buf, nullptr, nullptr, nullptr);
    ut_ad(innobase_strcasecmp(dd_table.name().c_str(), tbl_buf) == 0);
  }
#endif /* UNIV_DEBUG */

  TABLE_SHARE ts;
  dd::Schema *schema;
  const char *table_cache_key;
  size_t table_cache_key_len;

  if (tbl_name != nullptr) {
    schema = nullptr;
    table_cache_key = tbl_name;
    table_cache_key_len = dict_get_db_name_len(tbl_name);
  } else {
    error = client->acquire_uncached<dd::Schema>(dd_table.schema_id(), &schema);

    if (error != 0) {
      return (error);
    }

    table_cache_key = schema->name().c_str();
    table_cache_key_len = schema->name().size();
  }

  init_tmp_table_share(thd, &ts, table_cache_key, table_cache_key_len,
                       dd_table.name().c_str(), "" /* file name */, nullptr);

  error = open_table_def_suppress_invalid_meta_data(thd, &ts, dd_table);

  if (error == 0) {
    TABLE td;

    error = open_table_from_share(thd, &ts, dd_table.name().c_str(), 0,
                                  OPEN_FRM_FILE_ONLY, 0, &td, false, &dd_table);
    if (error == 0) {
      char tmp_name[MAX_FULL_NAME_LEN + 1];
      const char *tab_namep;

      if (tbl_name) {
        tab_namep = tbl_name;
      } else {
        char tmp_schema[MAX_DATABASE_NAME_LEN + 1];
        char tmp_tablename[MAX_TABLE_NAME_LEN + 1];
        tablename_to_filename(schema->name().c_str(), tmp_schema,
                              MAX_DATABASE_NAME_LEN + 1);
        tablename_to_filename(dd_table.name().c_str(), tmp_tablename,
                              MAX_TABLE_NAME_LEN + 1);
        snprintf(tmp_name, sizeof tmp_name, "%s/%s", tmp_schema, tmp_tablename);
        tab_namep = tmp_name;
      }

      if (dd_part == nullptr) {
        table = dd_open_table(client, &td, tab_namep, &dd_table, thd);
      } else {
        table = dd_open_table(client, &td, tab_namep, dd_part, thd);
      }

      closefrm(&td, false);
    }
  }

  free_table_share(&ts);

  return (error);
}

/** Load an InnoDB table definition by InnoDB table ID.
@param[in,out]	thd		current thread
@param[in,out]	mdl		metadata lock;
nullptr if we are resurrecting table IX locks in recovery
@param[in]	table_id	InnoDB table or partition ID
@return	InnoDB table
@retval	nullptr	if the table is not found, or there was an error */
static dict_table_t *dd_table_open_on_id_low(THD *thd, MDL_ticket **mdl,
                                             table_id_t table_id) {
  char part_name[FN_REFLEN * 2];
  const char *name_to_open = nullptr;

  ut_ad(thd == nullptr || thd == current_thd);
#ifdef UNIV_DEBUG
  btrsea_sync_check check(false);
  ut_ad(!sync_check_iterate(check));
#endif
  ut_ad(!srv_is_being_shutdown);

  if (thd == nullptr) {
    ut_ad(mdl == nullptr);
    thd = current_thd;
  }

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
      return (nullptr);
    }

    const bool not_table = schema.empty();

    if (not_table) {
      if (dc->get_table_name_by_partition_se_private_id(handler_name, table_id,
                                                        &schema, &tablename) ||
          schema.empty()) {
        return (nullptr);
      }
    }

    /* Now we have tablename, and MDL locked it if necessary. */
    if (mdl != nullptr) {
      if (*mdl == nullptr &&
          dd_mdl_acquire(thd, mdl, schema.c_str(), tablename.c_str())) {
        return (nullptr);
      }

      ut_ad(*mdl != nullptr);
    }

    if (dc->acquire(schema, tablename, &dd_table) || dd_table == nullptr) {
      if (mdl != nullptr) {
        dd_mdl_release(thd, mdl);
      }
      return (nullptr);
    }

    const bool is_part = dd_table_is_partitioned(*dd_table);

    /* Verify facts between dd_table and facts we know
    1) Partiton table or not
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
        size_t name_len;

        dd_part = *i;
        ut_ad(dd_part_is_stored(dd_part));
        /* For partition, we need to compose the
        name. */
        char tmp_schema[MAX_DATABASE_NAME_LEN + 1];
        char tmp_tablename[MAX_TABLE_NAME_LEN + 1];
        tablename_to_filename(schema.c_str(), tmp_schema,
                              MAX_DATABASE_NAME_LEN + 1);
        tablename_to_filename(tablename.c_str(), tmp_tablename,
                              MAX_TABLE_NAME_LEN + 1);
        snprintf(part_name, sizeof part_name, "%s/%s", tmp_schema,
                 tmp_tablename);
        name_len = strlen(part_name);
        Ha_innopart_share::create_partition_postfix(
            part_name + name_len, FN_REFLEN - name_len, dd_part);
        name_to_open = part_name;
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

  dict_table_t *ib_table = nullptr;

  dd_table_open_on_dd_obj(dc, *dd_table, dd_part, name_to_open, ib_table, thd);

  if (mdl && ib_table == nullptr) {
    dd_mdl_release(thd, mdl);
  }

  return (ib_table);
}
#endif /* !UNIV_HOTBACKUP */

/** Check if access to a table should be refused.
@param[in,out]	table	InnoDB table or partition
@return	error code
@retval	0	on success */
static MY_ATTRIBUTE((warn_unused_result)) int dd_check_corrupted(
    dict_table_t *&table) {
  if (table->is_corrupted()) {
    if (dict_table_is_sdi(table->id)
#ifndef UNIV_HOTBACKUP
        || dict_table_is_system(table->id)
#endif /* !UNIV_HOTBACKUP */
    ) {
#ifndef UNIV_HOTBACKUP
      my_error(ER_TABLE_CORRUPT, MYF(0), "", table->name.m_name);
#else  /* !UNIV_HOTBACKUP */
      ib::fatal(ER_IB_MSG_168) << "table is corrupt: " << table->name.m_name;
#endif /* !UNIV_HOTBACKUP */
    } else {
      char db_buf[MAX_DATABASE_NAME_LEN + 1];
      char tbl_buf[MAX_TABLE_NAME_LEN + 1];

#ifndef UNIV_HOTBACKUP
      dd_parse_tbl_name(table->name.m_name, db_buf, tbl_buf, nullptr, nullptr,
                        nullptr);
      my_error(ER_TABLE_CORRUPT, MYF(0), db_buf, tbl_buf);
#else  /* !UNIV_HOTBACKUP */
      ib::fatal(ER_IB_MSG_169) << "table is corrupt: " << table->name.m_name;
#endif /* !UNIV_HOTBACKUP */
    }
    table = nullptr;
    return (HA_ERR_TABLE_CORRUPT);
  }

  dict_index_t *index = table->first_index();
  if (!dict_table_is_sdi(table->id) && fil_space_get(index->space) == nullptr) {
#ifndef UNIV_HOTBACKUP
    my_error(ER_TABLESPACE_MISSING, MYF(0), table->name.m_name);
#else  /* !UNIV_HOTBACKUP */
    ib::fatal(ER_IB_MSG_170)
        << "table space is missing: " << table->name.m_name;
#endif /* !UNIV_HOTBACKUP */
    table = nullptr;
    return (HA_ERR_TABLESPACE_MISSING);
  }

  /* Ignore missing tablespaces for secondary indexes. */
  while ((index = index->next())) {
    if (!index->is_corrupted() && fil_space_get(index->space) == nullptr) {
      dict_set_corrupted(index);
    }
  }

  return (0);
}

/** Open a persistent InnoDB table based on InnoDB table id, and
hold Shared MDL lock on it.
@param[in]	table_id		table identifier
@param[in,out]	thd			current MySQL connection (for mdl)
@param[in,out]	mdl			metadata lock (*mdl set if
table_id was found)
@param[in]	dict_locked		dict_sys mutex is held
@param[in]	check_corruption	check if the table is corrupted or not.
mdl=NULL if we are resurrecting table IX locks in recovery
@return table
@retval NULL if the table does not exist or cannot be opened */
dict_table_t *dd_table_open_on_id(table_id_t table_id, THD *thd,
                                  MDL_ticket **mdl, bool dict_locked,
                                  bool check_corruption) {
  dict_table_t *ib_table;
  const ulint fold = ut_fold_ull(table_id);
  char db_buf[MAX_DATABASE_NAME_LEN + 1];
  char tbl_buf[MAX_TABLE_NAME_LEN + 1];
  char full_name[MAX_FULL_NAME_LEN + 1];

  if (!dict_locked) {
    mutex_enter(&dict_sys->mutex);
  }

  HASH_SEARCH(id_hash, dict_sys->table_id_hash, fold, dict_table_t *, ib_table,
              ut_ad(ib_table->cached), ib_table->id == table_id);

reopen:
  if (ib_table == nullptr) {
#ifndef UNIV_HOTBACKUP
    if (dict_table_is_sdi(table_id)) {
      /* The table is SDI table */
      space_id_t space_id = dict_sdi_get_space_id(table_id);

      /* Create in-memory table oject for SDI table */
      dict_index_t *sdi_index =
          dict_sdi_create_idx_in_mem(space_id, false, 0, false);

      if (sdi_index == nullptr) {
        if (!dict_locked) {
          mutex_exit(&dict_sys->mutex);
        }
        return (nullptr);
      }

      ib_table = sdi_index->table;

      ut_ad(ib_table != nullptr);
      ib_table->acquire();

      if (!dict_locked) {
        mutex_exit(&dict_sys->mutex);
      }
    } else {
      mutex_exit(&dict_sys->mutex);

      ib_table = dd_table_open_on_id_low(thd, mdl, table_id);

      if (dict_locked) {
        mutex_enter(&dict_sys->mutex);
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
      mutex_exit(&dict_sys->mutex);
    }
  } else {
    for (;;) {
#ifndef UNIV_HOTBACKUP
      bool ret = dd_parse_tbl_name(ib_table->name.m_name, db_buf, tbl_buf,
                                   nullptr, nullptr, nullptr);
#endif /* !UNIV_HOTBACKUP */

      memset(full_name, 0, MAX_FULL_NAME_LEN + 1);

      strcpy(full_name, ib_table->name.m_name);

      ut_ad(!ib_table->is_temporary());

      mutex_exit(&dict_sys->mutex);

#ifndef UNIV_HOTBACKUP
      if (ret == false) {
        if (dict_locked) {
          mutex_enter(&dict_sys->mutex);
        }
        return (nullptr);
      }

      if (dd_mdl_acquire(thd, mdl, db_buf, tbl_buf)) {
        if (dict_locked) {
          mutex_enter(&dict_sys->mutex);
        }
        return (nullptr);
      }
#endif /* !UNIV_HOTBACKUP */

      /* Re-lookup the table after acquiring MDL. */
      mutex_enter(&dict_sys->mutex);

      HASH_SEARCH(id_hash, dict_sys->table_id_hash, fold, dict_table_t *,
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

      mutex_exit(&dict_sys->mutex);
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
      mutex_enter(&dict_sys->mutex);
    }
  }

  ut_ad(dict_locked == mutex_own(&dict_sys->mutex));

  return (ib_table);
}

#ifndef UNIV_HOTBACKUP
/** Set the discard flag for a non-partitioned dd table.
@param[in,out]	thd		current thread
@param[in]	table		InnoDB table
@param[in,out]	table_def	MySQL dd::Table to update
@param[in]	discard		discard flag
@return	true	if success
@retval false if fail. */
bool dd_table_discard_tablespace(THD *thd, const dict_table_t *table,
                                 dd::Table *table_def, bool discard) {
  bool ret = false;

  DBUG_ENTER("dd_table_set_discard_flag");

  ut_ad(thd == current_thd);
#ifdef UNIV_DEBUG
  btrsea_sync_check check(false);
  ut_ad(!sync_check_iterate(check));
#endif /* UNIV_DEBUG */

  ut_ad(!srv_is_being_shutdown);

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
      p.set_uint32(dd_index_key_strings[DD_INDEX_ROOT], index->page);
    }

    /* Set new table id for dd columns when it's importing
    tablespace. */
    if (!discard) {
      for (auto dd_column : *table_def->columns()) {
        dd_column->se_private_data().set_uint64(
            dd_index_key_strings[DD_TABLE_ID], table->id);
      }
    }

    /* Set discard flag. */
    dd::Properties &p = table_def->se_private_data();
    p.set_bool(dd_table_key_strings[DD_TABLE_DISCARD], discard);

    using Client = dd::cache::Dictionary_client;
    using Releaser = dd::cache::Dictionary_client::Auto_releaser;

    /* Get Tablespace object */
    dd::Tablespace *dd_space = nullptr;
    Client *client = dd::get_dd_client(thd);
    Releaser releaser{client};

    dd::Object_id dd_space_id =
        (*table_def->indexes()->begin())->tablespace_id();

    std::string space_name;

    dd_filename_to_spacename(table->name.m_name, &space_name);

    if (dd::acquire_exclusive_tablespace_mdl(thd, space_name.c_str(), false)) {
      ut_a(false);
    }

    if (client->acquire_for_modification(dd_space_id, &dd_space)) {
      ut_a(false);
    }

    ut_a(dd_space != NULL);

    dd_tablespace_set_discard(dd_space, discard);

    if (client->update(dd_space)) {
      ut_ad(0);
    }
    ret = true;
  } else {
    ret = false;
  }

  DBUG_RETURN(ret);
}

/** Open an internal handle to a persistent InnoDB table by name.
@param[in,out]	thd		current thread
@param[out]	mdl		metadata lock
@param[in]	name		InnoDB table name
@param[in]	dict_locked	has dict_sys mutex locked
@param[in]	ignore_err	whether to ignore err
@return handle to non-partitioned table
@retval NULL if the table does not exist */
dict_table_t *dd_table_open_on_name(THD *thd, MDL_ticket **mdl,
                                    const char *name, bool dict_locked,
                                    ulint ignore_err) {
  DBUG_ENTER("dd_table_open_on_name");

#ifdef UNIV_DEBUG
  btrsea_sync_check check(false);
  ut_ad(!sync_check_iterate(check));
#endif
  ut_ad(!srv_is_being_shutdown);

  char db_buf[MAX_DATABASE_NAME_LEN + 1];
  char tbl_buf[MAX_TABLE_NAME_LEN + 1];
  char part_buf[MAX_TABLE_NAME_LEN + 1];
  char sub_buf[MAX_TABLE_NAME_LEN + 1];
  bool skip_mdl = !(thd && mdl);
  dict_table_t *table = nullptr;

  /* Get pointer to a table object in InnoDB dictionary cache.
  For intrinsic table, get it from session private data */
  if (thd) {
    table = thd_to_innodb_session(thd)->lookup_table_handler(name);
  }

  if (table != nullptr) {
    table->acquire();
    DBUG_RETURN(table);
  }

  db_buf[0] = tbl_buf[0] = part_buf[0] = sub_buf[0] = '\0';
  if (!dd_parse_tbl_name(name, db_buf, tbl_buf, part_buf, sub_buf, nullptr)) {
    DBUG_RETURN(nullptr);
  }

  if (!skip_mdl && dd_mdl_acquire(thd, mdl, db_buf, tbl_buf)) {
    DBUG_RETURN(nullptr);
  }

  if (!dict_locked) {
    mutex_enter(&dict_sys->mutex);
  }

  table = dict_table_check_if_in_cache_low(name);

  if (table != nullptr) {
    table->acquire_with_lock();
    if (!dict_locked) {
      mutex_exit(&dict_sys->mutex);
    }
    DBUG_RETURN(table);
  }

  mutex_exit(&dict_sys->mutex);

  const dd::Table *dd_table = nullptr;
  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  if (client->acquire(db_buf, tbl_buf, &dd_table) || dd_table == nullptr ||
      dd_table->engine() != innobase_hton_name) {
    /* The checking for engine should be only useful(valid)
    for getting table statistics for IS. Two relevant API
    functions are:
    1. innobase_get_table_statistics
    2. innobase_get_index_column_cardinality */
    table = nullptr;
  } else {
    if (dd_table->se_private_id() == dd::INVALID_OBJECT_ID) {
      ut_ad(!dd_table->leaf_partitions().empty());
      if (strlen(part_buf) != 0) {
        const dd::Partition *dd_part = nullptr;
        for (auto part : dd_table->leaf_partitions()) {
          if (part->parent() != nullptr) {
            ut_ad(strlen(sub_buf) != 0);
            if (part->name() == sub_buf && part->parent()->name() == part_buf) {
              dd_part = part;
              break;
            }
          } else if (part->name() == part_buf) {
            dd_part = part;
            break;
          }
        }

        ut_ad(dd_part != nullptr);
        dd_table_open_on_dd_obj(client, *dd_table, dd_part, name, table, thd);
      } else {
        /* FIXME: Once FK functions will not open
        partitioned table in current improper way,
        just assert this false */
        table = nullptr;
      }
    } else {
      ut_ad(dd_table->leaf_partitions().empty());
      dd_table_open_on_dd_obj(client, *dd_table, nullptr, name, table, thd);
    }
  }

  if (table && table->is_corrupted() &&
      !(ignore_err & DICT_ERR_IGNORE_CORRUPT)) {
    mutex_enter(&dict_sys->mutex);
    table->release();
    dict_table_remove_from_cache(table);
    table = nullptr;
    mutex_exit(&dict_sys->mutex);
  }

  if (table == nullptr && mdl) {
    dd_mdl_release(thd, mdl);
    *mdl = nullptr;
  }

  if (dict_locked) {
    mutex_enter(&dict_sys->mutex);
  }

  DBUG_RETURN(table);
}
#endif /* !UNIV_HOTBACKUP */

/** Close an internal InnoDB table handle.
@param[in,out]	table		InnoDB table handle
@param[in,out]	thd		current MySQL connection (for mdl)
@param[in,out]	mdl		metadata lock (will be set NULL)
@param[in]	dict_locked	whether we hold dict_sys mutex */
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
/** Update filename of dd::Tablespace
@param[in]	dd_space_id	DD tablespace id
@param[in]	new_space_name	New tablespace name
@param[in]	new_path	New data file path
@retval DB_SUCCESS on success. */
dberr_t dd_rename_tablespace(dd::Object_id dd_space_id,
                             const char *new_space_name, const char *new_path) {
  THD *thd = current_thd;
  std::string tablespace_name;

  DBUG_ENTER("dd_rename_tablespace");
#ifdef UNIV_DEBUG
  btrsea_sync_check check(false);
  ut_ad(!sync_check_iterate(check));
#endif /* UNIV_DEBUG */
  ut_ad(!srv_is_being_shutdown);

  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  dd::Tablespace *dd_space = nullptr;

  /* Get the dd tablespace */
  if (client->acquire_uncached_uncommitted<dd::Tablespace>(dd_space_id,
                                                           &dd_space)) {
    ut_ad(false);
    DBUG_RETURN(DB_ERROR);
  }

  ut_a(dd_space != nullptr);
  MDL_ticket *src_ticket = nullptr;
  if (dd::acquire_exclusive_tablespace_mdl(thd, dd_space->name().c_str(), false,
                                           &src_ticket)) {
    ut_ad(false);
    DBUG_RETURN(DB_ERROR);
  }

  dd_filename_to_spacename(new_space_name, &tablespace_name);
  MDL_ticket *dst_ticket = nullptr;
  if (dd::acquire_exclusive_tablespace_mdl(thd, tablespace_name.c_str(), false,
                                           &dst_ticket)) {
    ut_ad(false);
    DBUG_RETURN(DB_ERROR);
  }

  dd::Tablespace *new_space = nullptr;

  /* Acquire the new dd tablespace for modification */
  if (client->acquire_for_modification<dd::Tablespace>(dd_space_id,
                                                       &new_space)) {
    ut_ad(false);
    DBUG_RETURN(DB_ERROR);
  }

  ut_ad(new_space->files().size() == 1);

  new_space->set_name(tablespace_name.c_str());

  if (new_path != nullptr) {
    dd::Tablespace_file *dd_file =
        const_cast<dd::Tablespace_file *>(*(new_space->files().begin()));

    dd_file->set_filename(new_path);

  } else {
#ifdef UNIV_DEBUG
    const dd::Properties &p = dd_space->se_private_data();
    bool is_discarded = false;
    ut_ad(p.exists(dd_space_key_strings[DD_SPACE_DISCARD]));
    p.get_bool(dd_space_key_strings[DD_SPACE_DISCARD], &is_discarded);
    ut_ad(is_discarded);
#endif /* UNIV_DEBUG */
  }

  bool fail = client->update(new_space);
  ut_ad(!fail);
  dd::rename_tablespace_mdl_hook(thd, src_ticket, dst_ticket);
  DBUG_RETURN(fail ? DB_ERROR : DB_SUCCESS);
}

/** Validate the table format options.
@param[in]	thd		THD instance
@param[in]	form		MySQL table definition
@param[in]	real_type	real row type if it's not ROW_TYPE_NOT_USED
@param[in]	zip_allowed	whether ROW_FORMAT=COMPRESSED is OK
@param[in]	strict		whether innodb_strict_mode=ON
@param[out]	is_redundant	whether ROW_FORMAT=REDUNDANT
@param[out]	blob_prefix	whether ROW_FORMAT=DYNAMIC
                                or ROW_FORMAT=COMPRESSED
@param[out]	zip_ssize	log2(compressed page size),
                                or 0 if not ROW_FORMAT=COMPRESSED
@param[out]	is_implicit	if tablespace is implicit
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
                            ER_DEFAULT(error), innobase_hton_name, kbs,
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
                            ER_DEFAULT(error), innobase_hton_name, kbs,
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
      /* fall through */
    case ROW_TYPE_DEFAULT:
      switch (real_type) {
        case ROW_TYPE_FIXED:
        case ROW_TYPE_PAGED:
        case ROW_TYPE_NOT_USED:
        case ROW_TYPE_DEFAULT:
          /* get_real_row_type() should not return these */
          ut_ad(0);
          /* fall through */
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
      /* fall through */
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
      my_error(ER_WRONG_VALUE, MYF(0), "COMPRESSION", algorithm);
      invalid = true;
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
      return (true);
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

  return (invalid);
}

/** Set the AUTO_INCREMENT attribute.
@param[in,out]	se_private_data		dd::Table::se_private_data
@param[in]	autoinc			the auto-increment value */
void dd_set_autoinc(dd::Properties &se_private_data, uint64 autoinc) {
  /* The value of "autoinc" here is the AUTO_INCREMENT attribute
  specified at table creation. AUTO_INCREMENT=0 will silently
  be treated as AUTO_INCREMENT=1. Likewise, if no AUTO_INCREMENT
  attribute was specified, the value would be 0. */

  if (autoinc > 0) {
    /* InnoDB persists the "previous" AUTO_INCREMENT value. */
    autoinc--;
  }

  uint64 version = 0;

  if (se_private_data.exists(dd_table_key_strings[DD_TABLE_AUTOINC])) {
    /* Increment the dynamic metadata version, so that
    any previously buffered persistent dynamic metadata
    will be ignored after this transaction commits. */

    if (!se_private_data.get_uint64(dd_table_key_strings[DD_TABLE_VERSION],
                                    &version)) {
      version++;
    } else {
      /* incomplete se_private_data */
      ut_ad(false);
    }
  }

  se_private_data.set_uint64(dd_table_key_strings[DD_TABLE_VERSION], version);
  se_private_data.set_uint64(dd_table_key_strings[DD_TABLE_AUTOINC], autoinc);
}

/** Copy the AUTO_INCREMENT and version attribute if exist.
@param[in]	src	dd::Table::se_private_data to copy from
@param[out]	dest	dd::Table::se_private_data to copy to */
void dd_copy_autoinc(const dd::Properties &src, dd::Properties &dest) {
  uint64_t autoinc = 0;
  uint64_t version = 0;

  if (!src.exists(dd_table_key_strings[DD_TABLE_AUTOINC])) {
    return;
  }

  if (src.get_uint64(dd_table_key_strings[DD_TABLE_AUTOINC],
                     reinterpret_cast<uint64 *>(&autoinc)) ||
      src.get_uint64(dd_table_key_strings[DD_TABLE_VERSION],
                     reinterpret_cast<uint64 *>(&version))) {
    ut_ad(0);
    return;
  }

  dest.set_uint64(dd_table_key_strings[DD_TABLE_VERSION], version);
  dest.set_uint64(dd_table_key_strings[DD_TABLE_AUTOINC], autoinc);
}

/** Copy the metadata of a table definition if there was an instant
ADD COLUMN happened. This should be done when it's not an ALTER TABLE
with rebuild.
@param[in,out]	new_table	New table definition
@param[in]	old_table	Old table definition */
void dd_copy_instant_n_cols(dd::Table &new_table, const dd::Table &old_table) {
  ut_ad(dd_table_has_instant_cols(old_table));

  if (!dd_table_has_instant_cols(new_table)) {
    uint32_t cols;
    old_table.se_private_data().get_uint32(
        dd_table_key_strings[DD_TABLE_INSTANT_COLS], &cols);
    new_table.se_private_data().set_uint32(
        dd_table_key_strings[DD_TABLE_INSTANT_COLS], cols);
  }
#ifdef UNIV_DEBUG
  else {
    uint32_t old_cols, new_cols;
    old_table.se_private_data().get_uint32(
        dd_table_key_strings[DD_TABLE_INSTANT_COLS], &old_cols);
    new_table.se_private_data().get_uint32(
        dd_table_key_strings[DD_TABLE_INSTANT_COLS], &new_cols);
    ut_ad(old_cols == new_cols);
  }
#endif /* UNIV_DEBUG */
}

/** Copy the engine-private parts of a table or partition definition
when the change does not affect InnoDB. This mainly copies the common
private data between dd::Table and dd::Partition
@tparam		Table		dd::Table or dd::Partition
@param[in,out]	new_table	Copy of old table or partition definition
@param[in]	old_table	Old table or partition definition */
template <typename Table>
void dd_copy_private(Table &new_table, const Table &old_table) {
  uint64 autoinc = 0;
  uint64 version = 0;
  bool reset = false;
  dd::Properties &se_private_data = new_table.se_private_data();

  /* AUTOINC metadata could be set at the beginning for
  non-partitioned tables. So already set metadata should be kept */
  if (se_private_data.exists(dd_table_key_strings[DD_TABLE_AUTOINC])) {
    se_private_data.get_uint64(dd_table_key_strings[DD_TABLE_AUTOINC],
                               &autoinc);
    se_private_data.get_uint64(dd_table_key_strings[DD_TABLE_VERSION],
                               &version);
    reset = true;
  }

  new_table.se_private_data().clear();

  new_table.set_se_private_id(old_table.se_private_id());
  new_table.set_se_private_data(old_table.se_private_data());

  if (reset) {
    se_private_data.set_uint64(dd_table_key_strings[DD_TABLE_VERSION], version);
    se_private_data.set_uint64(dd_table_key_strings[DD_TABLE_AUTOINC], autoinc);
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

    dd::Properties &new_options = new_index->options();
    new_options.clear();
    new_options.assign(old_index->options());
  }

  new_table.table().set_row_format(old_table.table().row_format());
  dd::Properties &new_options = new_table.options();
  new_options.clear();
  new_options.assign(old_table.options());
}

template void dd_copy_private<dd::Table>(dd::Table &, const dd::Table &);
template void dd_copy_private<dd::Partition>(dd::Partition &,
                                             const dd::Partition &);

/** Copy the engine-private parts of column definitions of a table.
@param[in,out]	new_table	Copy of old table
@param[in]	old_table	Old table */
void dd_copy_table_columns(dd::Table &new_table, const dd::Table &old_table) {
  /* Columns in new table maybe more than old tables, when this is
  called for adding instant columns. Also adding and dropping
  virtual columns instantly is another case. */
  for (const auto old_col : old_table.columns()) {
    dd::Column *new_col = const_cast<dd::Column *>(
        dd_find_column(&new_table, old_col->name().c_str()));

    if (new_col == nullptr) {
      ut_ad(old_col->is_virtual());
      continue;
    }

    if (!old_col->se_private_data().empty()) {
      new_col->se_private_data().clear();
      new_col->se_private_data().assign(old_col->se_private_data());
    }
  }
}

void dd_part_adjust_table_id(dd::Table *new_table) {
  ut_ad(dd_table_is_partitioned(*new_table));

  auto part = new_table->leaf_partitions()->begin();
  table_id_t table_id = (*part)->se_private_id();

  for (auto dd_column : *new_table->table().columns()) {
    dd_column->se_private_data().set_uint64(dd_index_key_strings[DD_TABLE_ID],
                                            table_id);
  }
}

/** Clear the instant ADD COLUMN information of a table
@param[in,out]	dd_table	dd::Table */
void dd_clear_instant_table(dd::Table &dd_table) {
  ut_ad(dd_table_has_instant_cols(dd_table));
  dd_table.se_private_data().remove(
      dd_table_key_strings[DD_TABLE_INSTANT_COLS]);

  ut_d(bool found = false);
  for (auto col : *dd_table.columns()) {
    dd::Properties &col_private = col->se_private_data();
    if (col_private.exists(
            dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT_NULL])) {
      ut_d(found = true);
      col_private.remove(dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT_NULL]);
    } else if (col_private.exists(
                   dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT])) {
      ut_d(found = true);
      col_private.remove(dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT]);
    }
  }

  ut_ad(found);
}

/** Clear the instant ADD COLUMN information of a partition, to make it
as a normal partition
@param[in,out]	dd_part		dd::Partition */
void dd_clear_instant_part(dd::Partition &dd_part) {
  ut_ad(dd_part_has_instant_cols(dd_part));

  dd_part.se_private_data().remove(
      dd_partition_key_strings[DD_PARTITION_INSTANT_COLS]);
}

#ifdef UNIV_DEBUG
bool dd_instant_columns_exist(const dd::Table &dd_table) {
  uint32_t n_cols = 0;
  uint32_t non_instant_cols = 0;
  bool found = false;

  ut_ad(dd_table.se_private_data().exists(
      dd_table_key_strings[DD_TABLE_INSTANT_COLS]));

  dd_table.se_private_data().get_uint32(
      dd_table_key_strings[DD_TABLE_INSTANT_COLS], &n_cols);

  for (auto col : dd_table.columns()) {
    if (col->is_virtual() || col->is_se_hidden()) {
      continue;
    }

    const dd::Properties &col_private = col->se_private_data();
    if (col_private.exists(
            dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT_NULL]) ||
        col_private.exists(dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT])) {
      found = true;
      continue;
    }

    ++non_instant_cols;
  }

  ut_ad(found);
  /* Please note that n_cols could be 0 if the table only had some virtual
  columns before instant ADD COLUMN. So below check should be sufficient */
  ut_ad(non_instant_cols == n_cols);
  return (found && non_instant_cols == n_cols);
}
#endif /* UNIV_DEBUG */

/** Add column default values for new instantly added columns
@param[in]	old_table	MySQL table as it is before the ALTER operation
@param[in]	altered_table	MySQL table that is being altered
@param[in,out]	new_dd_table	New dd::Table
@param[in]	new_table	New InnoDB table object */
void dd_add_instant_columns(const TABLE *old_table, const TABLE *altered_table,
                            dd::Table *new_dd_table,
                            const dict_table_t *new_table) {
  ut_ad(altered_table->s->fields > old_table->s->fields);

#ifdef UNIV_DEBUG
  for (uint32_t i = 0; i < old_table->s->fields; ++i) {
    ut_ad(strcmp(old_table->field[i]->field_name,
                 altered_table->field[i]->field_name) == 0);
  }
#endif /* UNIV_DEBUG */

  DD_instant_col_val_coder coder;
  ut_d(uint16_t num_instant_cols = 0);

  for (uint32_t i = old_table->s->fields; i < altered_table->s->fields; ++i) {
    Field *field = altered_table->field[i];

    if (innobase_is_v_fld(field)) {
      continue;
    }

    /* The MySQL type code has to fit in 8 bits
    in the metadata stored in the InnoDB change buffer. */
    ut_ad(field->charset() == nullptr ||
          field->charset()->number <= MAX_CHAR_COLL_NUM);
    ut_ad(field->charset() == nullptr || field->charset()->number > 0);

    dd::Column *column = const_cast<dd::Column *>(
        dd_find_column(new_dd_table, field->field_name));
    ut_ad(column != nullptr);
    dd::Properties &se_private = column->se_private_data();

    ut_d(++num_instant_cols);

    se_private.set_uint64(dd_index_key_strings[DD_TABLE_ID], new_table->id);

    if (field->is_real_null()) {
      se_private.set_bool(dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT_NULL],
                          true);
      continue;
    }

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

    nulls_allowed = field->real_maybe_null() ? 0 : DATA_NOT_NULL;

    binary_type = field->binary() ? DATA_BINARY_TYPE : 0;

    charset_no = 0;
    if (dtype_is_string_type(mtype)) {
      charset_no = static_cast<ulint>(field->charset()->number);
    }

    long_true_varchar = 0;
    if (field->type() == MYSQL_TYPE_VARCHAR) {
      col_len -= ((Field_varstring *)field)->length_bytes;

      if (((Field_varstring *)field)->length_bytes == 2) {
        long_true_varchar = DATA_LONG_TRUE_VARCHAR;
      }
    }

    prtype =
        dtype_form_prtype((ulint)field->type() | nulls_allowed | unsigned_type |
                              binary_type | long_true_varchar,
                          charset_no);

    dict_col_t col;
    memset(&col, 0, sizeof(dict_col_t));
    /* Set a fake col_pos, since this should be useless */
    dict_mem_fill_column_struct(&col, 0, mtype, prtype, col_len);
    dfield_t dfield;
    col.copy_type(dfield_get_type(&dfield));

    ulint size = field->pack_length();
    uint64_t buf;
    const byte *mysql_data = field->ptr;

    row_mysql_store_col_in_innobase_format(
        &dfield, reinterpret_cast<byte *>(&buf), true, mysql_data, size,
        dict_table_is_comp(new_table));

    size_t length = 0;
    const char *value = coder.encode(reinterpret_cast<byte *>(dfield.data),
                                     dfield.len, &length);

    dd::String_type default_value;
    default_value.assign(dd::String_type(value, length));
    se_private.set(dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT],
                   default_value);
  }

  ut_ad(num_instant_cols > 0);
}

/** Compare the default values between imported column and column defined
in the server. Note that it's absolutely OK if there is no default value
in the column defined in server, since it can be filled in later.
@param[in]	dd_col	dd::Column
@param[in]	col	InnoDB column object
@return	true	The default values match
@retval	false	Not match */
bool dd_match_default_value(const dd::Column *dd_col, const dict_col_t *col) {
  ut_ad(col->instant_default != nullptr);

  const dd::Properties &private_data = dd_col->se_private_data();

  if (private_data.exists(dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT])) {
    dd::String_type value;
    const byte *default_value;
    size_t len;
    bool match;
    DD_instant_col_val_coder coder;

    private_data.get(dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT], value);
    default_value = coder.decode(value.c_str(), value.length(), &len);

    match = col->instant_default->len == len &&
            memcmp(col->instant_default->value, default_value, len) == 0;

    return (match);

  } else if (private_data.exists(
                 dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT_NULL])) {
    return (col->instant_default->len == UNIV_SQL_NULL);
  }

  return (true);
}

/** Write default value of a column to dd::Column
@param[in]	col	default value of this column to write
@param[in,out]	dd_col	where to store the default value */
void dd_write_default_value(const dict_col_t *col, dd::Column *dd_col) {
  if (col->instant_default->len == UNIV_SQL_NULL) {
    dd_col->se_private_data().set_uint32(
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
@param[in]	se_private_data	dd::Column::se_private
@param[in,out]	col		InnoDB column object
@param[in,out]	heap		Heap to store the default value */
static void dd_parse_default_value(const dd::Properties &se_private_data,
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
                        value);

    default_value = coder.decode(value.c_str(), value.length(), &len);

    col->set_default(default_value, len, heap);
  }
}

/** Import all metadata which is related to instant ADD COLUMN of a table
to dd::Table. This is used for IMPORT.
@param[in]	table		InnoDB table object
@param[in,out]	dd_table	dd::Table */
void dd_import_instant_add_columns(const dict_table_t *table,
                                   dd::Table *dd_table) {
  ut_ad(table->has_instant_cols());
  ut_ad(dict_table_is_partition(table) == dd_table_is_partitioned(*dd_table));

  if (!dd_table_is_partitioned(*dd_table)) {
    dd_table->se_private_data().set_uint32(
        dd_table_key_strings[DD_TABLE_INSTANT_COLS], table->get_instant_cols());
  } else {
    uint32_t instant_cols = std::numeric_limits<uint32_t>::max();

    if (dd_table->se_private_data().exists(
            dd_table_key_strings[DD_TABLE_INSTANT_COLS])) {
      dd_table->se_private_data().get_uint32(
          dd_table_key_strings[DD_TABLE_INSTANT_COLS], &instant_cols);
    }

    if (instant_cols > table->get_instant_cols()) {
      dd_table->se_private_data().set_uint32(
          dd_table_key_strings[DD_TABLE_INSTANT_COLS],
          table->get_instant_cols());
    }

    char postfix_name[FN_REFLEN];
    dd::Partition *partition = nullptr;
    for (const auto dd_part : *dd_table->leaf_partitions()) {
      ut_d(size_t len =) Ha_innopart_share::create_partition_postfix(
          postfix_name, FN_REFLEN, dd_part);
      ut_ad(len < FN_REFLEN);

      if (strstr(table->name.m_name, postfix_name) != 0) {
        partition = dd_part;
        break;
      }
    }

    ut_ad(partition != nullptr);

    partition->se_private_data().set_uint32(
        dd_partition_key_strings[DD_PARTITION_INSTANT_COLS],
        table->get_instant_cols());
  }

  /* Copy all default values if necessary */
  ut_d(bool first_instant = false);
  for (uint16_t i = 0; i < table->get_n_user_cols(); ++i) {
    dict_col_t *col = table->get_col(i);
    if (col->instant_default == nullptr) {
      ut_ad(!first_instant);
      continue;
    }

    ut_d(first_instant = true);

    dd::Column *dd_col = const_cast<dd::Column *>(
        dd_find_column(dd_table, table->get_col_name(i)));
    ut_ad(dd_col != nullptr);

    /* Default values mismatch should have been done.
    So only write default value when it's not ever recorded */
    if (!dd_col->se_private_data().exists(
            dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT_NULL]) &&
        !dd_col->se_private_data().exists(
            dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT])) {
      dd_write_default_value(col, dd_col);
    }
  }
}

/** Write metadata of a index to dd::Index
@param[in]	dd_space_id	Tablespace id, which server allocates
@param[in,out]	dd_index	dd::Index
@param[in]	index		InnoDB index object */
template <typename Index>
static void dd_write_index(dd::Object_id dd_space_id, Index *dd_index,
                           const dict_index_t *index) {
  ut_ad(index->id != 0);
  ut_ad(index->page >= FSP_FIRST_INODE_PAGE_NO);

  dd_index->set_tablespace_id(dd_space_id);

  dd::Properties &p = dd_index->se_private_data();
  p.set_uint64(dd_index_key_strings[DD_INDEX_ID], index->id);
  p.set_uint64(dd_index_key_strings[DD_INDEX_SPACE_ID], index->space);
  p.set_uint64(dd_index_key_strings[DD_TABLE_ID], index->table->id);
  p.set_uint32(dd_index_key_strings[DD_INDEX_ROOT], index->page);
  p.set_uint64(dd_index_key_strings[DD_INDEX_TRX_ID], index->trx_id);
}

template void dd_write_index<dd::Index>(dd::Object_id, dd::Index *,
                                        const dict_index_t *);
template void dd_write_index<dd::Partition_index>(dd::Object_id,
                                                  dd::Partition_index *,
                                                  const dict_index_t *);

/** Write metadata of a table to dd::Table
@tparam		Table		dd::Table or dd::Partition
@param[in]	dd_space_id	Tablespace id, which server allocates
@param[in,out]	dd_table	dd::Table or dd::Partition
@param[in]	table		InnoDB table object */
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
    dd_table->se_private_data().set_bool(
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

  if (!dd_table_is_partitioned(dd_table->table()) ||
      dd_part_is_first(reinterpret_cast<dd::Partition *>(dd_table))) {
    for (auto dd_column : *dd_table->table().columns()) {
      dd_column->se_private_data().set_uint64(dd_index_key_strings[DD_TABLE_ID],
                                              table->id);
    }
  }
}

template void dd_write_table<dd::Table>(dd::Object_id, dd::Table *,
                                        const dict_table_t *);
template void dd_write_table<dd::Partition>(dd::Object_id, dd::Partition *,
                                            const dict_table_t *);

/** Set options of dd::Table according to InnoDB table object
@tparam		Table		dd::Table or dd::Partition
@param[in,out]	dd_table	dd::Table or dd::Partition
@param[in]	table		InnoDB table object */
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
      ut_a(0);
  }

  if (!dd_table_is_partitioned(*dd_table_def)) {
    if (auto zip_ssize = DICT_TF_GET_ZIP_SSIZE(table->flags)) {
      uint32 old_size;
      if (!options.get_uint32("key_block_size", &old_size) && old_size != 0) {
        options.set_uint32("key_block_size", 1 << (zip_ssize - 1));
      }
    } else {
      options.set_uint32("key_block_size", 0);
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
      options.set_uint32("row_type", type);
    }
  } else if (dd_table_def->row_format() != format) {
    dd_table->se_private_data().set_uint32(
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
      dd_column->se_private_data().get_uint64(
          dd_index_key_strings[DD_TABLE_ID],
          reinterpret_cast<uint64 *>(&table_id));
      ut_ad(table_id == id);
    }
#endif /* UNIV_DEBUG */

    if (!dd_column->is_virtual()) {
      continue;
    }

    dd::Properties &p = dd_column->se_private_data();

    if (!p.exists(dd_index_key_strings[DD_TABLE_ID])) {
      p.set_uint64(dd_index_key_strings[DD_TABLE_ID], id);
    }
  }
}

/** Write metadata of a tablespace to dd::Tablespace
@param[in,out]	dd_space	dd::Tablespace
@param[in]	tablespace	InnoDB tablespace object */
void dd_write_tablespace(dd::Tablespace *dd_space,
                         const Tablespace &tablespace) {
  dd::Properties &p = dd_space->se_private_data();
  p.set_uint32(dd_space_key_strings[DD_SPACE_ID], tablespace.space_id());
  p.set_uint32(dd_space_key_strings[DD_SPACE_FLAGS],
               static_cast<uint32>(tablespace.flags()));
  p.set_uint32(dd_space_key_strings[DD_SPACE_SERVER_VERSION],
               DD_SPACE_CURRENT_SRV_VERSION);
  p.set_uint32(dd_space_key_strings[DD_SPACE_VERSION],
               DD_SPACE_CURRENT_SPACE_VERSION);
}

/** Add fts doc id column and index to new table
when old table has hidden fts doc id without fulltext index
@param[in,out]	new_table	New dd table
@param[in]	old_table	Old dd table */
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
@tparam		Index			dd::Index or dd::Partition_index
@param[in]	table			InnoDB table object
@param[in]	dd_index		Index to search
@return	the dict_index_t object related to the index */
template <typename Index>
const dict_index_t *dd_find_index(const dict_table_t *table, Index *dd_index) {
  /* If the name is PRIMARY, return the first index directly,
  because the internal index name could be 'GEN_CLUST_INDEX'.
  It could be possible that the primary key name is not PRIMARY,
  because it's an implicitly upgraded unique index. We have to
  search all the indexes */
  if (dd_index->name() == "PRIMARY") {
    return (table->first_index());
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

  return (index);
}

template const dict_index_t *dd_find_index<dd::Index>(const dict_table_t *,
                                                      dd::Index *);
template const dict_index_t *dd_find_index<dd::Partition_index>(
    const dict_table_t *, dd::Partition_index *);

/** Create an index.
@param[in]	dd_index	DD Index
@param[in,out]	table		InnoDB table
@param[in]	strict		whether to be strict about the max record size
@param[in]	form		MySQL table structure
@param[in]	key_num		key_info[] offset
@return		error code
@retval		0 on success
@retval		HA_ERR_INDEX_COL_TOO_LONG if a column is too long
@retval		HA_ERR_TOO_BIG_ROW if the record is too long */
static MY_ATTRIBUTE((warn_unused_result)) int dd_fill_one_dict_index(
    const dd::Index *dd_index, dict_table_t *table, bool strict,
    const TABLE_SHARE *form, uint key_num) {
  const KEY &key = form->key_info[key_num];
  ulint type = 0;
  unsigned n_fields = key.user_defined_key_parts;
  unsigned n_uniq = n_fields;

  ut_ad(!mutex_own(&dict_sys->mutex));
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
                  return (HA_ERR_TOO_BIG_ROW););

  for (unsigned i = 0; i < key.user_defined_key_parts; i++) {
    const KEY_PART_INFO *key_part = &key.key_part[i];
    unsigned prefix_len = 0;
    const Field *field = key_part->field;
    ut_ad(field == form->field[key_part->fieldnr - 1]);
    ut_ad(field == form->field[field->field_index]);

    if (field->is_virtual_gcol()) {
      index->type |= DICT_VIRTUAL;
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
      return (HA_ERR_INDEX_COL_TOO_LONG);
    }

    dict_col_t *col = nullptr;

    if (innobase_is_v_fld(field)) {
      dict_v_col_t *v_col =
          dict_table_get_nth_v_col_mysql(table, field->field_index);
      col = reinterpret_cast<dict_col_t *>(v_col);
    } else {
      ulint t_num_v = 0;
      for (ulint z = 0; z < field->field_index; z++) {
        if (innobase_is_v_fld(form->field[z])) {
          t_num_v++;
        }
      }

      col = &table->cols[field->field_index - t_num_v];
    }

    dict_index_add_col(index, table, col, prefix_len, is_asc);
  }

  ut_ad(((key.flags & HA_FULLTEXT) == HA_FULLTEXT) ==
        !!(index->type & DICT_FTS));

  index->n_user_defined_cols = key.user_defined_key_parts;

  if (dict_index_add_to_cache(table, index, 0, FALSE) != DB_SUCCESS) {
    ut_ad(0);
    return (HA_ERR_GENERIC);
  }

  index = UT_LIST_GET_LAST(table->indexes);

  if (index->type & DICT_FTS) {
    ut_ad((key.flags & HA_FULLTEXT) == HA_FULLTEXT);
    ut_ad(index->n_uniq == 0);
    ut_ad(n_uniq == 0);

    if (table->fts->cache == nullptr) {
      DICT_TF2_FLAG_SET(table, DICT_TF2_FTS);
      table->fts->cache = fts_cache_create(table);

      rw_lock_x_lock(&table->fts->cache->init_lock);
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

  return (0);
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
      return (static_cast<ulint>(ret));
    }

    push_warning_printf(thd, Sql_condition::SL_WARNING, WARN_OPTION_IGNORED,
                        ER_DEFAULT(WARN_OPTION_IGNORED), "MERGE_THRESHOLD");
  }

  return (DICT_INDEX_MERGE_THRESHOLD_DEFAULT);
}

/** Copy attributes from MySQL TABLE_SHARE into an InnoDB table object.
@param[in,out]	thd		thread context
@param[in,out]	table		InnoDB table
@param[in]	table_share	TABLE_SHARE */
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
@param[in,out]	dd_table	Global DD table metadata
@param[in]	m_form		MySQL table definition
@param[in,out]	m_table		InnoDB table definition
@param[in]	create_info	create table information
@param[in]	zip_allowed	if compression is allowed
@param[in]	strict		if report error in strict mode
@param[in]	m_thd		THD instance
@return 0 if successful, otherwise error number */
inline int dd_fill_dict_index(const dd::Table &dd_table, const TABLE *m_form,
                              dict_table_t *m_table,
                              HA_CREATE_INFO *create_info, bool zip_allowed,
                              bool strict, THD *m_thd) {
  int error = 0;

  ut_ad(!mutex_own(&dict_sys->mutex));

  /* Create the keys */
  if (m_form->s->keys == 0 || m_form->s->primary_key == MAX_KEY) {
    /* Create an index which is used as the clustered index;
    order the rows by the hidden InnoDB column DB_ROW_ID. */
    dict_index_t *index = dict_mem_index_create(
        m_table->name.m_name, "GEN_CLUST_INDEX", 0, DICT_CLUSTERED, 0);
    index->n_uniq = 0;

    dberr_t new_err =
        dict_index_add_to_cache(m_table, index, index->page, FALSE);
    if (new_err != DB_SUCCESS) {
      error = HA_ERR_GENERIC;
      goto dd_error;
    }
  } else {
    /* In InnoDB, the clustered index must always be
    created first. */
    error = dd_fill_one_dict_index(dd_table.indexes()[m_form->s->primary_key],
                                   m_table, strict, m_form->s,
                                   m_form->s->primary_key);
    if (error != 0) {
      goto dd_error;
    }
  }

  for (uint i = !m_form->s->primary_key; i < m_form->s->keys; i++) {
    ulint dd_index_num = i + ((m_form->s->primary_key == MAX_KEY) ? 1 : 0);

    error = dd_fill_one_dict_index(dd_table.indexes()[dd_index_num], m_table,
                                   strict, m_form->s, i);
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
        return (HA_ERR_GENERIC);
      case FTS_EXIST_DOC_ID_INDEX:
        break;
      case FTS_NOT_EXIST_DOC_ID_INDEX:
        dict_index_t *doc_id_index;
        doc_id_index = dict_mem_index_create(
            m_table->name.m_name, FTS_DOC_ID_INDEX_NAME, 0, DICT_UNIQUE, 1);
        doc_id_index->add_field(FTS_DOC_ID_COL_NAME, 0, true);

        dberr_t new_err = dict_index_add_to_cache(m_table, doc_id_index,
                                                  doc_id_index->page, FALSE);
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
    mutex_enter(&dict_sys->mutex);

    for (dict_index_t *f_index = UT_LIST_GET_LAST(m_table->indexes);
         f_index != nullptr; f_index = UT_LIST_GET_LAST(m_table->indexes)) {
      dict_index_remove_from_cache(m_table, f_index);
    }

    mutex_exit(&dict_sys->mutex);

    dict_mem_table_free(m_table);
  }

  return (error);
}

/** Determine if a table contains a fulltext index.
@param[in]      table		dd::Table
@return whether the table contains any fulltext index */
inline bool dd_table_contains_fulltext(const dd::Table &table) {
  for (const dd::Index *index : table.indexes()) {
    if (index->type() == dd::Index::IT_FULLTEXT) {
      return (true);
    }
  }
  return (false);
}

/** Read the metadata of default values for all columns added instantly
@param[in]	dd_table	dd::Table
@param[in,out]	table		InnoDB table object */
static void dd_fill_instant_columns(const dd::Table &dd_table,
                                    dict_table_t *table) {
  ut_ad(table->has_instant_cols());
  ut_ad(dd_table_has_instant_cols(dd_table));

#ifdef UNIV_DEBUG
  for (uint16_t i = 0; i < table->get_n_cols(); ++i) {
    ut_ad(table->get_col(i)->instant_default == nullptr);
  }
#endif /* UNIV_DEBUG */

  uint32_t skip = 0;

  if (dd_table_is_partitioned(dd_table)) {
    uint32_t cols;

    dd_table.se_private_data().get_uint32(
        dd_table_key_strings[DD_TABLE_INSTANT_COLS], &cols);
    ut_ad(cols <= table->get_instant_cols());

    /* The dd::Columns should have `cols` default values,
    however, this partition table only needs
    `table->get_instant_cols()` default values. */
    skip = table->get_instant_cols() - cols;
  }

  /* Assume the order of non-virtual columns are the same */
  uint32_t innodb_pos = 0;
  for (const auto col : dd_table.columns()) {
    if (col->is_virtual() || col->is_se_hidden()) {
      continue;
    }

    dict_col_t *column = table->get_col(innodb_pos++);
    ut_ad(!column->is_virtual());

#ifdef UNIV_DEBUG
    const char *name = table->col_names;
    for (uint32_t i = 0; i < innodb_pos - 1; ++i) {
      name += strlen(name) + 1;
    }
    ut_ad(col->name() == name);
#endif /* UNIV_DEBUG */

    const dd::Properties &private_data = col->se_private_data();
    if (!private_data.exists(
            dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT_NULL]) &&
        !private_data.exists(
            dd_column_key_strings[DD_INSTANT_COLUMN_DEFAULT])) {
      continue;
    }

    if (skip > 0) {
      --skip;
      continue;
    }

    /* Note that it's before dict_table_add_to_cache(),
    don't worry about the dict_sys->size. */
    dd_parse_default_value(private_data, column, table->heap);
  }

#ifdef UNIV_DEBUG
  uint16_t n_default = 0;
  for (uint16_t i = 0; i < table->get_n_user_cols(); ++i) {
    if (table->get_col(i)->instant_default != nullptr) {
      ++n_default;
    }
  }

  ut_ad(n_default + table->get_instant_cols() == table->get_n_user_cols());
#endif /* UNIV_DEBUG */
}

/** Instantiate in-memory InnoDB table metadata (dict_table_t),
without any indexes.
@tparam		Table		dd::Table or dd::Partition
@param[in]	dd_tab		Global Data Dictionary metadata,
                                or NULL for internal temporary table
@param[in]	m_form		MySQL TABLE for current table
@param[in]	norm_name	normalized table name
@param[in]	create_info	create info
@param[in]	zip_allowed	whether ROW_FORMAT=COMPRESSED is OK
@param[in]	strict		whether to use innodb_strict_mode=ON
@param[in]	m_thd		thread THD
@param[in]	is_implicit	if it is an implicit tablespace
@return created dict_table_t on success or nullptr */
template <typename Table>
static inline dict_table_t *dd_fill_dict_table(const Table *dd_tab,
                                               const TABLE *m_form,
                                               const char *norm_name,
                                               HA_CREATE_INFO *create_info,
                                               bool zip_allowed, bool strict,
                                               THD *m_thd, bool is_implicit) {
  mem_heap_t *heap;
  bool is_encrypted = false;
  bool is_discard = false;

  ut_ad(dd_tab != nullptr);
  ut_ad(m_thd != nullptr);
  ut_ad(norm_name != nullptr);
  ut_ad(create_info == nullptr || m_form->s->row_type == create_info->row_type);
  ut_ad(create_info == nullptr ||
        m_form->s->key_block_size == create_info->key_block_size);
  ut_ad(dd_tab != nullptr);

  if (m_form->s->fields > REC_MAX_N_USER_FIELDS) {
    my_error(ER_TOO_MANY_FIELDS, MYF(0));
    return (nullptr);
  }

  /* Set encryption option for file-per-table tablespace. */
  dd::String_type encrypt;
  if (dd_tab->table().options().exists("encrypt_type")) {
    dd_tab->table().options().get("encrypt_type", encrypt);
    if (!Encryption::is_none(encrypt.c_str())) {
      ut_ad(innobase_strcasecmp(encrypt.c_str(), "y") == 0);
      is_encrypted = true;
    }
  }

  /* Check discard flag. */
  const dd::Properties &table_private = dd_tab->table().se_private_data();
  if (table_private.exists(dd_table_key_strings[DD_TABLE_DISCARD])) {
    table_private.get_bool(dd_table_key_strings[DD_TABLE_DISCARD], &is_discard);
  }

  const unsigned n_mysql_cols = m_form->s->fields;

  bool has_doc_id = false;

  /* First check if dd::Table contains the right hidden column
  as FTS_DOC_ID */
  const dd::Column *doc_col;
  doc_col = dd_find_column(&dd_tab->table(), FTS_DOC_ID_COL_NAME);

  /* Check weather this is a proper typed FTS_DOC_ID */
  if (doc_col && doc_col->type() == dd::enum_column_types::LONGLONG &&
      !doc_col->is_nullable()) {
    has_doc_id = true;
  }

  const bool fulltext =
      dd_tab != nullptr && dd_table_contains_fulltext(dd_tab->table());

  /* If there is a fulltext index, then it must have a FTS_DOC_ID */
  if (fulltext) {
    ut_ad(has_doc_id);
  }

  bool add_doc_id = false;

  /* Need to add FTS_DOC_ID column if it is not defined by user,
  since TABLE_SHARE::fields does not contain it if it is a hidden col */
  if (has_doc_id && doc_col->is_se_hidden()) {
#ifdef UNIV_DEBUG
    ulint doc_id_col;
    ut_ad(!create_table_check_doc_id_col(m_thd, m_form, &doc_id_col));
#endif
    add_doc_id = true;
  }

  const unsigned n_cols = n_mysql_cols + add_doc_id;

  bool is_redundant;
  bool blob_prefix;
  ulint zip_ssize;
  row_type real_type = ROW_TYPE_NOT_USED;

  if (dd_table_is_partitioned(dd_tab->table())) {
    const dd::Properties &part_p = dd_tab->se_private_data();
    if (part_p.exists(dd_partition_key_strings[DD_PARTITION_ROW_FORMAT])) {
      dd::Table::enum_row_format format;
      part_p.get_uint32(dd_partition_key_strings[DD_PARTITION_ROW_FORMAT],
                        reinterpret_cast<uint32 *>(&format));
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
          ut_ad(0);
      }
    }
  }

  /* Validate the table format options */
  if (format_validate(m_thd, m_form, real_type, zip_allowed, strict,
                      &is_redundant, &blob_prefix, &zip_ssize, is_implicit)) {
    return (nullptr);
  }

  ulint n_v_cols = 0;

  /* Find out the number of virtual columns */
  for (ulint i = 0; i < m_form->s->fields; i++) {
    Field *field = m_form->field[i];

    if (innobase_is_v_fld(field)) {
      n_v_cols++;
    }
  }

  ut_ad(n_v_cols <= n_cols);

  /* Create the dict_table_t */
  dict_table_t *m_table =
      dict_mem_table_create(norm_name, 0, n_cols, n_v_cols, 0, 0);

  /* Set up the field in the newly allocated dict_table_t */
  m_table->id = dd_tab->se_private_id();

  if (dd_tab->se_private_data().exists(
          dd_table_key_strings[DD_TABLE_DATA_DIRECTORY])) {
    m_table->flags |= DICT_TF_MASK_DATA_DIR;
  }

  /* If the table has instantly added columns, it's necessary to read
  the number of instant columns for either normal table(from dd::Table),
  or partitioned table(from dd::Partition). One partition may have no
  instant columns, which is fine. */
  if (dd_table_has_instant_cols(dd_tab->table())) {
    uint32_t instant_cols;

    if (!dd_table_is_partitioned(dd_tab->table())) {
      table_private.get_uint32(dd_table_key_strings[DD_TABLE_INSTANT_COLS],
                               &instant_cols);
      m_table->set_instant_cols(instant_cols);
      ut_ad(m_table->has_instant_cols());
    } else if (dd_part_has_instant_cols(
                   *reinterpret_cast<const dd::Partition *>(dd_tab))) {
      dd_tab->se_private_data().get_uint32(
          dd_partition_key_strings[DD_PARTITION_INSTANT_COLS], &instant_cols);

      m_table->set_instant_cols(instant_cols);
      ut_ad(m_table->has_instant_cols());
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
    /* This flag will be used to set file-per-table tablesapce
    encryption flag */
    DICT_TF2_FLAG_SET(m_table, DICT_TF2_ENCRYPTION_FILE_PER_TABLE);
  }

  heap = mem_heap_create(1000);

  /* Fill out each column info */
  for (unsigned i = 0; i < n_mysql_cols; i++) {
    const Field *field = m_form->field[i];
    ulint prtype = 0;
    unsigned col_len = field->pack_length();

    /* The MySQL type code has to fit in 8 bits
    in the metadata stored in the InnoDB change buffer. */
    ut_ad(field->charset() == nullptr ||
          field->charset()->number <= MAX_CHAR_COLL_NUM);
    ut_ad(field->charset() == nullptr || field->charset()->number > 0);

    ulint nulls_allowed;
    ulint unsigned_type;
    ulint binary_type;
    ulint long_true_varchar;
    ulint charset_no;
    ulint mtype = get_innobase_type_from_mysql_type(&unsigned_type, field);

    nulls_allowed = field->real_maybe_null() ? 0 : DATA_NOT_NULL;

    /* Convert non nullable fields in FTS AUX tables as nullable.
    This is because in 5.7, we created FTS AUX tables clustered
    index with nullable field, although NULLS are not inserted.
    When fields are nullable, the record layout is dependent on
    that. When registering FTS AUX Tables with new DD, we cannot
    register nullable fields as part of Primary Key. Hence we register
    them as non-nullabe in DD but treat as nullable in InnoDB.
    This way the compatibility with 5.7 FTS AUX tables is also
    maintained. */
    if (m_table->is_fts_aux()) {
      const dd::Table &dd_table = dd_tab->table();
      const dd::Column *dd_col = dd_find_column(&dd_table, field->field_name);
      const dd::Properties &p = dd_col->se_private_data();
      if (p.exists("nullable")) {
        bool nullable;
        p.get_bool("nullable", &nullable);
        nulls_allowed = nullable ? 0 : DATA_NOT_NULL;
      }
    }

    binary_type = field->binary() ? DATA_BINARY_TYPE : 0;

    charset_no = 0;
    if (dtype_is_string_type(mtype)) {
      charset_no = static_cast<ulint>(field->charset()->number);
    }

    long_true_varchar = 0;
    if (field->type() == MYSQL_TYPE_VARCHAR) {
      col_len -= ((Field_varstring *)field)->length_bytes;

      if (((Field_varstring *)field)->length_bytes == 2) {
        long_true_varchar = DATA_LONG_TRUE_VARCHAR;
      }
    }

    ulint is_virtual = (innobase_is_v_fld(field)) ? DATA_VIRTUAL : 0;

    bool is_stored = innobase_is_s_fld(field);

    if (!is_virtual) {
      prtype =
          dtype_form_prtype((ulint)field->type() | nulls_allowed |
                                unsigned_type | binary_type | long_true_varchar,
                            charset_no);
      dict_mem_table_add_col(m_table, heap, field->field_name, mtype, prtype,
                             col_len);
    } else {
      prtype = dtype_form_prtype((ulint)field->type() | nulls_allowed |
                                     unsigned_type | binary_type |
                                     long_true_varchar | is_virtual,
                                 charset_no);
      dict_mem_table_add_v_col(m_table, heap, field->field_name, mtype, prtype,
                               col_len, i,
                               field->gcol_info->non_virtual_base_columns());
    }

    if (is_stored) {
      ut_ad(!is_virtual);
      /* Added stored column in m_s_cols list. */
      dict_mem_table_add_s_col(m_table,
                               field->gcol_info->non_virtual_base_columns());
    }
  }

  ulint j = 0;

  /* For each virtual column, we will need to set up its base column
  info */
  if (m_table->n_v_cols > 0) {
    for (unsigned i = 0; i < n_mysql_cols; i++) {
      dict_v_col_t *v_col;

      Field *field = m_form->field[i];

      if (!innobase_is_v_fld(field)) {
        continue;
      }

      v_col = dict_table_get_nth_v_col(m_table, j);

      j++;

      innodb_base_col_setup(m_table, field, v_col);
    }
  }

  if (add_doc_id) {
    /* Add the hidden FTS_DOC_ID column. */
    fts_add_doc_id_column(m_table, heap);
  }

  /* Add system columns to make adding index work */
  dict_table_add_system_columns(m_table, heap);

  if (m_table->has_instant_cols()) {
    dd_fill_instant_columns(dd_tab->table(), m_table);
  }

  mem_heap_free(heap);

  return (m_table);
}

/** Parse the tablespace name from filename charset to table name charset
@param[in]      space_name      tablespace name
@param[in,out]	tablespace_name	tablespace name which is in table name
                                charset. */
void dd_filename_to_spacename(const char *space_name,
                              std::string *tablespace_name) {
  char db_buf[NAME_LEN + 1];
  char tbl_buf[NAME_LEN + 1];
  char part_buf[NAME_LEN + 1];
  char sub_buf[NAME_LEN + 1];
  char orig_tablespace[NAME_LEN + 1];
  bool is_tmp = false;

  db_buf[0] = tbl_buf[0] = part_buf[0] = sub_buf[0] = '\0';

  dd_parse_tbl_name(space_name, db_buf, tbl_buf, part_buf, sub_buf, &is_tmp);

  if (db_buf[0] == '\0') {
    filename_to_tablename((char *)space_name, orig_tablespace, (NAME_LEN + 1));
    tablespace_name->append(orig_tablespace);

    return;
  }

  tablespace_name->append(db_buf);
  tablespace_name->append("/");
  tablespace_name->append(tbl_buf);

  if (part_buf[0] != '\0') {
    tablespace_name->append(PART_SEPARATOR);
    tablespace_name->append(part_buf);
  }

  if (sub_buf[0] != '\0') {
    tablespace_name->append(SUB_PART_SEPARATOR);
    tablespace_name->append(sub_buf);
  }

  if (is_tmp) {
    tablespace_name->append(TMP_POSTFIX);
  }

  /* Name should not exceed schema/table#P#partition#SP#subpartition. */
  ut_ad(tablespace_name->size() < MAX_SPACE_NAME_LEN);
}

/* Create metadata for specified tablespace, acquiring exlcusive MDL first
@param[in,out]	dd_client	data dictionary client
@param[in,out]	thd		THD
@param[in,out]	dd_space_name	dd tablespace name
@param[in]	space		InnoDB tablespace ID
@param[in]	flags		InnoDB tablespace flags
@param[in]	filename	filename of this tablespace
@param[in]	discarded	true if this tablespace was discarded
@param[in,out]	dd_space_id	dd_space_id
@retval false on success
@retval true on failure */
bool create_dd_tablespace(dd::cache::Dictionary_client *dd_client, THD *thd,
                          const char *dd_space_name, space_id_t space_id,
                          ulint flags, const char *filename, bool discarded,
                          dd::Object_id &dd_space_id) {
  std::unique_ptr<dd::Tablespace> dd_space(dd::create_object<dd::Tablespace>());

  if (dd_space_name != nullptr) {
    dd_space->set_name(dd_space_name);
  }

  if (dd::acquire_exclusive_tablespace_mdl(thd, dd_space->name().c_str(),
                                           true)) {
    return (true);
  }

  dd_space->set_engine(innobase_hton_name);
  dd::Properties &p = dd_space->se_private_data();
  p.set_uint32(dd_space_key_strings[DD_SPACE_ID],
               static_cast<uint32>(space_id));
  p.set_uint32(dd_space_key_strings[DD_SPACE_FLAGS],
               static_cast<uint32>(flags));
  p.set_uint32(dd_space_key_strings[DD_SPACE_SERVER_VERSION],
               DD_SPACE_CURRENT_SRV_VERSION);
  p.set_uint32(dd_space_key_strings[DD_SPACE_VERSION],
               DD_SPACE_CURRENT_SPACE_VERSION);

  if (discarded) {
    p.set_bool(dd_space_key_strings[DD_SPACE_DISCARD], discarded);
  }

  dd::Tablespace_file *dd_file = dd_space->add_file();
  dd_file->set_filename(filename);
  dd_file->se_private_data().set_uint32(dd_space_key_strings[DD_SPACE_ID],
                                        static_cast<uint32>(space_id));

  if (dd_client->store(dd_space.get())) {
    return (true);
  }

  dd_space_id = dd_space.get()->id();

  return (false);
}

/** Create metadata for implicit tablespace
@param[in,out]	dd_client	data dictionary client
@param[in,out]	thd		THD
@param[in]	space_id	InnoDB tablespace ID
@param[in]	tablespace_name	tablespace name to be set for the
                                newly created tablespace
@param[in]	filename	tablespace filename
@param[in]	discarded	true if this tablespace was discarded
@param[in,out]	dd_space_id	dd tablespace id
@retval false	on success
@retval true	on failure */
bool dd_create_implicit_tablespace(dd::cache::Dictionary_client *dd_client,
                                   THD *thd, space_id_t space_id,
                                   const char *tablespace_name,
                                   const char *filename, bool discarded,
                                   dd::Object_id &dd_space_id) {
  std::string space_name;
  fil_space_t *space = fil_space_get(space_id);
  ulint flags = space->flags;

  dd_filename_to_spacename(tablespace_name, &space_name);

  bool fail = create_dd_tablespace(dd_client, thd, space_name.c_str(), space_id,
                                   flags, filename, discarded, dd_space_id);

  return (fail);
}

/** Drop a tablespace
@param[in,out]  dd_client       data dictionary client
@param[in,out]  thd             THD object
@param[in]      dd_space_id     dd tablespace id
@retval false   On success
@retval true    On failure */
bool dd_drop_tablespace(dd::cache::Dictionary_client *dd_client, THD *thd,
                        dd::Object_id dd_space_id) {
  dd::Tablespace *dd_space;

  if (dd_client->acquire_uncached_uncommitted(dd_space_id, &dd_space)) {
    my_error(ER_INTERNAL_ERROR, MYF(0),
             " InnoDB can't get tablespace object"
             " for space ",
             dd_space_id);

    return (true);
  }

  ut_a(dd_space != nullptr);

  if (dd::acquire_exclusive_tablespace_mdl(thd, dd_space->name().c_str(),
                                           false)) {
    my_error(ER_INTERNAL_ERROR, MYF(0),
             " InnoDB can't set exclusive MDL on"
             " tablespace ",
             dd_space->name().c_str());

    return (true);
  }

  bool error = dd_client->drop(dd_space);
  DBUG_EXECUTE_IF("fail_while_dropping_dd_object", error = true;);

  if (error) {
    my_error(ER_INTERNAL_ERROR, MYF(0), " InnoDB can't drop tablespace object",
             dd_space->name().c_str());
  }

  return (error);
}

/** Determine if a tablespace is implicit.
@param[in,out]	client		data dictionary client
@param[in]	dd_space_id	dd tablespace id
@param[out]	implicit	whether the tablespace is implicit tablespace
@param[out]	dd_space	DD space object
@retval false	on success
@retval true	on failure */
bool dd_tablespace_is_implicit(dd::cache::Dictionary_client *client,
                               dd::Object_id dd_space_id, bool *implicit,
                               dd::Tablespace **dd_space) {
  space_id_t id = 0;
  uint32 flags;

  const bool fail = client->acquire_uncached_uncommitted<dd::Tablespace>(
                        dd_space_id, dd_space) ||
                    (*dd_space) == nullptr ||
                    (*dd_space)->se_private_data().get_uint32(
                        dd_space_key_strings[DD_SPACE_ID], &id);

  if (!fail) {
    (*dd_space)->se_private_data().get_uint32(
        dd_space_key_strings[DD_SPACE_FLAGS], &flags);
    *implicit = fsp_is_file_per_table(id, flags);
  }

  return (fail);
}

/** Load foreign key constraint info for the dd::Table object.
@param[out]	m_table		InnoDB table handle
@param[in]	dd_table	Global DD table
@param[in]	col_names	column names, or NULL
@param[in]	ignore_err	DICT_ERR_IGNORE_FK_NOKEY or DICT_ERR_IGNORE_NONE
@param[in]	dict_locked	True if dict_sys->mutex is already held,
                                otherwise false
@return DB_SUCCESS	if successfully load FK constraint */
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
                         NULL, 0, &truncated);
    ut_ad(!truncated);
    char norm_name[FN_REFLEN * 2];
    normalize_table_name(norm_name, buf);

    dict_foreign_t *foreign = dict_mem_foreign_create();
    foreign->foreign_table_name =
        mem_heap_strdup(foreign->heap, m_table->name.m_name);

    dict_mem_foreign_table_name_lookup_set(foreign, TRUE);

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
    dict_mem_referenced_table_name_lookup_set(foreign, TRUE);
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
        ut_ad(0);
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
        ut_ad(0);
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
      mutex_enter(&dict_sys->mutex);
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
        dict_foreign_add_to_cache(foreign, col_names, FALSE, true, ignore_err);
    if (!dict_locked) {
      mutex_exit(&dict_sys->mutex);
    }

    if (err != DB_SUCCESS) {
      break;
    }

    /* Set up the FK virtual column info */
    dict_mem_table_free_foreign_vcol_set(m_table);
    dict_mem_table_fill_foreign_vcol_set(m_table);
  }
  return (err);
}

/** Load foreign key constraint for the table. Note, it could also open
the foreign table, if this table is referenced by the foreign table
@param[in,out]	client		data dictionary client
@param[in]	tbl_name	Table Name
@param[in]	col_names	column names, or NULL
@param[out]	m_table		InnoDB table handle
@param[in]	dd_table	Global DD table
@param[in]	thd		thread THD
@param[in]	dict_locked	True if dict_sys->mutex is already held,
                                otherwise false
@param[in]	check_charsets	whether to check charset compatibility
@param[in,out]	fk_tables	name list for tables that refer to this table
@return DB_SUCCESS	if successfully load FK constraint */
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
    return (err);
  }

  if (dict_locked) {
    mutex_exit(&dict_sys->mutex);
  }

  DBUG_EXECUTE_IF("enable_stack_overrun_post_alter_commit",
                  { DBUG_SET("+d,simulate_stack_overrun"); });
  err = dd_table_check_for_child(client, tbl_name, col_names, m_table, dd_table,
                                 thd, check_charsets, ignore_err, fk_tables);
  DBUG_EXECUTE_IF("enable_stack_overrun_post_alter_commit",
                  { DBUG_SET("-d,simulate_stack_overrun"); });

  if (dict_locked) {
    mutex_enter(&dict_sys->mutex);
  }

  return (err);
}

/** Load foreign key constraint for the table. Note, it could also open
the foreign table, if this table is referenced by the foreign table
@param[in,out]	client		data dictionary client
@param[in]	tbl_name	Table Name
@param[in]	col_names	column names, or NULL
@param[out]	m_table		InnoDB table handle
@param[in]	dd_table	Global DD table
@param[in]	thd		thread THD
@param[in]	check_charsets	whether to check charset compatibility
@param[in]	ignore_err	DICT_ERR_IGNORE_FK_NOKEY or DICT_ERR_IGNORE_NONE
@param[in,out]	fk_tables	name list for tables that refer to this table
@return DB_SUCCESS	if successfully load FK constraint */
dberr_t dd_table_check_for_child(dd::cache::Dictionary_client *client,
                                 const char *tbl_name, const char **col_names,
                                 dict_table_t *m_table,
                                 const dd::Table *dd_table, THD *thd,
                                 bool check_charsets,
                                 dict_err_ignore_t ignore_err,
                                 dict_names_t *fk_tables) {
  dberr_t err = DB_SUCCESS;

  /* TODO: NewDD: Temporary ignore DD system table until
  WL#6049 inplace */
  if (!dict_sys_t::is_dd_table_id(m_table->id) && fk_tables != nullptr) {
    std::vector<dd::String_type> child_schema;
    std::vector<dd::String_type> child_name;

    char name_buf1[MAX_DATABASE_NAME_LEN + 1];
    char name_buf2[MAX_TABLE_NAME_LEN + 1];

    dd_parse_tbl_name(m_table->name.m_name, name_buf1, name_buf2, nullptr,
                      nullptr, nullptr);

    if (client->fetch_fk_children_uncached(name_buf1, name_buf2, "InnoDB",
                                           false, &child_schema, &child_name)) {
      return (DB_ERROR);
    }

    std::vector<dd::String_type>::iterator it = child_name.begin();
    for (auto &db_name : child_schema) {
      dd::String_type tb_name = *it;
      char buf[2 * NAME_CHAR_LEN * 5 + 2 + 1];
      bool truncated;
      build_table_filename(buf, sizeof(buf), db_name.c_str(), tb_name.c_str(),
                           NULL, 0, &truncated);
      ut_ad(!truncated);
      char full_name[FN_REFLEN];
      normalize_table_name(full_name, buf);

      if (innobase_get_lower_case_table_names() == 2) {
        innobase_casedn_str(full_name);
      } else {
#ifndef _WIN32
        if (innobase_get_lower_case_table_names() == 1) {
          innobase_casedn_str(full_name);
        }
#endif /* !_WIN32 */
      }

      mutex_enter(&dict_sys->mutex);

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
              mutex_exit(&dict_sys->mutex);
              return (err);
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

      mutex_exit(&dict_sys->mutex);

      ut_ad(it != child_name.end());
      ++it;
    }
  }

  return (err);
}

/** Get tablespace name of dd::Table
@tparam		Table		dd::Table or dd::Partition
@param[in]	dd_table	dd table object
@return the tablespace name. */
template <typename Table>
const char *dd_table_get_space_name(const Table *dd_table) {
  dd::Tablespace *dd_space = nullptr;
  THD *thd = current_thd;
  const char *space_name;

  DBUG_ENTER("dd_table_get_space_name");
  ut_ad(!srv_is_being_shutdown);

  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  dd::Object_id dd_space_id = (*dd_table->indexes().begin())->tablespace_id();

  if (client->acquire_uncached_uncommitted<dd::Tablespace>(dd_space_id,
                                                           &dd_space)) {
    ut_ad(false);
    DBUG_RETURN(nullptr);
  }

  ut_a(dd_space != nullptr);
  space_name = dd_space->name().c_str();

  DBUG_RETURN(space_name);
}

/** Get the first filepath from mysql.tablespace_datafiles
for a given space_id.
@tparam		Table		dd::Table or dd::Partition
@param[in,out]	heap		heap for store file name.
@param[in]	table		dict table
@param[in]	dd_table	dd table obj
@return First filepath (caller must invoke ut_free() on it)
@retval NULL if no mysql.tablespace_datafilesentry was found. */
template <typename Table>
char *dd_get_first_path(mem_heap_t *heap, dict_table_t *table,
                        Table *dd_table) {
  char *filepath = nullptr;
  dd::Tablespace *dd_space = nullptr;
  THD *thd = current_thd;
  MDL_ticket *mdl = nullptr;
  dd::Object_id dd_space_id;

  ut_ad(!srv_is_being_shutdown);
  ut_ad(!mutex_own(&dict_sys->mutex));

  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  if (dd_table == nullptr) {
    char db_buf[MAX_DATABASE_NAME_LEN + 1];
    char tbl_buf[MAX_TABLE_NAME_LEN + 1];
    const dd::Table *table_def = nullptr;

    if (!dd_parse_tbl_name(table->name.m_name, db_buf, tbl_buf, nullptr,
                           nullptr, nullptr) ||
        dd_mdl_acquire(thd, &mdl, db_buf, tbl_buf)) {
      return (nullptr);
    }

    if (client->acquire(db_buf, tbl_buf, &table_def) || table_def == nullptr) {
      dd_mdl_release(thd, &mdl);
      return (nullptr);
    }

    dd_space_id = dd_first_index(table_def)->tablespace_id();

    dd_mdl_release(thd, &mdl);
  } else {
    dd_space_id = dd_first_index(dd_table)->tablespace_id();
  }

  if (client->acquire_uncached_uncommitted<dd::Tablespace>(dd_space_id,
                                                           &dd_space)) {
    ut_a(false);
  }

  if (dd_space != nullptr) {
    dd::Tablespace_file *dd_file =
        const_cast<dd::Tablespace_file *>(*(dd_space->files().begin()));

    filepath = mem_heap_strdup(heap, dd_file->filename().c_str());

    return (filepath);
  }

  return (nullptr);
}

/** Make sure the data_dir_path is saved in dict_table_t if this is a
remote single file tablespace. This allows DATA DIRECTORY to be
displayed correctly for SHOW CREATE TABLE. Try to read the filepath
from the fil_system first, then from the DD.
@tparam		Table		dd::Table or dd::Partition
@param[in,out]	table		Table object
@param[in]	dd_table	DD table object
@param[in]	dict_mutex_own	true if dict_sys->mutex is owned already */
template <typename Table>
void dd_get_and_save_data_dir_path(dict_table_t *table, const Table *dd_table,
                                   bool dict_mutex_own) {
  mem_heap_t *heap = NULL;

  if (!DICT_TF_HAS_DATA_DIR(table->flags) || table->data_dir_path != nullptr) {
    return;
  }

  char *path = fil_space_get_first_path(table->space);

  if (path == nullptr) {
    heap = mem_heap_create(1000);
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
    ut_free(path);
  }
}

template void dd_get_and_save_data_dir_path<dd::Table>(dict_table_t *,
                                                       const dd::Table *, bool);

template void dd_get_and_save_data_dir_path<dd::Partition>(
    dict_table_t *, const dd::Partition *, bool);

/** Get the meta-data filename from the table name for a
single-table tablespace.
@param[in,out]	table		table object
@param[in]	dd_table	DD table object
@param[out]	filename	filename
@param[in]	max_len		filename max length */
void dd_get_meta_data_filename(dict_table_t *table, dd::Table *dd_table,
                               char *filename, ulint max_len) {
  /* Make sure the data_dir_path is set. */
  dd_get_and_save_data_dir_path(table, dd_table, false);

  std::string path = dict_table_get_datadir(table);

  auto filepath = Fil_path::make(path, table->name.m_name, CFG, true);

  ut_a(max_len >= strlen(filepath) + 1);

  strcpy(filename, filepath);

  ut_free(filepath);
}

/** Opens a tablespace for dd_load_table_one()
@tparam		Table			dd::Table or dd::Partition
@param[in,out]	dd_table		dd table
@param[in,out]	table			A table that refers to the tablespace to
                                        open
@param[in,out]	heap			A memory heap
@param[in]	expected_fsp_flags	expected flags of tablespace to be
                                        loaded
@param[in]	ignore_err		Whether to ignore an error. */
template <typename Table>
void dd_load_tablespace(const Table *dd_table, dict_table_t *table,
                        mem_heap_t *heap, dict_err_ignore_t ignore_err,
                        ulint expected_fsp_flags) {
  bool alloc_from_heap = false;

  ut_ad(!table->is_temporary());
  ut_ad(mutex_own(&dict_sys->mutex));

  /* The system and temporary tablespaces are preloaded and
  always available. */
  if (fsp_is_system_or_temp_tablespace(table->space)) {
    return;
  }

  if (table->flags2 & DICT_TF2_DISCARDED) {
    ib::warn(ER_IB_MSG_171)
        << "Tablespace for table " << table->name << " is set as discarded.";
    table->ibd_file_missing = TRUE;
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
    if (table->space == dict_sys_t::s_space_id) {
      shared_space_name = mem_strdup(dict_sys_t::s_dd_space_name);
    } else if (srv_sys_tablespaces_open) {
      /* For avoiding deadlock, we need to exit
      dict_sys->mutex. */
      mutex_exit(&dict_sys->mutex);
      shared_space_name = mem_strdup(dd_table_get_space_name(dd_table));
      mutex_enter(&dict_sys->mutex);
    } else {
      /* Make the temporary tablespace name. */
      shared_space_name =
          static_cast<char *>(ut_malloc_nokey(strlen(general_space_name) + 20));

      sprintf(shared_space_name, "%s_" ULINTPF, general_space_name,
              static_cast<ulint>(table->space));
    }

    space_name = shared_space_name;
    tbl_name = space_name;
  } else {
    tbl_name = table->name.m_name;
    dd_filename_to_spacename(tbl_name, &tablespace_name);
    space_name = tablespace_name.c_str();
  }

  /* The tablespace may already be open. */
  if (fil_space_exists_in_mem(table->space, space_name, false, true, heap,
                              table->id)) {
    dd_get_and_save_data_dir_path(table, dd_table, true);
    ut_free(shared_space_name);
    return;
  }

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

  if (filepath == nullptr) {
    /* boot_tablespaces() made sure that the scanned filepath
    is in the DD even if the datafile was moved. So let's use
    that path to open this tablespace. */
    mutex_exit(&dict_sys->mutex);
    char *filepath = dd_get_first_path(heap, table, dd_table);
    mutex_enter(&dict_sys->mutex);

    if (filepath == nullptr) {
      ib::warn(ER_IB_MSG_173) << "Could not find the filepath"
                              << " for table " << table->name << ", space ID "
                              << table->space << " in the data dictionary.";
    } else {
      alloc_from_heap = true;
    }
  }

  /* Try to open the tablespace.  We set the 2nd param (fix_dict) to
  false because we do not have an x-lock on dict_operation_lock */
  dberr_t err =
      fil_ibd_open(true, FIL_TYPE_TABLESPACE, table->space, expected_fsp_flags,
                   space_name, tbl_name, filepath, true, false);

  if (err == DB_SUCCESS) {
    /* This will set the DATA DIRECTORY for SHOW CREATE TABLE. */
    dd_get_and_save_data_dir_path(table, dd_table, true);

  } else {
    /* We failed to find a sensible tablespace file */
    table->ibd_file_missing = TRUE;
  }

  ut_free(shared_space_name);
  if (!alloc_from_heap && filepath) {
    ut_free(filepath);
  }
}

/** Get the space name from mysql.tablespaces for a given space_id.
@tparam		Table		dd::Table or dd::Partition
@param[in,out]	heap		heap for store file name.
@param[in]	table		dict table
@param[in]	dd_table	dd table obj
@return First filepath (caller must invoke ut_free() on it)
@retval NULL if no mysql.tablespace_datafilesentry was found. */
template <typename Table>
char *dd_space_get_name(mem_heap_t *heap, dict_table_t *table,
                        Table *dd_table) {
  dd::Object_id dd_space_id;
  THD *thd = current_thd;
  dd::Tablespace *dd_space = nullptr;

  ut_ad(!srv_is_being_shutdown);
  ut_ad(!mutex_own(&dict_sys->mutex));

  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  if (dd_table == nullptr) {
    char db_buf[MAX_DATABASE_NAME_LEN + 1];
    char tbl_buf[MAX_TABLE_NAME_LEN + 1];
    const dd::Table *table_def = nullptr;

    MDL_ticket *mdl = nullptr;

    if (!dd_parse_tbl_name(table->name.m_name, db_buf, tbl_buf, nullptr,
                           nullptr, nullptr) ||
        dd_mdl_acquire(thd, &mdl, db_buf, tbl_buf)) {
      return (nullptr);
    }

    if (client->acquire(db_buf, tbl_buf, &table_def) || table_def == nullptr) {
      dd_mdl_release(thd, &mdl);
      return (nullptr);
    }

    dd_space_id = dd_first_index(table_def)->tablespace_id();

    dd_mdl_release(thd, &mdl);
  } else {
    dd_space_id = dd_first_index(dd_table)->tablespace_id();
  }

  if (client->acquire_uncached_uncommitted<dd::Tablespace>(dd_space_id,
                                                           &dd_space)) {
    ut_a(false);
  }

  ut_a(dd_space != nullptr);

  return (mem_heap_strdup(heap, dd_space->name().c_str()));
}

/** Make sure the tablespace name is saved in dict_table_t if the table
uses a general tablespace.
Try to read it from the fil_system_t first, then from DD.
@param[in]	table		Table object
@param[in]	dd_table	Global DD table or partition object
@param[in]	dict_mutex_own)	true if dict_sys->mutex is owned already */
template <typename Table>
void dd_get_and_save_space_name(dict_table_t *table, const Table *dd_table,
                                bool dict_mutex_own) {
  /* Do this only for general tablespaces. */
  if (!DICT_TF_HAS_SHARED_SPACE(table->flags)) {
    return;
  }

  bool use_cache = true;
  if (table->tablespace != NULL) {
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

    if (space != NULL) {
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
@tparam		Table		dd::Table or dd::Partition
@param[in,out]	client		data dictionary client
@param[in]	table		MySQL table definition
@param[in]	norm_name	Table Name
@param[in]	dd_table	Global DD table or partition object
@param[in]	thd		thread THD
@param[in,out]	fk_list		stack of table names which neet to load
@return ptr to dict_table_t filled, otherwise, nullptr */
template <typename Table>
dict_table_t *dd_open_table_one(dd::cache::Dictionary_client *client,
                                const TABLE *table, const char *norm_name,
                                const Table *dd_table, THD *thd,
                                dict_names_t &fk_list) {
  ut_ad(dd_table != nullptr);

  bool implicit;
  dd::Tablespace *dd_space = nullptr;

  if (dd_table->tablespace_id() == dict_sys_t::s_dd_space_id) {
    /* DD tables are in shared DD tablespace */
    implicit = false;
  } else if (dd_tablespace_is_implicit(
                 client, dd_first_index(dd_table)->tablespace_id(), &implicit,
                 &dd_space)) {
    /* Tablespace no longer exist, it could be already dropped */
    return (nullptr);
  }

  const bool zip_allowed = srv_page_size <= UNIV_ZIP_SIZE_MAX;
  const bool strict = false;
  bool first_index = true;

  /* Create dict_table_t for the table */
  dict_table_t *m_table = dd_fill_dict_table(
      dd_table, table, norm_name, NULL, zip_allowed, strict, thd, implicit);

  if (m_table == nullptr) {
    return (nullptr);
  }

  /* Create dict_index_t for the table */
  int ret;
  ret = dd_fill_dict_index(dd_table->table(), table, m_table, NULL, zip_allowed,
                           strict, thd);

  if (ret != 0) {
    return (nullptr);
  }

  if (dd_space && !implicit) {
    const char *name = dd_space->name().c_str();
    if (name) {
      m_table->tablespace = mem_heap_strdupl(m_table->heap, name, strlen(name));
    }
  }

  if (Field **autoinc_col = table->s->found_next_number_field) {
    const dd::Properties &p = dd_table->table().se_private_data();
    dict_table_autoinc_set_col_pos(m_table, (*autoinc_col)->field_index);
    uint64 version, autoinc = 0;
    if (p.get_uint64(dd_table_key_strings[DD_TABLE_VERSION], &version) ||
        p.get_uint64(dd_table_key_strings[DD_TABLE_AUTOINC], &autoinc)) {
      ut_ad(!"problem setting AUTO_INCREMENT");
      return (nullptr);
    }

    m_table->version = version;
    dict_table_autoinc_lock(m_table);
    dict_table_autoinc_initialize(m_table, autoinc + 1);
    dict_table_autoinc_unlock(m_table);
    m_table->autoinc_persisted = autoinc;
  }

  mem_heap_t *heap = mem_heap_create(1000);
  bool fail = false;

  /* Now fill the space ID and Root page number for each index */
  dict_index_t *index = m_table->first_index();
  for (const auto dd_index : dd_table->indexes()) {
    ut_ad(index != nullptr);

    const dd::Properties &se_private_data = dd_index->se_private_data();
    uint64 id = 0;
    uint32 root = 0;
    uint32 sid = 0;
    uint64 trx_id = 0;
    dd::Object_id index_space_id = dd_index->tablespace_id();
    dd::Tablespace *index_space = nullptr;

    if (dd_table->tablespace_id() == dict_sys_t::s_dd_space_id) {
      sid = dict_sys_t::s_space_id;
    } else if (dd_table->tablespace_id() == dict_sys_t::s_dd_temp_space_id) {
      sid = dict_sys_t::s_temp_space_id;
    } else {
      if (client->acquire_uncached_uncommitted<dd::Tablespace>(index_space_id,
                                                               &index_space)) {
        my_error(ER_TABLESPACE_MISSING, MYF(0), m_table->name.m_name);
        fail = true;
        break;
      }

      if (index_space->se_private_data().get_uint32(
              dd_space_key_strings[DD_SPACE_ID], &sid)) {
        fail = true;
        break;
      }
    }

    if (first_index) {
      ut_ad(m_table->space == 0);
      m_table->space = sid;
      m_table->dd_space_id = index_space_id;

      uint32 dd_fsp_flags;
      if (dd_table->tablespace_id() == dict_sys_t::s_dd_space_id) {
        dd_fsp_flags = dict_tf_to_fsp_flags(m_table->flags);
      } else {
        ut_ad(dd_space != nullptr);
        dd_space->se_private_data().get_uint32(
            dd_space_key_strings[DD_SPACE_FLAGS], &dd_fsp_flags);
      }

      mutex_enter(&dict_sys->mutex);
      dd_load_tablespace(dd_table, m_table, heap, DICT_ERR_IGNORE_RECOVER_LOCK,
                         dd_fsp_flags);
      mutex_exit(&dict_sys->mutex);
      first_index = false;
    }

    if (se_private_data.get_uint64(dd_index_key_strings[DD_INDEX_ID], &id) ||
        se_private_data.get_uint32(dd_index_key_strings[DD_INDEX_ROOT],
                                   &root) ||
        se_private_data.get_uint64(dd_index_key_strings[DD_INDEX_TRX_ID],
                                   &trx_id)) {
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

  mutex_enter(&dict_sys->mutex);

  if (fail) {
    for (dict_index_t *index = UT_LIST_GET_LAST(m_table->indexes);
         index != nullptr; index = UT_LIST_GET_LAST(m_table->indexes)) {
      dict_index_remove_from_cache(m_table, index);
    }
    dict_mem_table_free(m_table);
    mutex_exit(&dict_sys->mutex);
    mem_heap_free(heap);

    return (nullptr);
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
    dict_table_add_to_cache(m_table, TRUE, heap);

    if (m_table->fts && dict_table_has_fts_index(m_table)) {
      fts_optimize_add_table(m_table);
    }

    if (dict_sys->dynamic_metadata != nullptr) {
      dict_table_load_dynamic_metadata(m_table);
    }
  }

  m_table->acquire();

  mutex_exit(&dict_sys->mutex);

  /* Check if this is a DD system table */
  if (m_table != nullptr) {
    char db_buf[MAX_DATABASE_NAME_LEN + 1];
    char tbl_buf[MAX_TABLE_NAME_LEN + 1];
    dd_parse_tbl_name(m_table->name.m_name, db_buf, tbl_buf, nullptr, nullptr,
                      nullptr);
    m_table->is_dd_table =
        dd::get_dictionary()->is_dd_table_name(db_buf, tbl_buf);
  }

  /* Load foreign key info. It could also register child table(s) that
  refers to current table */
  if (exist == nullptr) {
    dberr_t error =
        dd_table_load_fk(client, norm_name, nullptr, m_table,
                         &dd_table->table(), thd, false, true, &fk_list);
    if (error != DB_SUCCESS) {
      dict_table_close(m_table, FALSE, FALSE);
      m_table = nullptr;
    }
  }
  mem_heap_free(heap);

  return (m_table);
}

/** Open single table with name
@param[in]	name		table name
@param[in]	dict_locked	dict_sys mutex is held or not
@param[in,out]	fk_list		foreign key name list
@param[in]	thd		thread THD */
static void dd_open_table_one_on_name(const char *name, bool dict_locked,
                                      dict_names_t &fk_list, THD *thd) {
  dict_table_t *table = nullptr;
  const dd::Table *dd_table = nullptr;
  MDL_ticket *mdl = nullptr;

  if (!dict_locked) {
    mutex_enter(&dict_sys->mutex);
  }

  table = dict_table_check_if_in_cache_low(name);

  if (table != NULL) {
    /* If the table is in cached already, do nothing. */
    if (!dict_locked) {
      mutex_exit(&dict_sys->mutex);
    }

    return;
  } else {
    /* Otherwise, open it by dd obj. */
    char db_buf[MAX_DATABASE_NAME_LEN + 1];
    char tbl_buf[MAX_TABLE_NAME_LEN + 1];

    /* Exit sys mutex to access server info */
    mutex_exit(&dict_sys->mutex);

    if (!dd_parse_tbl_name(name, db_buf, tbl_buf, nullptr, nullptr, nullptr)) {
      goto func_exit;
    }

    if (dd_mdl_acquire(thd, &mdl, db_buf, tbl_buf)) {
      goto func_exit;
    }

    dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
    dd::cache::Dictionary_client::Auto_releaser releaser(client);

    if (client->acquire(db_buf, tbl_buf, &dd_table) || dd_table == nullptr) {
      goto func_exit;
    }

    ut_ad(dd_table->se_private_id() != dd::INVALID_OBJECT_ID);

    TABLE_SHARE ts;

    init_tmp_table_share(thd, &ts, db_buf, strlen(db_buf),
                         dd_table->name().c_str(), "" /* file name */, nullptr);

    ulint error =
        open_table_def_suppress_invalid_meta_data(thd, &ts, *dd_table);

    if (error != 0) {
      goto func_exit;
    }

    TABLE td;

    error = open_table_from_share(thd, &ts, dd_table->name().c_str(), 0,
                                  OPEN_FRM_FILE_ONLY, 0, &td, false, dd_table);

    if (error != 0) {
      free_table_share(&ts);
      goto func_exit;
    }

    table = dd_open_table_one(client, &td, name, dd_table, thd, fk_list);

    closefrm(&td, false);
    free_table_share(&ts);
  }

func_exit:
  if (table != NULL) {
    dd_table_close(table, thd, &mdl, false);
  } else {
    dd_mdl_release(thd, &mdl);
  }

  if (dict_locked) {
    mutex_enter(&dict_sys->mutex);
  }
}

/** Open foreign tables reference a table.
@param[in]	fk_list		foreign key name list
@param[in]	dict_locked	dict_sys mutex is locked or not
@param[in]	thd		thread THD */
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
@tparam		Table		dd::Table or dd::Partition
@param[in,out]	client		data dictionary client
@param[in]	table		MySQL table definition
@param[in]	norm_name	Table Name
@param[in]	dd_table	Global DD table or partition object
@param[in]	thd		thread THD
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

  return (m_table);
}

template dict_table_t *dd_open_table<dd::Table>(dd::cache::Dictionary_client *,
                                                const TABLE *, const char *,
                                                const dd::Table *, THD *);

template dict_table_t *dd_open_table<dd::Partition>(
    dd::cache::Dictionary_client *, const TABLE *, const char *,
    const dd::Partition *, THD *);

/** Get next record from a new dd system table, like mysql.tables...
@param[in,out] pcur            persistent cursor
@param[in]     mtr             the mini-transaction
@return the next rec of the dd system table */
static const rec_t *dd_getnext_system_low(btr_pcur_t *pcur, mtr_t *mtr) {
  rec_t *rec = NULL;
  bool is_comp = dict_table_is_comp(pcur->index()->table);

  while (!rec || rec_get_deleted_flag(rec, is_comp)) {
    btr_pcur_move_to_next_user_rec(pcur, mtr);

    rec = btr_pcur_get_rec(pcur);

    if (!btr_pcur_is_on_user_rec(pcur)) {
      /* end of index */
      btr_pcur_close(pcur);

      return (NULL);
    }
  }

  /* Get a record, let's save the position */
  btr_pcur_store_position(pcur, mtr);

  return (rec);
}

/** Get next record of new DD system tables
@param[in,out]	pcur		persistent cursor
@param[in]	mtr		the mini-transaction
@retval next record */
const rec_t *dd_getnext_system_rec(btr_pcur_t *pcur, mtr_t *mtr) {
  /* Restore the position */
  btr_pcur_restore_position(BTR_SEARCH_LEAF, pcur, mtr);

  return (dd_getnext_system_low(pcur, mtr));
}

/** Scan a new dd system table, like mysql.tables...
@param[in]	thd		thd
@param[in,out]	mdl		mdl lock
@param[in,out]	pcur		persistent cursor
@param[in,out]	mtr		the mini-transaction
@param[in]	system_table_name	which dd system table to open
@param[in,out]	table		dict_table_t obj of dd system table
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
  btr_pcur_open_at_index_side(true, clust_index, BTR_SEARCH_LEAF, pcur, true, 0,
                              mtr);

  rec = dd_getnext_system_low(pcur, mtr);

  return (rec);
}

/**
  All DD tables would contain DB_TRX_ID and DB_ROLL_PTR fields
  before other fields. This offset indicates the position at
  which the first DD column is located.
*/
static const int DD_FIELD_OFFSET = 2;

/** Process one mysql.tables record and get the dict_table_t
@param[in]	heap		temp memory heap
@param[in,out]	rec		mysql.tables record
@param[in,out]	table		dict_table_t to fill
@param[in]	dd_tables	dict_table_t obj of dd system table
@param[in]	mdl		mdl on the table
@param[in]	mtr		the mini-transaction
@retval error message, or NULL on success */
const char *dd_process_dd_tables_rec_and_mtr_commit(
    mem_heap_t *heap, const rec_t *rec, dict_table_t **table,
    dict_table_t *dd_tables, MDL_ticket **mdl, mtr_t *mtr) {
  ulint len;
  const byte *field;
  const char *err_msg = NULL;
  ulint table_id;

  ut_ad(!rec_get_deleted_flag(rec, dict_table_is_comp(dd_tables)));
  ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));

  ulint *offsets = rec_get_offsets(rec, dd_tables->first_index(), NULL,
                                   ULINT_UNDEFINED, &heap);

  const dd::Object_table &dd_object_table = dd::get_dd_table<dd::Table>();

  field = rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_ENGINE") + DD_FIELD_OFFSET, &len);

  /* If "engine" field is not "innodb", return. */
  if (strncmp((const char *)field, "InnoDB", 6) != 0) {
    *table = NULL;
    mtr_commit(mtr);
    return (err_msg);
  }

  /* Get the se_private_id field. */
  field = (const byte *)rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_SE_PRIVATE_ID") + DD_FIELD_OFFSET,
      &len);

  if (len != 8) {
    *table = NULL;
    mtr_commit(mtr);
    return (err_msg);
  }

  /* Get the table id */
  table_id = mach_read_from_8(field);

  /* Skip mysql.* tables. */
  if (dict_sys_t::is_dd_table_id(table_id)) {
    *table = NULL;
    mtr_commit(mtr);
    return (err_msg);
  }

  /* Commit before load the table again */
  mtr_commit(mtr);
  THD *thd = current_thd;

  *table = dd_table_open_on_id(table_id, thd, mdl, true, false);

  if (!(*table)) {
    err_msg = "Table not found";
  }

  return (err_msg);
}

/** Process one mysql.table_partitions record and get the dict_table_t
@param[in]	heap		temp memory heap
@param[in,out]	rec		mysql.table_partitions record
@param[in,out]	table		dict_table_t to fill
@param[in]	dd_tables	dict_table_t obj of dd partition table
@param[in]	mdl		mdl on the table
@param[in]	mtr		the mini-transaction
@retval error message, or NULL on success */
const char *dd_process_dd_partitions_rec_and_mtr_commit(
    mem_heap_t *heap, const rec_t *rec, dict_table_t **table,
    dict_table_t *dd_tables, MDL_ticket **mdl, mtr_t *mtr) {
  ulint len;
  const byte *field;
  const char *err_msg = NULL;
  ulint table_id;

  ut_ad(mtr_memo_contains_page(mtr, rec, MTR_MEMO_PAGE_S_FIX));

  ut_ad(!rec_get_deleted_flag(rec, dict_table_is_comp(dd_tables)));

  ulint *offsets = rec_get_offsets(rec, dd_tables->first_index(), NULL,
                                   ULINT_UNDEFINED, &heap);

  const dd::Object_table &dd_object_table = dd::get_dd_table<dd::Partition>();

  /* Get the engine field. */
  field = rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_ENGINE") + DD_FIELD_OFFSET, &len);

  /* If "engine" field is not "innodb", return. */
  if (strncmp((const char *)field, "InnoDB", 6) != 0) {
    *table = NULL;
    mtr_commit(mtr);
    return (err_msg);
  }

  /* Get the se_private_id field. */
  field = (const byte *)rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_SE_PRIVATE_ID") + DD_FIELD_OFFSET,
      &len);
  /* When table is partitioned table, the se_private_id is null. */
  if (len != 8) {
    *table = NULL;
    mtr_commit(mtr);
    return (err_msg);
  }

  /* Get the table id */
  table_id = mach_read_from_8(field);

  /* Skip mysql.* tables. */
  if (dict_sys_t::is_dd_table_id(table_id)) {
    *table = NULL;
    mtr_commit(mtr);
    return (err_msg);
  }

  /* Commit before load the table again */
  mtr_commit(mtr);
  THD *thd = current_thd;

  *table = dd_table_open_on_id(table_id, thd, mdl, true, false);

  if (!(*table)) {
    err_msg = "Table not found";
  }

  return (err_msg);
}

/** Process one mysql.columns record and get info to dict_col_t
@param[in,out]	heap		temp memory heap
@param[in]	rec		mysql.columns record
@param[in,out]	col		dict_col_t to fill
@param[in,out]	table_id	table id
@param[in,out]	col_name	column name
@param[in,out]	nth_v_col	nth v column
@param[in]	dd_columns	dict_table_t obj of mysql.columns
@param[in,out]	mtr		the mini-transaction
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

  ulint *offsets = rec_get_offsets(rec, dd_columns->first_index(), NULL,
                                   ULINT_UNDEFINED, &heap);

  const dd::Object_table &dd_object_table = dd::get_dd_table<dd::Column>();

  /* Get the hidden attribute, and skip if it's a hidden column. */
  field = (const byte *)rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_HIDDEN") + DD_FIELD_OFFSET, &len);
  hidden = static_cast<dd::Column::enum_hidden_type>(mach_read_from_1(field));
  if (hidden == dd::Column::enum_hidden_type::HT_HIDDEN_SE) {
    mtr_commit(mtr);
    return (false);
  }

  /* Get the column name. */
  field = (const byte *)rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_NAME") + DD_FIELD_OFFSET, &len);
  *col_name = mem_heap_strdupl(heap, (const char *)field, len);

  /* Get the position. */
  field = (const byte *)rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_ORDINAL_POSITION") + DD_FIELD_OFFSET,
      &len);
  pos = mach_read_from_4(field) - 1;

  /* Get the is_virtual attribute. */
  field = (const byte *)rec_get_nth_field(rec, offsets, 21, &len);
  is_virtual = mach_read_from_1(field) & 0x01;

  /* Get the se_private_data field. */
  field = (const byte *)rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_SE_PRIVATE_DATA") + DD_FIELD_OFFSET,
      &len);

  if (len == 0 || len == UNIV_SQL_NULL) {
    mtr_commit(mtr);
    return (false);
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
    return (false);
  }

  if (!p->get_uint64(dd_index_key_strings[DD_TABLE_ID], (uint64 *)table_id)) {
    THD *thd = current_thd;
    dict_table_t *table;
    MDL_ticket *mdl = NULL;

    /* Commit before we try to load the table. */
    mtr_commit(mtr);
    table = dd_table_open_on_id(*table_id, thd, &mdl, true, true);

    if (!table) {
      delete p;
      return (false);
    }

    if (is_virtual) {
      vcol = dict_table_get_nth_v_col_mysql(table, pos);

      if (vcol == nullptr) {
        dd_table_close(table, thd, &mdl, true);
        delete p;
        return (false);
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
    return (false);
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

  return (true);
}

/** Process one mysql.columns record for virtual columns
@param[in]	heap		temp memory heap
@param[in,out]	rec		mysql.columns record
@param[in,out]	table_id	table id
@param[in,out]	pos		position
@param[in,out]	base_pos	base column position
@param[in,out]	n_row		number of rows
@param[in]	dd_columns	dict_table_t obj of mysql.columns
@param[in]	mtr		the mini-transaction
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

  ulint *offsets = rec_get_offsets(rec, dd_columns->first_index(), NULL,
                                   ULINT_UNDEFINED, &heap);

  const dd::Object_table &dd_object_table = dd::get_dd_table<dd::Column>();

  /* Get the is_virtual attribute, and skip if it's not a virtual column. */
  field = (const byte *)rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_IS_VIRTUAL") + DD_FIELD_OFFSET, &len);
  is_virtual = mach_read_from_1(field) & 0x01;
  if (!is_virtual) {
    mtr_commit(mtr);
    return (false);
  }

  /* Get the hidden attribute, and skip if it's a hidden column. */
  field = (const byte *)rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_HIDDEN") + DD_FIELD_OFFSET, &len);
  hidden = static_cast<dd::Column::enum_hidden_type>(mach_read_from_1(field));
  if (hidden == dd::Column::enum_hidden_type::HT_HIDDEN_SE) {
    mtr_commit(mtr);
    return (false);
  }

  /* Get the position. */
  field = (const byte *)rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_ORDINAL_POSITION") + DD_FIELD_OFFSET,
      &len);
  origin_pos = mach_read_from_4(field) - 1;

  /* Get the se_private_data field. */
  field = (const byte *)rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_SE_PRIVATE_DATA") + DD_FIELD_OFFSET,
      &len);

  if (len == 0 || len == UNIV_SQL_NULL) {
    mtr_commit(mtr);
    return (false);
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
    return (false);
  }

  if (!p->get_uint64(dd_index_key_strings[DD_TABLE_ID], (uint64 *)table_id)) {
    THD *thd = current_thd;
    dict_table_t *table;
    MDL_ticket *mdl = NULL;
    dict_v_col_t *vcol = NULL;

    /* Commit before we try to load the table. */
    mtr_commit(mtr);
    table = dd_table_open_on_id(*table_id, thd, &mdl, true, true);

    if (!table) {
      delete p;
      return (false);
    }

    vcol = dict_table_get_nth_v_col_mysql(table, origin_pos);

    if (vcol == NULL || vcol->num_base == 0) {
      dd_table_close(table, thd, &mdl, true);
      delete p;
      return (false);
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
    return (false);
  }

  return (true);
}
/** Process one mysql.indexes record and get dict_index_t
@param[in]	heap		temp memory heap
@param[in,out]	rec		mysql.indexes record
@param[in,out]	index		dict_index_t to fill
@param[in]	mdl		mdl on index->table
@param[in,out]	parent		parent table if it's fts aux table.
@param[in,out]	parent_mdl	mdl on parent if it's fts aux table.
@param[in]	dd_indexes	dict_table_t obj of mysql.indexes
@param[in]	mtr		the mini-transaction
@retval true if index is filled */
bool dd_process_dd_indexes_rec(mem_heap_t *heap, const rec_t *rec,
                               const dict_index_t **index, MDL_ticket **mdl,
                               dict_table_t **parent, MDL_ticket **parent_mdl,
                               dict_table_t *dd_indexes, mtr_t *mtr) {
  ulint len;
  const byte *field;
  uint32 index_id;
  uint32 space_id;
  uint64 table_id;

  *index = nullptr;

  ut_ad(!rec_get_deleted_flag(rec, dict_table_is_comp(dd_indexes)));

  ulint *offsets = rec_get_offsets(rec, dd_indexes->first_index(), NULL,
                                   ULINT_UNDEFINED, &heap);

  const dd::Object_table &dd_object_table = dd::get_dd_table<dd::Index>();

  field = rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_ENGINE") + DD_FIELD_OFFSET, &len);

  /* If "engine" field is not "innodb", return. */
  if (strncmp((const char *)field, "InnoDB", 6) != 0) {
    mtr_commit(mtr);
    return (false);
  }

  /* Get the se_private_data field. */
  field = (const byte *)rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_SE_PRIVATE_DATA") + DD_FIELD_OFFSET,
      &len);

  if (len == 0 || len == UNIV_SQL_NULL) {
    mtr_commit(mtr);
    return (false);
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
    return (false);
  }

  if (p->get_uint32(dd_index_key_strings[DD_INDEX_ID], &index_id)) {
    delete p;
    mtr_commit(mtr);
    return (false);
  }

  /* Get the tablespace id. */
  if (p->get_uint32(dd_index_key_strings[DD_INDEX_SPACE_ID], &space_id)) {
    delete p;
    mtr_commit(mtr);
    return (false);
  }

  /* Skip mysql.* indexes. */
  if (space_id == dict_sys->s_space_id) {
    delete p;
    mtr_commit(mtr);
    return (false);
  }

  /* Load the table and get the index. */
  if (!p->exists(dd_index_key_strings[DD_TABLE_ID])) {
    delete p;
    mtr_commit(mtr);
    return (false);
  }

  if (!p->get_uint64(dd_index_key_strings[DD_TABLE_ID], &table_id)) {
    THD *thd = current_thd;
    dict_table_t *table;

    /* Commit before load the table */
    mtr_commit(mtr);
    table = dd_table_open_on_id(table_id, thd, mdl, true, true);

    if (!table) {
      delete p;
      return (false);
    }

    /* For fts aux table, we need to acuqire mdl lock on parent. */
    if (table->is_fts_aux()) {
      fts_aux_table_t fts_table;
      fts_is_aux_table_name(&fts_table, table->name.m_name,
                            strlen(table->name.m_name));
      table_id_t parent_id = fts_table.parent_id;

      dd_table_close(table, thd, mdl, true);

      *parent = dd_table_open_on_id(parent_id, thd, parent_mdl, true, true);

      if (*parent == nullptr) {
        delete p;
        return (false);
      }

      table = dd_table_open_on_id(table_id, thd, mdl, true, true);

      if (!table) {
        dd_table_close(*parent, thd, parent_mdl, true);
        delete p;
        return (false);
      }
    }

    for (const dict_index_t *t_index = table->first_index(); t_index != NULL;
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
      return (false);
    }

    delete p;
  } else {
    delete p;
    mtr_commit(mtr);
    return (false);
  }

  return (true);
}

/** Process one mysql.indexes record and get breif info to dict_index_t
@param[in]	heap		temp memory heap
@param[in,out]	rec		mysql.indexes record
@param[in,out]	index_id	index id
@param[in,out]	space_id	space id
@param[in]	dd_indexes	dict_table_t obj of mysql.indexes
@retval true if index is filled */
bool dd_process_dd_indexes_rec_simple(mem_heap_t *heap, const rec_t *rec,
                                      space_index_t *index_id,
                                      space_id_t *space_id,
                                      dict_table_t *dd_indexes) {
  ulint len;
  const byte *field;
  uint32 idx_id;

  ut_ad(!rec_get_deleted_flag(rec, dict_table_is_comp(dd_indexes)));

  ulint *offsets = rec_get_offsets(rec, dd_indexes->first_index(), NULL,
                                   ULINT_UNDEFINED, &heap);

  const dd::Object_table &dd_object_table = dd::get_dd_table<dd::Index>();

  field = rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_ENGINE") + DD_FIELD_OFFSET, &len);

  /* If "engine" field is not "innodb", return. */
  if (strncmp((const char *)field, "InnoDB", 6) != 0) {
    return (false);
  }

  /* Get the se_private_data field. */
  field = (const byte *)rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_SE_PRIVATE_DATA") + DD_FIELD_OFFSET,
      &len);

  if (len == 0 || len == UNIV_SQL_NULL) {
    return (false);
  }

  /* Get index id. */
  dd::String_type prop((char *)field);
  dd::Properties *p = dd::Properties::parse_properties(prop);

  if (!p || !p->exists(dd_index_key_strings[DD_INDEX_ID]) ||
      !p->exists(dd_index_key_strings[DD_INDEX_SPACE_ID])) {
    if (p) {
      delete p;
    }
    return (false);
  }

  if (p->get_uint32(dd_index_key_strings[DD_INDEX_ID], &idx_id)) {
    delete p;
    return (false);
  }
  *index_id = idx_id;

  /* Get the tablespace_id. */
  if (p->get_uint32(dd_index_key_strings[DD_INDEX_SPACE_ID], space_id)) {
    delete p;
    return (false);
  }

  delete p;

  return (true);
}

/** Process one mysql.tablespaces record and get info
@param[in]	heap		temp memory heap
@param[in,out]	rec		mysql.tablespaces record
@param[in,out]	space_id	space id
@param[in,out]	name		space name
@param[in,out]	flags		space flags
@param[in,out]	server_version	server version
@param[in,out]	space_version	space version
@param[in,out]	is_encrypted	true if tablespace is encrypted
@param[in]	dd_spaces	dict_table_t obj of mysql.tablespaces
@return true if data is retrived */
bool dd_process_dd_tablespaces_rec(mem_heap_t *heap, const rec_t *rec,
                                   space_id_t *space_id, char **name,
                                   uint *flags, uint32 *server_version,
                                   uint32 *space_version, bool *is_encrypted,
                                   dict_table_t *dd_spaces) {
  ulint len;
  const byte *field;
  char *prop_str;

  ut_ad(!rec_get_deleted_flag(rec, dict_table_is_comp(dd_spaces)));

  ulint *offsets = rec_get_offsets(rec, dd_spaces->first_index(), NULL,
                                   ULINT_UNDEFINED, &heap);

  const dd::Object_table &dd_object_table = dd::get_dd_table<dd::Tablespace>();

  field = rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_ENGINE") + DD_FIELD_OFFSET, &len);

  /* If "engine" field is not "innodb", return. */
  if (strncmp((const char *)field, "InnoDB", 6) != 0) {
    return (false);
  }

  /* Get name field. */
  field = rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_NAME") + DD_FIELD_OFFSET, &len);
  *name = reinterpret_cast<char *>(mem_heap_zalloc(heap, len + 1));
  memcpy(*name, field, len);

  /* Get the se_private_data field. */
  field = (const byte *)rec_get_nth_field(
      rec, offsets,
      dd_object_table.field_number("FIELD_SE_PRIVATE_DATA") + DD_FIELD_OFFSET,
      &len);

  if (len == 0 || len == UNIV_SQL_NULL) {
    return (false);
  }

  prop_str = static_cast<char *>(mem_heap_zalloc(heap, len + 1));
  memcpy(prop_str, field, len);
  dd::String_type prop(prop_str);
  dd::Properties *p = dd::Properties::parse_properties(prop);

  if (!p || !p->exists(dd_space_key_strings[DD_SPACE_ID]) ||
      !p->exists(dd_index_key_strings[DD_SPACE_FLAGS])) {
    if (p) {
      delete p;
    }
    return (false);
  }

  /* Get space id. */
  if (p->get_uint32(dd_space_key_strings[DD_SPACE_ID], space_id)) {
    delete p;
    return (false);
  }

  /* Get space flag. */
  if (p->get_uint32(dd_space_key_strings[DD_SPACE_FLAGS], flags)) {
    delete p;
    return (false);
  }

  /* Get server flag. */
  if (p->get_uint32(dd_space_key_strings[DD_SPACE_SERVER_VERSION],
                    server_version)) {
    delete p;
    return (false);
  }

  /* Get space flag. */
  if (p->get_uint32(dd_space_key_strings[DD_SPACE_VERSION], space_version)) {
    delete p;
    return (false);
  }

  /* Get Encryption. */
  if (FSP_FLAGS_GET_ENCRYPTION(*flags)) {
    *is_encrypted = true;
  }

  delete p;

  return (true);
}

/** Get dd tablespace id for fts table
@param[in]	parent_table	parent table of fts table
@param[in]	table		fts table
@param[in,out]	dd_space_id	dd table space id
@return true on success, false on failure. */
bool dd_get_fts_tablespace_id(const dict_table_t *parent_table,
                              const dict_table_t *table,
                              dd::Object_id &dd_space_id) {
  char db_name[MAX_DATABASE_NAME_LEN + 1];
  char table_name[MAX_TABLE_NAME_LEN + 1];

  dd_parse_tbl_name(parent_table->name.m_name, db_name, table_name, nullptr,
                    nullptr, nullptr);

  THD *thd = current_thd;
  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  dd::Object_id space_id = parent_table->dd_space_id;

  dd_space_id = dd::INVALID_OBJECT_ID;

  if (dict_table_is_file_per_table(table)) {
    /* This means user table and file_per_table */
    bool ret;
    char *filename = fil_space_get_first_path(table->space);

    ret = dd_create_implicit_tablespace(client, thd, table->space,
                                        table->name.m_name, filename, false,
                                        dd_space_id);

    ut_free(filename);
    if (ret) {
      return (false);
    }

  } else if (table->space != TRX_SYS_SPACE &&
             table->space != srv_tmp_space.space_id()) {
    /* This is a user table that resides in shared tablespace */
    ut_ad(!dict_table_is_file_per_table(table));
    ut_ad(DICT_TF_HAS_SHARED_SPACE(table->flags));

    /* Currently the tablespace id is hard coded as 0 */
    dd_space_id = space_id;

    const dd::Tablespace *index_space = NULL;
    if (client->acquire<dd::Tablespace>(space_id, &index_space)) {
      return (false);
    }

    uint32 id;
    if (index_space == NULL) {
      return (false);
    } else if (index_space->se_private_data().get_uint32(
                   dd_space_key_strings[DD_SPACE_ID], &id) ||
               id != table->space) {
      ut_ad(!"missing or incorrect tablespace id");
      return (false);
    }
  } else if (table->space == TRX_SYS_SPACE) {
    /* This is a user table that resides in innodb_system
    tablespace */
    ut_ad(!dict_table_is_file_per_table(table));
    dd_space_id = dict_sys_t::s_dd_sys_space_id;
  }

  return (true);
}

/** Set table options for fts dd tables according to dict table
@param[in,out]	dd_table	dd table instance
@param[in]	table		dict table instance */
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
      ut_a(0);
  }

  dd_table->set_row_format(row_format);

  /* FTS AUX tables are always not encrypted/compressed
  as it is designed now. So both "compress" and "encrypt_type"
  option are not set */

  dd::Properties *table_options = &dd_table->options();
  table_options->set_bool("pack_record", true);
  table_options->set_bool("checksum", false);
  table_options->set_bool("delay_key_write", false);
  table_options->set_uint32("avg_row_length", 0);
  table_options->set_uint32("stats_sample_pages", 0);
  table_options->set_uint32("stats_auto_recalc", HA_STATS_AUTO_RECALC_DEFAULT);

  if (auto zip_ssize = DICT_TF_GET_ZIP_SSIZE(table->flags)) {
    table_options->set_uint32("key_block_size", 1 << (zip_ssize - 1));
  } else {
    table_options->set_uint32("key_block_size", 0);
  }
}

/** Add nullability info to column se_private_data
@param[in,out]	dd_col	DD table column
@param[in]	col	InnoDB table column */
static void dd_set_fts_nullability(dd::Column *dd_col, const dict_col_t *col) {
  bool is_nullable = !(col->prtype & DATA_NOT_NULL);
  dd::Properties &p = dd_col->se_private_data();
  p.set_bool("nullable", is_nullable);
}

/** Create dd table for fts aux index table
@param[in]	parent_table	parent table of fts table
@param[in,out]	table		fts table
@param[in]	charset		fts index charset
@return true on success, false on failure */
bool dd_create_fts_index_table(const dict_table_t *parent_table,
                               dict_table_t *table,
                               const CHARSET_INFO *charset) {
  ut_ad(charset != nullptr);

  char db_name[MAX_DATABASE_NAME_LEN + 1];
  char table_name[MAX_TABLE_NAME_LEN + 1];

  dd_parse_tbl_name(table->name.m_name, db_name, table_name, nullptr, nullptr,
                    nullptr);

  /* Create dd::Table object */
  THD *thd = current_thd;
  dd::Schema_MDL_locker mdl_locker(thd);
  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  const dd::Schema *schema = nullptr;
  if (mdl_locker.ensure_locked(db_name) ||
      client->acquire<dd::Schema>(db_name, &schema)) {
    return (false);
  }

  /* Check if schema is nullptr? */
  if (schema == nullptr) {
    my_error(ER_BAD_DB_ERROR, MYF(0), db_name);
    return (false);
  }

  std::unique_ptr<dd::Table> dd_table_obj(schema->create_table(thd));
  dd::Table *dd_table = dd_table_obj.get();

  dd_table->set_name(table_name);
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

  index->options().set_uint32("flags", 32);

  dd::Index_element *index_elem;
  index_elem = index->add_element(key_col1);
  index_elem->set_length(FTS_INDEX_WORD_LEN);

  index_elem = index->add_element(key_col2);
  index_elem->set_length(FTS_INDEX_FIRST_DOC_ID_LEN);

  /* Fill table space info, etc */
  dd::Object_id dd_space_id;
  if (!dd_get_fts_tablespace_id(parent_table, table, dd_space_id)) {
    return (false);
  }

  table->dd_space_id = dd_space_id;

  dd_write_table(dd_space_id, dd_table, table);

  MDL_ticket *mdl_ticket = NULL;
  if (dd::acquire_exclusive_table_mdl(thd, db_name, table_name, false,
                                      &mdl_ticket)) {
    ut_ad(0);
    return (false);
  }

  /* Store table to dd */
  bool fail = client->store(dd_table);
  if (fail) {
    ut_ad(0);
    return (false);
  }

  return (true);
}

/** Create dd table for fts aux common table
@param[in]	parent_table	parent table of fts table
@param[in,out]	table		fts table
@param[in]	is_config	flag whether it's fts aux configure table
@return true on success, false on failure */
bool dd_create_fts_common_table(const dict_table_t *parent_table,
                                dict_table_t *table, bool is_config) {
  char db_name[MAX_DATABASE_NAME_LEN + 1];
  char table_name[MAX_TABLE_NAME_LEN + 1];

  dd_parse_tbl_name(table->name.m_name, db_name, table_name, nullptr, nullptr,
                    nullptr);

  /* Create dd::Table object */
  THD *thd = current_thd;
  dd::Schema_MDL_locker mdl_locker(thd);
  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  const dd::Schema *schema = nullptr;
  if (mdl_locker.ensure_locked(db_name) ||
      client->acquire<dd::Schema>(db_name, &schema)) {
    return (false);
  }

  /* Check if schema is nullptr */
  if (schema == nullptr) {
    my_error(ER_BAD_DB_ERROR, MYF(0), db_name);
    return (false);
  }

  std::unique_ptr<dd::Table> dd_table_obj(schema->create_table(thd));
  dd::Table *dd_table = dd_table_obj.get();

  dd_table->set_name(table_name);
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

    index->options().set_uint32("flags", 32);

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

    index->options().set_uint32("flags", 32);

    dd::Index_element *index_elem;
    index_elem = index->add_element(key_col1);
    index_elem->set_length(FTS_CONFIG_TABLE_KEY_COL_LEN);
  }

  /* Fill table space info, etc */
  dd::Object_id dd_space_id;
  if (!dd_get_fts_tablespace_id(parent_table, table, dd_space_id)) {
    ut_ad(0);
    return (false);
  }

  table->dd_space_id = dd_space_id;

  dd_write_table(dd_space_id, dd_table, table);

  MDL_ticket *mdl_ticket = NULL;
  if (dd::acquire_exclusive_table_mdl(thd, db_name, table_name, false,
                                      &mdl_ticket)) {
    return (false);
  }

  /* Store table to dd */
  bool fail = client->store(dd_table);
  if (fail) {
    ut_ad(0);
    return (false);
  }

  return (true);
}

/** Drop dd table & tablespace for fts aux table
@param[in]	name		table name
@param[in]	file_per_table	flag whether use file per table
@return true on success, false on failure. */
bool dd_drop_fts_table(const char *name, bool file_per_table) {
  char db_name[MAX_DATABASE_NAME_LEN + 1];
  char table_name[MAX_TABLE_NAME_LEN + 1];

  dd_parse_tbl_name(name, db_name, table_name, nullptr, nullptr, nullptr);

  /* Create dd::Table object */
  THD *thd = current_thd;
  dd::Schema_MDL_locker mdl_locker(thd);
  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  MDL_ticket *mdl_ticket = NULL;
  if (dd::acquire_exclusive_table_mdl(thd, db_name, table_name, false,
                                      &mdl_ticket)) {
    return (false);
  }

  const dd::Table *dd_table = nullptr;
  if (client->acquire<dd::Table>(db_name, table_name, &dd_table)) {
    return (false);
  }

  if (dd_table == nullptr) {
    return (false);
  }

  if (file_per_table) {
    dd::Object_id dd_space_id = (*dd_table->indexes().begin())->tablespace_id();
    bool error;
    error = dd_drop_tablespace(client, thd, dd_space_id);
    ut_a(!error);
  }

  if (client->drop(dd_table)) {
    return (false);
  }

  return (true);
}

/** Rename dd table & tablespace files for fts aux table
@param[in]	table		dict table
@param[in]	old_name	old innodb table name
@return true on success, false on failure. */
bool dd_rename_fts_table(const dict_table_t *table, const char *old_name) {
  char new_db_name[MAX_DATABASE_NAME_LEN + 1];
  char new_table_name[MAX_TABLE_NAME_LEN + 1];
  char old_db_name[MAX_DATABASE_NAME_LEN + 1];
  char old_table_name[MAX_TABLE_NAME_LEN + 1];
  char *new_name = table->name.m_name;

  dd_parse_tbl_name(new_name, new_db_name, new_table_name, nullptr, nullptr,
                    nullptr);
  dd_parse_tbl_name(old_name, old_db_name, old_table_name, nullptr, nullptr,
                    nullptr);

  ut_ad(strcmp(new_db_name, old_db_name) != 0);
  ut_ad(strcmp(new_table_name, old_table_name) == 0);

  /* Create dd::Table object */
  THD *thd = current_thd;
  dd::Schema_MDL_locker mdl_locker(thd);
  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  const dd::Schema *to_sch = nullptr;
  if (client->acquire<dd::Schema>(new_db_name, &to_sch)) {
    return (false);
  }

  MDL_ticket *mdl_ticket = nullptr;
  if (dd::acquire_exclusive_table_mdl(thd, old_db_name, old_table_name, false,
                                      &mdl_ticket)) {
    return (false);
  }

  MDL_ticket *mdl_ticket2 = nullptr;
  if (dd::acquire_exclusive_table_mdl(thd, new_db_name, new_table_name, false,
                                      &mdl_ticket2)) {
    return (false);
  }

  dd::Table *dd_table = nullptr;
  if (client->acquire_for_modification<dd::Table>(old_db_name, old_table_name,
                                                  &dd_table)) {
    return (false);
  }

  // Set schema id
  dd_table->set_schema_id(to_sch->id());

  /* Rename dd tablespace file */
  if (dict_table_is_file_per_table(table)) {
    char *new_path = fil_space_get_first_path(table->space);

    if (dd_rename_tablespace(table->dd_space_id, table->name.m_name,
                             new_path) != DB_SUCCESS) {
      ut_a(false);
    }

    ut_free(new_path);
  }

  if (client->update(dd_table)) {
    ut_ad(0);
    return (false);
  }

  return (true);
}

/** Set Discard attribute in se_private_data of tablespace
@param[in,out]	dd_space	dd::Tablespace object
@param[in]	discard		true if discarded, else false */
void dd_tablespace_set_discard(dd::Tablespace *dd_space, bool discard) {
  dd::Properties &p = dd_space->se_private_data();
  p.set_bool(dd_space_key_strings[DD_SPACE_DISCARD], discard);
}

/** Get discard attribute value stored in se_private_dat of tablespace
@param[in]	dd_space	dd::Tablespace object
@retval		true		if Tablespace is discarded
@retval		false		if attribute doesn't exist or if the
                                tablespace is not discarded */
bool dd_tablespace_get_discard(const dd::Tablespace *dd_space) {
  const dd::Properties &p = dd_space->se_private_data();
  if (p.exists(dd_space_key_strings[DD_SPACE_DISCARD])) {
    bool is_discarded;
    p.get_bool(dd_space_key_strings[DD_SPACE_DISCARD], &is_discarded);
    return (is_discarded);
  }
  return (false);
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
  return (indexes_count);
}
#endif /* UNIV_DEBUG */

/** Open a table from its database and table name, this is currently used by
foreign constraint parser to get the referenced table.
@param[in]	name			foreign key table name
@param[in]	database_name		table db name
@param[in]	database_name_len	db name length
@param[in]	table_name		table db name
@param[in]	table_name_len		table name length
@param[in,out]	table			table object or NULL
@param[in,out]	mdl			mdl on table
@param[in,out]	heap			heap memory
@return complete table name with database and table name, allocated from
heap memory passed in */
char *dd_get_referenced_table(const char *name, const char *database_name,
                              ulint database_name_len, const char *table_name,
                              ulint table_name_len, dict_table_t **table,
                              MDL_ticket **mdl, mem_heap_t *heap) {
  char *ref;
  const char *db_name;
  bool is_part;

  is_part = (strstr(name, PART_SEPARATOR) != nullptr);

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

  return (ref);
}

/** Update all InnoDB tablespace cache objects. This step is done post
dictionary trx rollback, binlog recovery and DDL_LOG apply. So DD is consistent.
Update the cached tablespace objects, if they differ from dictionary
@param[in,out]	thd	thread handle
@retval	true	on error
@retval	false	on success */
bool dd_tablespace_update_cache(THD *thd) {
  /* If there are no prepared trxs, then DD reads would have been
  already consistent. No need to update cache */
  if (!trx_sys->found_prepared_trx) {
    return (false);
  }

  dd::cache::Dictionary_client *dc = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(dc);
  std::vector<const dd::Tablespace *> tablespaces;

  space_id_t max_id = 0;

  if (dc->fetch_global_components(&tablespaces)) {
    return (true);
  }

  bool fail = false;

  for (const dd::Tablespace *t : tablespaces) {
    ut_ad(!fail);

    if (t->engine() != innobase_hton_name) {
      continue;
    }

    const dd::Properties &p = t->se_private_data();
    uint32 id;
    uint32 flags = 0;

    /* There should be exactly one file name associated
    with each InnoDB tablespace, except innodb_system */
    fail = p.get_uint32(dd_space_key_strings[DD_SPACE_ID], &id) ||
           p.get_uint32(dd_space_key_strings[DD_SPACE_FLAGS], &flags) ||
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

      /* Exclude Encryption flag as (un)encryption operation might be rolling
      forward in background thread. */
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
      dberr_t err = fil_ibd_open(false, purpose, id, flags, space_name, nullptr,
                                 filename, false, false);
      switch (err) {
        case DB_SUCCESS:
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
  }

  fil_set_max_space_id_if_bigger(max_id);
  return (fail);
}

/* Check if the table belongs to an encrypted tablespace.
@param[in]	table	table for which check is to be done
@return true if it does. */
bool dd_is_table_in_encrypted_tablespace(const dict_table_t *table) {
  fil_space_t *space = fil_space_get(table->space);
  if (space != nullptr) {
    return (FSP_FLAGS_GET_ENCRYPTION(space->flags));
  } else {
    /* Its possible that tablespace flag is missing (for ex: after
    discard tablespace). In that case get tablespace flags from Data
    Dictionary/ */
    THD *thd = current_thd;
    dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
    dd::cache::Dictionary_client::Auto_releaser releaser(client);
    dd::Tablespace *dd_space;

    if (!client->acquire_uncached_uncommitted<dd::Tablespace>(
            table->dd_space_id, &dd_space)) {
      ut_ad(dd_space);
      uint32 flags;
      dd_space->se_private_data().get_uint32(
          dd_space_key_strings[DD_SPACE_FLAGS], &flags);

      return (FSP_FLAGS_GET_ENCRYPTION(flags));
    }
    /* We should not reach here */
    ut_ad(0);
    return false;
  }
}
#endif /* !UNIV_HOTBACKUP */
