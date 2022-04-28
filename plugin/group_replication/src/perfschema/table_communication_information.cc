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
#include "plugin/group_replication/include/mysql_version_gcs_protocol_map.h"
#include "plugin/group_replication/include/perfschema/table_communication_information.h"
#include "plugin/group_replication/include/perfschema/utilities.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_types.h"

extern Gcs_operations *gcs_module;
extern Group_member_info_manager_interface *group_member_mgr;

namespace gr {
namespace perfschema {
namespace pfs_table_communication_information {

static constexpr int SUCCESS = 0;

struct dummy_table_handle_type {};
static dummy_table_handle_type dummy_table_handle{};

static constexpr unsigned long long NR_ROWS{1};
static unsigned long long s_current_pos{0};
static uint32_t s_write_concurrency{0};
static Member_version s_mysql_version{0x00000};

/**
  Fetch preferred leaders instance.

  @return Reference to the Group_member_info_list instance.
*/
Group_member_info_list &get_preferred_leaders() {
  static Group_member_info_list s_preferred_leaders(
      (Malloc_allocator<Group_member_info *>(key_group_member_info)));
  return s_preferred_leaders;
}

/**
  Fetch actual leaders instance.
  The actual leaders are members which is carrying out leader at this moment.

  @return Reference to the Group_member_info_list instance.
*/
Group_member_info_list &get_actual_leaders() {
  static Group_member_info_list s_actual_leaders(
      (Malloc_allocator<Group_member_info *>(key_group_member_info)));
  return s_actual_leaders;
}

static bool fetch_row_data() {
  bool constexpr ERROR = true;
  bool constexpr OK = false;

  if (gcs_module == nullptr || group_member_mgr == nullptr) {
    return ERROR;
  }

  enum enum_gcs_error error_code =
      gcs_module->get_write_concurrency(s_write_concurrency);
  if (error_code != GCS_OK) {
    return ERROR;
  }

  Gcs_protocol_version const gcs_version = gcs_module->get_protocol_version();
  if (gcs_version == Gcs_protocol_version::UNKNOWN) {
    return ERROR;
  }
  s_mysql_version = convert_to_mysql_version(gcs_version);

  std::vector<Gcs_member_identifier> preferred_leaders;
  std::vector<Gcs_member_identifier> actual_leaders;
  error_code = gcs_module->get_leaders(preferred_leaders, actual_leaders);
  if (error_code != GCS_OK) {
    return ERROR;
  }

  Group_member_info_list found_preferred_leaders(
      (Malloc_allocator<Group_member_info *>(key_group_member_info)));
  for (const auto &preferred_leader : preferred_leaders) {
    auto member_id =
        group_member_mgr->get_group_member_info_by_member_id(preferred_leader);
    if (member_id) found_preferred_leaders.emplace_back(member_id);
  }
  get_preferred_leaders() = found_preferred_leaders;

  Group_member_info_list found_actual_leaders(
      (Malloc_allocator<Group_member_info *>(key_group_member_info)));
  for (const auto &actual_leader : actual_leaders) {
    auto member_id =
        group_member_mgr->get_group_member_info_by_member_id(actual_leader);
    if (member_id) found_actual_leaders.emplace_back(member_id);
  }
  get_actual_leaders() = found_actual_leaders;

  return OK;
}

static unsigned long long get_row_count() { return NR_ROWS; }

static int rnd_next(PSI_table_handle *handle MY_ATTRIBUTE((unused))) {
  if (s_current_pos >= NR_ROWS) {
    return PFS_HA_ERR_END_OF_FILE;
  }

  bool const error = fetch_row_data();
  if (error) {
    // Is there a more suitable error code?
    return PFS_HA_ERR_END_OF_FILE;
  }

  s_current_pos++;

  return SUCCESS;
}

static int rnd_init(PSI_table_handle *handle MY_ATTRIBUTE((unused)),
                    bool scan MY_ATTRIBUTE((unused))) {
  return SUCCESS;
}

static int rnd_pos(PSI_table_handle *handle MY_ATTRIBUTE((unused))) {
  return SUCCESS;
}

static void reset_position(PSI_table_handle *handle MY_ATTRIBUTE((unused))) {
  s_current_pos = 0;
}

static int read_column_value(PSI_table_handle *handle MY_ATTRIBUTE((unused)),
                             PSI_field *field,
                             unsigned int index MY_ATTRIBUTE((unused))) {
  Registry_guard guard;
  my_service<SERVICE_TYPE(pfs_plugin_table)> table_service{
      "pfs_plugin_table", guard.get_registry()};

  switch (index) {
    case 0: {  // WRITE_CONCURRENCY
      table_service->set_field_bigint(field, {s_write_concurrency, false});
      break;
    }
    case 1: {  // PROTOCOL_VERSION
      table_service->set_field_blob(
          field, s_mysql_version.get_version_string().c_str(),
          s_mysql_version.get_version_string().size());
      break;
    }
    case 2: {  // WRITE_CONSENSUS_LEADERS_PREFERRED
      std::stringstream ss;
      for (std::size_t i = 0; i < get_preferred_leaders().size(); i++) {
        ss << get_preferred_leaders().at(i)->get_uuid();
        if (i < get_preferred_leaders().size() - 1) {
          ss << ',';
        }
      }
      table_service->set_field_blob(field, ss.str().c_str(), ss.str().size());
      break;
    }
    case 3: {  // WRITE_CONSENSUS_LEADERS_ACTUAL
      std::stringstream ss;
      for (std::size_t i = 0; i < get_actual_leaders().size(); i++) {
        ss << get_actual_leaders().at(i)->get_uuid();
        if (i < get_actual_leaders().size() - 1) {
          ss << ',';
        }
      }
      table_service->set_field_blob(field, ss.str().c_str(), ss.str().size());
      break;
    }
  }
  return 0;
}

static PSI_table_handle *open_table(PSI_pos **pos MY_ATTRIBUTE((unused))) {
  auto *dummy = reinterpret_cast<PSI_table_handle *>(&dummy_table_handle);
  reset_position(dummy);
  *pos = reinterpret_cast<PSI_pos *>(&s_current_pos);
  return dummy;
}

static void close_table(PSI_table_handle *handle MY_ATTRIBUTE((unused))) {
  for (auto &it : get_preferred_leaders()) delete it;
  get_preferred_leaders().clear();

  for (auto &it : get_actual_leaders()) delete it;
  get_actual_leaders().clear();
}

}  // namespace pfs_table_communication_information

bool Pfs_table_communication_information::deinit() { return false; }

bool Pfs_table_communication_information::init() {
  m_share.m_table_name = "replication_group_communication_information";
  m_share.m_table_name_length = ::strlen(m_share.m_table_name);
  m_share.m_table_definition =
      "WRITE_CONCURRENCY BIGINT unsigned not null, "
      "PROTOCOL_VERSION LONGTEXT not null, "
      "WRITE_CONSENSUS_LEADERS_PREFERRED LONGTEXT not null, "
      "WRITE_CONSENSUS_LEADERS_ACTUAL LONGTEXT not null";
  m_share.m_ref_length =
      sizeof pfs_table_communication_information::s_current_pos;
  m_share.m_acl = READONLY;
  m_share.get_row_count = pfs_table_communication_information::get_row_count;

  /* Initialize PFS_engine_table_proxy */
  m_share.m_proxy_engine_table = {
      pfs_table_communication_information::rnd_next,
      pfs_table_communication_information::rnd_init,
      pfs_table_communication_information::rnd_pos,
      nullptr,  // index_init,
      nullptr,  // index_read,
      nullptr,  // index_next,
      pfs_table_communication_information::read_column_value,
      pfs_table_communication_information::reset_position,
      nullptr,  // write_column_value,
      nullptr,  // write_row_values,
      nullptr,  // update_column_value,
      nullptr,  // update_row_values,
      nullptr,  // delete_row_values,
      pfs_table_communication_information::open_table,
      pfs_table_communication_information::close_table};
  return false;
}

}  // namespace perfschema
}  // namespace gr
