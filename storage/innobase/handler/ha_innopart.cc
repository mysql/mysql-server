/*****************************************************************************

Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

/** @file ha_innopart.cc
Code for native partitioning in InnoDB.

Created Nov 22, 2013 Mattias Jonsson */

/* Include necessary SQL headers */
#include <debug_sync.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <log.h>
#include <my_check_opt.h>
#include <mysqld.h>
#include <sql_acl.h>
#include <sql_class.h>
#include <sql_show.h>
#include <sql_table.h>
#include <strfunc.h>
#include <new>

#include "dd/dd.h"
#include "dd/dictionary.h"
#include "dd/properties.h"
#include "dd/types/partition.h"
#include "dd/types/table.h"

/* Include necessary InnoDB headers */
#include "btr0sea.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "dict0priv.h"
#include "dict0stats.h"
#include "fsp0sysspace.h"
#include "ha_innodb.h"
#include "ha_innopart.h"
#include "key.h"
#include "lex_string.h"
#include "lock0lock.h"
#include "my_dbug.h"
#include "my_io.h"
#include "my_macros.h"
#include "partition_info.h"
#include "row0import.h"
#include "row0ins.h"
#include "row0merge.h"
#include "row0mysql.h"
#include "row0quiesce.h"
#include "row0sel.h"
#include "row0upd.h"
#include "univ.i"
#include "ut0ut.h"

/* To be backwards compatible we also fold partition separator on windows. */

Ha_innopart_share::Ha_innopart_share(TABLE_SHARE *table_share)
    : Partition_share(),
      m_table_parts(),
      m_index_mapping(),
      m_tot_parts(),
      m_index_count(),
      m_ref_count(),
      m_table_share(table_share) {}

Ha_innopart_share::~Ha_innopart_share() {
  ut_ad(m_ref_count == 0);
  if (m_table_parts != NULL) {
    ut_free(m_table_parts);
    m_table_parts = NULL;
  }
  if (m_index_mapping != NULL) {
    ut_free(m_index_mapping);
    m_index_mapping = NULL;
  }
}

/** Fold to lower case if windows or lower_case_table_names == 1.
@param[in,out]	s	String to fold.*/
void Ha_innopart_share::partition_name_casedn_str(char *s) {
#ifdef _WIN32
  innobase_casedn_str(s);
#endif
}

/** Translate and append partition name.
@param[out]	to	String to write in filesystem charset
@param[in]	from	Name in system charset
@param[in]	sep	Separator
@param[in]	len	Max length of to buffer
@return	length of written string. */
size_t Ha_innopart_share::append_sep_and_name(char *to, const char *from,
                                              const char *sep, size_t len) {
  size_t ret;
  size_t sep_len = strlen(sep);
  ut_ad(len > sep_len + strlen(from));
  ut_ad(to != NULL);
  ut_ad(from != NULL);
  ut_ad(from[0] != '\0');
  memcpy(to, sep, sep_len);

  ret = tablename_to_filename(from, to + sep_len, len - sep_len);
  partition_name_casedn_str(to);
  return (ret + sep_len);
}

/** Create the postfix of a partitioned table name
@param[in,out]	partition_name	Buffer to write the postfix
@param[in]	size		Size of the buffer
@param[in]	dd_part		Partition
@return the length of written postfix. */
size_t Ha_innopart_share::create_partition_postfix(
    char *partition_name, size_t size, const dd::Partition *dd_part) {
  size_t part_name_len = 0;
  size_t subpart_name_len = 0;
  const dd::Partition *part;

  part = dd_part->parent() != NULL ? dd_part->parent() : dd_part;

  part_name_len = append_sep_and_name(partition_name, part->name().c_str(),
                                      PART_SEPARATOR, size);

  if (part_name_len < size && part != dd_part) {
    char *part_name_end = partition_name + part_name_len;

    subpart_name_len =
        append_sep_and_name(part_name_end, dd_part->name().c_str(),
                            SUB_PART_SEPARATOR, size - part_name_len);

    if (subpart_name_len >= SUB_PART_SEPARATOR_LEN) {
      partition_name_casedn_str(part_name_end + SUB_PART_SEPARATOR_LEN);
    }
  }

  return (part_name_len + subpart_name_len);
}

/** Copy a cached MySQL row.
If requested, also avoids overwriting non-read columns.
@param[out]	buf		Row in MySQL format.
@param[in]	cached_row	Which row to copy. */
inline void ha_innopart::copy_cached_row(uchar *buf, const uchar *cached_row) {
  if (m_prebuilt->keep_other_fields_on_keyread) {
    row_sel_copy_cached_fields_for_mysql(buf, cached_row, m_prebuilt);
  } else {
    memcpy(buf, cached_row, m_rec_length);
  }
}

/** Open one partition
@param[in,out]	client		Data dictionary client
@param[in]	thd		Thread THD
@param[in]	table		MySQL table definition
@param[in]	dd_part		dd::Partition
@param[in]	part_name	Table name of this partition
@param[in]	part_id		Partition id
@retval	false on success
@retval	true on failure */
bool Ha_innopart_share::open_one_table_part(
    dd::cache::Dictionary_client *client, THD *thd, const TABLE *table,
    const dd::Partition *dd_part, const char *part_name, uint part_id) {
  dict_table_t *part_table = nullptr;
  bool cached = false;

  mutex_enter(&dict_sys->mutex);
  part_table = dict_table_check_if_in_cache_low(part_name);
  if (part_table != nullptr) {
    cached = true;
    if (part_table->is_corrupted()) {
      dict_table_remove_from_cache(part_table);
      part_table = nullptr;
    } else if (part_table->discard_after_ddl) {
      btr_drop_ahi_for_table(part_table);
      dict_table_remove_from_cache(part_table);
      part_table = nullptr;
      cached = false;
    } else {
      if (!dd_table_match(part_table, dd_part)) {
        dict_set_corrupted(part_table->first_index());
        dict_table_remove_from_cache(part_table);
        part_table = NULL;
      } else {
        part_table->acquire_with_lock();
      }
    }

    if (part_table != nullptr) {
      part_table->version = dd_get_version(&dd_part->table());
      dict_table_ddl_release(part_table);
    }
  }
  mutex_exit(&dict_sys->mutex);

  if (!cached) {
    part_table = dd_open_table(client, table, part_name, dd_part, thd);
  }

  if (part_table != NULL) {
    /* Set compression type like ha_innobase::open() does */
    dberr_t err = fil_set_compression(part_table, table->s->compress.str);
    switch (err) {
      case DB_NOT_FOUND:
      case DB_UNSUPPORTED:
        /* We will do another check before the create
        table and push the error to the client there. */
        break;

      case DB_IO_NO_PUNCH_HOLE_TABLESPACE:
        /* We did the check in the 'if' above. */

      case DB_IO_NO_PUNCH_HOLE_FS:
        /* During open we can't check whether the FS supports
        punch hole or not, at least on Linux. */
        break;

      default:
        ut_error;

      case DB_SUCCESS:
        break;
    }
  }

  m_table_parts[part_id] = part_table;
  return (part_table == nullptr);
}

/** Set up the virtual column template for partition table, and points
all m_table_parts[]->vc_templ to it.
@param[in]	table		MySQL TABLE object
@param[in]	ib_table	InnoDB dict_table_t
@param[in]	name		Table name (db/table_name) */
void Ha_innopart_share::set_v_templ(TABLE *table, dict_table_t *ib_table,
                                    const char *name) {
  ut_ad(mutex_own(&dict_sys->mutex));

  if (ib_table->n_v_cols > 0) {
    for (ulint i = 0; i < m_tot_parts; i++) {
      if (m_table_parts[i]->vc_templ == NULL) {
        m_table_parts[i]->vc_templ = UT_NEW_NOKEY(dict_vcol_templ_t());
        m_table_parts[i]->vc_templ->vtempl = NULL;
      } else if (m_table_parts[i]->get_ref_count() == 1) {
        /* Clean and refresh the template */
        dict_free_vc_templ(m_table_parts[i]->vc_templ);
        m_table_parts[i]->vc_templ->vtempl = NULL;
      }

      if (m_table_parts[i]->vc_templ->vtempl == NULL) {
        innobase_build_v_templ(table, ib_table, m_table_parts[i]->vc_templ,
                               NULL, true, name);
      }
    }
  }
}

/** Initialize the share with table and indexes per partition.
@param[in,out]	thd		Thread context
@param[in]	table		MySQL table definition
@param[in]	dd_table	Global DD table object
@param[in]	part_info	Partition info (partition names to use).
@param[in]	table_name	Table name (db/table_name).
@return	false on success else true. */
bool Ha_innopart_share::open_table_parts(THD *thd, const TABLE *table,
                                         const dd::Table *dd_table,
                                         partition_info *part_info,
                                         const char *table_name) {
  size_t table_name_len;
  uint ib_num_index;
  uint mysql_num_index;
  char partition_name[FN_REFLEN];
  bool index_loaded = true;

#ifdef UNIV_DEBUG
  if (m_table_share->tmp_table == NO_TMP_TABLE) {
    mysql_mutex_assert_owner(&m_table_share->LOCK_ha_data);
  }
#endif /* UNIV_DEBUG */
  m_ref_count++;
  if (m_table_parts != NULL) {
    ut_ad(m_ref_count > 1);
    ut_ad(m_tot_parts > 0);

    /* Increment dict_table_t reference count for all partitions */
    mutex_enter(&dict_sys->mutex);
    for (uint i = 0; i < m_tot_parts; i++) {
      dict_table_t *table = m_table_parts[i];
      table->acquire();
      ut_ad(table->get_ref_count() >= m_ref_count);
    }
    mutex_exit(&dict_sys->mutex);

    return (false);
  }
  ut_ad(m_ref_count == 1);
  m_tot_parts = part_info->get_tot_partitions();
  size_t table_parts_size = sizeof(dict_table_t *) * m_tot_parts;
  m_table_parts = static_cast<dict_table_t **>(
      ut_zalloc(table_parts_size, mem_key_partitioning));
  if (m_table_parts == NULL) {
    m_ref_count--;
    return (true);
  }

  /* Set up the array over all table partitions. */
  table_name_len = strlen(table_name);
  memcpy(partition_name, table_name, table_name_len);

  dd::cache::Dictionary_client *client;
  client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);
  uint i = 0;

  for (const auto dd_part : dd_table->leaf_partitions()) {
    size_t len = create_partition_postfix(partition_name + table_name_len,
                                          FN_REFLEN - table_name_len, dd_part);
    ut_a(len + table_name_len < FN_REFLEN);

    if (open_one_table_part(client, thd, table, dd_part, partition_name, i)) {
      ut_ad(m_table_parts[i] == nullptr);
      goto err;
    }
    i++;
  }
  ut_ad(i == m_tot_parts);

  /* Create the mapping of mysql index number to innodb indexes. */

  ib_num_index = (uint)UT_LIST_GET_LEN(m_table_parts[0]->indexes);
  mysql_num_index = part_info->table->s->keys;

  /* If there exists inconsistency between MySQL and InnoDB dictionary
  (metadata) information, the number of index defined in MySQL
  could exceed that in InnoDB, do not build index translation
  table in such case. */

  if (ib_num_index < mysql_num_index) {
    ut_ad(0);
    goto err;
  }

  if (mysql_num_index != 0) {
    size_t alloc_size =
        mysql_num_index * m_tot_parts * sizeof(*m_index_mapping);
    m_index_mapping = static_cast<dict_index_t **>(
        ut_zalloc(alloc_size, mem_key_partitioning));
    if (m_index_mapping == NULL) {
      /* Report an error if index_mapping continues to be
      NULL and mysql_num_index is a non-zero value. */

      ib::error(ER_IB_MSG_582) << "Failed to allocate memory for"
                                  " index translation table. Number of"
                                  " Index:"
                               << mysql_num_index;
      goto err;
    }
  }

  /* For each index in the mysql key_info array, fetch its
  corresponding InnoDB index pointer into index_mapping
  array. */

  for (ulint idx = 0; idx < mysql_num_index; idx++) {
    for (ulint part = 0; part < m_tot_parts; part++) {
      ulint count = part * mysql_num_index + idx;

      /* Fetch index pointers into index_mapping according
      to mysql index sequence. */

      m_index_mapping[count] = dict_table_get_index_on_name(
          m_table_parts[part], part_info->table->key_info[idx].name);

      if (m_index_mapping[count] == NULL) {
        ib::error(ER_IB_MSG_583)
            << "Cannot find index `" << part_info->table->key_info[idx].name
            << "` in InnoDB index dictionary"
               " partition `"
            << get_partition_name(part) << "`.";
        index_loaded = false;
        break;
      }

      /* Double check fetched index has the same
      column info as those in mysql key_info. */

      if (!innobase_match_index_columns(&part_info->table->key_info[idx],
                                        m_index_mapping[count])) {
        ib::error(ER_IB_MSG_584)
            << "Found index `" << part_info->table->key_info[idx].name
            << "` whose column info does not match"
               " that of MySQL.";
        index_loaded = false;
        break;
      }
    }
  }
  if (!index_loaded && m_index_mapping != NULL) {
    ut_free(m_index_mapping);
    m_index_mapping = NULL;
  }

  /* Successfully built the translation table. */
  m_index_count = mysql_num_index;

  return (false);
err:
  close_table_parts(false);

  return (true);
}

/** Close the table partitions.
If all instances are closed, also release the resources.
@param[in]	only_free	true if the tables have already been
                                closed, which happens during inplace
                                DDL, and we just need to release the
                                resources */
void Ha_innopart_share::close_table_parts(bool only_free) {
#ifdef UNIV_DEBUG
  if (m_table_share->tmp_table == NO_TMP_TABLE) {
    mysql_mutex_assert_owner(&m_table_share->LOCK_ha_data);
  }
#endif /* UNIV_DEBUG */
  m_ref_count--;
  if (m_ref_count != 0) {
    /* Decrement dict_table_t reference count for all partitions */
    for (uint i = 0; i < m_tot_parts; i++) {
      dict_table_t *table = m_table_parts[i];
      ut_d(uint64_t ref_count = table->get_ref_count());
      table->release();
      /* ref_count is got before release, so to minus 1 */
      ut_ad(ref_count >= m_ref_count + 1);
    }

    return;
  }

  /* Last instance closed, close all table partitions and
  free the memory. */

  if (m_table_parts != NULL) {
    if (!only_free) {
      for (uint i = 0; i < m_tot_parts; i++) {
        if (m_table_parts[i] != NULL) {
          dd_table_close(m_table_parts[i], NULL, NULL, false);
        }
      }
    }
    ut_free(m_table_parts);
    m_table_parts = NULL;
  }

  if (m_index_mapping != NULL) {
    ut_free(m_index_mapping);
    m_index_mapping = NULL;
  }

  m_tot_parts = 0;
  m_index_count = 0;

  /* All table partitions have been closed, autoinc initialization
  should be done again. */
  auto_inc_initialized = false;
}

/** Get index.
Find the index of the specified partition and key number.
@param[in]	part_id	Partition number.
@param[in]	keynr	Key number.
@return	Index pointer or NULL. */
inline dict_index_t *Ha_innopart_share::get_index(uint part_id, uint keynr) {
  ut_a(part_id < m_tot_parts);
  ut_ad(keynr < m_index_count || keynr == MAX_KEY);
  if (m_index_mapping == NULL || keynr >= m_index_count) {
    if (keynr == MAX_KEY) {
      return (get_table_part(part_id)->first_index());
    }
    return (NULL);
  }
  return (m_index_mapping[m_index_count * part_id + keynr]);
}

/** Get MySQL key number corresponding to InnoDB index.
Calculates the key number used inside MySQL for an Innobase index. We will
first check the "index translation table" for a match of the index to get
the index number. If there does not exist an "index translation table",
or not able to find the index in the translation table, then we will fall back
to the traditional way of looping through dict_index_t list to find a
match. In this case, we have to take into account if we generated a
default clustered index for the table
@param[in]	part_id	Partition the index belongs to.
@param[in]	index	Index to return MySQL key number for.
@return	the key number used inside MySQL or UINT_MAX if key is not found. */
inline uint Ha_innopart_share::get_mysql_key(uint part_id,
                                             const dict_index_t *index) {
  ut_ad(index != NULL);
  ut_ad(m_index_mapping != NULL);
  ut_ad(m_tot_parts);

  if (index != NULL && m_index_mapping != NULL) {
    uint start;
    uint end;

    if (part_id < m_tot_parts) {
      start = part_id * m_index_count;
      end = start + m_index_count;
    } else {
      start = 0;
      end = m_tot_parts * m_index_count;
    }
    for (uint i = start; i < end; i++) {
      if (m_index_mapping[i] == index) {
        return (i % m_index_count);
      }
    }

    /* Print an error message if we cannot find the index
    in the "index translation table". */

    if (index->is_committed()) {
      ib::error(ER_IB_MSG_585) << "Cannot find index " << index->name
                               << " in InnoDB index translation table.";
    }
  }

  return (UINT_MAX);
}

