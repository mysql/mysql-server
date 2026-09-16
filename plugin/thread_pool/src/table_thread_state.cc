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

#include "plugin/thread_pool/src/table_thread_state.h"

#include "plugin/thread_pool/src/methods.h"
#include "plugin/thread_pool/src/thread_pool.h"
#include "plugin/thread_pool/src/thread_pool_tables.h"

struct Table_thread_state_pos {
  Table_thread_state_pos() : m_index_1(0), m_index_2(0) {}

  void reset() {
    m_index_1 = 0;
    m_index_2 = 0;
  }

  void set_at(const Table_thread_state_pos *other) {
    m_index_1 = other->m_index_1;
    m_index_2 = other->m_index_2;
  }

  void set_after(const Table_thread_state_pos *other) {
    m_index_1 = other->m_index_1;
    m_index_2 = other->m_index_2 + 1;
  }

  void next_group() {
    m_index_1++;
    m_index_2 = 0;
  }

  void next_thread() { m_index_2++; }

 public:
  /** Group id, 0 based. */
  unsigned int m_index_1;
  /** Thread number, 0 based. */
  unsigned int m_index_2;
};

// See Doxygen comment for the open_table_t function pointer typedef - in
// include/mysql/components/services/pfs_plugin_table_service.h
static_assert(std::is_trivially_copyable<Table_thread_state_pos>::value,
              "Position object may be memcpy'ied by the PFS engine, "
              "so it needs to be trivially copyable.");

/** Keep table definition here so that it is easy to refer to it when
    modifying row struct, make_row() and read_column_value(). This value is
    assigned to the struct describing the table further down. */
static constexpr const char *table_definition =
    "`TP_GROUP_ID` int unsigned NOT NULL, "
    "`TP_THREAD_NUMBER` int unsigned NOT NULL, "
    "`PROCESS_COUNT` bigint unsigned NOT NULL, "
    "`WAIT_TYPE` varchar(30), "
    "`TP_THREAD_TYPE` varchar(32) NOT NULL, "
    "`THREAD_ID` bigint unsigned, "
    "`TIME_OF_ATTACH` bigint unsigned NOT NULL,"
    "`MARKED_STALLED` int unsigned NOT NULL, "
    "`STATE` varchar(32) NOT NULL, "
    "EVENT_COUNT bigint unsigned NOT NULL, "
    "ACCUMULATED_EVENT_TIME bigint unsigned NOT NULL, "
    "EXEC_COUNT bigint unsigned NOT NULL, "
    "ACCUMULATED_EXEC_TIME bigint unsigned NOT NULL, "
    "UNIQUE KEY(`TP_GROUP_ID`, `TP_THREAD_NUMBER`)";

/** Copy of the values in a row, initialized when first moving to
    the row. Uses types which correspond to the table definition,
    NOT the types used internally. Values may need to be converted
    in make_row(). */
struct Table_thread_state_row {
  /** Column TP_GROUP_ID */
  srv_utils::PSI_int_unsigned_NOT_NULL m_group_id;
  /** Column TP_THREAD_NUMBER */
  srv_utils::PSI_int_unsigned_NOT_NULL m_thread_number;
  /** Column PROCESS_COUNT */
  srv_utils::PSI_bigint_unsigned m_process_count;
  /** Column WAIT_TYPE */
  const char *m_wait_type;
  /** Column WORKER_THREAD_TYPE */
  const char *m_worker_thread_type;
  /** Column PFS_THREAD_ID */
  srv_utils::PSI_bigint_unsigned m_pfs_thread_id;

  /** Column TIME_OF_ATTACH */
  PSI_time_point m_time_of_attach;

  /** Column MARKED_STALLED */
  srv_utils::PSI_int_unsigned_NOT_NULL m_stalled;

  /** Column THREAD_STATE */
  const char *m_state;

  /** Column EVENT_COUNT */
  std::uint64_t m_event_count;

  /** Column ACCUMULATED_EVENT_TIME */
  std::uint64_t m_acc_event_time;

  /** Column EXEC_COUNT */
  std::uint64_t m_exec_count;

