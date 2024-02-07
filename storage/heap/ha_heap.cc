/*
   Copyright (c) 2000, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#define MYSQL_SERVER 1
#include "storage/heap/ha_heap.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>

#include "my_macros.h"
#include "my_pointer_arithmetic.h"
#include "my_psi_config.h"
#include "mysql/plugin.h"
#include "sql/current_thd.h"
#include "sql/field.h"
#include "sql/sql_base.h"  // enum_tdc_remove_table_type
#include "sql/sql_class.h"
#include "sql/sql_plugin.h"
#include "storage/heap/heapdef.h"

static handler *heap_create_handler(handlerton *hton, TABLE_SHARE *table,
                                    bool partitioned, MEM_ROOT *mem_root);
static int heap_prepare_hp_create_info(TABLE *table_arg, bool single_instance,
                                       bool delete_on_close,
                                       HP_CREATE_INFO *hp_create_info);

static int heap_panic(handlerton *, ha_panic_function flag) {
  return hp_panic(flag);
}

static int heap_init(void *p) {
  handlerton *heap_hton;

#ifdef HAVE_PSI_INTERFACE
  init_heap_psi_keys();
#endif

  heap_hton = (handlerton *)p;
  heap_hton->state = SHOW_OPTION_YES;
  heap_hton->db_type = DB_TYPE_HEAP;
  heap_hton->create = heap_create_handler;
  heap_hton->panic = heap_panic;
  heap_hton->flags = HTON_CAN_RECREATE;

  return 0;
}

static handler *heap_create_handler(handlerton *hton, TABLE_SHARE *table, bool,
                                    MEM_ROOT *mem_root) {
  return new (mem_root) ha_heap(hton, table);
}

/*****************************************************************************
** HEAP tables
*****************************************************************************/

ha_heap::ha_heap(handlerton *hton, TABLE_SHARE *table_arg)
    : handler(hton, table_arg),
      file(nullptr),
      records_changed(0),
      key_stat_version(0),
      single_instance(false) {}

/*
  Hash index statistics is updated (copied from HP_KEYDEF::hash_buckets to
  records_per_key) after 1/HEAP_STATS_UPDATE_THRESHOLD fraction of table
  records have been inserted/updated/deleted. delete_all_rows() and table flush
  cause immediate update.

  NOTE
   hash index statistics must be updated when number of table records changes
   from 0 to non-zero value and vice versa. Otherwise records_in_range may
   erroneously return 0 and 'range' may miss records.
*/
#define HEAP_STATS_UPDATE_THRESHOLD 10

int ha_heap::open(const char *name, int mode, uint test_if_locked,
                  const dd::Table *) {
  const bool delete_on_close = test_if_locked & HA_OPEN_INTERNAL_TABLE;
  single_instance = delete_on_close && table_share->ref_count() == 1;
  /*
    (1) if single instance it cannot possibly exist, create it.
    (2) otherwise it may exist, try to open it, if not found, create it
  */
  if (single_instance ||
      (!(file = heap_open(name, mode)) && my_errno() == ENOENT)) {
    HP_CREATE_INFO create_info;
    bool created_new_share;
    int rc;
    file = nullptr;
    if (heap_prepare_hp_create_info(table, single_instance, delete_on_close,
                                    &create_info))
      goto end;
    create_info.pin_share = true;

    rc = heap_create(name, &create_info, &internal_share, &created_new_share);
    my_free(create_info.keydef);
    if (rc) goto end;

    implicit_emptied = created_new_share;
    if (single_instance)
      file = heap_open_from_share(internal_share, mode);
    else  // open and register in list, so future opens can find it
      file = heap_open_from_share_and_register(internal_share, mode);

    if (!file) {
      heap_release_share(internal_share, single_instance);
      goto end;
    }
  }

  ref_length = sizeof(HP_HEAP_POSITION);
  /*
    We cannot run update_key_stats() here because we do not have a
    lock on the table. The 'records' count might just be changed
    temporarily at this moment and we might get wrong statistics (Bug
    #10178). Instead we request for update. This will be done in
    ha_heap::info(), which is always called before key statistics are
    used.
    */
  key_stat_version = file->s->key_stat_version - 1;
end:

  const int ret = file ? 0 : 1;

  return (ret);
}