/** Get explicit specified tablespace for one (sub)partition, checking
from lowest level
@param[in]	tablespace	table-level tablespace if specified
@param[in]	part		Partition to check
@param[in]	sub_part	Sub-partition to check, if no, just NULL
@return Tablespace name, if nullptr or [0] = '\0' then nothing specified */
const char *partition_get_tablespace(const char *tablespace,
                                     const partition_element *part,
                                     const partition_element *sub_part) {
  if (sub_part != nullptr) {
    if (sub_part->tablespace_name != nullptr &&
        sub_part->tablespace_name[0] != '\0') {
      return (sub_part->tablespace_name);
    }
    /* Once DATA DIRECTORY specified, it implies
    non-default tablespace, same as below */
    if (sub_part->data_file_name != nullptr &&
        sub_part->data_file_name[0] != '\0') {
      return (nullptr);
    }
  }

  ut_ad(part != nullptr);
  if (part->tablespace_name != nullptr && part->tablespace_name[0] != '\0') {
    return (part->tablespace_name);
  }

  if (part->data_file_name != nullptr && part->data_file_name[0] != '\0') {
    return (nullptr);
  }

  return (tablespace);
}

/** Construct ha_innopart handler.
@param[in]	hton		Handlerton.
@param[in]	table_arg	MySQL Table.
@return	a new ha_innopart handler. */
ha_innopart::ha_innopart(handlerton *hton, TABLE_SHARE *table_arg)
    : ha_innobase(hton, table_arg),
      Partition_helper(this),
      m_ins_node_parts(),
      m_upd_node_parts(),
      m_blob_heap_parts(),
      m_trx_id_parts(),
      m_row_read_type_parts(),
      m_bitset(),
      m_sql_stat_start_parts(),
      m_pcur(),
      m_clust_pcur(),
      m_new_partitions() {
  m_int_table_flags &= ~(HA_INNOPART_DISABLED_TABLE_FLAGS);

  /* INNOBASE_SHARE is not used in ha_innopart.
  This also flags for ha_innobase that it is a partitioned table.
  And make it impossible to use legacy share functionality. */

  m_share = NULL;
}

/** Destruct ha_innopart handler. */
ha_innopart::~ha_innopart() {}

/** Returned supported alter table flags.
@param[in]	flags	Flags to support.
@return	Supported flags. */
uint ha_innopart::alter_table_flags(uint flags) {
  return (HA_PARTITION_FUNCTION_SUPPORTED | HA_INPLACE_CHANGE_PARTITION);
}

/** Internally called for initializing auto increment value.
Only called from ha_innobase::discard_or_import_table_space()
and should not do anything, since it is ha_innopart will initialize
it on first usage. */
int ha_innopart::innobase_initialize_autoinc() {
  ut_ad(0);
  return (0);
}

/** Set the autoinc column max value.
This should only be called once from ha_innobase::open().
Therefore there's no need for a covering lock.
@param[in]	-	If locking should be skipped. Not used!
@return	0 for success or error code. */
inline int ha_innopart::initialize_auto_increment(bool /* no_lock */) {
  int error = 0;
  ulonglong auto_inc = 0;
  const Field *field = table->found_next_number_field;

#ifdef UNIV_DEBUG
  if (table_share->tmp_table == NO_TMP_TABLE) {
    mysql_mutex_assert_owner(m_part_share->auto_inc_mutex);
  }
#endif

  /* Since a table can already be "open" in InnoDB's internal
  data dictionary, we only init the autoinc counter once, the
  first time the table is loaded. We can safely reuse the
  autoinc value from a previous MySQL open. */

  if (m_part_share->auto_inc_initialized) {
    /* Already initialized, nothing to do. */
    return (0);
  }

  if (field == NULL) {
    ib::info(ER_IB_MSG_586) << "Unable to determine the AUTOINC column name";
  }

  if (srv_force_recovery >= SRV_FORCE_NO_IBUF_MERGE) {
    /* If the recovery level is set so high that writes
    are disabled we force the AUTOINC counter to 0
    value effectively disabling writes to the table.
    Secondly, we avoid reading the table in case the read
    results in failure due to a corrupted table/index.

    We will not return an error to the client, so that the
    tables can be dumped with minimal hassle. If an error
    were returned in this case, the first attempt to read
    the table would fail and subsequent SELECTs would succeed. */

  } else if (field == NULL) {
    /* This is a far more serious error, best to avoid
    opening the table and return failure. */

    my_error(ER_AUTOINC_READ_FAILED, MYF(0));
    error = HA_ERR_AUTOINC_READ_FAILED;
  } else {
    dict_index_t *index;
    const char *col_name;
    ib_uint64_t read_auto_inc;
    ib_uint64_t persisted_auto_inc;
    ib_uint64_t max_auto_inc = 0;
    ulint err;
    dict_table_t *ib_table;
    ulonglong col_max_value;

    col_max_value = field->get_max_int_value();

    update_thd(ha_thd());

    col_name = field->field_name;
    for (uint part = 0; part < m_tot_parts; part++) {
      ib_table = m_part_share->get_table_part(part);

      dict_table_autoinc_set_col_pos(ib_table, field->field_index);

      dict_table_autoinc_lock(ib_table);
      read_auto_inc = dict_table_autoinc_read(ib_table);

      persisted_auto_inc = ib_table->autoinc_persisted;

      /* During startup, we may set both these two autoinc
      to same value after recovery of the counter. In this
      case, it's the first time we initialize the counter
      here, and we have to calculate the next counter.
      Otherwise, if they are not equal, we can use it
      directly. */
      if (read_auto_inc != 0 && read_auto_inc != persisted_auto_inc) {
        /* Sometimes, such as after UPDATE,
        we may have the persisted counter bigger
        than the in-memory one, because UPDATE in
        partition tables still doesn't modify the
        in-memory counter while persisted one could
        be updated if it's updated to larger value. */
        set_if_bigger(max_auto_inc, ut_max(read_auto_inc, persisted_auto_inc));
        dict_table_autoinc_unlock(ib_table);
        continue;
      }

      if (persisted_auto_inc == 0) {
        /* Execute SELECT MAX(col_name) FROM TABLE; */
        index = m_part_share->get_index(part, table->s->next_number_index);
        err = row_search_max_autoinc(index, col_name, &read_auto_inc);
      } else {
        /* We have the persisted AUTOINC counter,
        have to calculate the next one. */
        ut_ad(read_auto_inc == persisted_auto_inc);
        err = DB_SUCCESS;
      }

      switch (err) {
        case DB_SUCCESS: {
          /* At the this stage we do not know the
          increment nor the offset,
          so use a default increment of 1. */

          auto_inc =
              innobase_next_autoinc(read_auto_inc, 1, 1, 0, col_max_value);
          set_if_bigger(max_auto_inc, auto_inc);
          dict_table_autoinc_initialize(ib_table, auto_inc);
          break;
        }
        case DB_RECORD_NOT_FOUND:
          ib::error(ER_IB_MSG_587)
              << "MySQL and InnoDB data"
                 " dictionaries are out of sync. Unable"
                 " to find the AUTOINC column "
              << col_name << " in the InnoDB table " << ib_table->name
              << ". We set the"
                 " next AUTOINC column value to 0, in"
                 " effect disabling the AUTOINC next"
                 " value generation.";

          ib::info(ER_IB_MSG_588) << "You can either set the next"
                                     " AUTOINC value explicitly using ALTER"
                                     " TABLE or fix the data dictionary by"
                                     " recreating the table.";

          /* We want the open to succeed, so that the
          user can take corrective action. ie. reads
          should succeed but updates should fail. */

          /* This will disable the AUTOINC generation. */
          auto_inc = 0;
          goto done;
        default:
          /* row_search_max_autoinc() should only return
          one of DB_SUCCESS or DB_RECORD_NOT_FOUND. */

          ut_error;
      }
      dict_table_autoinc_unlock(ib_table);
    }
    auto_inc = max_auto_inc;
  }

done:
  m_part_share->next_auto_inc_val = auto_inc;
  m_part_share->auto_inc_initialized = true;
  return (error);
}

/** Open a partitioned InnoDB table.
@param[in]	name	table name
@param[in]	table_def	dd::Table describing table to be opened
@retval 1 if error
@retval 0 if success */
int ha_innopart::open(const char *name, int, uint, const dd::Table *table_def) {
  dict_table_t *ib_table;
  char norm_name[FN_REFLEN];
  THD *thd;

  DBUG_ENTER("ha_innopart::open");
  DBUG_ASSERT(table_share == table->s);

  if (m_part_info == NULL) {
    /* Must be during ::clone()! */
    ut_ad(table->part_info != NULL);
    m_part_info = table->part_info;
  }
  thd = ha_thd();

  normalize_table_name(norm_name, name);

  m_user_thd = NULL;

  /* Get the Ha_innopart_share from the TABLE_SHARE. */
  lock_shared_ha_data();

  m_part_share = static_cast<Ha_innopart_share *>(get_ha_share_ptr());
  if (m_part_share == NULL) {
    m_part_share = new (std::nothrow) Ha_innopart_share(table_share);
    if (m_part_share == NULL) {
    share_error:
      unlock_shared_ha_data();
      DBUG_RETURN(HA_ERR_INTERNAL_ERROR);
    }
    set_ha_share_ptr(static_cast<Handler_share *>(m_part_share));
  }

  if (m_part_share->open_table_parts(thd, table, table_def, m_part_info,
                                     norm_name) ||
      m_part_share->populate_partition_name_hash(m_part_info)) {
    goto share_error;
  }

  if (m_part_share->auto_inc_mutex == NULL &&
      table->found_next_number_field != NULL) {
    if (m_part_share->init_auto_inc_mutex(table_share)) {
      goto share_error;
    }
  }

  unlock_shared_ha_data();

  /* Will be allocated if it is needed in ::update_row(). */
  m_upd_buf = NULL;
  m_upd_buf_size = 0;

  /* Get pointer to a table object in InnoDB dictionary cache. */
  ib_table = m_part_share->get_table_part(0);

  m_prebuilt = nullptr;
  m_pcur_parts = NULL;
  m_clust_pcur_parts = NULL;
  m_pcur_map = NULL;

  /* TODO: Handle mismatching #P# vs #p# in upgrading to new DD instead!
  See bug#58406, The problem exists when moving partitioned tables
  between Windows and Unix-like platforms. InnoDB always folds the name
  on windows, partitioning never folds partition (and #P# separator).
  I.e. non of it follows lower_case_table_names correctly :( */

  if (open_partitioning(m_part_share)) {
    close();
    DBUG_RETURN(HA_ERR_INITIALIZATION);
  }

  /* Currently we track statistics for all partitions, but for
  the secondary indexes we only use the biggest partition. */

  for (uint part_id = 0; part_id < m_tot_parts; part_id++) {
    innobase_copy_frm_flags_from_table_share(
        m_part_share->get_table_part(part_id), table->s);
    dict_stats_init(m_part_share->get_table_part(part_id));
  }

  MONITOR_INC(MONITOR_TABLE_OPEN);

  bool no_tablespace;

  /* TODO: Should we do this check for every partition during ::open()? */
  /* TODO: refactor this in ha_innobase so it can increase code reuse. */
  if (dict_table_is_discarded(ib_table)) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_TABLESPACE_DISCARDED,
                table->s->table_name.str);

    /* Allow an open because a proper DISCARD should have set
    all the flags and index root page numbers to FIL_NULL that
    should prevent any DML from running but it should allow DDL
    operations. */

    no_tablespace = false;

  } else if (ib_table->ibd_file_missing) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_TABLESPACE_MISSING, norm_name);

    /* This means we have no idea what happened to the tablespace
    file, best to play it safe. */

    no_tablespace = true;
  } else {
    no_tablespace = false;
  }

  if (!thd_tablespace_op(thd) && no_tablespace) {
    set_my_errno(ENOENT);
    close();
    DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
  }

  m_prebuilt = row_create_prebuilt(ib_table, table->s->reclength);

  m_prebuilt->default_rec = table->s->default_values;
  ut_ad(m_prebuilt->default_rec);

  DBUG_ASSERT(table != NULL);
  m_prebuilt->m_mysql_table = table;
  m_prebuilt->m_mysql_handler = this;

  if (ib_table->n_v_cols > 0) {
    mutex_enter(&dict_sys->mutex);
    m_part_share->set_v_templ(table, ib_table, name);
    mutex_exit(&dict_sys->mutex);
  }

  key_used_on_scan = table_share->primary_key;

  /* Allocate a buffer for a 'row reference'. A row reference is
  a string of bytes of length ref_length which uniquely specifies
  a row in our table. Note that MySQL may also compare two row
  references for equality by doing a simple memcmp on the strings
  of length ref_length! */

  if (!row_table_got_default_clust_index(ib_table)) {
    m_prebuilt->clust_index_was_generated = FALSE;

    if (table_share->is_missing_primary_key()) {
      table_name_t table_name;
      table_name.m_name = const_cast<char *>(name);
      ib::error(ER_IB_MSG_589) << "Table " << table_name
                               << " has a primary key in InnoDB data"
                                  " dictionary, but not in MySQL!";

      /* This mismatch could cause further problems
      if not attended, bring this to the user's attention
      by printing a warning in addition to log a message
      in the errorlog. */

      push_warning_printf(thd, Sql_condition::SL_WARNING, ER_NO_SUCH_INDEX,
                          "Table %s has a"
                          " primary key in InnoDB data"
                          " dictionary, but not in"
                          " MySQL!",
                          name);

      /* If table_share->is_missing_primary_key(),
      the table_share->primary_key
      value could be out of bound if continue to index
      into key_info[] array. Find InnoDB primary index,
      and assign its key_length to ref_length.
      In addition, since MySQL indexes are sorted starting
      with primary index, unique index etc., initialize
      ref_length to the first index key length in
      case we fail to find InnoDB cluster index.

      Please note, this will not resolve the primary
      index mismatch problem, other side effects are
      possible if users continue to use the table.
      However, we allow this table to be opened so
      that user can adopt necessary measures for the
      mismatch while still being accessible to the table
      date. */

      if (table->key_info == NULL) {
        ut_ad(table->s->keys == 0);
        ref_length = 0;
      } else {
        ref_length = table->key_info[0].key_length;
      }

      /* Find corresponding cluster index
      key length in MySQL's key_info[] array. */

      for (uint i = 0; i < table->s->keys; i++) {
        dict_index_t *index;
        index = innopart_get_index(0, i);
        if (index->is_clustered()) {
          ref_length = table->key_info[i].key_length;
        }
      }
      ut_a(ref_length);
      ref_length += PARTITION_BYTES_IN_POS;
    } else {
      /* MySQL allocates the buffer for ref.
      key_info->key_length includes space for all key
      columns + one byte for each column that may be
      NULL. ref_length must be as exact as possible to
      save space, because all row reference buffers are
      allocated based on ref_length. */

      ref_length = table->key_info[table_share->primary_key].key_length;
      ref_length += PARTITION_BYTES_IN_POS;
    }
  } else {
    if (!table_share->is_missing_primary_key()) {
      table_name_t table_name;
      table_name.m_name = const_cast<char *>(name);
      ib::error(ER_IB_MSG_590) << "Table " << table_name
                               << " has no primary key in InnoDB data"
                                  " dictionary, but has one in MySQL! If you"
                                  " created the table with a MySQL version <"
                                  " 3.23.54 and did not define a primary key,"
                                  " but defined a unique key with all non-NULL"
                                  " columns, then MySQL internally treats that"
                                  " key as the primary key. You can fix this"
                                  " error by dump + DROP + CREATE + reimport"
                                  " of the table.";

      /* This mismatch could cause further problems
      if not attended, bring this to the user attention
      by printing a warning in addition to log a message
      in the errorlog. */

      push_warning_printf(thd, Sql_condition::SL_WARNING, ER_NO_SUCH_INDEX,
                          "InnoDB: Table %s has no"
                          " primary key in InnoDB data"
                          " dictionary, but has one in"
                          " MySQL!",
                          name);
    }

    m_prebuilt->clust_index_was_generated = TRUE;

    ref_length = DATA_ROW_ID_LEN;
    ref_length += PARTITION_BYTES_IN_POS;

    /* If we automatically created the clustered index, then
    MySQL does not know about it, and MySQL must NOT be aware
    of the index used on scan, to make it avoid checking if we
    update the column of the index. That is why we assert below
    that key_used_on_scan is the undefined value MAX_KEY.
    The column is the row id in the automatical generation case,
    and it will never be updated anyway. */

    if (key_used_on_scan != MAX_KEY) {
      table_name_t table_name;
      table_name.m_name = const_cast<char *>(name);
      ib::warn(ER_IB_MSG_591) << "Table " << table_name
                              << " key_used_on_scan is " << key_used_on_scan
                              << " even though there is"
                                 " no primary key inside InnoDB.";
    }
  }

  /* Index block size in InnoDB: used by MySQL in query optimization. */
  stats.block_size = UNIV_PAGE_SIZE;

  /* Only if the table has an AUTOINC column. */
  if (m_prebuilt->table != NULL && !m_prebuilt->table->ibd_file_missing &&
      table->found_next_number_field != NULL) {
    int error;

    /* Since a table can already be "open" in InnoDB's internal
    data dictionary, we only init the autoinc counter once, the
    first time the table is loaded,
    see ha_innopart::initialize_auto_increment.
    We can safely reuse the autoinc value from a previous MySQL
    open. */

    lock_auto_increment();
    error = initialize_auto_increment(false);
    unlock_auto_increment();
    if (error != 0) {
      close();
      DBUG_RETURN(error);
    }
  }