  /** Column ACCUMULATED_EXEC_TIME */
  std::uint64_t m_acc_exec_time;
};

/** Copies values from TP code (primarily tp_client_low_level_t)
    into the row object. Called when first entering a row. */
static Table_thread_state_row read_row(const tp_group_t &grp,
                                       const tp_thread_t &thr) {
  return {grp.group_idx,
          thr.numeric_id,
          get_process_count_time_unit(&thr),
          [client_cntx = thr.client_low_level_cntx] {
            auto wt = client_cntx ? client_cntx->wait_type : THD_WAIT_NONE;
            return (wt != THD_WAIT_NONE ? THD_wait_type_str(wt) : nullptr);
          }(),
          worker_thread_type_str(thr.worker_type),
          thr.pfs_thread_id,
          psi_time_point(thr.time_of_attach),
          thr.stalled,
          thread_state_str(thr.state),
          thr.event_count.load(std::memory_order_relaxed),
          static_cast<std::uint64_t>(
              thr.acc_event_time.load(std::memory_order_relaxed).count()),
          thr.command_count.load(std::memory_order_relaxed),
          static_cast<std::uint64_t>(
              thr.acc_command_time.load(std::memory_order_relaxed).count())};
}

using FieldSetter = void (*)(PSI_field *, const Table_thread_state_row &);

/** Mapping between column indices and the member variables of the row struct.
 */
static FieldSetter field_setters[] = {
    [](auto *f, const auto &r) { srv_utils::overloaded_set(f, r.m_group_id); },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_thread_number);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_process_count);
    },
    [](auto f, const auto &r) { srv_utils::overloaded_set(f, r.m_wait_type); },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_worker_thread_type);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_pfs_thread_id);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_time_of_attach);
    },
    [](auto *f, const auto &r) { srv_utils::overloaded_set(f, r.m_stalled); },
    [](auto f, const auto &r) { srv_utils::overloaded_set(f, r.m_state); },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_event_count);
    },
    [](auto *f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_acc_event_time);
    },
    [](auto f, const auto &r) { srv_utils::overloaded_set(f, r.m_exec_count); },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_acc_exec_time);
    },
};

class Table_thread_state;

class Index_thread_state {
 public:
  Index_thread_state(Table_thread_state *table) : m_table(table) {}
  virtual ~Index_thread_state() = default;

  virtual int read_key(PSI_key_reader *reader, int find_flag) = 0;
  virtual bool match(tp_group_t *group, tp_thread_t *thread) = 0;

  Table_thread_state *m_table;
  unsigned int m_fields;
};

class Index_thread_state_by_group_id_by_thread_id : public Index_thread_state {
 public:
  Index_thread_state_by_group_id_by_thread_id(Table_thread_state *table)
      : Index_thread_state(table) {}
  ~Index_thread_state_by_group_id_by_thread_id() override = default;

  int read_key(PSI_key_reader *reader, int find_flag) override;
  bool match(tp_group_t *group, tp_thread_t *thread) override;

  PSI_plugin_key_uinteger m_key_group_id;
  PSI_plugin_key_uinteger m_key_thread_id;
};

class Table_thread_state {
 public:
  typedef Table_thread_state_pos pos_t;
  typedef Table_thread_state_row row_t;
  typedef Index_thread_state_by_group_id_by_thread_id index_t;

  Table_thread_state();
  ~Table_thread_state();

  int rnd_next();
  int rnd_init(bool scan);
  int rnd_pos();

  int index_init(unsigned int idx, bool sorted);
  int index_read(PSI_key_reader *reader, unsigned int idx, int find_flag);
  int index_next();

  void reset_position();
  int read_column_value(PSI_field *field, unsigned int index);
  static Table_thread_state *open_table(pos_t **pos);
  void close_table();

  int make_row(const tp_group_t &grp, const tp_thread_t &thr);

  pos_t m_pos;
  pos_t m_next_pos;
  row_t m_row;
  index_t *m_opened_index;
};

Table_thread_state *Table_thread_state::open_table(pos_t **pos) {
  auto *table = new Table_thread_state();
  *pos = &table->m_pos;
  return table;
}

