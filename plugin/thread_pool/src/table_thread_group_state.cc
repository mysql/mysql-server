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

#include "plugin/thread_pool/src/table_thread_group_state.h"

#include "plugin/thread_pool/src/methods.h"
#include "plugin/thread_pool/src/thread_pool.h"
#include "plugin/thread_pool/src/thread_pool_tables.h"

#include "template_utils.h"  // pointer_cast

/** Keep table definition here so that it is easy to refer to it when
    modifying row struct, make_row() and read_column_value(). This value is
    assigned to the struct describing the table further down. */
static constexpr const char *table_definition =
    "`TP_GROUP_ID` int unsigned NOT NULL, "
    "`CONSUMER_THREADS` int unsigned NOT NULL, "
    "`RESERVE_THREADS` int unsigned NOT NULL, "
    "`CONNECT_THREAD_COUNT` int unsigned NOT NULL, "
    "`CONNECTION_COUNT` int unsigned NOT NULL, "
    "`QUEUED_QUERIES` int unsigned NOT NULL, "
    "`QUEUED_TRANSACTIONS` int unsigned NOT NULL, "
    "`STALL_LIMIT` int unsigned NOT NULL, "
    "`PRIO_KICKUP_TIMER` int unsigned NOT NULL, "
    "`ALGORITHM` varchar(20) NOT NULL, "
    "`THREAD_COUNT` int unsigned NOT NULL, "
    "`ACTIVE_THREAD_COUNT` int unsigned NOT NULL, "
    "`STALLED_THREAD_COUNT` int unsigned NOT NULL, "
    "`WAITING_THREAD_NUMBER` int unsigned, "
    "`OLDEST_QUEUED` bigint unsigned, "
    "`MAX_THREAD_IDS_IN_GROUP` int unsigned NOT NULL, "
    "`EFFECTIVE_MAX_TRANSACTIONS_LIMIT` int unsigned, "
    "`NUM_QUERY_THREADS` bigint unsigned NOT NULL, "
    "`TIME_OF_LAST_THREAD_CREATION` bigint unsigned, "
    "`NUM_CONNECT_HANDLER_THREAD_IN_SLEEP` int unsigned NOT NULL, "
    "`THREADS_BOUND_TO_TRANSACTION` int unsigned NOT NULL, "
    "`QUERY_THREADS_COUNT` int unsigned NOT NULL, "
    "`TIME_OF_EARLIEST_CON_EXPIRE` bigint unsigned, "
    "`TIME_OF_LAST_LONGRUNNING_TRXS_CHECK` bigint unsigned, "
    "UNIQUE KEY(`TP_GROUP_ID`)";

/** Copy of the values in a row, initialized when first moving to
    the row. Uses types which correspond to the table definition,
    NOT the types used internally. Values may need to be converted
    in make_row(). */