#ifdef HA_INNOPART_SUPPORTS_FULLTEXT
  /* Set plugin parser for fulltext index. */
  for (uint i = 0; i < table->s->keys; i++) {
    if (table->key_info[i].flags & HA_USES_PARSER) {
      dict_index_t *index = innobase_get_index(i);
      plugin_ref parser = table->key_info[i].parser;

      ut_ad(index->type & DICT_FTS);
      index->parser =
          static_cast<st_mysql_ftparser *>(plugin_decl(parser)->info);

      DBUG_EXECUTE_IF("fts_instrument_use_default_parser",
                      index->parser = &fts_default_parser;);
    }
  }
#endif /* HA_INNOPART_SUPPORTS_FULLTEXT */

  size_t alloc_size = sizeof(*m_ins_node_parts) * m_tot_parts;
  m_ins_node_parts =
      static_cast<ins_node_t **>(ut_zalloc(alloc_size, mem_key_partitioning));

  alloc_size = sizeof(*m_upd_node_parts) * m_tot_parts;
  m_upd_node_parts =
      static_cast<upd_node_t **>(ut_zalloc(alloc_size, mem_key_partitioning));

  alloc_blob_heap_array();

  alloc_size = sizeof(*m_trx_id_parts) * m_tot_parts;
  m_trx_id_parts =
      static_cast<trx_id_t *>(ut_zalloc(alloc_size, mem_key_partitioning));

  alloc_size = sizeof(*m_row_read_type_parts) * m_tot_parts;
  m_row_read_type_parts =
      static_cast<ulint *>(ut_zalloc(alloc_size, mem_key_partitioning));

  alloc_size = sizeof(*m_bitset) * UT_BITS_IN_BYTES(m_tot_parts);
  m_bitset = static_cast<byte *>(ut_zalloc(alloc_size, mem_key_partitioning));

  if (m_ins_node_parts == NULL || m_upd_node_parts == NULL ||
      m_blob_heap_parts == NULL || m_trx_id_parts == NULL ||
      m_row_read_type_parts == NULL || m_bitset == NULL) {
    close();  // Frees all the above.
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);
  }

  m_sql_stat_start_parts.init(m_bitset, UT_BITS_IN_BYTES(m_tot_parts));

  info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);

  DBUG_RETURN(0);
}

/** Get a cloned ha_innopart handler.
@param[in]	name		Table name.
@param[in]	mem_root	MySQL mem_root to use.
@return	new ha_innopart handler. */
handler *ha_innopart::clone(const char *name, MEM_ROOT *mem_root) {
  ha_innopart *new_handler;

  DBUG_ENTER("ha_innopart::clone");

  new_handler = dynamic_cast<ha_innopart *>(handler::clone(name, mem_root));
  if (new_handler != NULL) {
    ut_ad(new_handler->m_prebuilt != NULL);

    new_handler->m_prebuilt->select_lock_type = m_prebuilt->select_lock_type;
    new_handler->m_prebuilt->select_mode = m_prebuilt->select_mode;
  }

  DBUG_RETURN(new_handler);
}

/** Clear used ins_nodes and upd_nodes. */
void ha_innopart::clear_ins_upd_nodes() {
  /* Free memory from insert nodes. */
  if (m_ins_node_parts != NULL) {
    for (uint i = 0; i < m_tot_parts; i++) {
      if (m_ins_node_parts[i] != NULL) {
        ins_node_t *ins = m_ins_node_parts[i];
        if (ins->select != NULL) {
          que_graph_free_recursive(ins->select);
          ins->select = NULL;
        }

        if (ins->entry_sys_heap != NULL) {
          mem_heap_free(ins->entry_sys_heap);
          ins->entry_sys_heap = NULL;
        }
        m_ins_node_parts[i] = NULL;
      }
    }
  }

  /* Free memory from update nodes. */
  if (m_upd_node_parts != NULL) {
    for (uint i = 0; i < m_tot_parts; i++) {
      if (m_upd_node_parts[i] != NULL) {
        upd_node_t *upd = m_upd_node_parts[i];
        if (upd->cascade_heap) {
          mem_heap_free(upd->cascade_heap);
          upd->cascade_heap = NULL;
        }
        if (upd->in_mysql_interface) {
          btr_pcur_free_for_mysql(upd->pcur);
          upd->in_mysql_interface = FALSE;
        }

        if (upd->select != NULL) {
          que_graph_free_recursive(upd->select);
          upd->select = NULL;
        }
        if (upd->heap != NULL) {
          mem_heap_free(upd->heap);
          upd->heap = NULL;
        }
        m_upd_node_parts[i] = NULL;
      }
    }
  }
}

/** Closes a handle to an InnoDB table.
@return	0 */
int ha_innopart::close() {
  DBUG_ENTER("ha_innopart::close");

  ut_ad(m_pcur_parts == NULL);
  ut_ad(m_clust_pcur_parts == NULL);
  close_partitioning();

  ut_ad(m_part_share != NULL);
  if (m_part_share != NULL) {
    lock_shared_ha_data();
    m_part_share->close_table_parts(m_prebuilt != nullptr &&
                                    m_prebuilt->table == nullptr);
    unlock_shared_ha_data();
    m_part_share = NULL;
  }
  clear_ins_upd_nodes();
  free_blob_heap_array();

  /* Prevent double close of m_prebuilt->table. The real one was done
  done in m_part_share->close_table_parts(). */
  if (m_prebuilt != nullptr) {
    m_prebuilt->table = NULL;
    row_prebuilt_free(m_prebuilt, FALSE);
  }

  if (m_upd_buf != NULL) {
    ut_ad(m_upd_buf_size != 0);
    /* Allocated with my_malloc! */
    my_free(m_upd_buf);
    m_upd_buf = NULL;
    m_upd_buf_size = 0;
  }

  if (m_ins_node_parts != NULL) {
    ut_free(m_ins_node_parts);
    m_ins_node_parts = NULL;
  }
  if (m_upd_node_parts != NULL) {
    ut_free(m_upd_node_parts);
    m_upd_node_parts = NULL;
  }
  if (m_trx_id_parts != NULL) {
    ut_free(m_trx_id_parts);
    m_trx_id_parts = NULL;
  }
  if (m_row_read_type_parts != NULL) {
    ut_free(m_row_read_type_parts);
    m_row_read_type_parts = NULL;
  }

  ut_free(m_bitset);
  m_bitset = nullptr;

  MONITOR_INC(MONITOR_TABLE_CLOSE);

  /* Tell InnoDB server that there might be work for
  utility threads: */

  srv_active_wake_master_thread();

  DBUG_RETURN(0);
}

/** Change active partition.
Copies needed info into m_prebuilt from the partition specific memory.
@param[in]	part_id	Partition to set as active. */
void ha_innopart::set_partition(uint part_id) {
  DBUG_ENTER("ha_innopart::set_partition");

  DBUG_PRINT("ha_innopart", ("partition id: %u", part_id));

  if (part_id >= m_tot_parts) {
    ut_ad(0);
    DBUG_VOID_RETURN;
  }
  if (m_pcur_parts != NULL) {
    m_prebuilt->pcur = &m_pcur_parts[m_pcur_map[part_id]];
  }
  if (m_clust_pcur_parts != NULL) {
    m_prebuilt->clust_pcur = &m_clust_pcur_parts[m_pcur_map[part_id]];
  }
  m_prebuilt->ins_node = m_ins_node_parts[part_id];
  m_prebuilt->upd_node = m_upd_node_parts[part_id];

  /* For unordered scan and table scan, use blob_heap from first
  partition as we need exactly one blob. */
  m_prebuilt->blob_heap = m_blob_heap_parts[m_ordered ? part_id : 0];

#ifdef UNIV_DEBUG
  if (m_prebuilt->blob_heap != NULL) {
    DBUG_PRINT("ha_innopart",
               ("validating blob_heap: %p", m_prebuilt->blob_heap));
    mem_heap_validate(m_prebuilt->blob_heap);
  }
#endif

  m_prebuilt->trx_id = m_trx_id_parts[part_id];
  m_prebuilt->row_read_type = m_row_read_type_parts[part_id];
  m_prebuilt->sql_stat_start = m_sql_stat_start_parts.test(part_id);
  m_prebuilt->table = m_part_share->get_table_part(part_id);
  m_prebuilt->index = innopart_get_index(part_id, active_index);

  DBUG_VOID_RETURN;
}

/** Update active partition.
Copies needed info from m_prebuilt into the partition specific memory.
@param[in]	part_id	Partition to set as active. */
void ha_innopart::update_partition(uint part_id) {
  DBUG_ENTER("ha_innopart::update_partition");
  DBUG_PRINT("ha_innopart", ("partition id: %u", part_id));

  if (part_id >= m_tot_parts) {
    ut_ad(0);
    DBUG_VOID_RETURN;
  }
  m_ins_node_parts[part_id] = m_prebuilt->ins_node;
  m_upd_node_parts[part_id] = m_prebuilt->upd_node;

#ifdef UNIV_DEBUG
  if (m_prebuilt->blob_heap != NULL) {
    DBUG_PRINT("ha_innopart",
               ("validating blob_heap: %p", m_prebuilt->blob_heap));
    mem_heap_validate(m_prebuilt->blob_heap);
  }
#endif

  /* For unordered scan and table scan, use blob_heap from first
  partition as we need exactly one blob anytime. */
  m_blob_heap_parts[m_ordered ? part_id : 0] = m_prebuilt->blob_heap;

  m_trx_id_parts[part_id] = m_prebuilt->trx_id;
  m_row_read_type_parts[part_id] = m_prebuilt->row_read_type;
  if (m_prebuilt->sql_stat_start == 0) {
    m_sql_stat_start_parts.set(part_id, 0);
  }
  m_last_part = part_id;
  DBUG_VOID_RETURN;
}

/** Save currently highest auto increment value.
@param[in]	nr	Auto increment value to save. */
void ha_innopart::save_auto_increment(ulonglong nr) {
  /* Store it in the shared dictionary of the partition.
  TODO: When the new DD is done, store it in the table and make it
  persistent! */

  dict_table_autoinc_lock(m_prebuilt->table);
  dict_table_autoinc_update_if_greater(m_prebuilt->table, nr + 1);
  dict_table_autoinc_unlock(m_prebuilt->table);
}

/** Was the last returned row semi consistent read.
In an UPDATE or DELETE, if the row under the cursor was locked by
another transaction, and the engine used an optimistic read of the last
committed row value under the cursor, then the engine returns 1 from
this function. MySQL must NOT try to update this optimistic value. If
the optimistic value does not match the WHERE condition, MySQL can
decide to skip over this row. This can be used to avoid unnecessary
lock waits.

If this method returns true, it will also signal the storage
engine that the next read will be a locking re-read of the row.
@see handler.h and row0mysql.h
@return	true if last read was semi consistent else false. */
bool ha_innopart::was_semi_consistent_read() {
  return (m_row_read_type_parts[m_last_part] == ROW_READ_DID_SEMI_CONSISTENT);
}

/** Try semi consistent read.
Tell the engine whether it should avoid unnecessary lock waits.
If yes, in an UPDATE or DELETE, if the row under the cursor was locked
by another transaction, the engine may try an optimistic read of
the last committed row value under the cursor.
@see handler.h and row0mysql.h
@param[in]	yes	Should semi-consistent read be used. */
void ha_innopart::try_semi_consistent_read(bool yes) {
  ha_innobase::try_semi_consistent_read(yes);
  for (uint i = m_part_info->get_first_used_partition(); i < m_tot_parts;
       i = m_part_info->get_next_used_partition(i)) {
    m_row_read_type_parts[i] = m_prebuilt->row_read_type;
  }
}

/** Removes a lock on a row.
Removes a new lock set on a row, if it was not read optimistically.
This can be called after a row has been read in the processing of
an UPDATE or a DELETE query. @see ha_innobase::unlock_row(). */
void ha_innopart::unlock_row() {
  ut_ad(m_last_part < m_tot_parts);
  set_partition(m_last_part);
  ha_innobase::unlock_row();
  update_partition(m_last_part);
}

/** Write a row in partition.
Stores a row in an InnoDB database, to the table specified in this
handle.
@param[in]	part_id	Partition to write to.
@param[in]	record	A row in MySQL format.
@return	0 or error code. */
int ha_innopart::write_row_in_part(uint part_id, uchar *record) {
  int error;
  Field *saved_next_number_field = table->next_number_field;
  DBUG_ENTER("ha_innopart::write_row_in_part");
  set_partition(part_id);

  /* Prevent update_auto_increment to be called
  again in ha_innobase::write_row(). */

  table->next_number_field = NULL;

  /* TODO: try to avoid creating a new dtuple
  (in row_get_prebuilt_insert_row()) for each partition).
  Might be needed due to ins_node implementation. */

  error = ha_innobase::write_row(record);
  update_partition(part_id);
  table->next_number_field = saved_next_number_field;
  DBUG_RETURN(error);
}

/** Update a row in partition.
Updates a row given as a parameter to a new value.
@param[in]	part_id	Partition to update row in.
@param[in]	old_row	Old row in MySQL format.
@param[in]	new_row	New row in MySQL format.
@return	0 or error number. */
int ha_innopart::update_row_in_part(uint part_id, const uchar *old_row,
                                    uchar *new_row) {
  int error;
  DBUG_ENTER("ha_innopart::update_row_in_part");

  set_partition(part_id);
  error = ha_innobase::update_row(old_row, new_row);
  update_partition(part_id);
  DBUG_RETURN(error);
}

/** Deletes a row in partition.
@param[in]	part_id	Partition to delete from.
@param[in]	record	Row to delete in MySQL format.
@return	0 or error number. */
int ha_innopart::delete_row_in_part(uint part_id, const uchar *record) {
  int error;
  DBUG_ENTER("ha_innopart::delete_row_in_part");
  m_err_rec = NULL;

  m_last_part = part_id;
  set_partition(part_id);
  error = ha_innobase::delete_row(record);
  update_partition(part_id);
  DBUG_RETURN(error);
}

/** Initializes a handle to use an index.
@param[in]	keynr	Key (index) number.
@param[in]	sorted	True if result MUST be sorted according to index.
@return	0 or error number. */
int ha_innopart::index_init(uint keynr, bool sorted) {
  int error;
  uint part_id = m_part_info->get_first_used_partition();
  DBUG_ENTER("ha_innopart::index_init");

  active_index = keynr;
  if (part_id == MY_BIT_NONE) {
    DBUG_RETURN(0);
  }

  error = ph_index_init_setup(keynr, sorted);
  if (error != 0) {
    DBUG_RETURN(error);
  }

  if (sorted) {
    error = init_record_priority_queue();
    if (error != 0) {
      /* Needs cleanup in case it returns error. */
      destroy_record_priority_queue();
      DBUG_RETURN(error);
    }
    /* Disable prefetch.
    The prefetch buffer is not partitioning aware, so it may return
    rows from a different partition if either the prefetch buffer is
    full, or it is non-empty and the partition is exhausted. */
    m_prebuilt->m_no_prefetch = true;
  }

  /* For scan across partitions, the keys needs to be materialized */
  m_prebuilt->m_read_virtual_key = true;

  error = change_active_index(part_id, keynr);
  if (error != 0) {
    destroy_record_priority_queue();
    DBUG_RETURN(error);
  }

  DBUG_EXECUTE_IF("partition_fail_index_init", {
    destroy_record_priority_queue();
    DBUG_RETURN(HA_ERR_NO_PARTITION_FOUND);
  });

  DBUG_RETURN(0);
}

/** End index cursor.
@return	0 or error code. */
int ha_innopart::index_end() {
  uint part_id = m_part_info->get_first_used_partition();
  DBUG_ENTER("ha_innopart::index_end");

  if (part_id == MY_BIT_NONE) {
    /* Never initialized any index. */
    active_index = MAX_KEY;
    DBUG_RETURN(0);
  }
  if (m_ordered) {
    destroy_record_priority_queue();
    m_prebuilt->m_no_prefetch = false;
  }
  m_prebuilt->m_read_virtual_key = false;

  DBUG_RETURN(ha_innobase::index_end());
}

/* Partitioning support functions. */

/** Setup the ordered record buffer and the priority queue.
@param[in]	used_parts	Number of used partitions in query.
@return	false for success else true. */
int ha_innopart::init_record_priority_queue_for_parts(uint used_parts) {
  size_t alloc_size;
  void *buf;

  DBUG_ENTER("ha_innopart::init_record_priority_queue_for_parts");
  ut_ad(used_parts >= 1);
  /* TODO: Don't use this if only one partition is used! */
  // ut_ad(used_parts > 1);

  /* We could reuse current m_prebuilt->pcur/clust_pcur for the first
  used partition, but it would complicate and affect performance,
  so we trade some extra memory instead. */

  m_pcur = m_prebuilt->pcur;
  m_clust_pcur = m_prebuilt->clust_pcur;

  /* If we searching for secondary key or doing a write/update
  we will need two pcur, one for the active (secondary) index and
  one for the clustered index. */

  bool need_clust_index =
      m_curr_key_info[1] != NULL || get_lock_type() != F_RDLCK;

  /* pcur and clust_pcur per partition.
  By using zalloc, we do not need to initialize the pcur's! */

  alloc_size = used_parts * sizeof(btr_pcur_t);
  if (need_clust_index) {
    alloc_size *= 2;
  }
  buf = ut_zalloc(alloc_size, mem_key_partitioning);
  if (buf == NULL) {
    DBUG_RETURN(true);
  }
  m_pcur_parts = static_cast<btr_pcur_t *>(buf);
  if (need_clust_index) {
    m_clust_pcur_parts = &m_pcur_parts[used_parts];
  }
  /* mapping from part_id to pcur. */
  alloc_size = m_tot_parts * sizeof(*m_pcur_map);
  buf = ut_zalloc(alloc_size, mem_key_partitioning);
  if (buf == NULL) {
    DBUG_RETURN(true);
  }
  m_pcur_map = static_cast<uint16_t *>(buf);
  {
    uint16_t pcur_count = 0;
    for (uint i = m_part_info->get_first_used_partition(); i < m_tot_parts;
         i = m_part_info->get_next_used_partition(i)) {
      m_pcur_map[i] = pcur_count++;
    }
  }

  DBUG_RETURN(false);
}