void Table_thread_state::close_table() {}

Table_thread_state::Table_thread_state()
    : m_pos(), m_next_pos(), m_opened_index(nullptr) {}

Table_thread_state::~Table_thread_state() { delete m_opened_index; }

int Table_thread_state::rnd_next() {
  tp_group_t *cur_group;
  tp_thread_t *cur_thread;
  m_pos.set_at(&m_next_pos);

  for (;;) {
    cur_group = get_group(m_pos.m_index_1);

    if (cur_group == nullptr) {
      return PFS_HA_ERR_END_OF_FILE;
    }

    mysql_mutex_lock(&cur_group->LOCK_group);

    while (m_pos.m_index_2 < cur_group->max_thread_ids_in_group) {
      cur_thread = &(cur_group->group_threads[m_pos.m_index_2]);
      if (make_row(*cur_group, *cur_thread) == 0) {
        m_next_pos.set_after(&m_pos);
        mysql_mutex_unlock(&cur_group->LOCK_group);
        return 0;
      }
      m_pos.next_thread();
    }

    mysql_mutex_unlock(&cur_group->LOCK_group);

    m_pos.next_group();
  }
}

int Table_thread_state::rnd_pos() {
  tp_group_t *cur_group;
  tp_thread_t *cur_thread;

  cur_group = get_group(m_pos.m_index_1);

  if (cur_group == nullptr) {
    return PFS_HA_ERR_RECORD_DELETED;
  }

  mysql_mutex_lock(&cur_group->LOCK_group);

  if (m_pos.m_index_2 < cur_group->max_thread_ids_in_group) {
    cur_thread = &(cur_group->group_threads[m_pos.m_index_2]);
    if (make_row(*cur_group, *cur_thread) == 0) {
      mysql_mutex_unlock(&cur_group->LOCK_group);
      return 0;
    }
  }

  mysql_mutex_unlock(&cur_group->LOCK_group);

  return PFS_HA_ERR_RECORD_DELETED;
}

int Table_thread_state::index_init(unsigned int idx [[maybe_unused]],
                                   bool /* sorted */) {
  /* There is only 1 index. */
  assert(idx == 0);

  assert(m_opened_index == nullptr);
  m_opened_index = new Index_thread_state_by_group_id_by_thread_id(this);
  return 0;
}

int Table_thread_state::index_read(PSI_key_reader *reader,
                                   unsigned int idx [[maybe_unused]],
                                   int find_flag) {
  assert(idx == 0);
  assert(m_opened_index != nullptr);

  return m_opened_index->read_key(reader, find_flag);
}

int Index_thread_state_by_group_id_by_thread_id::read_key(
    PSI_key_reader *reader, int find_flag) {
  pc_integer_srv->read_key_unsigned(reader, &m_key_group_id, find_flag);
  pc_integer_srv->read_key_unsigned(reader, &m_key_thread_id, find_flag);

  m_fields = pt_srv->get_parts_found(reader);
  return 0;
}

int Table_thread_state::index_next() {
  tp_group_t *cur_group;
  tp_thread_t *cur_thread;
  m_pos.set_at(&m_next_pos);

  for (;;) {
    cur_group = get_group(m_pos.m_index_1);

    if (cur_group == nullptr) {
      return PFS_HA_ERR_END_OF_FILE;
    }

    mysql_mutex_lock(&cur_group->LOCK_group);

    while (m_pos.m_index_2 < cur_group->max_thread_ids_in_group) {
      cur_thread = &(cur_group->group_threads[m_pos.m_index_2]);
      if (m_opened_index->match(cur_group, cur_thread)) {
        if (make_row(*cur_group, *cur_thread) == 0) {
          m_next_pos.set_after(&m_pos);
          mysql_mutex_unlock(&cur_group->LOCK_group);
          return 0;
        }
      }
      m_pos.next_thread();
    }

    mysql_mutex_unlock(&cur_group->LOCK_group);

    m_pos.next_group();
  }
}