int ha_heap::close(void) {
  return single_instance ? hp_close(file)
                         :  // close without concurrency control
             heap_close(file);
}

/*
  Create a copy of this table

  DESCRIPTION
    Do same as default implementation but use file->s->name instead of
    table->s->path. This is needed by Windows where the clone() call sees
    '/'-delimited path in table->s->path, while ha_heap::open() was called
    with '\'-delimited path.
*/

handler *ha_heap::clone(const char *, MEM_ROOT *mem_root) {
  handler *new_handler =
      get_new_handler(table->s, false, mem_root, table->s->db_type());
  if (new_handler && !new_handler->ha_open(table, file->s->name, table->db_stat,
                                           HA_OPEN_IGNORE_IF_LOCKED, nullptr))
    return new_handler;
  return nullptr; /* purecov: inspected */
}

const char *ha_heap::table_type() const { return "MEMORY"; }

/**
  Update index statistics for the table.
*/

void ha_heap::update_key_stats() {
  for (uint i = 0; i < table->s->keys; i++) {
    KEY *key = table->key_info + i;

    key->set_in_memory_estimate(1.0);  // Index is in memory

    if (!key->supports_records_per_key()) continue;
    if (key->algorithm != HA_KEY_ALG_BTREE) {
      if (key->flags & HA_NOSAME)
        key->set_records_per_key(key->user_defined_key_parts - 1, 1.0f);
      else {
        const ha_rows hash_buckets = file->s->keydef[i].hash_buckets;
        rec_per_key_t rec_per_key =
            hash_buckets
                ? static_cast<rec_per_key_t>(file->s->records) / hash_buckets
                : 2.0f;
        if (rec_per_key < 2.0f) rec_per_key = 2.0f;
        key->set_records_per_key(key->user_defined_key_parts - 1, rec_per_key);
      }
    }
  }
  records_changed = 0;
  /* At the end of update_key_stats() we can proudly claim they are OK. */
  key_stat_version = file->s->key_stat_version;
}

int ha_heap::write_row(uchar *buf) {
  int res;
  ha_statistic_increment(&System_status_var::ha_write_count);
  if (table->next_number_field && buf == table->record[0]) {
    if ((res = update_auto_increment())) return res;
  }
  res = heap_write(file, buf);
  if (!res &&
      (++records_changed * HEAP_STATS_UPDATE_THRESHOLD > file->s->records)) {
    /*
       We can perform this safely since only one writer at the time is
       allowed on the table.
    */
    file->s->key_stat_version++;
  }

  return res;
}

int ha_heap::update_row(const uchar *old_data, uchar *new_data) {
  int res;
  ha_statistic_increment(&System_status_var::ha_update_count);
  res = heap_update(file, old_data, new_data);
  if (!res &&
      ++records_changed * HEAP_STATS_UPDATE_THRESHOLD > file->s->records) {
    /*
       We can perform this safely since only one writer at the time is
       allowed on the table.
    */
    file->s->key_stat_version++;
  }
  return res;
}

int ha_heap::delete_row(const uchar *buf) {
  int res;
  ha_statistic_increment(&System_status_var::ha_delete_count);
  res = heap_delete(file, buf);
  if (!res && table->s->tmp_table == NO_TMP_TABLE &&
      ++records_changed * HEAP_STATS_UPDATE_THRESHOLD > file->s->records) {
    /*
       We can perform this safely since only one writer at the time is
       allowed on the table.
    */
    file->s->key_stat_version++;
  }
  return res;
}

int ha_heap::index_read_map(uchar *buf, const uchar *key,
                            key_part_map keypart_map,
                            enum ha_rkey_function find_flag) {
  assert(inited == INDEX);
  ha_statistic_increment(&System_status_var::ha_read_key_count);
  int error = heap_rkey(file, buf, active_index, key, keypart_map, find_flag);

  return error;
}