/** Destroy the ordered record buffer and the priority queue. */
inline void ha_innopart::destroy_record_priority_queue_for_parts() {
  DBUG_ENTER("ha_innopart::destroy_record_priority_queue");
  if (m_pcur_parts != NULL) {
    uint used_parts;
    used_parts = bitmap_bits_set(&m_part_info->read_partitions);
    for (uint i = 0; i < used_parts; i++) {
      btr_pcur_free(&m_pcur_parts[i]);
      if (m_clust_pcur_parts != NULL) {
        btr_pcur_free(&m_clust_pcur_parts[i]);
      }
    }
    ut_free(m_pcur_parts);
    m_clust_pcur_parts = NULL;
    m_pcur_parts = NULL;
    /* Reset the original m_prebuilt->pcur. */
    m_prebuilt->pcur = m_pcur;
    m_prebuilt->clust_pcur = m_clust_pcur;
  }
  if (m_pcur_map != NULL) {
    ut_free(m_pcur_map);
    m_pcur_map = NULL;
  }
  DBUG_VOID_RETURN;
}

/** Print error information.
@param[in]	error	Error code (MySQL).
@param[in]	errflag	Flags. */
void ha_innopart::print_error(int error, myf errflag) {
  DBUG_ENTER("ha_innopart::print_error");
  if (print_partition_error(error)) {
    ha_innobase::print_error(error, errflag);
  }

  DBUG_VOID_RETURN;
}

/** Can error be ignored.
@param[in]	error	Error code to check.
@return	true if ignorable else false. */
bool ha_innopart::is_ignorable_error(int error) {
  if (ha_innobase::is_ignorable_error(error) ||
      error == HA_ERR_NO_PARTITION_FOUND ||
      error == HA_ERR_NOT_IN_LOCK_PARTITIONS) {
    return (true);
  }
  return (false);
}

/** Get the index for the current partition
@param[in]	keynr	MySQL index number.
@return	InnoDB index or NULL. */
inline dict_index_t *ha_innopart::innobase_get_index(uint keynr) {
  uint part_id = m_last_part;
  if (part_id >= m_tot_parts) {
    ut_ad(0);
    part_id = 0;
  }
  return (innopart_get_index(part_id, keynr));
}

/** Get the index for a handle.
Does not change active index.
@param[in]	keynr	Use this index; MAX_KEY means always clustered index,
even if it was internally generated by InnoDB.
@param[in]	part_id	From this partition.
@return	NULL or index instance. */
inline dict_index_t *ha_innopart::innopart_get_index(uint part_id, uint keynr) {
  KEY *key = NULL;
  dict_index_t *index = NULL;

  DBUG_ENTER("innopart_get_index");

  if (keynr != MAX_KEY && table->s->keys > 0) {
    key = table->key_info + keynr;

    index = m_part_share->get_index(part_id, keynr);

    if (index != NULL) {
      ut_a(ut_strcmp(index->name, key->name) == 0);
    } else {
      /* Can't find index with keynr in the translation
      table. Only print message if the index translation
      table exists. */

      ib::warn(ER_IB_MSG_592)
          << "InnoDB could not find index " << (key ? key->name : "NULL")
          << " key no " << keynr << " for table " << m_prebuilt->table->name
          << " through its index translation table";

      index = dict_table_get_index_on_name(m_prebuilt->table, key->name);
    }
  } else {
    /* Get the generated index. */
    ut_ad(keynr == MAX_KEY);
    index = m_part_share->get_table_part(part_id)->first_index();
  }

  if (index == NULL) {
    ib::error(ER_IB_MSG_593)
        << "InnoDB could not find key n:o " << keynr << " with name "
        << (key ? key->name : "NULL") << " from dict cache for table "
        << m_prebuilt->table->name << " partition n:o " << part_id;
  }

  DBUG_RETURN(index);
}

/** Changes the active index of a handle.
@param[in]	part_id	Use this partition.
@param[in]	keynr	Use this index; MAX_KEY means always clustered index,
even if it was internally generated by InnoDB.
@return	0 or error number. */
int ha_innopart::change_active_index(uint part_id, uint keynr) {
  DBUG_ENTER("ha_innopart::change_active_index");

  ut_ad(m_user_thd == ha_thd());
  ut_a(m_prebuilt->trx == thd_to_trx(m_user_thd));

  active_index = keynr;
  set_partition(part_id);

  if (UNIV_UNLIKELY(m_prebuilt->index == NULL)) {
    ib::warn(ER_IB_MSG_594)
        << "change_active_index(" << part_id << "," << keynr << ") failed";
    m_prebuilt->index_usable = FALSE;
    DBUG_RETURN(1);
  }

  m_prebuilt->index_usable = m_prebuilt->index->is_usable(m_prebuilt->trx);

  if (UNIV_UNLIKELY(!m_prebuilt->index_usable)) {
    if (m_prebuilt->index->is_corrupted()) {
      char table_name[MAX_FULL_NAME_LEN + 1];

      innobase_format_name(table_name, sizeof table_name,
                           m_prebuilt->index->table->name.m_name);

      push_warning_printf(m_user_thd, Sql_condition::SL_WARNING,
                          HA_ERR_INDEX_CORRUPT,
                          "InnoDB: Index %s for table %s is"
                          " marked as corrupted"
                          " (partition %u)",
                          m_prebuilt->index->name(), table_name, part_id);
      DBUG_RETURN(HA_ERR_INDEX_CORRUPT);
    } else {
      push_warning_printf(m_user_thd, Sql_condition::SL_WARNING,
                          HA_ERR_TABLE_DEF_CHANGED,
                          "InnoDB: insufficient history for index %u", keynr);
    }

    /* The caller seems to ignore this. Thus, we must check
    this again in row_search_for_mysql(). */

    DBUG_RETURN(HA_ERR_TABLE_DEF_CHANGED);
  }

  ut_a(m_prebuilt->search_tuple != NULL);

  /* If too expensive, cache the keynr and only update search_tuple when
  keynr changes. Remember that the clustered index is also used for
  MAX_KEY. */
  dtuple_set_n_fields(m_prebuilt->search_tuple, m_prebuilt->index->n_fields);

  dict_index_copy_types(m_prebuilt->search_tuple, m_prebuilt->index,
                        m_prebuilt->index->n_fields);

  /* MySQL changes the active index for a handle also during some
  queries, for example SELECT MAX(a), SUM(a) first retrieves the
  MAX() and then calculates the sum. Previously we played safe
  and used the flag ROW_MYSQL_WHOLE_ROW below, but that caused
  unnecessary copying. Starting from MySQL-4.1 we use a more
  efficient flag here. */

  /* TODO: Is this really needed?
  Will it not be built in index_read? */

  build_template(false);

  DBUG_RETURN(0);
}

/** Return first record in index from a partition.
@param[in]	part	Partition to read from.
@param[out]	record	First record in index in the partition.
@return	error number or 0. */
int ha_innopart::index_first_in_part(uint part, uchar *record) {
  int error;
  DBUG_ENTER("ha_innopart::index_first_in_part");

  set_partition(part);
  error = ha_innobase::index_first(record);
  update_partition(part);

  DBUG_RETURN(error);
}

/** Return next record in index from a partition.
@param[in]	part	Partition to read from.
@param[out]	record	Last record in index in the partition.
@return	error number or 0. */
int ha_innopart::index_next_in_part(uint part, uchar *record) {
  DBUG_ENTER("ha_innopart::index_next_in_part");

  int error;

  set_partition(part);
  error = ha_innobase::index_next(record);
  update_partition(part);

  ut_ad(m_ordered_scan_ongoing || m_ordered_rec_buffer == NULL ||
        m_prebuilt->used_in_HANDLER ||
        m_part_spec.start_part >= m_part_spec.end_part);

  DBUG_RETURN(error);
}

/** Return next same record in index from a partition.
This routine is used to read the next record, but only if the key is
the same as supplied in the call.
@param[in]	part	Partition to read from.
@param[out]	record	Last record in index in the partition.
@param[in]	key	Key to match.
@param[in]	length	Length of key.
@return	error number or 0. */
int ha_innopart::index_next_same_in_part(uint part, uchar *record,
                                         const uchar *key, uint length) {
  int error;

  set_partition(part);
  error = ha_innobase::index_next_same(record, key, length);
  update_partition(part);
  return (error);
}

/** Return last record in index from a partition.
@param[in]	part	Partition to read from.
@param[out]	record	Last record in index in the partition.
@return	error number or 0. */
int ha_innopart::index_last_in_part(uint part, uchar *record) {
  int error;

  set_partition(part);
  error = ha_innobase::index_last(record);
  update_partition(part);
  return (error);
}

/** Return previous record in index from a partition.
@param[in]	part	Partition to read from.
@param[out]	record	Last record in index in the partition.
@return	error number or 0. */
int ha_innopart::index_prev_in_part(uint part, uchar *record) {
  int error;

  set_partition(part);
  error = ha_innobase::index_prev(record);
  update_partition(part);

  ut_ad(m_ordered_scan_ongoing || m_ordered_rec_buffer == NULL ||
        m_prebuilt->used_in_HANDLER ||
        m_part_spec.start_part >= m_part_spec.end_part);

  return (error);
}

/** Start index scan and return first record from a partition.
This routine starts an index scan using a start key. The calling
function will check the end key on its own.
@param[in]	part		Partition to read from.
@param[out]	record		First matching record in index in the partition.
@param[in]	key		Key to match.
@param[in]	keypart_map	Which part of the key to use.
@param[in]	find_flag	Key condition/direction to use.
@return	error number or 0. */
int ha_innopart::index_read_map_in_part(uint part, uchar *record,
                                        const uchar *key,
                                        key_part_map keypart_map,
                                        enum ha_rkey_function find_flag) {
  int error;

  set_partition(part);
  error = ha_innobase::index_read_map(record, key, keypart_map, find_flag);
  update_partition(part);
  return (error);
}

/** Start index scan and return first record from a partition.
This routine starts an index scan using a start key. The calling
function will check the end key on its own.
@param[in]	part		Partition to read from.
@param[out]	record		First matching record in index in the partition.
@param[in]	index		Index to read from.
@param[in]	key		Key to match.
@param[in]	keypart_map	Which part of the key to use.
@param[in]	find_flag	Key condition/direction to use.
@return	error number or 0. */
int ha_innopart::index_read_idx_map_in_part(uint part, uchar *record,
                                            uint index, const uchar *key,
                                            key_part_map keypart_map,
                                            enum ha_rkey_function find_flag) {
  int error;

  set_partition(part);
  error = ha_innobase::index_read_idx_map(record, index, key, keypart_map,
                                          find_flag);
  update_partition(part);
  return (error);
}

/** Return last matching record in index from a partition.
@param[in]	part		Partition to read from.
@param[out]	record		Last matching record in index in the partition.
@param[in]	key		Key to match.
@param[in]	keypart_map	Which part of the key to use.
@return	error number or 0. */
int ha_innopart::index_read_last_map_in_part(uint part, uchar *record,
                                             const uchar *key,
                                             key_part_map keypart_map) {
  int error;
  set_partition(part);
  error = ha_innobase::index_read_last_map(record, key, keypart_map);
  update_partition(part);
  return (error);
}

/** Start index scan and return first record from a partition.
This routine starts an index scan using a start and end key.
@param[in]	part		Partition to read from.
@param[in,out]	record		First matching record in index in the partition,
if NULL use table->record[0] as return buffer.
@param[in]	start_key	Start key to match.
@param[in]	end_key		End key to match.
@param[in]	eq_range	Is equal range, start_key == end_key.
@param[in]	sorted		Return rows in sorted order.
@return	error number or 0. */
int ha_innopart::read_range_first_in_part(uint part, uchar *record,
                                          const key_range *start_key,
                                          const key_range *end_key,
                                          bool eq_range, bool sorted) {
  int error;
  uchar *read_record = record;
  set_partition(part);
  if (read_record == NULL) {
    read_record = table->record[0];
  }
  if (m_start_key.key != NULL) {
    error = ha_innobase::index_read(read_record, m_start_key.key,
                                    m_start_key.length, m_start_key.flag);
  } else {
    error = ha_innobase::index_first(read_record);
  }
  if (error == HA_ERR_KEY_NOT_FOUND) {
    error = HA_ERR_END_OF_FILE;
  } else if (error == 0 && !in_range_check_pushed_down) {
    /* compare_key uses table->record[0], so we
    need to copy the data if not already there. */

    if (record != NULL) {
      copy_cached_row(table->record[0], read_record);
    }
    if (compare_key(end_range) > 0) {
      /* must use ha_innobase:: due to set/update_partition
      could overwrite states if ha_innopart::unlock_row()
      was used. */
      ha_innobase::unlock_row();
      error = HA_ERR_END_OF_FILE;
    }
  }
  update_partition(part);
  return (error);
}

/** Return next record in index range scan from a partition.
@param[in]	part	Partition to read from.
@param[in,out]	record	First matching record in index in the partition,
if NULL use table->record[0] as return buffer.
@return	error number or 0. */
int ha_innopart::read_range_next_in_part(uint part, uchar *record) {
  int error;
  uchar *read_record = record;

  set_partition(part);
  if (read_record == NULL) {
    read_record = table->record[0];
  }

  /* TODO: Implement ha_innobase::read_range*?
  So it will return HA_ERR_END_OF_FILE or
  HA_ERR_KEY_NOT_FOUND when passing end_range. */

  error = ha_innobase::index_next(read_record);
  if (error == 0 && !in_range_check_pushed_down) {
    /* compare_key uses table->record[0], so we
    need to copy the data if not already there. */

    if (record != NULL) {
      copy_cached_row(table->record[0], read_record);
    }
    if (compare_key(end_range) > 0) {
      /* must use ha_innobase:: due to set/update_partition
      could overwrite states if ha_innopart::unlock_row()
      was used. */
      ha_innobase::unlock_row();
      error = HA_ERR_END_OF_FILE;
    }
  }
  update_partition(part);

  return (error);
}

/** Initialize a table scan in a specific partition.
@param[in]	part_id	Partition to initialize.
@param[in]	scan	True if table/index scan false otherwise (for rnd_pos)
@return	0 or error number. */
int ha_innopart::rnd_init_in_part(uint part_id, bool scan) {
  DBUG_ENTER("ha_innopart::rnd_init_in_part");
  DBUG_ASSERT(table_share->is_missing_primary_key() ==
              m_prebuilt->clust_index_was_generated);

  int err = change_active_index(part_id, table_share->primary_key);

  /* Don't use semi-consistent read in random row reads (by position).
  This means we must disable semi_consistent_read if scan is false. */

  if (!scan) {
    m_prebuilt->row_read_type = ROW_READ_WITH_LOCKS;
  }

  m_start_of_scan = true;
  DBUG_RETURN(err);
}

/** Ends a table scan.
@param[in]	part_id	Partition to end table scan in.
@param[in]	scan	True for scan else random access.
@return	0 or error number. */
int ha_innopart::rnd_end_in_part(uint part_id, bool scan) {
  return (index_end());
}

/** Read next row in partition.
Reads the next row in a table scan (also used to read the FIRST row
in a table scan).
@param[in]	part_id	Partition to end table scan in.
@param[out]	buf	Returns the row in this buffer, in MySQL format.
@return	0, HA_ERR_END_OF_FILE or error number. */
int ha_innopart::rnd_next_in_part(uint part_id, uchar *buf) {
  int error;

  DBUG_ENTER("ha_innopart::rnd_next_in_part");

  set_partition(part_id);
  if (m_start_of_scan) {
    error = ha_innobase::index_first(buf);

    if (error == HA_ERR_KEY_NOT_FOUND) {
      error = HA_ERR_END_OF_FILE;
    }
    m_start_of_scan = 0;
  } else {
    ha_statistic_increment(&System_status_var::ha_read_rnd_next_count);
    error = ha_innobase::general_fetch(buf, ROW_SEL_NEXT, 0);
  }

  update_partition(part_id);
  DBUG_RETURN(error);
}