struct Table_thread_group_state_row {
  /** Column TP_GROUP_ID */
  srv_utils::PSI_int_unsigned_NOT_NULL m_group_id;
  /** Column CONSUMER_THREADS */
  srv_utils::PSI_int_unsigned_NOT_NULL m_consumer_threads;
  /** Column RESERVE_THREADS */
  srv_utils::PSI_int_unsigned_NOT_NULL m_reserve_threads;
  /** Column CONNECT_THREAD_COUNT */
  srv_utils::PSI_int_unsigned_NOT_NULL m_connect_thread_count;
  /** Column CONNECTION_COUNT */
  srv_utils::PSI_int_unsigned_NOT_NULL m_connection_count;
  /** Column QUEUED_QUERIES */
  srv_utils::PSI_int_unsigned_NOT_NULL m_queued_queries;
  /** Column QUEUED_TRANSACTIONS */
  srv_utils::PSI_int_unsigned_NOT_NULL m_queued_transactions;
  /** Column STALL_LIMIT */
  srv_utils::PSI_int_unsigned_NOT_NULL m_stall_limit;
  /** Column PRIO_KICKUP_TIMER */
  srv_utils::PSI_int_unsigned_NOT_NULL m_prio_kickup_timer;
  /** Column ALGORITHM */
  const char *m_algorithm;
  /** Column THREAD_COUNT */
  srv_utils::PSI_int_unsigned_NOT_NULL m_thread_count;
  /** Column ACTIVE_THREAD_COUNT */
  srv_utils::PSI_int_unsigned_NOT_NULL m_active_thread_count;
  /** Column STALLED_THREAD_COUNT */
  srv_utils::PSI_int_unsigned_NOT_NULL m_stalled_thread_count;
  /** Column WAITING_THREAD_NUMBER */
  srv_utils::PSI_int_unsigned m_waiting_thread_number;
  /** Column OLDEST_QUEUED */
  srv_utils::PSI_bigint_unsigned m_oldest_queued;
  /** Column MAX_THREAD_IDS_IN_GROUP */
  srv_utils::PSI_int_unsigned_NOT_NULL m_max_thread_ids_in_group;
  /** Column EFFECTIVE_MAX_TRANSACTIONS_LIMIT */
  std::uint32_t m_effective_max_transactions_limit = 0;
  /** Column NUM_QUERY_THREADS */
  srv_utils::PSI_bigint_unsigned_NOT_NULL m_num_query_threads = 0;
  /** Column TIME_OF_LAST_THREAD_CREATION */
  srv_utils::PSI_bigint_unsigned_NOT_NULL m_last_thread_creation_time = 0;
  /** Column NUM_CONNECT_HANDLER_THREAD_IN_SLEEP */
  srv_utils::PSI_int_unsigned_NOT_NULL m_num_connect_handler_thread_in_sleep =
      0;
  /** Column TRXN_THREADS */
  srv_utils::PSI_int_unsigned_NOT_NULL m_trxn_threads = 0;
  /** Column QUERY_THREADS_COUNT */
  srv_utils::PSI_int_unsigned_NOT_NULL m_query_threads_count = 0;
  /** Column TIME_OF_EARLIEST_CON_EXPIRE */
  PSI_time_point m_earliest_con_expire_timept = 0;
  PSI_time_point m_time_of_last_longrunning_trxs_check = 0;
};

/** Copies values from TP code (primarily tp_client_low_level_t)
    into the row object. Called when first entering a row. */
static Table_thread_group_state_row make_row(const tp_group_t &grp) {
  return {grp.group_idx,
          grp.threads_for_consumer,
          grp.threads_for_reserve,
          static_cast<srv_utils::PSI_int_unsigned_NOT_NULL>(
              grp.connect_threads.load(std::memory_order_relaxed)),
          grp.stats.connection_count,
          grp.queued_queries.elements(),
          grp.queued_trans.elements(),
          static_cast<srv_utils::PSI_int_unsigned_NOT_NULL>(
              thread_pool_stall_limit),
          static_cast<srv_utils::PSI_int_unsigned_NOT_NULL>(
              thread_pool_prio_kickup_timer),
          tp_algorithm(),
          grp.threads_initialized,
          get_active_threads(&grp),
          grp.max_active_threads - grp.threads_user_request,
          grp.waiting_thread
              ? srv_utils::PSI_int_unsigned(grp.waiting_thread->numeric_id)
              : std::nullopt,
          [&]() -> srv_utils::PSI_bigint_unsigned {
            const auto *cc = grp.queued_trans.front();
            return cc != nullptr
                       ? srv_utils::PSI_bigint_unsigned(
                             std::chrono::duration_cast<Misec>(
                                 tp_now() - cc->time_of_event_arrival.load(
                                                std::memory_order_relaxed))
                                 .count())
                       : std::nullopt;
          }(),
          grp.max_thread_ids_in_group,
          configured_max_transactions_limit_per_group(&grp),
          grp.num_query_threads.load(std::memory_order_relaxed),
          grp.last_thread_creation_time,
          grp.num_connect_handler_thread_in_sleep,
          grp.trxn_threads,
          grp.query_threads_count.load(std::memory_order_relaxed),
          psi_time_point(grp.earliest_con_expire_timpt),
          psi_time_point(grp.time_of_last_longrunning_trxs_check)};
}

using FieldSetter = void (*)(PSI_field *, const Table_thread_group_state_row &);

/** Mapping between column indices and the member variables of the row struct.
 */
