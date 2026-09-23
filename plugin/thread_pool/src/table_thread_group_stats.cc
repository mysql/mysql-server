/* Copyright (c) 2011, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation. The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

#include <type_traits>  // std::is_trivially_copyable

#include "plugin/thread_pool/src/table_thread_group_stats.h"

#include "plugin/thread_pool/src/methods.h"
#include "plugin/thread_pool/src/thread_pool.h"
#include "plugin/thread_pool/src/thread_pool_tables.h"

struct Table_thread_group_stats_pos {
  Table_thread_group_stats_pos() : m_index(0) {}

  void reset() { m_index = 0; }

  void set_at(const Table_thread_group_stats_pos *other) {
    m_index = other->m_index;
  }

  void set_after(const Table_thread_group_stats_pos *other) {
    m_index = other->m_index + 1;
  }

  void next_group() { m_index++; }

 public:
  /** Group id, 0 based. */
  unsigned int m_index;
};

// See Doxygen comment for the open_table_t function pointer typedef - in
// include/mysql/components/services/pfs_plugin_table_service.h
static_assert(std::is_trivially_copyable<Table_thread_group_stats_pos>::value,
              "Position object may be memcpy'ied by the PFS engine, "
              "so it needs to be trivially copyable.");

struct Table_thread_group_stats_row {
  /** Column TP_GROUP_ID */
  unsigned int m_group_id;
  /** Column CONNECTIONS_STARTED */
  unsigned long long m_connections_started;
  /** Column CONNECTIONS_CLOSED */
  unsigned long long m_connections_closed;
  /** Column QUERIES_EXECUTED */
  unsigned long long m_queries_executed;
  /** Column QUERIES_QUEUED */
  unsigned long long m_queries_queued;
  /** Column THREADS_STARTED */
  unsigned long long m_threads_started;
  /** Column PRIO_KICKUPS */
  unsigned long long m_prio_kickups;
  /** Column STALLED_QUERIES_EXECUTED */
  unsigned long long m_stalled_queries_executed;
  /** Column BECOME_CONSUMER_THREAD */
  unsigned long long m_become_consumer_thread;
  /** Column BECOME_RESERVE_THREAD */
  unsigned long long m_become_reserve_thread;
  /** Column BECOME_WAITING_THREAD */
  unsigned long long m_become_waiting_thread;
  /** Column WAKE_THREAD_STALL_CHECKER */
  unsigned long long m_wake_thread_stall_checker;
  /** Column SLEEP_WAITS */
  unsigned long long m_sleep_waits;
  /** Column DISK_IO_WAITS */
  unsigned long long m_disk_io_waits;
  /** Column ROW_LOCK_WAITS */
  unsigned long long m_row_lock_waits;
  /** Column GLOBAL_LOCK_WAITS */
  unsigned long long m_global_lock_waits;
  /** Column META_DATA_LOCK_WAITS */
  unsigned long long m_metadata_lock_waits;
  /** Column TABLE_LOCK_WAITS */
  unsigned long long m_table_lock_waits;
  /** Column USER_LOCK_WAITS */
  unsigned long long m_user_lock_waits;
  /** Column BINLOG_WAITS */
  unsigned long long m_binlog_waits;
  /** Column GROUP_COMMIT_WAITS */
  unsigned long long m_group_commit_waits;
  /** Column FSYNC_WAITS */
  unsigned long long m_fsync_waits;
};

class Table_thread_group_stats;

/** @sa Index_thread_group_state */
class Index_thread_group_stats {
 public:
  Index_thread_group_stats(Table_thread_group_stats *table) : m_table(table) {}
  virtual ~Index_thread_group_stats() = default;

  virtual int read_key(PSI_key_reader *reader, int find_flag) = 0;
  virtual bool match(tp_group_t *group) = 0;

  Table_thread_group_stats *m_table;
  unsigned int m_fields;
};

/** @sa Index_thread_group_state_by_group_id */
class Index_thread_group_stats_by_group_id : public Index_thread_group_stats {
 public:
  Index_thread_group_stats_by_group_id(Table_thread_group_stats *table)
      : Index_thread_group_stats(table) {}
  ~Index_thread_group_stats_by_group_id() override = default;

  int read_key(PSI_key_reader *reader, int find_flag) override;
  bool match(tp_group_t *group) override;

  PSI_plugin_key_uinteger m_key_group_id;
};

class Table_thread_group_stats {
 public:
  typedef Table_thread_group_stats_pos pos_t;
  typedef Table_thread_group_stats_row row_t;
  typedef Index_thread_group_stats_by_group_id index_t;

  Table_thread_group_stats();
  ~Table_thread_group_stats();

  int rnd_next();
  int rnd_init(bool scan);
  int rnd_pos();