int ha_heap::index_read_last_map(uchar *buf, const uchar *key,
                                 key_part_map keypart_map) {
  assert(inited == INDEX);
  ha_statistic_increment(&System_status_var::ha_read_key_count);
  int error =
      heap_rkey(file, buf, active_index, key, keypart_map, HA_READ_PREFIX_LAST);
  return error;
}

int ha_heap::index_read_idx_map(uchar *buf, uint index, const uchar *key,
                                key_part_map keypart_map,
                                enum ha_rkey_function find_flag) {
  ha_statistic_increment(&System_status_var::ha_read_key_count);
  int error = heap_rkey(file, buf, index, key, keypart_map, find_flag);
  return error;
}

int ha_heap::index_next(uchar *buf) {
  assert(inited == INDEX);
  ha_statistic_increment(&System_status_var::ha_read_next_count);
  int error = heap_rnext(file, buf);
  return error;
}

int ha_heap::index_prev(uchar *buf) {
  assert(inited == INDEX);
  ha_statistic_increment(&System_status_var::ha_read_prev_count);
  int error = heap_rprev(file, buf);
  return error;
}

int ha_heap::index_first(uchar *buf) {
  assert(inited == INDEX);
  ha_statistic_increment(&System_status_var::ha_read_first_count);
  int error = heap_rfirst(file, buf, active_index);
  return error;
}

int ha_heap::index_last(uchar *buf) {
  assert(inited == INDEX);
  ha_statistic_increment(&System_status_var::ha_read_last_count);
  int error = heap_rlast(file, buf, active_index);
  return error;
}

int ha_heap::rnd_init(bool scan) { return scan ? heap_scan_init(file) : 0; }

int ha_heap::rnd_next(uchar *buf) {
  ha_statistic_increment(&System_status_var::ha_read_rnd_next_count);
  int error = heap_scan(file, buf);

  return error;
}

int ha_heap::rnd_pos(uchar *buf, uchar *pos) {
  int error;
  HP_HEAP_POSITION heap_position;
  ha_statistic_increment(&System_status_var::ha_read_rnd_count);
  memcpy(&heap_position, pos, sizeof(HP_HEAP_POSITION));
  error = heap_rrnd(file, buf, &heap_position);
  return error;
}

void ha_heap::position(const uchar *) {
  heap_position(file,
                reinterpret_cast<HP_HEAP_POSITION *>(ref));  // Ref is aligned
}

int ha_heap::info(uint flag) {
  HEAPINFO hp_info;
  (void)heap_info(file, &hp_info, flag);

  errkey = hp_info.errkey;
  stats.records = hp_info.records;
  stats.deleted = hp_info.deleted;
  stats.mean_rec_length = hp_info.reclength;
  stats.data_file_length = hp_info.data_length;
  stats.index_file_length = hp_info.index_length;
  stats.max_data_file_length = hp_info.max_records * hp_info.reclength;
  stats.delete_length = hp_info.deleted * hp_info.reclength;
  stats.create_time = (ulong)hp_info.create_time;
  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value = hp_info.auto_increment;
  stats.table_in_mem_estimate = 1.0;  // Table entirely in memory
  /*
    If info() is called for the first time after open(), we will still
    have to update the key statistics. Hoping that a table lock is now
    in place.
  */
  if (key_stat_version != file->s->key_stat_version) update_key_stats();
  return 0;
}

int ha_heap::extra(enum ha_extra_function operation) {
  return heap_extra(file, operation);
}

int ha_heap::reset() { return heap_reset(file); }

int ha_heap::delete_all_rows() {
  heap_clear(file);
  if (table->s->tmp_table == NO_TMP_TABLE) {
    /*
       We can perform this safely since only one writer at the time is
       allowed on the table.
    */
    file->s->key_stat_version++;
  }
  return 0;
}

int ha_heap::external_lock(THD *, int) {
  return 0;  // No external locking
}