static FieldSetter field_setters[] = {
    [](auto *f, const auto &r) { srv_utils::overloaded_set(f, r.m_group_id); },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_consumer_threads);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_reserve_threads);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_connect_thread_count);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_connection_count);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_queued_queries);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_queued_transactions);
    },
    [](auto *f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_stall_limit);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_prio_kickup_timer);
    },
    [](auto f, const auto &r) { srv_utils::overloaded_set(f, r.m_algorithm); },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_thread_count);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_active_thread_count);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_stalled_thread_count);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_waiting_thread_number);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_oldest_queued);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_max_thread_ids_in_group);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_effective_max_transactions_limit);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_num_query_threads);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_last_thread_creation_time);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_num_connect_handler_thread_in_sleep);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_trxn_threads);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_query_threads_count);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_earliest_con_expire_timept);
    },
    [](auto f, const auto &r) {
      srv_utils::overloaded_set(f, r.m_time_of_last_longrunning_trxs_check);
    }};

class Table_thread_group_state;

class Index_thread_group_state {
 public:
  Index_thread_group_state(Table_thread_group_state *table) : m_table(table) {}
  virtual ~Index_thread_group_state() = default;

  virtual int read_key(PSI_key_reader *reader, int find_flag) = 0;
  virtual bool match(tp_group_t *group) = 0;

  Table_thread_group_state *m_table;
  unsigned int m_fields;
};

class Index_thread_group_state_by_group_id : public Index_thread_group_state {
 public:
  Index_thread_group_state_by_group_id(Table_thread_group_state *table)
      : Index_thread_group_state(table) {}
  ~Index_thread_group_state_by_group_id() override = default;

  int read_key(PSI_key_reader *reader, int find_flag) override;
  bool match(tp_group_t *group) override;

  PSI_plugin_key_uinteger m_key_group_id;
};

using pos_t = std::uint32_t;
// See Doxygen comment for the open_table_t function pointer typedef - in
// include/mysql/components/services/pfs_plugin_table_service.h
static_assert(std::is_trivially_copyable<pos_t>::value,
              "Position object may be memcpy'ied by the PFS engine, "
              "so it needs to be trivially copyable.");

using row_t = Table_thread_group_state_row;
using tgsindex_t = Index_thread_group_state_by_group_id;
class Table_thread_group_state {
 public:
  ~Table_thread_group_state();

  int rnd_init(bool scan);
  int rnd_next();
  int rnd_pos();

  int index_init(unsigned int idx, bool sorted);
  int index_read(PSI_key_reader *reader, unsigned int idx, int find_flag);
  int index_next();

  void reset_position() { m_pos = 0; }
  int read_column_value(PSI_field *field, unsigned int index);
  static Table_thread_group_state *open_table(pos_t **pos);

  pos_t m_pos = 0;
  pos_t m_next_pos = 0;
  row_t m_row;
  tgsindex_t *m_opened_index = nullptr;
};

Table_thread_group_state *Table_thread_group_state::open_table(pos_t **pos) {
  auto *table = new Table_thread_group_state();
  *pos = &table->m_pos;
  return table;
}

Table_thread_group_state::~Table_thread_group_state() { delete m_opened_index; }

int Table_thread_group_state::rnd_init(bool /* scan */) {
  reset_position();
  return 0;
}

int Table_thread_group_state::rnd_next() {
  // m_pos.set_at(&m_next_pos);
  m_pos = m_next_pos;

  tp_group_t *cur_group = get_group(m_pos);

  if (cur_group == nullptr) {
    return PFS_HA_ERR_END_OF_FILE;
  }

  mysql_mutex_lock(&cur_group->LOCK_group);
  m_row = make_row(*cur_group);
  mysql_mutex_unlock(&cur_group->LOCK_group);

  m_next_pos = m_pos + 1;
  return 0;
}

int Table_thread_group_state::rnd_pos() {
  tp_group_t *cur_group = get_group(m_pos);

  if (cur_group == nullptr) {
    return PFS_HA_ERR_RECORD_DELETED;
  }

  mysql_mutex_lock(&cur_group->LOCK_group);
  m_row = make_row(*cur_group);
  mysql_mutex_unlock(&cur_group->LOCK_group);

  return 0;
}

int Table_thread_group_state::index_init(unsigned int idx [[maybe_unused]],
                                         bool /* sorted */) {
  /* There is only 1 index. */
  assert(idx == 0);

  assert(m_opened_index == nullptr);
  m_opened_index = new Index_thread_group_state_by_group_id(this);
  return 0;
}

int Table_thread_group_state::index_read(PSI_key_reader *reader,
                                         unsigned int idx [[maybe_unused]],
                                         int find_flag) {
  assert(idx == 0);
  assert(m_opened_index != nullptr);

  return m_opened_index->read_key(reader, find_flag);
}