  int index_init(unsigned int idx, bool sorted);
  int index_read(PSI_key_reader *reader, unsigned int idx, int find_flag);
  int index_next();

  void reset_position();
  int read_column_value(PSI_field *field, unsigned int index);
  static Table_thread_group_stats *open_table(pos_t **pos);
  void close_table();

  void make_row(tp_group_t *group);

  pos_t m_pos;
  pos_t m_next_pos;
  row_t m_row;
  index_t *m_opened_index;
};

Table_thread_group_stats *Table_thread_group_stats::open_table(pos_t **pos) {
  auto *table = new Table_thread_group_stats();
  *pos = &table->m_pos;
  return table;
}

void Table_thread_group_stats::close_table() {}

Table_thread_group_stats::Table_thread_group_stats()
    : m_pos(), m_next_pos(), m_opened_index(nullptr) {}

Table_thread_group_stats::~Table_thread_group_stats() { delete m_opened_index; }

int Table_thread_group_stats::rnd_next() {
  tp_group_t *cur_group;
  m_pos.set_at(&m_next_pos);

  cur_group = get_group(m_pos.m_index);

  if (cur_group == nullptr) {
    return PFS_HA_ERR_END_OF_FILE;
  }

  mysql_mutex_lock(&cur_group->LOCK_group);
  make_row(cur_group);
  m_next_pos.set_after(&m_pos);
  mysql_mutex_unlock(&cur_group->LOCK_group);

  return 0;
}

int Table_thread_group_stats::rnd_pos() {
  tp_group_t *cur_group;

  cur_group = get_group(m_pos.m_index);

  if (cur_group == nullptr) {
    return PFS_HA_ERR_RECORD_DELETED;
  }

  mysql_mutex_lock(&cur_group->LOCK_group);
  make_row(cur_group);
  mysql_mutex_unlock(&cur_group->LOCK_group);

  return 0;
}

int Table_thread_group_stats::index_init(unsigned int idx [[maybe_unused]],
                                         bool /* sorted */) {
  /* There is only 1 index. */
  assert(idx == 0);

  assert(m_opened_index == nullptr);
  m_opened_index = new Index_thread_group_stats_by_group_id(this);
  return 0;
}

int Table_thread_group_stats::index_read(PSI_key_reader *reader,
                                         unsigned int idx [[maybe_unused]],
                                         int find_flag) {
  assert(idx == 0);
  assert(m_opened_index != nullptr);

  return m_opened_index->read_key(reader, find_flag);
}

int Index_thread_group_stats_by_group_id::read_key(PSI_key_reader *reader,
                                                   int find_flag) {
  pc_integer_srv->read_key_unsigned(reader, &m_key_group_id, find_flag);

  m_fields = pt_srv->get_parts_found(reader);
  return 0;
}

int Table_thread_group_stats::index_next() {
  tp_group_t *cur_group;
  m_pos.set_at(&m_next_pos);

  for (;;) {
    cur_group = get_group(m_pos.m_index);

    if (cur_group == nullptr) {
      return PFS_HA_ERR_END_OF_FILE;
    }

    mysql_mutex_lock(&cur_group->LOCK_group);
    if (m_opened_index->match(cur_group)) {
      make_row(cur_group);
      m_next_pos.set_after(&m_pos);
      mysql_mutex_unlock(&cur_group->LOCK_group);
      return 0;
    }
    mysql_mutex_unlock(&cur_group->LOCK_group);

    m_pos.next_group();
  }
}

bool Index_thread_group_stats_by_group_id::match(tp_group_t *group) {
  if (m_fields >= 1) {
    if (!pc_integer_srv->match_key_unsigned(false, group->group_idx,
                                            &m_key_group_id)) {
      return false;
    }
  }

  return true;
}

void Table_thread_group_stats::reset_position() { m_pos.reset(); }

