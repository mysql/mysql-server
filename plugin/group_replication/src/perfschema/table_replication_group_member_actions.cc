/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <string>
#include <vector>

#include "mysql/components/my_service.h"

#include "mysql/components/services/pfs_plugin_table_service.h"
#include "plugin/group_replication/include/perfschema/table_replication_group_member_actions.h"
#include "plugin/group_replication/include/perfschema/utilities.h"
#include "sql/rpl_sys_key_access.h"
#include "sql/rpl_sys_table_access.h"

namespace gr {
namespace perfschema {
namespace pfs_table_replication_group_member_actions {

struct dummy_table_handle_type {};
static dummy_table_handle_type dummy_table_handle{};

struct Row {
  std::string name;
  std::string event;
  unsigned long enabled;
  std::string type;
  unsigned long priority;
  std::string error_handling;
};
static std::vector<struct Row> s_rows;
static unsigned long long s_current_row_pos{0};
static unsigned long long s_next_row_pos{0};

static unsigned long long get_row_count() { return s_rows.size(); }

static int rnd_init(PSI_table_handle *handle [[maybe_unused]],
                    bool scan [[maybe_unused]]) {
  return 0;
}

static int rnd_next(PSI_table_handle *handle [[maybe_unused]]) {
  s_current_row_pos = s_next_row_pos;
  if (s_current_row_pos < get_row_count()) {
    s_next_row_pos++;
    return 0;
  }

  return PFS_HA_ERR_END_OF_FILE;
}

static int rnd_pos(PSI_table_handle *handle [[maybe_unused]]) {
  if (s_current_row_pos >= get_row_count()) {
    return PFS_HA_ERR_END_OF_FILE;
  }

  return 0;
}

static void reset_position(PSI_table_handle *handle [[maybe_unused]]) {
  s_current_row_pos = 0;
  s_next_row_pos = 0;
}

static int read_column_value(PSI_table_handle *handle [[maybe_unused]],
                             PSI_field *field, unsigned int index) {
  Registry_guard guard;
  my_service<SERVICE_TYPE(pfs_plugin_table)> table_service{
      "pfs_plugin_table", guard.get_registry()};

  switch (index) {
    case 0: {  // name
      table_service->set_field_char_utf8(
          field, s_rows[s_current_row_pos].name.c_str(),
          s_rows[s_current_row_pos].name.length());
      break;
    }
    case 1: {  // event
      table_service->set_field_char_utf8(
          field, s_rows[s_current_row_pos].event.c_str(),
          s_rows[s_current_row_pos].event.length());
      break;
    }
    case 2: {  // enabled
      table_service->set_field_utinyint(
          field, {s_rows[s_current_row_pos].enabled, false});
      break;
    }
    case 3: {  // type
      table_service->set_field_char_utf8(
          field, s_rows[s_current_row_pos].type.c_str(),
          s_rows[s_current_row_pos].type.length());
      break;
    }
    case 4: {  // priority
      table_service->set_field_utinyint(
          field, {s_rows[s_current_row_pos].priority, false});
      break;
    }
    case 5: {  // error_handling
      table_service->set_field_char_utf8(
          field, s_rows[s_current_row_pos].error_handling.c_str(),
          s_rows[s_current_row_pos].error_handling.length());
      break;
    }
    default: {
      /* purecov: begin inspected */
      assert(0);
      break;
      /* purecov: end */
    }
  }
  return 0;
}

static PSI_table_handle *open_table(PSI_pos **pos [[maybe_unused]]) {
  s_rows.clear();
  s_current_row_pos = 0;
  s_next_row_pos = 0;

  Rpl_sys_table_access table_op("mysql", "replication_group_member_actions", 6);
  if (table_op.open(TL_READ)) {
    return nullptr; /* purecov: inspected */
  }

  TABLE *table = table_op.get_table();
  Rpl_sys_key_access key_access;
  int key_error =
      key_access.init(table, Rpl_sys_key_access::enum_key_type::INDEX_NEXT);
  if (!key_error) {
    char buffer[MAX_FIELD_WIDTH];
    String string(buffer, sizeof(buffer), &my_charset_bin);

    do {
      struct Row row;

      table->field[0]->val_str(&string);
      row.name.assign(string.c_ptr_safe(), string.length());

      table->field[1]->val_str(&string);
      row.event.assign(string.c_ptr_safe(), string.length());

      row.enabled = table->field[2]->val_int();

      table->field[3]->val_str(&string);
      row.type.assign(string.c_ptr_safe(), string.length());

      row.priority = table->field[4]->val_int();

      table->field[5]->val_str(&string);
      row.error_handling.assign(string.c_ptr_safe(), string.length());

      s_rows.push_back(row);
    } while (!key_access.next());
  } else if (HA_ERR_END_OF_FILE == key_error) {
    /* Table is empty, nothing to read. */
    /* purecov: begin inspected */
    assert(0);
    /* purecov: end */
  } else {
    return nullptr; /* purecov: inspected */
  }

  key_access.deinit();
  table_op.close(false);

  auto *dummy = reinterpret_cast<PSI_table_handle *>(&dummy_table_handle);
  reset_position(dummy);
  *pos = reinterpret_cast<PSI_pos *>(&s_current_row_pos);
  return dummy;
}

static void close_table(PSI_table_handle *handle [[maybe_unused]]) {
  s_rows.clear();
}

}  // namespace pfs_table_replication_group_member_actions

bool Pfs_table_replication_group_member_actions::deinit() { return false; }

bool Pfs_table_replication_group_member_actions::init() {
  m_share.m_table_name = "replication_group_member_actions";
  m_share.m_table_name_length = ::strlen(m_share.m_table_name);
  m_share.m_table_definition =
      "name CHAR(255) CHARACTER SET ASCII NOT NULL, "
      "event CHAR(64) CHARACTER SET ASCII NOT NULL, "
      "enabled BOOLEAN NOT NULL, "
      "type CHAR(64) CHARACTER SET ASCII NOT NULL, "
      "priority TINYINT UNSIGNED NOT NULL, "
      "error_handling CHAR(64) CHARACTER SET ASCII NOT NULL";
  m_share.m_ref_length =
      sizeof pfs_table_replication_group_member_actions::s_current_row_pos;
  m_share.m_acl = READONLY;
  m_share.get_row_count =
      pfs_table_replication_group_member_actions::get_row_count;

  /* Initialize PFS_engine_table_proxy */
  m_share.m_proxy_engine_table = {
      pfs_table_replication_group_member_actions::rnd_next,
      pfs_table_replication_group_member_actions::rnd_init,
      pfs_table_replication_group_member_actions::rnd_pos,
      nullptr,  // index_init,
      nullptr,  // index_read,
      nullptr,  // index_next,
      pfs_table_replication_group_member_actions::read_column_value,
      pfs_table_replication_group_member_actions::reset_position,
      nullptr,  // write_column_value,
      nullptr,  // write_row_values,
      nullptr,  // update_column_value,
      nullptr,  // update_row_values,
      nullptr,  // delete_row_values,
      pfs_table_replication_group_member_actions::open_table,
      pfs_table_replication_group_member_actions::close_table};
  return false;
}

}  // namespace perfschema
}  // namespace gr