/** Get a row from a position.
Fetches a row from the table based on a row reference.
@param[out]	buf	Returns the row in this buffer, in MySQL format.
@param[in]	pos	Position, given as primary key value or DB_ROW_ID
(if no primary key) of the row in MySQL format.  The length of data in pos has
to be ref_length.
@return	0, HA_ERR_KEY_NOT_FOUND or error code. */
int ha_innopart::rnd_pos(uchar *buf, uchar *pos) {
  int error;
  uint part_id;
  DBUG_ENTER("ha_innopart::rnd_pos");
  ut_ad(PARTITION_BYTES_IN_POS == 2);
  DBUG_DUMP("pos", pos, ref_length);

  ha_statistic_increment(&System_status_var::ha_read_rnd_count);

  ut_a(m_prebuilt->trx == thd_to_trx(ha_thd()));

  /* Restore used partition. */
  part_id = uint2korr(pos);

  set_partition(part_id);

  /* Note that we assume the length of the row reference is fixed
  for the table, and it is == ref_length. */

  error = ha_innobase::index_read(buf, pos + PARTITION_BYTES_IN_POS,
                                  ref_length - PARTITION_BYTES_IN_POS,
                                  HA_READ_KEY_EXACT);
  DBUG_PRINT("info", ("part %u index_read returned %d", part_id, error));
  DBUG_DUMP("buf", buf, table_share->reclength);

  update_partition(part_id);

  DBUG_RETURN(error);
}

/** Return position for cursor in last used partition.
Stores a reference to the current row to 'ref' field of the handle. Note
that in the case where we have generated the clustered index for the
table, the function parameter is illogical: we MUST ASSUME that 'record'
is the current 'position' of the handle, because if row ref is actually
the row id internally generated in InnoDB, then 'record' does not contain
it. We just guess that the row id must be for the record where the handle
was positioned the last time.
@param[out]	ref_arg	Pointer to buffer where to write the position.
@param[in]	record	Record to position for. */
void ha_innopart::position_in_last_part(uchar *ref_arg, const uchar *record) {
  DBUG_ENTER("ha_innopart::position_in_last_part");
  DBUG_ASSERT(table_share->is_missing_primary_key() ==
              m_prebuilt->clust_index_was_generated);

  if (m_prebuilt->clust_index_was_generated) {
    /* No primary key was defined for the table and we
    generated the clustered index from row id: the
    row reference will be the row id, not any key value
    that MySQL knows of. */

    memcpy(ref_arg, m_prebuilt->row_id, DATA_ROW_ID_LEN);
  } else {
    /* Copy primary key as the row reference */
    KEY *key_info = table->key_info + table_share->primary_key;
    key_copy(ref_arg, (uchar *)record, key_info, key_info->key_length);
  }

  DBUG_VOID_RETURN;
}

/** Fill in data_dir_path and tablespace name from internal data dictionary.
@param[in,out]	part_elem		Partition element to fill.
@param[in]	ib_table		InnoDB table to copy from.
@param[in]	display_tablespace	Display tablespace name if set. */
void ha_innopart::update_part_elem(partition_element *part_elem,
                                   dict_table_t *ib_table,
                                   bool display_tablespace) {
  dd_get_and_save_data_dir_path<dd::Partition>(ib_table, nullptr, false);
  if (ib_table->data_dir_path != NULL) {
    if (part_elem->data_file_name == NULL ||
        strcmp(ib_table->data_dir_path, part_elem->data_file_name) != 0) {
      /* Play safe and allocate memory from TABLE and copy
      instead of expose the internal data dictionary. */
      part_elem->data_file_name =
          strdup_root(&table->mem_root, ib_table->data_dir_path);
    }
  } else {
    part_elem->data_file_name = NULL;
  }

  part_elem->index_file_name = NULL;
  dict_get_and_save_space_name(ib_table, false);
  if (ib_table->tablespace != NULL) {
    ut_ad(part_elem->tablespace_name == NULL ||
          0 == strcmp(part_elem->tablespace_name, ib_table->tablespace));
    if (part_elem->tablespace_name == NULL ||
        strcmp(ib_table->tablespace, part_elem->tablespace_name) != 0) {
      /* Play safe and allocate memory from TABLE and copy
      instead of expose the internal data dictionary. */
      part_elem->tablespace_name =
          strdup_root(&table->mem_root, ib_table->tablespace);
    }
  } else {
    const char *tablespace_name = ib_table->space == 0
                                      ? dict_sys_t::s_sys_space_name
                                      : dict_sys_t::s_file_per_table_name;

    if (part_elem->tablespace_name != NULL) {
      if (0 != strcmp(part_elem->tablespace_name, tablespace_name)) {
        /* Update part_elem tablespace to NULL same
        as in innodb data dictionary ib_table. */
        part_elem->tablespace_name = NULL;
      }
    } else if (display_tablespace) {
      /* Update tablespace values so that SHOW CREATE TABLE
      will display TABLESPACE=name for the partition when
      appropriate, if it's not mentioned explicitly during
      table creation. */
      part_elem->tablespace_name =
          strdup_root(&table->mem_root, tablespace_name);
    }
  }
}

/** Update create_info.
Used in SHOW CREATE TABLE et al.
@param[in,out]	create_info	Create info to update. */
void ha_innopart::update_create_info(HA_CREATE_INFO *create_info) {
  uint num_subparts = m_part_info->num_subparts;
  uint num_parts;
  uint part;
  bool display_tablespace = false;
  dict_table_t *table;
  List_iterator<partition_element> part_it(m_part_info->partitions);
  partition_element *part_elem;
  partition_element *sub_elem;
  DBUG_ENTER("ha_innopart::update_create_info");
  if ((create_info->used_fields & HA_CREATE_USED_AUTO) == 0) {
    info(HA_STATUS_AUTO);
    create_info->auto_increment_value = stats.auto_increment_value;
  }

  num_parts = (num_subparts != 0) ? m_tot_parts / num_subparts : m_tot_parts;

  /* DATA/INDEX DIRECTORY are never applied to the whole partitioned
  table, only to its parts. */

  create_info->data_file_name = NULL;
  create_info->index_file_name = NULL;

  /* Since update_create_info() can be called from
  mysql_prepare_alter_table() when not all partitions are set up,
  we look for that condition first.
  If all partitions are not available then simply return,
  since it does not need any updated partitioning info. */

  if (!m_part_info->temp_partitions.is_empty()) {
    DBUG_VOID_RETURN;
  }
  part = 0;
  while ((part_elem = part_it++)) {
    if (part >= num_parts) {
      DBUG_VOID_RETURN;
    }
    if (m_part_info->is_sub_partitioned()) {
      List_iterator<partition_element> subpart_it(part_elem->subpartitions);
      uint subpart = 0;
      while ((sub_elem = subpart_it++)) {
        if (subpart >= num_subparts) {
          DBUG_VOID_RETURN;
        }
        table = m_part_share->get_table_part(part * num_subparts + subpart);

        if (sub_elem->tablespace_name != NULL || table->tablespace != NULL ||
            table->space == 0) {
          display_tablespace = true;
        }
        subpart++;
      }
      if (subpart != num_subparts) {
        DBUG_VOID_RETURN;
      }
    } else {
      table = m_part_share->get_table_part(part);

      if (table->space == 0 || table->tablespace != NULL ||
          part_elem->tablespace_name != NULL) {
        display_tablespace = true;
      }
    }
    part++;
  }
  if (part != num_parts) {
    DBUG_VOID_RETURN;
  }

  /* part_elem->data_file_name and tablespace_name should be correct from
  the .frm, but may have been changed, so update from SYS_DATAFILES.
  index_file_name is ignored, so remove it. */

  part = 0;
  part_it.rewind();
  while ((part_elem = part_it++)) {
    if (m_part_info->is_sub_partitioned()) {
      List_iterator<partition_element> subpart_it(part_elem->subpartitions);
      while ((sub_elem = subpart_it++)) {
        table = m_part_share->get_table_part(part++);
        update_part_elem(sub_elem, table, display_tablespace);
      }
    } else {
      table = m_part_share->get_table_part(part++);
      update_part_elem(part_elem, table, display_tablespace);
    }
  }
  DBUG_VOID_RETURN;
}

/** Set flags and append '/' to remote path if necessary. */
void create_table_info_t::set_remote_path_flags() {
  if (m_remote_path[0] != '\0') {
    ut_ad(DICT_TF_HAS_DATA_DIR(m_flags) != 0);

    /* os_file_make_remote_pathname will truncate
    everything after the last '/', so append '/'
    if it is not the last character. */

    size_t len = strlen(m_remote_path);
    if (m_remote_path[len - 1] != OS_PATH_SEPARATOR) {
      m_remote_path[len] = OS_PATH_SEPARATOR;
      m_remote_path[len + 1] = '\0';
    }
  } else {
    ut_ad(DICT_TF_HAS_DATA_DIR(m_flags) == 0);
  }
}

/** Creates a new table to an InnoDB database.
@param[in]	name		Table name (in filesystem charset).
@param[in]	form		MySQL Table containing information of
partitions, columns and indexes etc.
@param[in]	create_info	Additional create information, like
create statement string.
@param[in,out]	table_def	dd::Table object for table to be created.
Can be adjusted by this call. Changes to the table definition will be
persisted in the data-dictionary at statement commit time.
@return	0 or error number. */
int ha_innopart::create(const char *name, TABLE *form,
                        HA_CREATE_INFO *create_info, dd::Table *table_def) {
  int error;
  /** {database}/{tablename} */
  char table_name[FN_REFLEN];
  /** absolute path of table */
  char remote_path[FN_REFLEN];
  char partition_name[FN_REFLEN];
  char tablespace_name[NAME_LEN + 1];
  size_t table_name_len;
  char *partition_name_start;
  char table_data_file_name[FN_REFLEN];
  char table_level_tablespace_name[NAME_LEN + 1];
  const char *table_index_file_name;
  uint created = 0;
  THD *thd = ha_thd();
  trx_t *trx;

  if (thd_sql_command(thd) == SQLCOM_TRUNCATE) {
    return (truncate_impl(name, form, table_def));
  }

  if (high_level_read_only) {
    return (HA_ERR_INNODB_READ_ONLY);
  }

  trx = check_trx_exists(thd);

  create_table_info_t info(thd, form, create_info, table_name, remote_path,
                           tablespace_name, srv_file_per_table, false, 0, 0);

  DBUG_ENTER("ha_innopart::create");
  ut_ad(create_info != NULL);
  ut_ad(m_part_info == form->part_info);
  ut_ad(table_share != NULL);

  /* Not allowed to create temporary partitioned tables. */
  if (create_info != NULL &&
      (create_info->options & HA_LEX_CREATE_TMP_TABLE) != 0) {
    my_error(ER_PARTITION_NO_TEMPORARY, MYF(0));
    ut_ad(0);  // Can we support partitioned temporary tables?
    DBUG_RETURN(HA_ERR_INTERNAL_ERROR);
  }

  innobase_register_trx(ht, thd, trx);

  if (form->found_next_number_field) {
    dd_set_autoinc(table_def->se_private_data(),
                   create_info->auto_increment_value);
  }

  error = info.initialize();
  if (error != 0) {
    DBUG_RETURN(error);
  }

  /* Setup and check table level options. */
  error = info.prepare_create_table(name);
  if (error != 0) {
    DBUG_RETURN(error);
  }

  strcpy(partition_name, table_name);
  table_name_len = strlen(table_name);
  partition_name_start = partition_name + table_name_len;

  if (create_info->data_file_name != NULL) {
    /* Strip the tablename from the path. */
    strncpy(table_data_file_name, create_info->data_file_name, FN_REFLEN - 1);
    table_data_file_name[FN_REFLEN - 1] = '\0';
    char *ptr = strrchr(table_data_file_name, OS_PATH_SEPARATOR);
    ut_ad(ptr != NULL);
    if (ptr != NULL) {
      ptr++;
      *ptr = '\0';
      create_info->data_file_name = table_data_file_name;
    }
  } else {
    table_data_file_name[0] = '\0';
  }
  table_index_file_name = create_info->index_file_name;
  if (create_info->tablespace != NULL) {
    strcpy(table_level_tablespace_name, create_info->tablespace);
  } else {
    table_level_tablespace_name[0] = '\0';
  }

  /* It's also doable to get tablespace names by accessing
  dd::Tablespace::name according to dd_part->tablespace_id().
  However, it costs more. So as long as partition_element contains
  tablespace name, it's easier to check it */
  std::vector<const char *> tablespace_names;
  List_iterator_fast<partition_element> part_it(form->part_info->partitions);
  partition_element *part_elem;
  while ((part_elem = part_it++)) {
    const char *tablespace;
    if (form->part_info->is_sub_partitioned()) {
      List_iterator_fast<partition_element> sub_it(part_elem->subpartitions);
      partition_element *sub_elem;
      while ((sub_elem = sub_it++)) {
        tablespace = partition_get_tablespace(table_level_tablespace_name,
                                              part_elem, sub_elem);
        tablespace_names.push_back(tablespace);
      }
    } else {
      tablespace = partition_get_tablespace(table_level_tablespace_name,
                                            part_elem, NULL);
      tablespace_names.push_back(tablespace);
    }
  }

  for (const auto dd_part : *table_def->leaf_partitions()) {
    size_t len = Ha_innopart_share::create_partition_postfix(
        partition_name_start, FN_REFLEN - table_name_len, dd_part);

    if ((table_name_len + len + sizeof "/") >= FN_REFLEN) {
      error = HA_ERR_INTERNAL_ERROR;
      my_error(ER_PATH_LENGTH, MYF(0), partition_name);
      break;
    }

    const dd::Properties &options = dd_part->options();
    dd::String_type index_file_name;
    dd::String_type data_file_name;
    const char *tablespace_name;

    options.get(index_file_name_key, index_file_name);
    options.get(data_file_name_key, data_file_name);
    ut_ad(created < tablespace_names.size());
    tablespace_name = tablespace_names[created];

    if (!data_file_name.empty()) {
      create_info->data_file_name = data_file_name.c_str();
    }

    if (!index_file_name.empty()) {
      create_info->index_file_name = index_file_name.c_str();
    }

    if (!data_file_name.empty() &&
        dd_part->tablespace_id() == dd::INVALID_OBJECT_ID &&
        (tablespace_name == NULL ||
         strcmp(tablespace_name, dict_sys_t::s_file_per_table_name) != 0)) {
      create_info->tablespace = NULL;
    } else {
      create_info->tablespace = tablespace_name;
    }
    info.flags_reset();
    info.flags2_reset();

    if ((error = info.prepare_create_table(partition_name)) != 0) {
      break;
    }

    info.set_remote_path_flags();

    if ((error = info.create_table(&dd_part->table())) != 0) {
      break;
    }

    if ((error = info.create_table_update_global_dd<dd::Partition>(
             const_cast<dd::Partition *>(dd_part))) != 0) {
      break;
    }

    if ((error = info.create_table_update_dict()) != 0) {
      break;
    }

    info.detach();

    ++created;
    create_info->data_file_name = table_data_file_name;
    create_info->index_file_name = table_index_file_name;
    create_info->tablespace = table_level_tablespace_name;
  }

  create_info->data_file_name = nullptr;
  create_info->index_file_name = nullptr;
  create_info->tablespace = nullptr;

  DBUG_RETURN(error);
}

/** Drop a table.
@param[in]	name		table name
@param[in,out]	dd_table	data dictionary table
@return	error number
@retval	0 on success */
int ha_innopart::delete_table(const char *name, const dd::Table *dd_table) {
  char partition_name[FN_REFLEN];
  char *partition_name_start;
  size_t table_name_len;
  THD *thd = ha_thd();
  trx_t *trx = check_trx_exists(thd);
  int error = 0;

  DBUG_ENTER("ha_innopart::delete_table");

  ut_ad(dd_table != NULL);
  ut_ad(dd_table_is_partitioned(*dd_table));
  ut_ad(dd_table->is_persistent());

  if (high_level_read_only) {
    DBUG_RETURN(HA_ERR_TABLE_READONLY);
  }

  innobase_register_trx(ht, thd, trx);

  strcpy(partition_name, name);
  partition_name_start = partition_name + strlen(name);
  table_name_len = strlen(name);

  for (const dd::Partition *dd_part : dd_table->leaf_partitions()) {
    size_t len = Ha_innopart_share::create_partition_postfix(
        partition_name_start, FN_REFLEN - table_name_len, dd_part);

    if (table_name_len + len >= FN_REFLEN) {
      ut_ad(0);
      DBUG_RETURN(error);
    }

    error = innobase_basic_ddl::delete_impl<dd::Partition>(thd, partition_name,
                                                           dd_part);

    if (error != 0) {
      break;
    }
  }

  DBUG_RETURN(error);
}

