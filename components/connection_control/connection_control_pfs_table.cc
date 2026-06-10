/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include "connection_control_pfs_table.h"
#include <cassert>
#include <components/libminchassis/gunit_harness/include/mock/mysql_mutex_v1_native.cc>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include "failed_attempts_list_imp.h"

using std::unique_ptr;

namespace connection_control {
Connection_control_pfs_table_data_row::Connection_control_pfs_table_data_row(
    const std::string &userhost, const PSI_ulong &failed_attempts)
    : m_userhost(userhost), m_failed_attempts(failed_attempts) {}

void Failed_attempts_list_imp::failed_attempts_define(const char *userhost) {
  const std::unique_lock<std::shared_mutex> lock(mutex_);
  auto pos = failed_attempts_map.find(userhost);
  if (pos == failed_attempts_map.end()) {
    PSI_ulong failed_attempts;
    failed_attempts.val = 1;
    failed_attempts.is_null = false;
    failed_attempts_map.emplace(std::string(userhost), failed_attempts);
  } else {
    pos->second.val++;
  }
}

bool Failed_attempts_list_imp::failed_attempts_undefine(const char *userhost) {
  const std::unique_lock<std::shared_mutex> lock(mutex_);
  return failed_attempts_map.erase(userhost) == 0;
}

Connection_control_pfs_table_data *
Failed_attempts_list_imp::copy_pfs_table_data() {
  const std::unique_lock<std::shared_mutex> lock(mutex_);
  try {
    auto *ret = new Connection_control_pfs_table_data;
    if (failed_attempts_map.empty()) return ret;
    for (auto &elt : failed_attempts_map) {
      ret->emplace_back(elt.first, elt.second);
    }
    return ret;
  } catch (...) {
    return nullptr;
  }
}

unsigned long long Failed_attempts_list_imp::get_failed_attempts_list_count() {
  const std::shared_lock<std::shared_mutex> lock(mutex_);
  return failed_attempts_map.size();
}

unsigned long long Failed_attempts_list_imp::get_failed_attempts_count(
    const char *userhost) {
  const std::shared_lock<std::shared_mutex> lock(mutex_);
  auto pos = failed_attempts_map.find(userhost);
  if (pos == failed_attempts_map.end()) {
    return 0;
  }
  return pos->second.val;
}

void Failed_attempts_list_imp::reset() { failed_attempts_map.clear(); }

typedef Connection_control_pfs_table_data::const_iterator
    Connection_control_pfs_table_pos;
static PFS_engine_table_share_proxy *share_list[1] = {nullptr}, share;

class Connection_control_tb_handle : public Connection_control_alloc {
 public:
  unique_ptr<Connection_control_pfs_table_data> m_table;
  Connection_control_pfs_table_data::const_iterator m_pos;
  bool before_first_row;

  void rewind() {
    if (m_table == nullptr || m_table->empty()) return;
    before_first_row = true;
    m_pos = m_table->cbegin();
  }

  [[nodiscard]] bool is_eof() const {
    if (m_table == nullptr || m_table->empty()) return true;
    return m_pos == m_table->cend();
  }

  Connection_control_tb_handle() {
    m_table.reset(g_failed_attempts_list.copy_pfs_table_data());
    rewind();
  }
};

static unsigned long long get_row_count() {
  return g_failed_attempts_list.get_failed_attempts_list_count();
}

static int rnd_init(PSI_table_handle *handle, bool /*scan*/) {
  auto *tb = reinterpret_cast<Connection_control_tb_handle *>(handle);
  tb->rewind();
  return tb->is_eof() ? PFS_HA_ERR_END_OF_FILE : 0;
}

static int rnd_next(PSI_table_handle *handle) {
  auto *tb = reinterpret_cast<Connection_control_tb_handle *>(handle);
  if (tb->before_first_row) {
    /* mysql always calls rnd_next after calling rnd_init */
    tb->before_first_row = false;
    return tb->is_eof() ? PFS_HA_ERR_END_OF_FILE : 0;
  }
  if (!tb->is_eof()) {
    tb->m_pos++;
  }
  return tb->is_eof() ? PFS_HA_ERR_END_OF_FILE : 0;
}

static int rnd_pos(PSI_table_handle *handle) { return rnd_next(handle); }

static int read_column_value(PSI_table_handle *handle, PSI_field *field,
                             unsigned int index) {
  auto *tb = reinterpret_cast<Connection_control_tb_handle *>(handle);
  SERVICE_TYPE(pfs_plugin_column_string_v2) *sstr =
      SERVICE_PLACEHOLDER(pfs_plugin_column_string_v2);
  SERVICE_TYPE(pfs_plugin_column_integer_v1) *sint =
      SERVICE_PLACEHOLDER(pfs_plugin_column_integer_v1);
  if (tb->before_first_row || tb->is_eof()) {
    return PFS_HA_ERR_END_OF_FILE;
  }
  switch (index) {
    case 0:  // USERHOST
      sstr->set_varchar_utf8mb4(field, tb->m_pos->m_userhost.c_str());
      break;
    case 1:  // FAILED_ATTEMPTS
      sint->set_unsigned(field, tb->m_pos->m_failed_attempts);
      break;
    default:
      assert(0);
      break;
  }
  return 0;
}

static void reset_position(PSI_table_handle *handle) {
  auto *tb = reinterpret_cast<Connection_control_tb_handle *>(handle);
  tb->rewind();
}

static PSI_table_handle *open_table(PSI_pos ** /*pos*/) {
  auto *newd = new Connection_control_tb_handle();
  return reinterpret_cast<PSI_table_handle *>(newd);
}
static void close_table(PSI_table_handle *handle) {
  delete reinterpret_cast<Connection_control_tb_handle *>(handle);
}

bool register_pfs_table() {
  /* Instantiate and initialize PFS_engine_table_share_proxy */
  share.m_table_name = "connection_control_failed_login_attempts";
  share.m_table_name_length =
      sizeof("connection_control_failed_login_attempts") - 1;
  share.m_table_definition =
      "USERHOST VARCHAR(6553) NOT NULL, "
      "FAILED_ATTEMPTS INT NOT NULL";
  share.m_ref_length = sizeof(Connection_control_pfs_table_data::iterator);
  share.m_acl = READONLY;
  share.get_row_count = get_row_count;
  share.delete_all_rows = nullptr; /* READONLY TABLE */

  /* Initialize PFS_engine_table_proxy */
  share.m_proxy_engine_table = {.rnd_next = rnd_next,
                                .rnd_init = rnd_init,
                                .rnd_pos = rnd_pos,
                                .index_init = nullptr,
                                .index_read = nullptr,
                                .index_next = nullptr,
                                .read_column_value = read_column_value,
                                .reset_position = reset_position,
                                /* READONLY TABLE */
                                .write_column_value = nullptr,
                                .write_row_values = nullptr,
                                .update_column_value = nullptr,
                                .update_row_values = nullptr,
                                .delete_row_values = nullptr,
                                .open_table = open_table,
                                .close_table = close_table};
  share_list[0] = &share;
  return SERVICE_PLACEHOLDER(pfs_plugin_table_v1)
      ->add_tables(&share_list[0], 1);
}
bool unregister_pfs_table() {
  return SERVICE_PLACEHOLDER(pfs_plugin_table_v1)
      ->delete_tables(&share_list[0], 1);
}

}  // namespace connection_control