void Table_thread_group_stats::make_row(tp_group_t *group) {
  struct tp_group_statistics_t *stats;
  stats = &group->stats;

  m_row.m_group_id = group->group_idx;
  m_row.m_connections_started = stats->connections_started;
  m_row.m_connections_closed = stats->connections_closed;
  m_row.m_queries_executed = stats->queries_executed;
  m_row.m_queries_queued = stats->queries_queued;
  m_row.m_threads_started = stats->threads_created;
  m_row.m_prio_kickups = stats->prio_kickups;
  m_row.m_stalled_queries_executed = stats->stalled_queries_executed;
  m_row.m_become_consumer_thread = stats->become_consumer_thread;
  m_row.m_become_reserve_thread = stats->become_reserve_thread;
  m_row.m_become_waiting_thread = stats->become_listen_thread;
  m_row.m_wake_thread_stall_checker = stats->wake_thread_stall_checker;
  m_row.m_sleep_waits = stats->wait_counts[THD_WAIT_SLEEP];
  m_row.m_disk_io_waits = stats->wait_counts[THD_WAIT_DISKIO];
  m_row.m_row_lock_waits = stats->wait_counts[THD_WAIT_ROW_LOCK];
  m_row.m_global_lock_waits = stats->wait_counts[THD_WAIT_GLOBAL_LOCK];
  m_row.m_metadata_lock_waits = stats->wait_counts[THD_WAIT_META_DATA_LOCK];
  m_row.m_table_lock_waits = stats->wait_counts[THD_WAIT_TABLE_LOCK];
  m_row.m_user_lock_waits = stats->wait_counts[THD_WAIT_USER_LOCK];
  m_row.m_binlog_waits = stats->wait_counts[THD_WAIT_BINLOG];
  m_row.m_group_commit_waits = stats->wait_counts[THD_WAIT_GROUP_COMMIT];
  m_row.m_fsync_waits = stats->wait_counts[THD_WAIT_SYNC];
}

int Table_thread_group_stats::read_column_value(PSI_field *field,
                                                unsigned int index) {
  PSI_uint value;
  PSI_ulonglong bigvalue;

  switch (index) {
    case 0: /* TP_GROUP_ID */
      value.val = m_row.m_group_id;
      value.is_null = false;
      pc_integer_srv->set_unsigned(field, value);
      break;
    case 1: /* CONNECTIONS_STARTED */
      bigvalue.val = m_row.m_connections_started;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 2: /* CONNECTIONS_CLOSED */
      bigvalue.val = m_row.m_connections_closed;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 3: /* QUERIES_EXECUTED */
      bigvalue.val = m_row.m_queries_executed;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 4: /* QUERIES_QUEUED */
      bigvalue.val = m_row.m_queries_queued;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 5: /* THREADS_STARTED */
      bigvalue.val = m_row.m_threads_started;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 6: /* PRIO_KICKUPS */
      bigvalue.val = m_row.m_prio_kickups;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 7: /* STALLED_QUERIES_EXECUTED */
      bigvalue.val = m_row.m_stalled_queries_executed;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 8: /* BECOME_CONSUMER_THREAD */
      bigvalue.val = m_row.m_become_consumer_thread;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 9: /* BECOME_RESERVE_THREAD */
      bigvalue.val = m_row.m_become_reserve_thread;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 10: /* BECOME_WAITING_THREAD */
      bigvalue.val = m_row.m_become_waiting_thread;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 11: /* WAKE_THREAD_STALL_CHECKER */
      bigvalue.val = m_row.m_wake_thread_stall_checker;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 12: /* SLEEP_WAITS */
      bigvalue.val = m_row.m_sleep_waits;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 13: /* DISK_IO_WAITS */
      bigvalue.val = m_row.m_disk_io_waits;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 14: /* ROW_LOCK_WAITS */
      bigvalue.val = m_row.m_row_lock_waits;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 15: /* GLOBAL_LOCK_WAITS */
      bigvalue.val = m_row.m_global_lock_waits;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 16: /* META_DATA_LOCK_WAITS */
      bigvalue.val = m_row.m_metadata_lock_waits;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 17: /* TABLE_LOCK_WAITS */
      bigvalue.val = m_row.m_table_lock_waits;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 18: /* USER_LOCK_WAITS */
      bigvalue.val = m_row.m_user_lock_waits;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 19: /* BINLOG_WAITS */
      bigvalue.val = m_row.m_binlog_waits;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 20: /* GROUP_COMMIT_WAITS */
      bigvalue.val = m_row.m_group_commit_waits;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    case 21: /* FSYNC_WAITS */
      bigvalue.val = m_row.m_fsync_waits;
      bigvalue.is_null = false;
      pc_bigint_srv->set_unsigned(field, bigvalue);
      break;
    default:
      assert(false);
  }
  return 0;
}

/* Map a C++ class to a C interface */

static int cb_rnd_init(PSI_table_handle *, bool) { return 0; }

static int cb_rnd_next(PSI_table_handle *handle) {
  auto *that = reinterpret_cast<Table_thread_group_stats *>(handle);
  return that->rnd_next();
}

static int cb_rnd_pos(PSI_table_handle *handle) {
  auto *that = reinterpret_cast<Table_thread_group_stats *>(handle);
  return that->rnd_pos();
}

static int cb_index_init(PSI_table_handle *table, unsigned int idx, bool sorted,
                         PSI_index_handle **index) {
  int rc;
  auto *that_table = reinterpret_cast<Table_thread_group_stats *>(table);
  auto **that_index = reinterpret_cast<Index_thread_group_stats **>(index);
  rc = that_table->index_init(idx, sorted);
  *that_index = that_table->m_opened_index;
  return rc;
}