/*
  Disable indexes.

  SYNOPSIS
    disable_indexes()
    mode        mode of operation:
                HA_KEY_SWITCH_NONUNIQ      disable all non-unique keys
                HA_KEY_SWITCH_ALL          disable all keys
                HA_KEY_SWITCH_NONUNIQ_SAVE dis. non-uni. and make persistent
                HA_KEY_SWITCH_ALL_SAVE     dis. all keys and make persistent

  DESCRIPTION
    Disable indexes and clear keys to use for scanning.

  IMPLEMENTATION
    HA_KEY_SWITCH_NONUNIQ       is not implemented.
    HA_KEY_SWITCH_NONUNIQ_SAVE  is not implemented with HEAP.
    HA_KEY_SWITCH_ALL_SAVE      is not implemented with HEAP.

  RETURN
    0  ok
    HA_ERR_WRONG_COMMAND  mode not implemented.
*/

int ha_heap::disable_indexes(uint mode) {
  int error;

  if (mode == HA_KEY_SWITCH_ALL) {
    error = heap_disable_indexes(file);
  } else {
    /* mode not implemented */
    error = HA_ERR_WRONG_COMMAND;
  }
  return error;
}

/*
  Enable indexes.

  SYNOPSIS
    enable_indexes()
    mode        mode of operation:
                HA_KEY_SWITCH_NONUNIQ      enable all non-unique keys
                HA_KEY_SWITCH_ALL          enable all keys
                HA_KEY_SWITCH_NONUNIQ_SAVE en. non-uni. and make persistent
                HA_KEY_SWITCH_ALL_SAVE     en. all keys and make persistent

  DESCRIPTION
    Enable indexes and set keys to use for scanning.
    The indexes might have been disabled by disable_index() before.
    The function works only if both data and indexes are empty,
    since the heap storage engine cannot repair the indexes.
    To be sure, call handler::delete_all_rows() before.

  IMPLEMENTATION
    HA_KEY_SWITCH_NONUNIQ       is not implemented.
    HA_KEY_SWITCH_NONUNIQ_SAVE  is not implemented with HEAP.
    HA_KEY_SWITCH_ALL_SAVE      is not implemented with HEAP.

  RETURN
    0  ok
    HA_ERR_CRASHED  data or index is non-empty. Delete all rows and retry.
    HA_ERR_WRONG_COMMAND  mode not implemented.
*/

int ha_heap::enable_indexes(uint mode) {
  int error;

  if (mode == HA_KEY_SWITCH_ALL) {
    error = heap_enable_indexes(file);
  } else {
    /* mode not implemented */
    error = HA_ERR_WRONG_COMMAND;
  }
  return error;
}

/*
  Test if indexes are disabled.

  SYNOPSIS
    indexes_are_disabled()
    no parameters

  RETURN
    0  indexes are not disabled
    1  all indexes are disabled
   [2  non-unique indexes are disabled - NOT YET IMPLEMENTED]
*/

int ha_heap::indexes_are_disabled(void) {
  return heap_indexes_are_disabled(file);
}

THR_LOCK_DATA **ha_heap::store_lock(THD *, THR_LOCK_DATA **to,
                                    enum thr_lock_type lock_type) {
  /*
    This method should not be called for internal temporary tables
    as they don't have properly initialized THR_LOCK and THR_LOCK_DATA
    structures.
  */
  assert(!single_instance);
  if (lock_type != TL_IGNORE && file->lock.type == TL_UNLOCK)
    file->lock.type = lock_type;
  *to++ = &file->lock;
  return to;
}

/*
  We have to ignore ENOENT entries as the HEAP table is created on open and
  not when doing a CREATE on the table.
*/

int ha_heap::delete_table(const char *name, const dd::Table *) {
  int error = heap_delete_table(name);
  return error == ENOENT ? 0 : error;
}

void ha_heap::drop_table(const char *) {
  file->s->delete_on_close = true;
  close();
}

int ha_heap::rename_table(const char *from, const char *to, const dd::Table *,
                          dd::Table *) {
  return heap_rename(from, to);
}