int Index_thread_group_state_by_group_id::read_key(PSI_key_reader *reader,
                                                   int find_flag) {
  pc_integer_srv->read_key_unsigned(reader, &m_key_group_id, find_flag);

  m_fields = pt_srv->get_parts_found(reader);
  return 0;
}

int Table_thread_group_state::index_next() {
  tp_group_t *cur_group;
  // m_pos.set_at(&m_next_pos);
  m_pos = m_next_pos;

  for (;;) {
    cur_group = get_group(m_pos);

    if (cur_group == nullptr) {
      return PFS_HA_ERR_END_OF_FILE;
    }

    mysql_mutex_lock(&cur_group->LOCK_group);
    if (m_opened_index->match(cur_group)) {
      m_row = make_row(*cur_group);
      // m_next_pos.set_after(&m_pos);
      m_next_pos = m_pos + 1;
      mysql_mutex_unlock(&cur_group->LOCK_group);
      return 0;
    }
    mysql_mutex_unlock(&cur_group->LOCK_group);

    // m_pos.next_group();
    ++m_pos;
  }
}

bool Index_thread_group_state_by_group_id::match(tp_group_t *group) {
  if (m_fields >= 1) {
    if (!pc_integer_srv->match_key_unsigned(false, group->group_idx,
                                            &m_key_group_id)) {
      return false;
    }
  }
  return true;
}

int Table_thread_group_state::read_column_value(PSI_field *field,
                                                unsigned int index) {
  assert(index <
         std::distance(std::begin(field_setters), std::end(field_setters)));
  field_setters[index](field, m_row);
  return 0;
}

/* Map a C++ class to a C interface */
using TH = Table_thread_group_state;
static int cb_rnd_init(PSI_table_handle *handle, bool scan) {
  return pointer_cast<TH *>(handle)->rnd_init(scan);
}

static int cb_rnd_next(PSI_table_handle *handle) {
  return pointer_cast<TH *>(handle)->rnd_next();
}

static int cb_rnd_pos(PSI_table_handle *handle) {
  return pointer_cast<TH *>(handle)->rnd_pos();
}

static int cb_index_init(PSI_table_handle *handle, unsigned int idx,
                         bool sorted, PSI_index_handle **index) {
  auto **that_index = reinterpret_cast<Index_thread_group_state **>(index);
  auto *that_table = pointer_cast<TH *>(handle);
  int const rc = that_table->index_init(idx, sorted);
  *that_index = that_table->m_opened_index;
  return rc;
}

static int cb_index_read(PSI_index_handle *index, PSI_key_reader *reader,
                         unsigned int idx, int find_flag) {
  auto *that_index = reinterpret_cast<Index_thread_group_state *>(index);
  Table_thread_group_state *that_table = that_index->m_table;
  return that_table->index_read(reader, idx, find_flag);
}

static int cb_index_next(PSI_table_handle *handle) {
  return pointer_cast<TH *>(handle)->index_next();
}

static void cb_reset_position(PSI_table_handle *handle) {
  pointer_cast<TH *>(handle)->reset_position();
}

static int cb_read_column_value(PSI_table_handle *handle, PSI_field *field,
                                unsigned int index) {
  return pointer_cast<TH *>(handle)->read_column_value(field, index);
}

static PSI_table_handle *cb_open_table(PSI_pos **pos) {
  auto **that_pos = pointer_cast<pos_t **>(pos);
  Table_thread_group_state *that =
      Table_thread_group_state::open_table(that_pos);
  auto *handle = pointer_cast<PSI_table_handle *>(that);
  return handle;
}

static void cb_close_table(PSI_table_handle *handle) {
  delete pointer_cast<TH *>(handle);
}

static unsigned long long cb_get_row_count() { return MAX_THREAD_GROUPS; }

/* Expose the table in the performance schema */

static PFS_engine_table_share_proxy table_share;

PFS_engine_table_share_proxy *table_share_tp_thread_group_state_init() {
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

  table_share.m_table_name = "tp_thread_group_state";
  table_share.m_table_name_length = 21;
  table_share.m_table_definition = table_definition;
  table_share.m_ref_length = sizeof(pos_t);
  table_share.m_acl = READONLY;
  table_share.delete_all_rows = nullptr;
  table_share.get_row_count = cb_get_row_count;
  return &table_share;
}
