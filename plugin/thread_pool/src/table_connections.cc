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

#include <atomic>
#include <tuple>
#include <type_traits>  // std::is_trivially_copyable
#include <utility>      // std::make_integer_sequence
#include <variant>
#include <vector>

#include "plugin/thread_pool/src/table_thread_state.h"

#include "plugin/thread_pool/src/methods.h"
#include "plugin/thread_pool/src/thread_pool.h"
#include "plugin/thread_pool/src/thread_pool_tables.h"

#include "my_dbug.h"
#include "string_with_len.h"  // STRING_WITH_LEN
#include "template_utils.h"   // pointer_cast

namespace {

template <typename... Ts>
void dbg(Ts... ts [[maybe_unused]]) {
  DBUG_LOG("tp_pfs", ((sout << ... << ts), ""));
}

/** Keep table definition here so that it is easy to refer to it when
    modifying row tuple, make_row() and read_column_value(). This value is
    assigned to the struct describing the table further down. */
static constexpr const char *table_definition =
    "CONNECTION_ID int unsigned NOT NULL, "
    "TP_GROUP_ID int unsigned NOT NULL, "
    "TP_PROCESSING_THREAD_NUMBER int unsigned, "
    "THREAD_ID bigint unsigned, "
    "STATE VARCHAR(30) NOT NULL, "
    "ACTIVE_FLAG int unsigned NOT NULL, "
    "KILLED_STATE VARCHAR(30) NOT NULL, "
    "CLEANUP_STATE VARCHAR(30) NOT NULL, "
    "TIME_OF_LAST_EVENT_COMPLETION bigint unsigned NOT NULL, "
    "TIME_OF_EXPIRY bigint unsigned NOT NULL, "
    "TIME_OF_ADD bigint unsigned NOT NULL, "
    "TIME_OF_POP bigint unsigned NOT NULL, "
    "TIME_OF_ARM bigint unsigned NOT NULL, "
    "CONNECT_HANDLER_INDEX int unsigned NOT NULL, "
    "TYPE VARCHAR(30) NOT NULL, "
    "DIRECT_QUERY_EVENTS bigint unsigned NOT NULL, "
    "QUEUED_QUERY_EVENTS bigint unsigned NOT NULL, "
    "TIME_OF_EVENT_ARRIVAL bigint unsigned NOT NULL, "
    "MANAGEMENT_TIME bigint unsigned NOT NULL";

/** Copy of the values in a row, initialized when first moving to
    the row. Uses types which correspond to the table definition,
    NOT the types used internally. Values may need to be converted
    in make_row(). */
using Connections_row =
    std::tuple<srv_utils::PSI_int_unsigned_NOT_NULL,  // connection_id
               srv_utils::PSI_int_unsigned_NOT_NULL,  // group_idx
               srv_utils::PSI_int_unsigned,     // processing_thread->numeric_id
               srv_utils::PSI_bigint_unsigned,  // processing_thread->pfs_id
               const char *,                    // conn_state
               srv_utils::PSI_int_unsigned_NOT_NULL,  // active_flag
               const char *,                          // connection_killed_state
               const char *,                          // cleanup_state
               PSI_time_point,  // time_of_last_event_completion
               PSI_time_point,  // expiry_timpt
               PSI_time_point,  // time_of_add
               PSI_time_point,  // time_of_pop
               PSI_time_point,  // time_of_arm
               srv_utils::PSI_int_unsigned_NOT_NULL,     // conn_handler_index
               const char *,                             // type
               srv_utils::PSI_bigint_unsigned_NOT_NULL,  // direct events
               srv_utils::PSI_bigint_unsigned_NOT_NULL,  // queued events
               PSI_time_point,                          // time_of_event_arrival
               srv_utils::PSI_bigint_unsigned_NOT_NULL  // management_time
               >;

/** Copies values from TP code (primarily tp_client_low_level_t)
    into the row object. Called when first entering a row. */
static Connections_row make_row(const tp_group_t &tg,
                                const tp_client_low_level_t &cc) {
  return {
      cc.connection_id,
      tg.group_idx,
      (cc.processing_thread == nullptr
           ? std::nullopt
           : srv_utils::PSI_int_unsigned(cc.processing_thread->numeric_id)),
      (cc.processing_thread == nullptr
           ? std::nullopt
           : srv_utils::PSI_bigint_unsigned(
                 cc.processing_thread->pfs_thread_id)),
      connection_state_str(cc.conn_state),
      cc.active_flag,
      killed_state_str(cc.connection_killed_state),
      cleanup_level_str(cc.cleanup_state),
      psi_time_point(cc.time_of_last_event_completion),
      psi_time_point(cc.expiry_timpt),
      psi_time_point(cc.time_of_add),
      psi_time_point(cc.time_of_pop),
      psi_time_point(cc.time_of_arm),
      cc.conn_handler_index,
      connection_type_str(cc.type),
      cc.direct_events.load(std::memory_order_relaxed),
      cc.queued_events.load(std::memory_order_relaxed),
      psi_time_point(cc.time_of_event_arrival.load(std::memory_order_relaxed)),
      cc.management_time.load(std::memory_order_relaxed).count()};
}
using Connections_table = std::vector<Connections_row>;

// Helpers to map a runtime column index to the compile-time tuple index.
template <int V>
struct Value {
  static constexpr int value = V;
};

template <int... ints>
using Var = std::variant<Value<ints>...>;

template <int... Ints>
static auto make_var(unsigned int val, std::integer_sequence<int, Ints...>) {
  Var<Ints...> const v[] = {Var<Ints...>{Value<Ints>{}}...};
  return v[val];
}

static constexpr auto column_sequence =
    std::make_integer_sequence<int, std::tuple_size_v<Connections_row>>();

static void set_column_value(PSI_field *field, unsigned int index,
                             const Connections_row &row) {
  assert(index < std::tuple_size_v<Connections_row>);
  std::visit(
      [&](auto &&v) {
        using ValueType = std::decay_t<decltype(v)>;
        srv_utils::overloaded_set(field, std::get<ValueType::value>(row));
      },
      make_var(index, column_sequence));
}

// open/reset need to make the postion "before first row", so that a call to
// rnd_next() takes us to the first row.
struct Position {
  int group_ix = -1;
  int conn_ix = -1;
};
struct Table_handle {
  Position cur_pos;