ha_rows ha_heap::records_in_range(uint inx, key_range *min_key,
                                  key_range *max_key) {
  KEY *key = table->key_info + inx;
  if (key->algorithm == HA_KEY_ALG_BTREE)
    return hp_rb_records_in_range(file, inx, min_key, max_key);

  if (!min_key || !max_key || min_key->length != max_key->length ||
      min_key->length != key->key_length ||
      min_key->flag != HA_READ_KEY_EXACT || max_key->flag != HA_READ_AFTER_KEY)
    return HA_POS_ERROR;  // Can only use exact keys

  if (stats.records <= 1) return stats.records;

  /* Assert that info() did run. We need current statistics here. */
  assert(key_stat_version == file->s->key_stat_version);
  const ha_rows rec_in_range = static_cast<ha_rows>(
      key->records_per_key(key->user_defined_key_parts - 1));
  return rec_in_range;
}

static int heap_prepare_hp_create_info(TABLE *table_arg, bool single_instance,
                                       bool delete_on_close,
                                       HP_CREATE_INFO *hp_create_info) {
  uint key, parts, mem_per_row = 0, keys = table_arg->s->keys;
  uint auto_key = 0, auto_key_type = 0;
  ha_rows max_rows;
  HP_KEYDEF *keydef;
  HA_KEYSEG *seg;
  TABLE_SHARE *share = table_arg->s;
  bool found_real_auto_increment = false;

  memset(hp_create_info, 0, sizeof(*hp_create_info));

  for (key = parts = 0; key < keys; key++)
    parts += table_arg->key_info[key].user_defined_key_parts;

  if (!(keydef = (HP_KEYDEF *)my_malloc(
            hp_key_memory_HP_KEYDEF,
            keys * sizeof(HP_KEYDEF) + parts * sizeof(HA_KEYSEG), MYF(MY_WME))))
    return my_errno();
  seg = reinterpret_cast<HA_KEYSEG *>(keydef + keys);
  for (key = 0; key < keys; key++) {
    KEY *pos = table_arg->key_info + key;
    KEY_PART_INFO *key_part = pos->key_part;
    KEY_PART_INFO *key_part_end = key_part + pos->user_defined_key_parts;

    keydef[key].keysegs = (uint)pos->user_defined_key_parts;
    keydef[key].flag = (pos->flags & (HA_NOSAME | HA_NULL_ARE_EQUAL));
    keydef[key].seg = seg;

    switch (pos->algorithm) {
      case HA_KEY_ALG_HASH:
        keydef[key].algorithm = HA_KEY_ALG_HASH;
        mem_per_row += sizeof(HASH_INFO);
        break;
      case HA_KEY_ALG_BTREE:
        keydef[key].algorithm = HA_KEY_ALG_BTREE;
        mem_per_row += sizeof(TREE_ELEMENT) + pos->key_length + sizeof(char *);
        break;
      default:
        assert(0);  // cannot happen
    }

    for (; key_part != key_part_end; key_part++, seg++) {
      Field *field = key_part->field;

      if (pos->algorithm == HA_KEY_ALG_BTREE)
        seg->type = field->key_type();
      else {
        if ((seg->type = field->key_type()) != (int)HA_KEYTYPE_TEXT &&
            seg->type != HA_KEYTYPE_VARTEXT1 &&
            seg->type != HA_KEYTYPE_VARTEXT2 &&
            seg->type != HA_KEYTYPE_VARBINARY1 &&
            seg->type != HA_KEYTYPE_VARBINARY2)
          seg->type = HA_KEYTYPE_BINARY;
      }
      seg->start = (uint)key_part->offset;
      seg->length = (uint)key_part->length;
      seg->flag = key_part->key_part_flag;

      if (field->is_flag_set(ENUM_FLAG) || field->is_flag_set(SET_FLAG))
        seg->charset = &my_charset_bin;
      else
        seg->charset = field->charset_for_protocol();
      if (field->is_nullable()) {
        seg->null_bit = field->null_bit;
        seg->null_pos = field->null_offset();
      } else {
        seg->null_bit = 0;
        seg->null_pos = 0;
      }
      if (field->is_flag_set(AUTO_INCREMENT_FLAG) &&
          table_arg->found_next_number_field &&
          key == share->next_number_index) {
        /*
          Store key number and type for found auto_increment key
          We have to store type as seg->type can differ from it
        */
        auto_key = key + 1;
        auto_key_type = field->key_type();
      }
    }
  }
  mem_per_row += MY_ALIGN(share->reclength + 1, sizeof(char *));
  if (table_arg->found_next_number_field) {
    keydef[share->next_number_index].flag |= HA_AUTO_KEY;
    found_real_auto_increment = share->next_number_key_offset == 0;
  }
  hp_create_info->auto_key = auto_key;
  hp_create_info->auto_key_type = auto_key_type;
  hp_create_info->max_table_size = current_thd->variables.max_heap_table_size;
  hp_create_info->with_auto_increment = found_real_auto_increment;
  hp_create_info->single_instance = single_instance;
  hp_create_info->delete_on_close = delete_on_close;

  max_rows = (ha_rows)(hp_create_info->max_table_size / mem_per_row);
  if (share->max_rows && share->max_rows < max_rows) max_rows = share->max_rows;

  hp_create_info->max_records = (ulong)max_rows;
  hp_create_info->min_records = (ulong)share->min_rows;
  hp_create_info->keys = share->keys;
  hp_create_info->reclength = share->reclength;
  hp_create_info->keydef = keydef;
  return 0;
}