/** Rename a table.
@param[in]	from		table name before rename
@param[in]	to		table name after rename
@param[in]	from_table	data dictionary table before rename
@param[in,out]	to_table	data dictionary table after rename
@return	error number
@retval	0 on success */
int ha_innopart::rename_table(const char *from, const char *to,
                              const dd::Table *from_table,
                              dd::Table *to_table) {
  THD *thd = ha_thd();
  int error = 0;

  DBUG_ENTER("ha_innopart::rename_table");

  ut_ad(from_table != NULL);
  ut_ad(to_table != NULL);
  ut_ad(from_table->se_private_id() == to_table->se_private_id());
  ut_ad(from_table->se_private_data().raw_string() ==
        to_table->se_private_data().raw_string());
  ut_ad(from_table->partition_type() == to_table->partition_type());

  if (high_level_read_only) {
    ib_senderrf(thd, IB_LOG_LEVEL_WARN, ER_READ_ONLY_MODE);
    DBUG_RETURN(HA_ERR_TABLE_READONLY);
  }

  /* Get the transaction associated with the current thd, or create one
  if not yet created */
  trx_t *trx = check_trx_exists(thd);

  trx_start_if_not_started(trx, false);
  innobase_register_trx(ht, thd, trx);

  TrxInInnoDB trx_in_innodb(trx);

  char from_name[FN_REFLEN];
  char to_name[FN_REFLEN];
  size_t from_table_name_len;
  size_t to_table_name_len;

  strcpy(from_name, from);
  strcpy(to_name, to);
  from_table_name_len = strlen(from_name);
  to_table_name_len = strlen(to_name);

  auto to_part = to_table->leaf_partitions()->begin();

  for (const auto from_part : from_table->leaf_partitions()) {
    ut_ad((*to_part) != NULL);

    size_t from_len = Ha_innopart_share::create_partition_postfix(
        from_name + from_table_name_len, FN_REFLEN - from_table_name_len,
        from_part);
    size_t to_len = Ha_innopart_share::create_partition_postfix(
        to_name + to_table_name_len, FN_REFLEN - to_table_name_len, *to_part);

    if (from_table_name_len + from_len >= FN_REFLEN ||
        to_table_name_len + to_len >= FN_REFLEN) {
      ut_ad(0);
      DBUG_RETURN(0);
    }

    error = innobase_basic_ddl::rename_impl<dd::Partition>(
        thd, from_name, to_name, from_part, *to_part);

    if (error != 0) {
      break;
    }

    ++to_part;
  }

  DBUG_RETURN(error);
}

/** Set DD discard attribute for tablespace.
@param[in]	table_def	dd table
@param[in]	discard		True if this table is discarded
@return	0 or error number. */
int ha_innopart::set_dd_discard_attribute(dd::Table *table_def, bool discard) {
  dict_table_t *table;
  uint i = 0;
  int error = 0;

  DBUG_ENTER("ha_innopart::update_dd_for_discard");

  for (dd::Partition *dd_part : *table_def->leaf_partitions()) {
    if (!m_part_info->is_partition_used(i++)) {
      continue;
    }

    table = m_part_share->get_table_part(i - 1);

    dd_part->set_se_private_id(table->id);

    /* Set discard flag. */
    dd::Properties &p = dd_part->table().se_private_data();
    p.set_bool(dd_table_key_strings[DD_TABLE_DISCARD], discard);

    /* Get Tablespace object */
    dd::Tablespace *dd_space = nullptr;
    THD *thd = ha_thd();
    dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
    dd::cache::Dictionary_client::Auto_releaser releaser(client);

    dd::Object_id dd_space_id = (*dd_part->indexes()->begin())->tablespace_id();
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
      ut_a(false);
    }

    for (auto dd_index : *dd_part->indexes()) {
      const dict_index_t *index = dd_find_index(table, dd_index);
      ut_ad(index != nullptr);

      dd::Properties &p = dd_index->se_private_data();
      p.set_uint32(dd_index_key_strings[DD_INDEX_ROOT], index->page);
    }
  }

  /* Set discard flag. */
  dd::Properties &p = table_def->table().se_private_data();
  p.set_bool(dd_table_key_strings[DD_TABLE_DISCARD], discard);

  /* Set new table id of the first partition to dd::Column::se_private_data */
  if (!discard) {
    table = m_part_share->get_table_part(0);

    for (auto dd_column : *table_def->columns()) {
      dd_column->se_private_data().set_uint64(dd_index_key_strings[DD_TABLE_ID],
                                              table->id);
    }
  }

  DBUG_RETURN(error);
}

/** Discards or imports an InnoDB tablespace.
@param[in]	discard		True if discard, else import.
@param[in,out]	table_def	dd::Table describing table which
tablespaces are to be imported or discarded. Can be adjusted by SE,
the changes will be saved into the data-dictionary at statement
commit time.
@return	0 or error number. */
int ha_innopart::discard_or_import_tablespace(bool discard,
                                              dd::Table *table_def) {
  int error = 0;
  uint i;

  DBUG_ENTER("ha_innopart::discard_or_import_tablespace");

  for (i = m_part_info->get_first_used_partition(); i < m_tot_parts;
       i = m_part_info->get_next_used_partition(i)) {
    m_prebuilt->table = m_part_share->get_table_part(i);
    error = ha_innobase::discard_or_import_tablespace(discard, table_def);
    if (error != 0) {
      break;
    }
  }

#ifdef UNIV_DEBUG
  if (!discard && table_def->se_private_data().exists(
                      dd_table_key_strings[DD_TABLE_INSTANT_COLS])) {
    ut_ad(dd_table_has_instant_cols(*table_def));
  }
#endif /* UNIV_DEBUG */

  m_prebuilt->table = m_part_share->get_table_part(0);

  /* IMPORT/DISCARD also means resetting auto_increment. Make sure
  that auto_increment initialization is done after all partitions
  are imported. */
  if (table->found_next_number_field != NULL) {
    lock_auto_increment();
    m_part_share->next_auto_inc_val = 0;
    m_part_share->auto_inc_initialized = false;
    unlock_auto_increment();
  }

  /* Update dd partition for discard, since the table ids changed,
  we need to change se_private_id accordingly. */
  if (error == 0) {
    error = set_dd_discard_attribute(table_def, discard);
  }

  DBUG_RETURN(error);
}

/** Compare key and rowid.
Helper function for sorting records in the priority queue.
a/b points to table->record[0] rows which must have the
key fields set. The bytes before a and b store the rowid.
This is used for comparing/sorting rows first according to
KEY and if same KEY, by rowid (ref).
@param[in]	key_info	Null terminated array of index information.
@param[in]	a		Pointer to record+ref in first record.
@param[in]	b		Pointer to record+ref in second record.
@return	Return value is SIGN(first_rec - second_rec)
@retval	0	Keys are equal.
@retval	-1	second_rec is greater than first_rec.
@retval	+1	first_rec is greater than second_rec. */
int ha_innopart::key_and_rowid_cmp(KEY **key_info, uchar *a, uchar *b) {
  int cmp = key_rec_cmp(key_info, a, b);
  if (cmp != 0) {
    return (cmp);
  }

  /* We must compare by rowid, which is added before the record,
  in the priority queue. */

  return (memcmp(a - DATA_ROW_ID_LEN, b - DATA_ROW_ID_LEN, DATA_ROW_ID_LEN));
}

/** Extra hints from MySQL.
@param[in]	operation	Operation hint.
@return	0 or error number. */
int ha_innopart::extra(enum ha_extra_function operation) {
  if (operation == HA_EXTRA_SECONDARY_SORT_ROWID) {
    /* index_init(sorted=true) must have been called! */
    ut_ad(m_ordered);
    ut_ad(m_ordered_rec_buffer != NULL);
    /* No index_read call must have been done! */
    ut_ad(m_queue->empty());

    /* If not PK is set as secondary sort, do secondary sort by
    rowid/ref. */

    ut_ad(m_curr_key_info[1] != NULL ||
          m_prebuilt->clust_index_was_generated != 0 ||
          m_curr_key_info[0] == table->key_info + table->s->primary_key);

    if (m_curr_key_info[1] == NULL && m_prebuilt->clust_index_was_generated) {
      m_ref_usage = Partition_helper::REF_USED_FOR_SORT;
      m_queue->m_fun = key_and_rowid_cmp;
    }
    return (0);
  }

  /* In case of alter copy operation, set/unset the skip_undo_flag
  for all partitions depends on the operation. */
  if (operation == HA_EXTRA_BEGIN_ALTER_COPY ||
      operation == HA_EXTRA_END_ALTER_COPY) {
    for (uint i = m_part_info->get_first_used_partition(); i < m_tot_parts;
         i = m_part_info->get_next_used_partition(i)) {
      dict_table_t *table_part = m_part_share->get_table_part(i);

      table_part->skip_alter_undo = (operation == HA_EXTRA_BEGIN_ALTER_COPY);
    }

    return (0);
  }

  return (ha_innobase::extra(operation));
}

int ha_innopart::truncate_impl(const char *name, TABLE *form,
                               dd::Table *table_def) {
  DBUG_ENTER("ha_innopart::truncate_impl");

  ut_ad(table_def != NULL);
  ut_ad(dd_table_is_partitioned(*table_def));
  ut_ad(table_def->is_persistent());

  if (high_level_read_only) {
    DBUG_RETURN(HA_ERR_TABLE_READONLY);
  }

  THD *thd = ha_thd();
  trx_t *trx = check_trx_exists(thd);
  char partition_name[FN_REFLEN];
  uint16_t table_name_len;
  bool has_autoinc = false;
  int error = 0;

  innobase_register_trx(ht, thd, trx);

  table_name_len = strlen(name);
  ut_ad(table_name_len < FN_REFLEN);
  memcpy(partition_name, name, table_name_len);

  for (const auto dd_part : *table_def->leaf_partitions()) {
    size_t len;
    char norm_name[FN_REFLEN];
    dict_table_t *part_table = nullptr;

    len = Ha_innopart_share::create_partition_postfix(
        partition_name + table_name_len, FN_REFLEN - table_name_len, dd_part);
    ut_a(len + table_name_len < FN_REFLEN);

    normalize_table_name(norm_name, partition_name);

    innobase_truncate<dd::Partition> truncator(thd, norm_name, form, dd_part,
                                               false);

    error = truncator.open_table(part_table);
    if (error != 0) {
      DBUG_RETURN(error);
    }

    has_autoinc = dict_table_has_autoinc_col(part_table);

    if (dict_table_is_discarded(part_table)) {
      ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_TABLESPACE_DISCARDED, norm_name);
      DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
    } else if (part_table->ibd_file_missing) {
      DBUG_RETURN(HA_ERR_TABLESPACE_MISSING);
    }

    error = truncator.exec();

    if (error != 0) {
      DBUG_RETURN(error);
    }
  }

  ut_ad(error == 0);

  if (has_autoinc) {
    dd_set_autoinc(table_def->se_private_data(), 0);
  }

  if (dd_table_has_instant_cols(*table_def)) {
    for (dd::Partition *dd_part : *table_def->leaf_partitions()) {
      if (dd_part_has_instant_cols(*dd_part)) {
        dd_clear_instant_part(*dd_part);
      }
    }

    dd_clear_instant_table(*table_def);
  }

  DBUG_RETURN(error);
}

/** Delete all rows in the requested partitions.
Done by deleting the partitions and recreate them again.
@param[in,out]	dd_table	dd::Table object for partitioned table
which partitions need to be truncated. Can be adjusted by this call.
Changes to the table definition will be persisted in the data-dictionary
at statement commit time.
@return	0 or error number. */
int ha_innopart::truncate_partition_low(dd::Table *dd_table) {
  int error = 0;
  const char *table_name = table->s->normalized_path.str;
  char partition_name[FN_REFLEN];
  uint16_t table_name_len;
  THD *thd = ha_thd();
  trx_t *trx = check_trx_exists(thd);
  uint16_t i = 0;
  uint64_t autoinc = 0;
  bool truncate_all = (m_part_info->num_partitions_used() == m_tot_parts);

  DBUG_ENTER("ha_innopart::truncate_partition_low");

  if (high_level_read_only) {
    DBUG_RETURN(HA_ERR_TABLE_READONLY);
  }

  innobase_register_trx(ht, thd, trx);

  table_name_len = strlen(table_name);
  ut_ad(table_name_len < FN_REFLEN);
  memcpy(partition_name, table_name, table_name_len);

  for (const auto dd_part : *dd_table->leaf_partitions()) {
    size_t len;
    char norm_name[FN_REFLEN];
    dict_table_t *part_table = nullptr;

    len = Ha_innopart_share::create_partition_postfix(
        partition_name + table_name_len, FN_REFLEN - table_name_len, dd_part);
    ut_a(len + table_name_len < FN_REFLEN);

    normalize_table_name(norm_name, partition_name);

    innobase_truncate<dd::Partition> truncator(thd, norm_name, table, dd_part,
                                               !truncate_all);

    error = truncator.open_table(part_table);
    if (error != 0) {
      DBUG_RETURN(error);
    }

    if (part_table->autoinc_persisted > autoinc) {
      autoinc = part_table->autoinc_persisted;
    }

    if (!m_part_info->is_partition_used(i++)) {
      continue;
    }

    if (dict_table_is_discarded(part_table)) {
      ib_senderrf(thd, IB_LOG_LEVEL_ERROR, ER_TABLESPACE_DISCARDED, table_name);
      DBUG_RETURN(HA_ERR_NO_SUCH_TABLE);
    } else if (part_table->ibd_file_missing) {
      DBUG_RETURN(HA_ERR_TABLESPACE_MISSING);
    }

    error = truncator.exec();

    if (error != 0) {
      DBUG_RETURN(error);
    }

    if (dd_table_has_instant_cols(*dd_table) &&
        dd_part_has_instant_cols(*dd_part)) {
      dd_clear_instant_part(*dd_part);
    }
  }

  ut_ad(error == 0);

  /* If it's TRUNCATE PARTITION ALL, reset the AUTOINC */
  if (table->found_next_number_field) {
    dd_set_autoinc(dd_table->se_private_data(),
                   (truncate_all ? 0 : autoinc + 1));
  }

  if (dd_table_has_instant_cols(*dd_table) &&
      !dd_table_part_has_instant_cols(*dd_table)) {
    dd_clear_instant_table(*dd_table);
  }

  DBUG_RETURN(error);
}

/** Total number of rows in all used partitions.
Returns the exact number of records that this client can see using this
handler object.
@param[out]	num_rows	Number of rows.
@return	0 or error number. */
int ha_innopart::records(ha_rows *num_rows) {
  ha_rows n_rows;
  int err;
  DBUG_ENTER("ha_innopart::records()");

  *num_rows = 0;

  /* The index scan is probably so expensive, so the overhead
  of the rest of the function is neglectable for each partition.
  So no current reason for optimizing this further. */

  for (uint i = m_part_info->get_first_used_partition(); i < m_tot_parts;
       i = m_part_info->get_next_used_partition(i)) {
    set_partition(i);
    err = ha_innobase::records(&n_rows);
    update_partition(i);
    if (err != 0) {
      *num_rows = HA_POS_ERROR;
      DBUG_RETURN(err);
    }
    *num_rows += n_rows;
  }
  DBUG_RETURN(0);
}

/** Estimates the number of index records in a range.
@param[in]	keynr	Index number.
@param[in]	min_key	Start key value (or NULL).
@param[in]	max_key	End key value (or NULL).
@return	estimated number of rows. */
ha_rows ha_innopart::records_in_range(uint keynr, key_range *min_key,
                                      key_range *max_key) {
  KEY *key;
  dict_index_t *index;
  dtuple_t *range_start;
  dtuple_t *range_end;
  int64_t n_rows = 0;
  page_cur_mode_t mode1;
  page_cur_mode_t mode2;
  mem_heap_t *heap;
  uint part_id;

  DBUG_ENTER("ha_innopart::records_in_range");
  DBUG_PRINT("info", ("keynr %u min %p max %p", keynr, min_key, max_key));

  ut_a(m_prebuilt->trx == thd_to_trx(ha_thd()));

  m_prebuilt->trx->op_info = (char *)"estimating records in index range";

  active_index = keynr;

  key = table->key_info + active_index;

  part_id = m_part_info->get_first_used_partition();
  if (part_id == MY_BIT_NONE) {
    DBUG_RETURN(0);
  }
  /* This also sets m_prebuilt->index! */
  set_partition(part_id);
  index = m_prebuilt->index;

  /* There exists possibility of not being able to find requested
  index due to inconsistency between MySQL and InoDB dictionary info.
  Necessary message should have been printed in innopart_get_index(). */
  if (index == NULL || dict_table_is_discarded(m_prebuilt->table) ||
      !index->is_usable(m_prebuilt->trx)) {
    n_rows = HA_POS_ERROR;
    goto func_exit;
  }

  heap = mem_heap_create(
      2 * (key->actual_key_parts * sizeof(dfield_t) + sizeof(dtuple_t)));

  range_start = dtuple_create(heap, key->actual_key_parts);
  dict_index_copy_types(range_start, index, key->actual_key_parts);

  range_end = dtuple_create(heap, key->actual_key_parts);
  dict_index_copy_types(range_end, index, key->actual_key_parts);

  row_sel_convert_mysql_key_to_innobase(
      range_start, m_prebuilt->srch_key_val1, m_prebuilt->srch_key_val_len,
      index, (byte *)(min_key ? min_key->key : (const uchar *)0),
      (ulint)(min_key ? min_key->length : 0), m_prebuilt->trx);

  ut_ad(min_key != NULL ? range_start->n_fields > 0
                        : range_start->n_fields == 0);

  row_sel_convert_mysql_key_to_innobase(
      range_end, m_prebuilt->srch_key_val2, m_prebuilt->srch_key_val_len, index,
      (byte *)(max_key != NULL ? max_key->key : (const uchar *)0),
      (ulint)(max_key != NULL ? max_key->length : 0), m_prebuilt->trx);

  ut_ad(max_key != NULL ? range_end->n_fields > 0 : range_end->n_fields == 0);

  mode1 = convert_search_mode_to_innobase(min_key ? min_key->flag
                                                  : HA_READ_KEY_EXACT);
  mode2 = convert_search_mode_to_innobase(max_key ? max_key->flag
                                                  : HA_READ_KEY_EXACT);

  if (mode1 != PAGE_CUR_UNSUPP && mode2 != PAGE_CUR_UNSUPP) {
    n_rows = btr_estimate_n_rows_in_range(index, range_start, mode1, range_end,
                                          mode2);
    DBUG_PRINT("info", ("part_id %u rows %ld", part_id, (long int)n_rows));
    for (part_id = m_part_info->get_next_used_partition(part_id);
         part_id < m_tot_parts;
         part_id = m_part_info->get_next_used_partition(part_id)) {
      index = m_part_share->get_index(part_id, keynr);
      /* Individual partitions can be discarded
      we need to check each partition */
      if (index == NULL || dict_table_is_discarded(index->table) ||
          !index->is_usable(m_prebuilt->trx)) {
        n_rows = HA_POS_ERROR;
        mem_heap_free(heap);
        goto func_exit;
      }
      int64_t n = btr_estimate_n_rows_in_range(index, range_start, mode1,
                                               range_end, mode2);
      n_rows += n;
      DBUG_PRINT("info", ("part_id %u rows %ld (%ld)", part_id, (long int)n,
                          (long int)n_rows));
    }
  } else {
    n_rows = HA_POS_ERROR;
  }

  mem_heap_free(heap);

func_exit:

  m_prebuilt->trx->op_info = (char *)"";

  /* The MySQL optimizer seems to believe an estimate of 0 rows is
  always accurate and may return the result 'Empty set' based on that.
  The accuracy is not guaranteed, and even if it were, for a locking
  read we should anyway perform the search to set the next-key lock.
  Add 1 to the value to make sure MySQL does not make the assumption! */

  if (n_rows == 0) {
    n_rows = 1;
  }

  DBUG_RETURN((ha_rows)n_rows);
}

