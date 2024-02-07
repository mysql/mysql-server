/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

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
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <string>
#include <vector>

#include "mysql/components/my_service.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC

#include "mysql/components/services/pfs_plugin_table_service.h"
#include "plugin/group_replication/include/perfschema/table_replication_group_member_actions.h"
#include "plugin/group_replication/include/perfschema/utilities.h"
#include "sql/rpl_sys_key_access.h"
#include "sql/rpl_sys_table_access.h"

namespace gr {
namespace perfschema {

/**
  A row in the replication_group_member_actions table.
*/
struct Replication_group_member_actions {
  std::string name;
  std::string event;
  unsigned long enabled;
  std::string type;
  unsigned long priority;
  std::string error_handling;
};

/**
  A structure to define a handle for table in plugin/component code.
*/
struct Replication_group_member_actions_table_handle {
  unsigned long long current_row_pos{0};
  unsigned long long next_row_pos{0};

  std::vector<struct Replication_group_member_actions> rows;
};

unsigned long long Pfs_table_replication_group_member_actions::get_row_count() {
  return 0;
}

int Pfs_table_replication_group_member_actions::rnd_init(
    PSI_table_handle *handle [[maybe_unused]], bool scan [[maybe_unused]]) {
  return 0;
}

int Pfs_table_replication_group_member_actions::rnd_next(
    PSI_table_handle *handle) {
  Replication_group_member_actions_table_handle *t =
      (Replication_group_member_actions_table_handle *)handle;
  t->current_row_pos = t->next_row_pos;
  if (t->current_row_pos < t->rows.size()) {
    t->next_row_pos++;
    return 0;
  }

  return PFS_HA_ERR_END_OF_FILE;
}

int Pfs_table_replication_group_member_actions::rnd_pos(
    PSI_table_handle *handle) {
  Replication_group_member_actions_table_handle *t =
      (Replication_group_member_actions_table_handle *)handle;
  if (t->current_row_pos >= t->rows.size()) {
    return PFS_HA_ERR_END_OF_FILE;
  }

  return 0;
}

void Pfs_table_replication_group_member_actions::reset_position(
    PSI_table_handle *handle) {
  Replication_group_member_actions_table_handle *t =
      (Replication_group_member_actions_table_handle *)handle;
  t->current_row_pos = 0;
  t->next_row_pos = 0;
}

int Pfs_table_replication_group_member_actions::read_column_value(
    PSI_table_handle *handle, PSI_field *field, unsigned int index) {
  Registry_guard guard;
  my_service<SERVICE_TYPE(pfs_plugin_column_string_v2)> column_string_service{
      "pfs_plugin_column_string_v2", guard.get_registry()};
  my_service<SERVICE_TYPE(pfs_plugin_column_tiny_v1)> column_tinyint_service{
      "pfs_plugin_column_tiny_v1", guard.get_registry()};

  DBUG_EXECUTE_IF(
      "group_replication_wait_before_group_member_actions_read_column_value", {
        const char act[] =
            "now signal "
            "signal.after_group_member_actions_read_column_value_waiting "
            "wait_for "
            "signal.after_group_member_actions_read_column_value_continue";
        assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
      };);

  Replication_group_member_actions_table_handle *t =
      (Replication_group_member_actions_table_handle *)handle;

  switch (index) {
    case 0: {  // name
      column_string_service->set_char_utf8mb4(
          field, t->rows[t->current_row_pos].name.c_str(),
          t->rows[t->current_row_pos].name.length());
      break;
    }
    case 1: {  // event
      column_string_service->set_char_utf8mb4(
          field, t->rows[t->current_row_pos].event.c_str(),
          t->rows[t->current_row_pos].event.length());
      break;
    }
    case 2: {  // enabled
      column_tinyint_service->set_unsigned(
          field, {t->rows[t->current_row_pos].enabled, false});
      break;
    }
    case 3: {  // type
      column_string_service->set_char_utf8mb4(
          field, t->rows[t->current_row_pos].type.c_str(),
          t->rows[t->current_row_pos].type.length());
      break;
    }
    case 4: {  // priority
      column_tinyint_service->set_unsigned(
          field, {t->rows[t->current_row_pos].priority, false});
      break;
    }
    case 5: {  // error_handling
      column_string_service->set_char_utf8mb4(
          field, t->rows[t->current_row_pos].error_handling.c_str(),
          t->rows[t->current_row_pos].error_handling.length());
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

PSI_table_handle *Pfs_table_replication_group_member_actions::open_table(
    PSI_pos **pos [[maybe_unused]]) {
  Rpl_sys_table_access table_op("mysql", "replication_group_member_actions", 6);
  if (table_op.open(TL_READ)) {
    return nullptr; /* purecov: inspected */
  }

  Replication_group_member_actions_table_handle *t =
      new Replication_group_member_actions_table_handle();
  t->rows.clear();
  t->current_row_pos = 0;
  t->next_row_pos = 0;

  TABLE *table = table_op.get_table();
  Rpl_sys_key_access key_access;
  int key_error =
      key_access.init(table, Rpl_sys_key_access::enum_key_type::INDEX_NEXT);
  if (!key_error) {
    char buffer[MAX_FIELD_WIDTH];
    String string(buffer, sizeof(buffer), &my_charset_bin);

    do {
      struct Replication_group_member_actions row;

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

      t->rows.push_back(row);
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

  reset_position((PSI_table_handle *)t);
  *pos = reinterpret_cast<PSI_pos *>(&(t->current_row_pos));
  return (PSI_table_handle *)t;
}

void Pfs_table_replication_group_member_actions::close_table(
    PSI_table_handle *handle) {
  Replication_group_member_actions_table_handle *t =
      (Replication_group_member_actions_table_handle *)handle;
  delete t;
}

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
      sizeof Replication_group_member_actions_table_handle::current_row_pos;
  m_share.m_acl = READONLY;
  m_share.get_row_count =
      Pfs_table_replication_group_member_actions::get_row_count;

  /* Initialize PFS_engine_table_proxy */
  m_share.m_proxy_engine_table = {
      Pfs_table_replication_group_member_actions::rnd_next,
      Pfs_table_replication_group_member_actions::rnd_init,
      Pfs_table_replication_group_member_actions::rnd_pos,
      nullptr,  // index_init,
      nullptr,  // index_read,
      nullptr,  // index_next,
      Pfs_table_replication_group_member_actions::read_column_value,
      Pfs_table_replication_group_member_actions::reset_position,
      nullptr,  // write_column_value,
      nullptr,  // write_row_values,
      nullptr,  // update_column_value,
      nullptr,  // update_row_values,
      nullptr,  // delete_row_values,
      Pfs_table_replication_group_member_actions::open_table,
      Pfs_table_replication_group_member_actions::close_table};
  return false;
}

}  // namespace perfschema
}  // namespace gr