 private:
  Connections_table conns_for_group;
  int fetched_group_idx = -1;  // No connections fetched yet

  void fetch_conns(const tp_group_t &tg) {
    conns_for_group.clear();
    for (const tp_client_low_level_t *c = tg.open_connections.front();
         c != nullptr; c = c->next_open_connection) {
      conns_for_group.push_back(make_row(tg, *c));
    }
    fetched_group_idx = tg.group_idx;
  }

 public:
  int rnd_next() {
    dbg("cb_rnd_next() at line ", __LINE__,
        "> cur_pos:{ group_ix:", cur_pos.group_ix,
        ", conn_ix:", cur_pos.conn_ix,
        " }, fetched_group_ix: ", fetched_group_idx);

    assert(cur_pos.conn_ix < static_cast<int>(conns_for_group.size()));
    ++cur_pos.conn_ix;

    while (conns_for_group.begin() + cur_pos.conn_ix == conns_for_group.end()) {
      ++cur_pos.group_ix;
      tp_group_t *gp = get_group(cur_pos.group_ix);
      if (gp == nullptr) {
        return PFS_HA_ERR_END_OF_FILE;
      }
      fetch_conns(*gp);
      cur_pos.conn_ix = 0;
    }
    assert(cur_pos.group_ix > -1);
    assert(cur_pos.group_ix == fetched_group_idx);
    assert(cur_pos.conn_ix > -1);
    return 0;
  }
  int rnd_init(bool) {
    dbg("cb_rnd_init() at line ", __LINE__,
        "> cur_pos:{ group_ix:", cur_pos.group_ix,
        ", conn_ix:", cur_pos.conn_ix,
        " }, fetched_group_ix: ", fetched_group_idx);
    assert(cur_pos.group_ix == -1 && cur_pos.conn_ix == -1);
    return 0;
  }
  int rnd_pos() {
    dbg("cb_rnd_pos() at line ", __LINE__,
        "> cur_pos:{ group_ix:", cur_pos.group_ix,
        ", conn_ix:", cur_pos.conn_ix,
        " }, fetched_group_ix: ", fetched_group_idx);

    if (cur_pos.group_ix != fetched_group_idx) {
      fetch_conns(*get_group(cur_pos.group_ix));
      if (conns_for_group.begin() + cur_pos.conn_ix >= conns_for_group.end()) {
        return PFS_HA_ERR_RECORD_DELETED;
      }
    }

    return 0;
  }
  void rnd_reset_position() {
    dbg("cb_reset_position() at line ", __LINE__,
        "> cur_pos:{ group_ix:", cur_pos.group_ix,
        ", conn_ix:", cur_pos.conn_ix,
        " }, fetched_group_ix: ", fetched_group_idx);
    cur_pos = Position();
  }
  int rnd_read_column_value(PSI_field *field, unsigned int index) {
    dbg("cb_read_column_value() at line ", __LINE__,
        "> cur_pos:{ group_ix:", cur_pos.group_ix,
        ", conn_ix:", cur_pos.conn_ix,
        " }, fetched_group_ix: ", fetched_group_idx, " index:", index);

    set_column_value(field, index, conns_for_group[cur_pos.conn_ix]);
    return 0;
  }
};

Table_handle *th_ptr(PSI_table_handle *pth) {
  return pointer_cast<Table_handle *>(pth);
}

constinit PFS_engine_table_share_proxy proxy_share = {
    // m_proxy_engine_table
    {// cb_rnd_next,
     [](PSI_table_handle *pth) { return th_ptr(pth)->rnd_next(); },

     // cb_rnd_init
     [](PSI_table_handle *pth, bool b) { return th_ptr(pth)->rnd_init(b); },

     // cb_rnd_pos,
     [](PSI_table_handle *pth) { return th_ptr(pth)->rnd_pos(); },

     // cb_index_init
     nullptr,

     // cb_index_read
     nullptr,

     // cb_index_next
     nullptr,

     // cb_read_column_value,
     [](PSI_table_handle *pth, PSI_field *field, unsigned int index) {
       return th_ptr(pth)->rnd_read_column_value(field, index);
     },

     // cb_reset_position
     [](PSI_table_handle *pth) { th_ptr(pth)->rnd_reset_position(); },

     // write_column_value
     nullptr,

     // write_row_value
     nullptr,

     // update_column_value
     nullptr,

     // update_row_values
     nullptr,

     // delete_row_values
     nullptr,

     // cb_open_table,
     [](PSI_pos **pos) {
       dbg("cb_open_table() at line ", __LINE__);
       auto *th_ptr = new Table_handle();
       *pos = pointer_cast<PSI_pos *>(&th_ptr->cur_pos);
       return pointer_cast<PSI_table_handle *>(th_ptr);
     },

     // cb_close_table
     [](PSI_table_handle *ph) {
       dbg("cb_close_table() at line ", __LINE__);
       delete th_ptr(ph);
     }},

    STRING_WITH_LEN("tp_connections"),
    table_definition,
    sizeof(Position),  // Note! This is the size which will be memcpyed by PFS
                       // engine
    READONLY,
    nullptr,  // delete_all_rows

    // get_row_count
    []() -> long long unsigned int {
      dbg("get_row_count() at line ", __LINE__);
      return get_max_connections();
    }};
}  // namespace

constinit PFS_engine_table_share_proxy *tp_connections_proxy_share =
    &proxy_share;