/** Gives an UPPER BOUND to the number of rows in a table.
This is used in filesort.cc.
@return	upper bound of rows. */
ha_rows ha_innopart::estimate_rows_upper_bound() {
  const dict_index_t *index;
  ulonglong estimate = 0;
  ulonglong local_data_file_length;
  ulint stat_n_leaf_pages;

  DBUG_ENTER("ha_innopart::estimate_rows_upper_bound");

  /* We do not know if MySQL can call this function before calling
  external_lock(). To be safe, update the thd of the current table
  handle. */

  update_thd(ha_thd());

  m_prebuilt->trx->op_info = "calculating upper bound for table rows";

  for (uint i = m_part_info->get_first_used_partition(); i < m_tot_parts;
       i = m_part_info->get_next_used_partition(i)) {
    m_prebuilt->table = m_part_share->get_table_part(i);
    index = m_prebuilt->table->first_index();

    stat_n_leaf_pages = index->stat_n_leaf_pages;

    ut_a(stat_n_leaf_pages > 0);

    local_data_file_length = ((ulonglong)stat_n_leaf_pages) * UNIV_PAGE_SIZE;

    /* Calculate a minimum length for a clustered index record
    and from that an upper bound for the number of rows.
    Since we only calculate new statistics in row0mysql.cc when a
    table has grown by a threshold factor,
    we must add a safety factor 2 in front of the formula below. */

    estimate += 2 * local_data_file_length / dict_index_calc_min_rec_len(index);
  }

  m_prebuilt->trx->op_info = "";

  DBUG_RETURN((ha_rows)estimate);
}

/** Time estimate for full table scan.
How many seeks it will take to read through the table. This is to be
comparable to the number returned by records_in_range so that we can
decide if we should scan the table or use keys.
@return	estimated time measured in disk seeks. */
double ha_innopart::scan_time() {
  double scan_time = 0.0;
  DBUG_ENTER("ha_innopart::scan_time");

  for (uint i = m_part_info->get_first_used_partition(); i < m_tot_parts;
       i = m_part_info->get_next_used_partition(i)) {
    m_prebuilt->table = m_part_share->get_table_part(i);
    scan_time += ha_innobase::scan_time();
  }
  DBUG_RETURN(scan_time);
}

/** Updates the statistics for one partition (table).
@param[in]	table		Table to update the statistics for.
@param[in]	is_analyze	True if called from "::analyze()".
@return	error code. */
static int update_table_stats(dict_table_t *table, bool is_analyze) {
  dict_stats_upd_option_t opt;
  dberr_t ret;

  if (dict_stats_is_persistent_enabled(table)) {
    if (is_analyze) {
      opt = DICT_STATS_RECALC_PERSISTENT;
    } else {
      /* This is e.g. 'SHOW INDEXES',
      fetch the persistent stats from disk. */
      opt = DICT_STATS_FETCH_ONLY_IF_NOT_IN_MEMORY;
    }
  } else {
    opt = DICT_STATS_RECALC_TRANSIENT;
  }

  ut_ad(!mutex_own(&dict_sys->mutex));
  ret = dict_stats_update(table, opt);

  if (ret != DB_SUCCESS) {
    return (HA_ERR_GENERIC);
  }
  return (0);
}

/** Updates and return statistics.
Returns statistics information of the table to the MySQL interpreter,
in various fields of the handle object.
@param[in]	flag		Flags for what to update and return.
@param[in]	is_analyze	True if called from "::analyze()".
@return	HA_ERR_* error code or 0. */
int ha_innopart::info_low(uint flag, bool is_analyze) {
  dict_table_t *ib_table;
  ib_uint64_t max_rows = 0;
  uint biggest_partition = 0;
  int error = 0;

  DBUG_ENTER("ha_innopart::info_low");

  /* If we are forcing recovery at a high level, we will suppress
  statistics calculation on tables, because that may crash the
  server if an index is badly corrupted. */

  /* We do not know if MySQL can call this function before calling
  external_lock(). To be safe, update the thd of the current table
  handle. */

  update_thd(ha_thd());

  m_prebuilt->trx->op_info = (char *)"returning various info to MySQL";

  ut_ad(m_part_share->get_table_part(0)->n_ref_count > 0);

  if ((flag & HA_STATUS_TIME) != 0) {
    stats.update_time = 0;

    if (is_analyze) {
      /* Only analyze the given partitions. */
      int error = set_altered_partitions();
      if (error != 0) {
        /* Already checked in mysql_admin_table! */
        ut_ad(0);
        DBUG_RETURN(error);
      }
    }
    if (is_analyze || innobase_stats_on_metadata) {
      m_prebuilt->trx->op_info = "updating table statistics";
    }

    /* TODO: Only analyze the PK for all partitions,
    then the secondary indexes only for the largest partition! */
    for (uint i = m_part_info->get_first_used_partition(); i < m_tot_parts;
         i = m_part_info->get_next_used_partition(i)) {
      ib_table = m_part_share->get_table_part(i);
      if (is_analyze || innobase_stats_on_metadata) {
        error = update_table_stats(ib_table, is_analyze);
        if (error != 0) {
          m_prebuilt->trx->op_info = "";
          DBUG_RETURN(error);
        }
      }
      set_if_bigger(stats.update_time, (ulong)ib_table->update_time);
    }

    if (is_analyze || innobase_stats_on_metadata) {
      m_prebuilt->trx->op_info = "returning various info to MySQL";
    }
  }

  if ((flag & HA_STATUS_VARIABLE) != 0) {
    /* TODO: If this is called after pruning, then we could
    also update the statistics according to the non-pruned
    partitions, by allocating new rec_per_key on the TABLE,
    instead of using the info from the TABLE_SHARE. */
    ulint stat_clustered_index_size = 0;
    ulint stat_sum_of_other_index_sizes = 0;
    ib_uint64_t n_rows = 0;
    ulint avail_space = 0;
    bool checked_sys_tablespace = false;

    if ((flag & HA_STATUS_VARIABLE_EXTRA) != 0) {
      stats.delete_length = 0;
    }

    for (uint i = m_part_info->get_first_used_partition(); i < m_tot_parts;
         i = m_part_info->get_next_used_partition(i)) {
      ib_table = m_part_share->get_table_part(i);
      if ((flag & HA_STATUS_NO_LOCK) == 0) {
        dict_table_stats_lock(ib_table, RW_S_LATCH);
      }

      ut_a(ib_table->stat_initialized);

      n_rows += ib_table->stat_n_rows;
      if (ib_table->stat_n_rows > max_rows) {
        max_rows = ib_table->stat_n_rows;
        biggest_partition = i;
      }

      stat_clustered_index_size += ib_table->stat_clustered_index_size;

      stat_sum_of_other_index_sizes += ib_table->stat_sum_of_other_index_sizes;

      if ((flag & HA_STATUS_NO_LOCK) == 0) {
        dict_table_stats_unlock(ib_table, RW_S_LATCH);
      }

      if ((flag & HA_STATUS_VARIABLE_EXTRA) != 0 &&
          (flag & HA_STATUS_NO_LOCK) == 0 &&
          srv_force_recovery < SRV_FORCE_NO_IBUF_MERGE &&
          avail_space != ULINT_UNDEFINED) {
        /* Only count system tablespace once! */
        if (fsp_is_system_or_temp_tablespace(ib_table->space)) {
          if (checked_sys_tablespace) {
            continue;
          }
          checked_sys_tablespace = true;
        }

        uintmax_t space =
            fsp_get_available_space_in_free_extents(ib_table->space);
        if (space == UINTMAX_MAX) {
          THD *thd = ha_thd();
          const char *table_name = ib_table->name.m_name;

          push_warning_printf(thd, Sql_condition::SL_WARNING, ER_CANT_GET_STAT,
                              "InnoDB: Trying to get the"
                              " free space for partition %s"
                              " but its tablespace has been"
                              " discarded or the .ibd file"
                              " is missing. Setting the free"
                              " space of the partition to"
                              " zero.",
                              ut_get_name(m_prebuilt->trx, table_name).c_str());
        } else {
          avail_space += static_cast<ulint>(space);
        }
      }
    }

    /*
    The MySQL optimizer seems to assume in a left join that n_rows
    is an accurate estimate if it is zero. Of course, it is not,
    since we do not have any locks on the rows yet at this phase.
    Since SHOW TABLE STATUS seems to call this function with the
    HA_STATUS_TIME flag set, while the left join optimizer does not
    set that flag, we add one to a zero value if the flag is not
    set. That way SHOW TABLE STATUS will show the best estimate,
    while the optimizer never sees the table empty. */

    if (n_rows == 0 && (flag & HA_STATUS_TIME) == 0) {
      n_rows++;
    }

    /* Fix bug#40386: Not flushing query cache after truncate.
    n_rows can not be 0 unless the table is empty, set to 1
    instead. The original problem of bug#29507 is actually
    fixed in the server code. */
    if (thd_sql_command(m_user_thd) == SQLCOM_TRUNCATE) {
      n_rows = 1;

      /* We need to reset the m_prebuilt value too, otherwise
      checks for values greater than the last value written
      to the table will fail and the autoinc counter will
      not be updated. This will force write_row() into
      attempting an update of the table's AUTOINC counter. */

      m_prebuilt->autoinc_last_value = 0;
    }

    /* Take page_size from first partition. */
    ib_table = m_part_share->get_table_part(0);
    const page_size_t &page_size = dict_table_page_size(ib_table);

    stats.records = (ha_rows)n_rows;
    stats.deleted = 0;
    stats.data_file_length =
        ((ulonglong)stat_clustered_index_size) * page_size.physical();
    stats.index_file_length =
        ((ulonglong)stat_sum_of_other_index_sizes) * page_size.physical();

    /* See ha_innobase::info_low() for comments! */
    if ((flag & HA_STATUS_NO_LOCK) == 0 &&
        (flag & HA_STATUS_VARIABLE_EXTRA) != 0 &&
        srv_force_recovery < SRV_FORCE_NO_IBUF_MERGE) {
      stats.delete_length = avail_space * 1024;
    }

    stats.check_time = 0;
    stats.mrr_length_per_rec =
        ref_length + sizeof(void *) - PARTITION_BYTES_IN_POS;

    if (stats.records == 0) {
      stats.mean_rec_length = 0;
    } else {
      stats.mean_rec_length = (ulong)(stats.data_file_length / stats.records);
    }
  }

  if ((flag & HA_STATUS_CONST) != 0) {
    /* Find max rows and biggest partition. */
    for (uint i = 0; i < m_tot_parts; i++) {
      /* Skip partitions from above. */
      if ((flag & HA_STATUS_VARIABLE) == 0 ||
          !bitmap_is_set(&(m_part_info->read_partitions), i)) {
        ib_table = m_part_share->get_table_part(i);
        if (ib_table->stat_n_rows > max_rows) {
          max_rows = ib_table->stat_n_rows;
          biggest_partition = i;
        }
      }
    }
    ib_table = m_part_share->get_table_part(biggest_partition);
    /* Verify the number of index in InnoDB and MySQL
    matches up. If m_prebuilt->clust_index_was_generated
    holds, InnoDB defines GEN_CLUST_INDEX internally. */
    ulint num_innodb_index = UT_LIST_GET_LEN(ib_table->indexes) -
                             m_prebuilt->clust_index_was_generated;
    if (table->s->keys < num_innodb_index) {
      /* If there are too many indexes defined
      inside InnoDB, ignore those that are being
      created, because MySQL will only consider
      the fully built indexes here. */

      for (const dict_index_t *index = UT_LIST_GET_FIRST(ib_table->indexes);
           index != NULL; index = UT_LIST_GET_NEXT(indexes, index)) {
        /* First, online index creation is
        completed inside InnoDB, and then
        MySQL attempts to upgrade the
        meta-data lock so that it can rebuild
        the .frm file. If we get here in that
        time frame, dict_index_is_online_ddl()
        would not hold and the index would
        still not be included in TABLE_SHARE. */
        if (!index->is_committed()) {
          num_innodb_index--;
        }
      }

      if (table->s->keys < num_innodb_index &&
          (innobase_fts_check_doc_id_index(ib_table, NULL, NULL) ==
           FTS_EXIST_DOC_ID_INDEX)) {
        num_innodb_index--;
      }
    }

    if (table->s->keys != num_innodb_index) {
      ib::error(ER_IB_MSG_595)
          << "Table " << ib_table->name << " contains " << num_innodb_index
          << " indexes inside InnoDB, which"
             " is different from the number of"
             " indexes "
          << table->s->keys << " defined in the MySQL";
    }

    if ((flag & HA_STATUS_NO_LOCK) == 0) {
      dict_table_stats_lock(ib_table, RW_S_LATCH);
    }

    ut_a(ib_table->stat_initialized);

    for (ulong i = 0; i < table->s->keys; i++) {
      ulong j;
      /* We could get index quickly through internal
      index mapping with the index translation table.
      The identity of index (match up index name with
      that of table->key_info[i]) is already verified in
      innopart_get_index(). */
      dict_index_t *index = innopart_get_index(biggest_partition, i);

      if (index == NULL) {
        ib::error(ER_IB_MSG_596) << "Table " << ib_table->name
                                 << " contains fewer"
                                    " indexes inside InnoDB than"
                                    " are defined in the MySQL"
                                    " .frm file. Have you mixed up"
                                    " .frm files from different"
                                    " installations? "
                                 << TROUBLESHOOTING_MSG;
        break;
      }

      KEY *key = &table->key_info[i];
      for (j = 0; j < key->actual_key_parts; j++) {
        if ((key->flags & HA_FULLTEXT) != 0) {
          /* The whole concept has no validity
          for FTS indexes. */
          key->set_records_per_key(j, 1.0f);
          continue;
        }

        if ((j + 1) > index->n_uniq) {
          ib::error(ER_IB_MSG_597) << "Index " << index->name << " of "
                                   << ib_table->name << " has " << index->n_uniq
                                   << " columns unique inside"
                                      " InnoDB, but MySQL is"
                                      " asking statistics for "
                                   << j + 1
                                   << " columns. Have"
                                      " you mixed up .frm files"
                                      " from different"
                                      " installations? "
                                   << TROUBLESHOOTING_MSG;
          break;
        }

        /* innodb_rec_per_key() will use
        index->stat_n_diff_key_vals[] and the value we
        pass index->table->stat_n_rows. Both are
        calculated by ANALYZE and by the background
        stats gathering thread (which kicks in when too
        much of the table has been changed). In
        addition table->stat_n_rows is adjusted with
        each DML (e.g. ++ on row insert). Those
        adjustments are not MVCC'ed and not even
        reversed on rollback. So,
        index->stat_n_diff_key_vals[] and
        index->table->stat_n_rows could have been
        calculated at different time. This is
        acceptable. */
        const rec_per_key_t rec_per_key =
            innodb_rec_per_key(index, j, max_rows);

        key->set_records_per_key(j, rec_per_key);
      }
    }

    if ((flag & HA_STATUS_NO_LOCK) == 0) {
      dict_table_stats_unlock(ib_table, RW_S_LATCH);
    }
  }

  if (srv_force_recovery >= SRV_FORCE_NO_IBUF_MERGE) {
    goto func_exit;
  }

  if ((flag & HA_STATUS_ERRKEY) != 0) {
    const dict_index_t *err_index;

    ut_a(m_prebuilt->trx);
    ut_a(m_prebuilt->trx->magic_n == TRX_MAGIC_N);

    err_index = trx_get_error_info(m_prebuilt->trx);

    if (err_index != NULL) {
      errkey = m_part_share->get_mysql_key(m_last_part, err_index);
    } else {
      errkey =
          (unsigned int)((m_prebuilt->trx->error_key_num == ULINT_UNDEFINED)
                             ? UINT_MAX
                             : m_prebuilt->trx->error_key_num);
    }
  }

  if ((flag & HA_STATUS_AUTO) != 0) {
    /* auto_inc is only supported in first key for InnoDB! */
    ut_ad(table_share->next_number_keypart == 0);
    DBUG_PRINT("info", ("HA_STATUS_AUTO"));
    if (table->found_next_number_field == NULL) {
      stats.auto_increment_value = 0;
    } else {
      /* Lock to avoid two concurrent initializations. */
      lock_auto_increment();
      if (m_part_share->auto_inc_initialized) {
        stats.auto_increment_value = m_part_share->next_auto_inc_val;
      } else {
        /* The auto-inc mutex in the table_share is
        locked, so we do not need to have the handlers
        locked. */

        error = initialize_auto_increment((flag & HA_STATUS_NO_LOCK) != 0);
        stats.auto_increment_value = m_part_share->next_auto_inc_val;
      }
      unlock_auto_increment();
    }
  }

func_exit:
  m_prebuilt->trx->op_info = (char *)"";

  DBUG_RETURN(error);
}