static int cb_index_read(PSI_index_handle *index, PSI_key_reader *reader,
                         unsigned int idx, int find_flag) {
  auto *that_index = reinterpret_cast<Index_thread_group_stats *>(index);
  Table_thread_group_stats *that_table = that_index->m_table;
  return that_table->index_read(reader, idx, find_flag);
}

static int cb_index_next(PSI_table_handle *table) {
  auto *that = reinterpret_cast<Table_thread_group_stats *>(table);
  return that->index_next();
}

static void cb_reset_position(PSI_table_handle *handle) {
  auto *that = reinterpret_cast<Table_thread_group_stats *>(handle);
  that->reset_position();
}

static int cb_read_column_value(PSI_table_handle *handle, PSI_field *field,
                                unsigned int index) {
  auto *that = reinterpret_cast<Table_thread_group_stats *>(handle);
  return that->read_column_value(field, index);
}

static PSI_table_handle *cb_open_table(PSI_pos **pos) {
  auto **that_pos = reinterpret_cast<Table_thread_group_stats::pos_t **>(pos);
  Table_thread_group_stats *that =
      Table_thread_group_stats::open_table(that_pos);
  auto *handle = reinterpret_cast<PSI_table_handle *>(that);
  return handle;
}

static void cb_close_table(PSI_table_handle *handle) {
  auto *that = reinterpret_cast<Table_thread_group_stats *>(handle);
  that->close_table();
  delete that;
}

static unsigned long long cb_get_row_count() { return MAX_THREAD_GROUPS; }

/* Expose the table in the performance schema */

static PFS_engine_table_share_proxy table_share;

PFS_engine_table_share_proxy *table_share_tp_thread_group_stats_init() {
  PFS_engine_table_proxy &cb = table_share.m_proxy_engine_table;

  cb.rnd_init = cb_rnd_init;
  cb.rnd_next = cb_rnd_next;
  cb.rnd_pos = cb_rnd_pos;

  cb.index_init = cb_index_init;
  cb.index_read = cb_index_read;
  cb.index_next = cb_index_next;

  cb.read_column_value = cb_read_column_value;
  cb.reset_position = cb_reset_position;
  cb.write_column_value = nullptr;
  cb.write_row_values = nullptr;
  cb.update_column_value = nullptr;
  cb.update_row_values = nullptr;
  cb.delete_row_values = nullptr;
  cb.open_table = cb_open_table;
  cb.close_table = cb_close_table;

  table_share.m_table_name = "tp_thread_group_stats";
  table_share.m_table_name_length = 21;
  table_share.m_table_definition =
      "`TP_GROUP_ID` int unsigned NOT NULL, "
      "`CONNECTIONS_STARTED` bigint unsigned NOT NULL, "
      "`CONNECTIONS_CLOSED` bigint unsigned NOT NULL, "
      "`QUERIES_EXECUTED` bigint unsigned NOT NULL, "
      "`QUERIES_QUEUED` bigint unsigned NOT NULL, "
      "`THREADS_STARTED` bigint unsigned NOT NULL, "
      "`PRIO_KICKUPS` bigint unsigned NOT NULL, "
      "`STALLED_QUERIES_EXECUTED` bigint unsigned NOT NULL, "
      "`BECOME_CONSUMER_THREAD` bigint unsigned NOT NULL, "
      "`BECOME_RESERVE_THREAD` bigint unsigned NOT NULL, "
      "`BECOME_WAITING_THREAD` bigint unsigned NOT NULL, "
      "`WAKE_THREAD_STALL_CHECKER` bigint unsigned NOT NULL, "
      "`SLEEP_WAITS` bigint unsigned NOT NULL, "
      "`DISK_IO_WAITS` bigint unsigned NOT NULL, "
      "`ROW_LOCK_WAITS` bigint unsigned NOT NULL, "
      "`GLOBAL_LOCK_WAITS` bigint unsigned NOT NULL, "
      "`META_DATA_LOCK_WAITS` bigint unsigned NOT NULL, "
      "`TABLE_LOCK_WAITS` bigint unsigned NOT NULL, "
      "`USER_LOCK_WAITS` bigint unsigned NOT NULL, "
      "`BINLOG_WAITS` bigint unsigned NOT NULL, "
      "`GROUP_COMMIT_WAITS` bigint unsigned NOT NULL, "
      "`FSYNC_WAITS` bigint unsigned NOT NULL, "
      "UNIQUE KEY(`TP_GROUP_ID`)";

  table_share.m_ref_length = sizeof(Table_thread_group_stats::pos_t);
  table_share.m_acl = READONLY;
  table_share.delete_all_rows = nullptr;
  table_share.get_row_count = cb_get_row_count;
  return &table_share;
}