bool Index_thread_state_by_group_id_by_thread_id::match(tp_group_t *group,
                                                        tp_thread_t *thread) {
  /* Filter empty records before matching */
  if (thread->thread_handle.thread == null_thread_initializer) {
    return false;
  }

  if (m_fields >= 1) {
    if (!pc_integer_srv->match_key_unsigned(false, group->group_idx,
                                            &m_key_group_id)) {
      return false;
    }
  }

  if (m_fields >= 2) {
    if (!pc_integer_srv->match_key_unsigned(false, thread->numeric_id,
                                            &m_key_thread_id)) {
      return false;
    }
  }

  return true;
}

void Table_thread_state::reset_position() { m_pos.reset(); }

int Table_thread_state::make_row(const tp_group_t &grp,
                                 const tp_thread_t &thr) {
  if (thr.thread_handle.thread == null_thread_initializer) {
    return PFS_HA_ERR_RECORD_DELETED;
  }
  m_row = read_row(grp, thr);
  return 0;
}

int Table_thread_state::read_column_value(PSI_field *field,
                                          unsigned int index) {
  assert(index <
         std::distance(std::begin(field_setters), std::end(field_setters)));
  field_setters[index](field, m_row);
  return 0;
}

/* Map a C++ class to a C interface */

static int cb_rnd_init(PSI_table_handle *, bool) { return 0; }

static int cb_rnd_next(PSI_table_handle *handle) {
  auto *that = reinterpret_cast<Table_thread_state *>(handle);
  return that->rnd_next();
}

static int cb_rnd_pos(PSI_table_handle *handle) {
  auto *that = reinterpret_cast<Table_thread_state *>(handle);
  return that->rnd_pos();
}

static int cb_index_init(PSI_table_handle *table, unsigned int idx, bool sorted,
                         PSI_index_handle **index) {
  int rc;
  auto *that_table = reinterpret_cast<Table_thread_state *>(table);
  auto **that_index = reinterpret_cast<Index_thread_state **>(index);
  rc = that_table->index_init(idx, sorted);
  *that_index = that_table->m_opened_index;
  return rc;
}

static int cb_index_read(PSI_index_handle *index, PSI_key_reader *reader,
                         unsigned int idx, int find_flag) {
  auto *that_index = reinterpret_cast<Index_thread_state *>(index);
  Table_thread_state *that_table = that_index->m_table;
  return that_table->index_read(reader, idx, find_flag);
}

static int cb_index_next(PSI_table_handle *table) {
  auto *that = reinterpret_cast<Table_thread_state *>(table);
  return that->index_next();
}

static void cb_reset_position(PSI_table_handle *handle) {
  auto *that = reinterpret_cast<Table_thread_state *>(handle);
  that->reset_position();
}

static int cb_read_column_value(PSI_table_handle *handle, PSI_field *field,
                                unsigned int index) {
  auto *that = reinterpret_cast<Table_thread_state *>(handle);
  return that->read_column_value(field, index);
}

static PSI_table_handle *cb_open_table(PSI_pos **pos) {
  auto **that_pos = reinterpret_cast<Table_thread_state::pos_t **>(pos);
  Table_thread_state *that = Table_thread_state::open_table(that_pos);
  auto *handle = reinterpret_cast<PSI_table_handle *>(that);
  return handle;
}

static void cb_close_table(PSI_table_handle *handle) {
  auto *that = reinterpret_cast<Table_thread_state *>(handle);
  that->close_table();
  delete that;
}

static unsigned long long cb_get_row_count() {
  return MAX_THREAD_GROUPS * MAX_THREADS_PER_GROUP;
}

/* Expose the table in the performance schema */

static PFS_engine_table_share_proxy table_share;

PFS_engine_table_share_proxy *table_share_tp_thread_state_init() {
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

  table_share.m_table_name = "tp_thread_state";
  table_share.m_table_name_length = 15;
  table_share.m_table_definition = table_definition;
  table_share.m_ref_length = sizeof(Table_thread_state::pos_t);
  table_share.m_acl = READONLY;
  table_share.delete_all_rows = nullptr;
  table_share.get_row_count = cb_get_row_count;
  return &table_share;
}