/** Optimize table.
This is mapped to "ALTER TABLE tablename ENGINE=InnoDB", which rebuilds
the table in MySQL.
@param[in]	thd		Connection thread handle.
@param[in]	check_opt	Currently ignored.
@return	0 for success else error code. */
int ha_innopart::optimize(THD *thd, HA_CHECK_OPT *check_opt) {
  return (HA_ADMIN_TRY_ALTER);
}

/** Checks a partitioned table.
Tries to check that an InnoDB table is not corrupted. If corruption is
noticed, prints to stderr information about it. In case of corruption
may also assert a failure and crash the server. Also checks for records
in wrong partition.
@param[in]	thd		MySQL THD object/thread handle.
@param[in]	check_opt	Check options.
@return	HA_ADMIN_CORRUPT or HA_ADMIN_OK. */
int ha_innopart::check(THD *thd, HA_CHECK_OPT *check_opt) {
  uint error = HA_ADMIN_OK;
  uint i;

  DBUG_ENTER("ha_innopart::check");
  /* TODO: Enhance this to:
  - Every partition has the same structure.
  - The names are correct (partition names checked in ::open()?)
  Currently it only does normal InnoDB check of each partition. */

  if (set_altered_partitions()) {
    ut_ad(0);  // Already checked by set_part_state()!
    DBUG_RETURN(HA_ADMIN_INVALID);
  }
  for (i = m_part_info->get_first_used_partition(); i < m_tot_parts;
       i = m_part_info->get_next_used_partition(i)) {
    m_prebuilt->table = m_part_share->get_table_part(i);
    error = ha_innobase::check(thd, check_opt);
    if (error != 0) {
      break;
    }
    if ((check_opt->flags & (T_MEDIUM | T_EXTEND)) != 0) {
      error = Partition_helper::check_misplaced_rows(i, false);
      if (error != 0) {
        break;
      }
    }
  }
  if (error != 0) {
    print_admin_msg(thd, 256, "error", table_share->db.str, table->alias,
                    "check",
                    m_is_sub_partitioned ? "Subpartition %s returned error"
                                         : "Partition %s returned error",
                    m_part_share->get_partition_name(i));
  }

  DBUG_RETURN(error);
}

/** Repair a partitioned table.
Only repairs records in wrong partitions (moves them to the correct
partition or deletes them if not in any partition).
@param[in]	thd		MySQL THD object/thread handle.
@param[in]	repair_opt	Repair options.
@return	0 or error code. */
int ha_innopart::repair(THD *thd, HA_CHECK_OPT *repair_opt) {
  uint error = HA_ADMIN_OK;

  DBUG_ENTER("ha_innopart::repair");

  /* TODO: enable this warning to be clear about what is repaired.
  Currently disabled to generate smaller test diffs. */
#ifdef ADD_WARNING_FOR_REPAIR_ONLY_PARTITION
  push_warning_printf(thd, Sql_condition::SL_WARNING, ER_ILLEGAL_HA,
                      "Only moving rows from wrong partition to correct"
                      " partition is supported,"
                      " repairing InnoDB indexes is not yet supported!");
#endif

  /* Only repair partitions for MEDIUM or EXTENDED options. */
  if ((repair_opt->flags & (T_MEDIUM | T_EXTEND)) == 0) {
    DBUG_RETURN(HA_ADMIN_OK);
  }
  if (set_altered_partitions()) {
    ut_ad(0);  // Already checked by set_part_state()!
    DBUG_RETURN(HA_ADMIN_INVALID);
  }
  for (uint i = m_part_info->get_first_used_partition(); i < m_tot_parts;
       i = m_part_info->get_next_used_partition(i)) {
    /* TODO: Implement and use ha_innobase::repair()! */
    error = Partition_helper::check_misplaced_rows(i, true);
    if (error != 0) {
      print_admin_msg(thd, 256, "error", table_share->db.str, table->alias,
                      "repair",
                      m_is_sub_partitioned ? "Subpartition %s returned error"
                                           : "Partition %s returned error",
                      m_part_share->get_partition_name(i));
      break;
    }
  }

  DBUG_RETURN(error);
}

/** Check if possible to switch engine (no foreign keys).
Checks if ALTER TABLE may change the storage engine of the table.
Changing storage engines is not allowed for tables for which there
are foreign key constraints (parent or child tables).
@return	true if can switch engines. */
bool ha_innopart::can_switch_engines() {
  bool can_switch;

  DBUG_ENTER("ha_innopart::can_switch_engines");
  can_switch = ha_innobase::can_switch_engines();
  ut_ad(can_switch);

  DBUG_RETURN(can_switch);
}

/** Checks if a table is referenced by a foreign key.
The MySQL manual states that a REPLACE is either equivalent to an INSERT,
or DELETE(s) + INSERT. Only a delete is then allowed internally to resolve
a duplicate key conflict in REPLACE, not an update.
@return	> 0 if referenced by a FOREIGN KEY. */
uint ha_innopart::referenced_by_foreign_key() {
  if (dict_table_is_referenced_by_foreign_key(m_prebuilt->table)) {
#ifndef HA_INNOPART_SUPPORTS_FOREIGN_KEYS
    ut_ad(0);
#endif /* HA_INNOPART_SUPPORTS_FOREIGN_KEYS */
    return (1);
  }

  return (0);
}

/** Start statement.
MySQL calls this function at the start of each SQL statement inside LOCK
TABLES. Inside LOCK TABLES the "::external_lock" method does not work to
mark SQL statement borders. Note also a special case: if a temporary table
is created inside LOCK TABLES, MySQL has not called external_lock() at all
on that table.
MySQL-5.0 also calls this before each statement in an execution of a stored
procedure. To make the execution more deterministic for binlogging, MySQL-5.0
locks all tables involved in a stored procedure with full explicit table
locks (thd_in_lock_tables(thd) holds in store_lock()) before executing the
procedure.
@param[in]	thd		Handle to the user thread.
@param[in]	lock_type	Lock type.
@return	0 or error code. */
int ha_innopart::start_stmt(THD *thd, thr_lock_type lock_type) {
  int error = 0;

  if (m_part_info->get_first_used_partition() == MY_BIT_NONE) {
    /* All partitions pruned away, do nothing! */
    return (error);
  }

  error = ha_innobase::start_stmt(thd, lock_type);
  if (m_prebuilt->sql_stat_start) {
    m_sql_stat_start_parts.set();
  } else {
    m_sql_stat_start_parts.reset();
  }
  return (error);
}

/** Function to store lock for all partitions in native partitioned table. Also
look at ha_innobase::store_lock for more details.
@param[in]	thd		user thread handle
@param[in]	to		pointer to the current element in an array of
pointers to lock structs
@param[in]	lock_type	lock type to store in 'lock'; this may also be
TL_IGNORE
@retval	to	pointer to the current element in the 'to' array */
THR_LOCK_DATA **ha_innopart::store_lock(THD *thd, THR_LOCK_DATA **to,
                                        thr_lock_type lock_type) {
  trx_t *trx = m_prebuilt->trx;
  const uint sql_command = thd_sql_command(thd);

  ha_innobase::store_lock(thd, to, lock_type);

  if (sql_command == SQLCOM_FLUSH && lock_type == TL_READ_NO_INSERT) {
    for (uint i = 1; i < m_tot_parts; i++) {
      dict_table_t *table = m_part_share->get_table_part(i);

      dberr_t err = row_quiesce_set_state(table, QUIESCE_START, trx);
      ut_a(err == DB_SUCCESS || err == DB_UNSUPPORTED);
    }
  }

  return to;
}

/** Lock/prepare to lock table.
As MySQL will execute an external lock for every new table it uses when it
starts to process an SQL statement (an exception is when MySQL calls
start_stmt for the handle) we can use this function to store the pointer to
the THD in the handle. We will also use this function to communicate
to InnoDB that a new SQL statement has started and that we must store a
savepoint to our transaction handle, so that we are able to roll back
the SQL statement in case of an error.
@param[in]	thd		Handle to the user thread.
@param[in]	lock_type	Lock type.
@return	0 or error number. */
int ha_innopart::external_lock(THD *thd, int lock_type) {
  int error = 0;

  if (m_part_info->get_first_used_partition() == MY_BIT_NONE &&
      !(m_mysql_has_locked && lock_type == F_UNLCK)) {
    /* All partitions pruned away, do nothing! */
    ut_ad(!m_mysql_has_locked);
    return (error);
  }
  ut_ad(m_mysql_has_locked || lock_type != F_UNLCK);

  if (m_prebuilt->table == nullptr) {
    ut_ad(lock_type == F_UNLCK);
    ut_ad(m_prebuilt->trx->n_mysql_tables_in_use > 0);
    TrxInInnoDB::end_stmt(m_prebuilt->trx);
    --m_prebuilt->trx->n_mysql_tables_in_use;
    m_mysql_has_locked = false;
    return (error);
  }

  m_prebuilt->table = m_part_share->get_table_part(0);
  error = ha_innobase::external_lock(thd, lock_type);

  for (uint i = 0; i < m_tot_parts; i++) {
    dict_table_t *table = m_part_share->get_table_part(i);

    switch (table->quiesce) {
      case QUIESCE_START:
        /* Check for FLUSH TABLE t WITH READ LOCK */
        if (!srv_read_only_mode && thd_sql_command(thd) == SQLCOM_FLUSH &&
            lock_type == F_RDLCK) {
          ut_ad(table->quiesce == QUIESCE_START);

          if (dict_table_is_discarded(table)) {
            ib_senderrf(m_prebuilt->trx->mysql_thd, IB_LOG_LEVEL_ERROR,
                        ER_TABLESPACE_DISCARDED, table->name.m_name);

            return (HA_ERR_NO_SUCH_TABLE);
          }

          row_quiesce_table_start(table, m_prebuilt->trx);

          /* Use the transaction instance to track
          UNLOCK TABLES. It can be done via START
          TRANSACTION; too implicitly. */

          ++m_prebuilt->trx->flush_tables;
        }
        break;

      case QUIESCE_COMPLETE:
        /* Check for UNLOCK TABLES; implicit or explicit
        or trx interruption. */
        if (m_prebuilt->trx->flush_tables > 0 &&
            (lock_type == F_UNLCK || trx_is_interrupted(m_prebuilt->trx))) {
          ut_ad(table->quiesce == QUIESCE_COMPLETE);
          row_quiesce_table_complete(table, m_prebuilt->trx);

          ut_a(m_prebuilt->trx->flush_tables > 0);
          --m_prebuilt->trx->flush_tables;
        }
        break;

      case QUIESCE_NONE:
        break;

      default:
        ut_ad(0);
    }
  }

  ut_ad(!m_auto_increment_lock);
  ut_ad(!m_auto_increment_safe_stmt_log_lock);

  if (m_prebuilt->sql_stat_start) {
    m_sql_stat_start_parts.set();
  } else {
    m_sql_stat_start_parts.reset();
  }
  return (error);
}

/** Get the current auto_increment value.
@param[in]	offset			Table auto-inc offset.
@param[in]	increment		Table auto-inc increment.
@param[in]	nb_desired_values	Number of required values.
@param[out]	first_value		The auto increment value.
@param[out]	nb_reserved_values	Number of reserved values.
@return	Auto increment value, or ~0 on failure. */
void ha_innopart::get_auto_increment(ulonglong offset, ulonglong increment,
                                     ulonglong nb_desired_values,
                                     ulonglong *first_value,
                                     ulonglong *nb_reserved_values) {
  DBUG_ENTER("ha_innopart::get_auto_increment");
  if (table_share->next_number_keypart != 0) {
    /* Only first key part allowed as autoinc for InnoDB tables! */
    ut_ad(0);
    *first_value = ULLONG_MAX;
    DBUG_VOID_RETURN;
  }
  get_auto_increment_first_field(increment, nb_desired_values, first_value,
                                 nb_reserved_values);
  DBUG_VOID_RETURN;
}

/** Compares two 'refs'.
A 'ref' is the (internal) primary key value of the row.
If there is no explicitly declared non-null unique key or a primary key, then
InnoDB internally uses the row id as the primary key.
It will use the partition id as secondary compare.
@param[in]	ref1	An (internal) primary key value in the MySQL key value
format.
@param[in]	ref2	Reference to compare with (same type as ref1).
@return	< 0 if ref1 < ref2, 0 if equal, else > 0. */
int ha_innopart::cmp_ref(const uchar *ref1, const uchar *ref2) const {
  int cmp;

  cmp = ha_innobase::cmp_ref(ref1 + PARTITION_BYTES_IN_POS,
                             ref2 + PARTITION_BYTES_IN_POS);

  if (cmp != 0) {
    return (cmp);
  }

  cmp = static_cast<int>(uint2korr(ref1)) - static_cast<int>(uint2korr(ref2));

  return (cmp);
}

/** Allocate the array to hold blob heaps for all partitions */
mem_heap_t **ha_innopart::alloc_blob_heap_array() {
  DBUG_ENTER("ha_innopart::alloc_blob_heap_array");

  const ulint len = sizeof(mem_heap_t *) * m_tot_parts;
  m_blob_heap_parts =
      static_cast<mem_heap_t **>(ut_zalloc(len, mem_key_partitioning));
  if (m_blob_heap_parts == NULL) {
    DBUG_RETURN(NULL);
  }

  DBUG_RETURN(m_blob_heap_parts);
}

/** Free the array that holds blob heaps for all partitions */
void ha_innopart::free_blob_heap_array() {
  DBUG_ENTER("ha_innopart::free_blob_heap_array");

  if (m_blob_heap_parts != NULL) {
    clear_blob_heaps();
    ut_free(m_blob_heap_parts);
    m_blob_heap_parts = NULL;
  }

  DBUG_VOID_RETURN;
}

void ha_innopart::clear_blob_heaps() {
  DBUG_ENTER("ha_innopart::clear_blob_heaps");

  if (m_blob_heap_parts == NULL) {
    DBUG_VOID_RETURN;
  }

  for (uint i = 0; i < m_tot_parts; i++) {
    if (m_blob_heap_parts[i] != NULL) {
      DBUG_PRINT("ha_innopart",
                 ("freeing blob_heap: %p", m_blob_heap_parts[i]));
      mem_heap_free(m_blob_heap_parts[i]);
      m_blob_heap_parts[i] = NULL;
    }
  }

  /* Reset blob_heap in m_prebuilt after freeing all heaps. It is set in
  ha_innopart::set_partition to the blob heap of current partition. */
  m_prebuilt->blob_heap = NULL;

  DBUG_VOID_RETURN;
}

/** Reset state of file to after 'open'. This function is called
after every statement for all tables used by that statement. */
int ha_innopart::reset() {
  DBUG_ENTER("ha_innopart::reset");

  clear_blob_heaps();

  DBUG_RETURN(ha_innobase::reset());
}

/**
 Read row using position using given record to find.

This works as position()+rnd_pos() functions, but does some
extra work,calculating m_last_part - the partition to where
the 'record' should go.	Only useful when position is based
on primary key (HA_PRIMARY_KEY_REQUIRED_FOR_POSITION).

@param[in]	record	Current record in MySQL Row Format.
@return	0 for success else error code. */
int ha_innopart::rnd_pos_by_record(uchar *record) {
  int error;
  DBUG_ENTER("ha_innopart::rnd_pos_by_record");
  DBUG_ASSERT(ha_table_flags() & HA_PRIMARY_KEY_REQUIRED_FOR_POSITION);
  /* TODO: Support HA_READ_BEFORE_WRITE_REMOVAL */
  /* Set m_last_part correctly. */
  if (unlikely(get_part_for_delete(record, m_table->record[0], m_part_info,
                                   &m_last_part))) {
    DBUG_RETURN(HA_ERR_INTERNAL_ERROR);
  }

  /* Init only the partition in which row resides */
  error = rnd_init_in_part(m_last_part, false);
  if (error != 0) {
    goto err;
  }

  position(record);
  error = handler::ha_rnd_pos(record, ref);
err:
  rnd_end_in_part(m_last_part, FALSE);
  DBUG_RETURN(error);
}

/****************************************************************************
 * DS-MRR implementation
 ***************************************************************************/

/* TODO: move the default implementations into the base handler class! */
/* TODO: See if it could be optimized for partitioned tables? */
/* Use default ha_innobase implementation for now... */