int ha_heap::create(const char *name, TABLE *table_arg,
                    HA_CREATE_INFO *create_info, dd::Table *) {
  int error;
  bool created;
  HP_CREATE_INFO hp_create_info;
  assert(!single_instance);

  error = heap_prepare_hp_create_info(table_arg, false, false, &hp_create_info);
  if (error == 0) {
    hp_create_info.auto_increment = (create_info->auto_increment_value
                                         ? create_info->auto_increment_value - 1
                                         : 0);
    error = heap_create(name, &hp_create_info, &internal_share, &created);
    my_free(hp_create_info.keydef);
    assert(file == nullptr);
  }

  return (error);
}

void ha_heap::update_create_info(HA_CREATE_INFO *create_info) {
  table->file->info(HA_STATUS_AUTO);
  if (!(create_info->used_fields & HA_CREATE_USED_AUTO))
    create_info->auto_increment_value = stats.auto_increment_value;
}

void ha_heap::get_auto_increment(ulonglong, ulonglong, ulonglong,
                                 ulonglong *first_value,
                                 ulonglong *nb_reserved_values) {
  ha_heap::info(HA_STATUS_AUTO);
  *first_value = stats.auto_increment_value;
  /* such table has only table-level locking so reserves up to +inf */
  *nb_reserved_values = ULLONG_MAX;
}

bool ha_heap::check_if_incompatible_data(HA_CREATE_INFO *info,
                                         uint table_changes) {
  /* Check that auto_increment value was not changed */
  if ((info->used_fields & HA_CREATE_USED_AUTO &&
       info->auto_increment_value != 0) ||
      table_changes == IS_EQUAL_NO ||
      table_changes & IS_EQUAL_PACK_LENGTH)  // Not implemented yet
    return COMPATIBLE_DATA_NO;
  return COMPATIBLE_DATA_YES;
}

struct st_mysql_storage_engine heap_storage_engine = {
    MYSQL_HANDLERTON_INTERFACE_VERSION};

mysql_declare_plugin(heap){
    MYSQL_STORAGE_ENGINE_PLUGIN,
    &heap_storage_engine,
    "MEMORY",
    PLUGIN_AUTHOR_ORACLE,
    "Hash based, stored in memory, useful for temporary tables",
    PLUGIN_LICENSE_GPL,
    heap_init,
    nullptr,
    nullptr,
    0x0100,  /* 1.0 */
    nullptr, /* status variables                */
    nullptr, /* system variables                */
    nullptr, /* config options                  */
    0,       /* flags                           */
} mysql_declare_plugin_end;
