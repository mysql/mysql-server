/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include <mysql/psi/mysql_mutex.h>
#include "my_time.h"

#include <iterator>
#include <tuple>
#include <vector>

#include "mysql/components/my_service.h"

#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/pfs_plugin_table_service.h"
#include "plugin/group_replication/include/gcs_operations.h"
#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/perfschema/table_communication_information.h"
#include "plugin/group_replication/include/perfschema/utilities.h"

extern Gcs_operations *gcs_module;
extern Group_member_info_manager_interface *group_member_mgr;
extern Group_member_info *local_member_info;

namespace gr {
namespace perfschema {

/**
  A row in the replication_group_communication_information table.
*/
struct Replication_group_communication_information {
  uint32_t write_concurrency{0};
  Member_version mysql_version{0x00000};
  unsigned long single_writer_capable{0};
  Group_member_info_list found_preferred_leaders{
      (Malloc_allocator<Group_member_info *>(key_group_member_info))};
  Group_member_info_list found_actual_leaders{
      (Malloc_allocator<Group_member_info *>(key_group_member_info))};
};

/**
  A structure to define a handle for table in plugin/component code.
*/
struct Replication_group_communication_information_table_handle {
  unsigned long long current_pos{0};
  Replication_group_communication_information row;

  /**
    Fetch values required for the row and stores into row struct
    members. This stored data is read later through read_row_values().

    @return Operation status
      @retval 0     Success
      @retval 1     Failure
  */
  bool fetch_row_data();
};

bool Replication_group_communication_information_table_handle::
    fetch_row_data() {
  bool constexpr ERROR = true;
  bool constexpr OK = false;

  if (gcs_module == nullptr || group_member_mgr == nullptr) {
    return ERROR;
  }

  enum enum_gcs_error error_code =
      gcs_module->get_write_concurrency(row.write_concurrency);
  if (error_code != GCS_OK) {
    return ERROR;
  }

  Gcs_protocol_version const gcs_version = gcs_module->get_protocol_version();
  if (gcs_version == Gcs_protocol_version::UNKNOWN) {
    return ERROR;
  }
  row.mysql_version = convert_to_mysql_version(gcs_version);

  std::vector<Gcs_member_identifier> preferred_leaders;
  std::vector<Gcs_member_identifier> actual_leaders;
  error_code = gcs_module->get_leaders(preferred_leaders, actual_leaders);
  if (error_code != GCS_OK) {
    return ERROR;
  }

  for (const auto &preferred_leader : preferred_leaders) {
    auto member_id =
        group_member_mgr->get_group_member_info_by_member_id(preferred_leader);
    if (member_id) row.found_preferred_leaders.emplace_back(member_id);
  }

  for (const auto &actual_leader : actual_leaders) {
    auto member_id =
        group_member_mgr->get_group_member_info_by_member_id(actual_leader);
    if (member_id) row.found_actual_leaders.emplace_back(member_id);
  }

  // If we are running a version that does not support Single Leader,
  // we will report it as not running in Single Leader.
  // Else, we will retrieve it form the running group.
  //
  // The value of get_allow_single_leader must be the same in all group
  // members if the protocol version is >=V3.
  // Hence we retrieve the first group member, and get the value from it.

  row.single_writer_capable = 0;
  if (local_member_info != nullptr && gcs_version >= Gcs_protocol_version::V3) {
    auto recovery_status = local_member_info->get_recovery_status();

    if (recovery_status == Group_member_info::MEMBER_IN_RECOVERY ||
        recovery_status == Group_member_info::MEMBER_ONLINE) {
      row.single_writer_capable = static_cast<unsigned long>(
          local_member_info->get_allow_single_leader());
    }
  }

  return OK;
}

unsigned long long Pfs_table_communication_information::get_row_count() {
  return NR_ROWS;
}

int Pfs_table_communication_information::rnd_next(PSI_table_handle *handle) {
  Replication_group_communication_information_table_handle *t =
      (Replication_group_communication_information_table_handle *)handle;

  if (t->current_pos >= NR_ROWS) {
    return PFS_HA_ERR_END_OF_FILE;
  }

  bool const error = t->fetch_row_data();
  if (error) {
    // Is there a more suitable error code?
    return PFS_HA_ERR_END_OF_FILE;
  }

  t->current_pos++;

  return SUCCESS;
}

int Pfs_table_communication_information::rnd_init(PSI_table_handle *handle
                                                  [[maybe_unused]],
                                                  bool scan [[maybe_unused]]) {
  return SUCCESS;
}

int Pfs_table_communication_information::rnd_pos(PSI_table_handle *handle
                                                 [[maybe_unused]]) {
  return SUCCESS;
}

void Pfs_table_communication_information::reset_position(
    PSI_table_handle *handle) {
  Replication_group_communication_information_table_handle *t =
      (Replication_group_communication_information_table_handle *)handle;
  t->current_pos = 0;
}

int Pfs_table_communication_information::read_column_value(
    PSI_table_handle *handle, PSI_field *field,
    unsigned int index [[maybe_unused]]) {
  Registry_guard guard;
  my_service<SERVICE_TYPE(pfs_plugin_column_tiny_v1)> column_tinyint_service{
      "pfs_plugin_column_tiny_v1", guard.get_registry()};
  my_service<SERVICE_TYPE(pfs_plugin_column_bigint_v1)> column_bigint_service{
      "pfs_plugin_column_bigint_v1", guard.get_registry()};
  my_service<SERVICE_TYPE(pfs_plugin_column_blob_v1)> column_blob_service{
      "pfs_plugin_column_blob_v1", guard.get_registry()};

  DBUG_EXECUTE_IF(
      "group_replication_wait_before_group_communication_information_read_"
      "column_"
      "value",
      {
        const char act[] =
            "now signal "
            "signal.after_group_communication_information_read_column_value_"
            "waiting "
            "wait_for "
            "signal.after_group_communication_information_read_column_value_"
            "continue";
        assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
      };);

  Replication_group_communication_information_table_handle *t =
      (Replication_group_communication_information_table_handle *)handle;

  switch (index) {
    case 0: {  // WRITE_CONCURRENCY
      column_bigint_service->set_unsigned(field,
                                          {t->row.write_concurrency, false});
      break;
    }
    case 1: {  // PROTOCOL_VERSION
      column_blob_service->set(
          field, t->row.mysql_version.get_version_string().c_str(),
          t->row.mysql_version.get_version_string().size());
      break;
    }
    case 2: {  // WRITE_CONSENSUS_LEADERS_PREFERRED
      std::stringstream ss;
      for (std::size_t i = 0; i < t->row.found_preferred_leaders.size(); i++) {
        ss << t->row.found_preferred_leaders.at(i)->get_uuid();
        if (i < t->row.found_preferred_leaders.size() - 1) {
          ss << ',';
        }
      }
      column_blob_service->set(field, ss.str().c_str(), ss.str().size());
      break;
    }
    case 3: {  // WRITE_CONSENSUS_LEADERS_ACTUAL
      std::stringstream ss;
      for (std::size_t i = 0; i < t->row.found_actual_leaders.size(); i++) {
        ss << t->row.found_actual_leaders.at(i)->get_uuid();
        if (i < t->row.found_actual_leaders.size() - 1) {
          ss << ',';
        }
      }
      column_blob_service->set(field, ss.str().c_str(), ss.str().size());
      break;
    }
    case 4: {  // WRITE_CONSENSUS_SINGLE_LEADER_CAPABLE
      column_tinyint_service->set_unsigned(
          field, {t->row.single_writer_capable, false});
      break;
    }
  }
  return 0;
}

PSI_table_handle *Pfs_table_communication_information::open_table(
    PSI_pos **pos) {
  Replication_group_communication_information_table_handle *handle =
      new Replication_group_communication_information_table_handle();

  reset_position((PSI_table_handle *)handle);
  *pos = reinterpret_cast<PSI_pos *>(&(handle->current_pos));
  return (PSI_table_handle *)handle;
}

void Pfs_table_communication_information::close_table(
    PSI_table_handle *handle) {
  Replication_group_communication_information_table_handle *t =
      (Replication_group_communication_information_table_handle *)handle;
  for (auto &it : t->row.found_preferred_leaders) delete it;
  for (auto &it : t->row.found_actual_leaders) delete it;
  delete t;
}

bool Pfs_table_communication_information::deinit() { return false; }

bool Pfs_table_communication_information::init() {
  m_share.m_table_name = "replication_group_communication_information";
  m_share.m_table_name_length = ::strlen(m_share.m_table_name);
  m_share.m_table_definition =
      "WRITE_CONCURRENCY BIGINT unsigned not null, "
      "PROTOCOL_VERSION LONGTEXT not null, "
      "WRITE_CONSENSUS_LEADERS_PREFERRED LONGTEXT not null, "
      "WRITE_CONSENSUS_LEADERS_ACTUAL LONGTEXT not null, "
      "WRITE_CONSENSUS_SINGLE_LEADER_CAPABLE BOOLEAN not null COMMENT 'What "
      "the option group_replication_paxos_single_leader was set to at the time "
      "this member joined the group. '";
  m_share.m_ref_length =
      sizeof Replication_group_communication_information_table_handle::
          current_pos;
  m_share.m_acl = READONLY;
  m_share.get_row_count = Pfs_table_communication_information::get_row_count;

  /* Initialize PFS_engine_table_proxy */
  m_share.m_proxy_engine_table = {
      Pfs_table_communication_information::rnd_next,
      Pfs_table_communication_information::rnd_init,
      Pfs_table_communication_information::rnd_pos,
      nullptr,  // index_init,
      nullptr,  // index_read,
      nullptr,  // index_next,
      Pfs_table_communication_information::read_column_value,
      Pfs_table_communication_information::reset_position,
      nullptr,  // write_column_value,
      nullptr,  // write_row_values,
      nullptr,  // update_column_value,
      nullptr,  // update_row_values,
      nullptr,  // delete_row_values,
      Pfs_table_communication_information::open_table,
      Pfs_table_communication_information::close_table};
  return false;
}

}  // namespace perfschema
}  // namespace gr
