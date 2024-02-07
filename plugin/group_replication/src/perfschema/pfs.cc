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

#include "plugin/group_replication/include/perfschema/pfs.h"
#include "mysql/components/my_service.h"
#include "plugin/group_replication/include/perfschema/table_communication_information.h"
#include "plugin/group_replication/include/perfschema/table_replication_group_configuration_version.h"
#include "plugin/group_replication/include/perfschema/table_replication_group_member_actions.h"
#include "plugin/group_replication/include/perfschema/utilities.h"

namespace gr {
namespace perfschema {

bool Perfschema_module::register_pfs_tables(Pfs_tables &tables) {
  Registry_guard guard;
  if (guard.get_registry() == nullptr) return true;

  my_service<SERVICE_TYPE(pfs_plugin_table_v1)> reg("pfs_plugin_table_v1",
                                                    guard.get_registry());
  std::vector<PFS_engine_table_share_proxy *> shares;

  for (auto &table : tables) shares.push_back(table->get_share());

  if (!reg.is_valid() || reg->add_tables(&shares[0], shares.size()))
    return true; /* purecov: inspected */

  return false;
}

bool Perfschema_module::unregister_pfs_tables(Pfs_tables &tables) {
  Registry_guard guard;
  if (guard.get_registry() == nullptr) return true;

  my_service<SERVICE_TYPE(pfs_plugin_table_v1)> reg("pfs_plugin_table_v1",
                                                    guard.get_registry());
  std::vector<PFS_engine_table_share_proxy *> shares;

  for (auto &table : tables) shares.push_back(table->get_share());

  if (!reg.is_valid() || reg->delete_tables(&shares[0], shares.size()))
    return true; /* purecov: inspected */

  return false;
}

bool Perfschema_module::initialize() {
  auto table_replication_group_configuration_version =
      std::make_unique<Pfs_table_replication_group_configuration_version>();
  table_replication_group_configuration_version->init();
  m_tables.push_back(std::move(table_replication_group_configuration_version));

  auto table_replication_group_member_actions =
      std::make_unique<Pfs_table_replication_group_member_actions>();
  table_replication_group_member_actions->init();
  m_tables.push_back(std::move(table_replication_group_member_actions));

  auto table_replication_communication_information =
      std::make_unique<Pfs_table_communication_information>();
  table_replication_communication_information->init();
  m_tables.push_back(std::move(table_replication_communication_information));

  // Register all tables in one go.
  if (register_pfs_tables(m_tables)) {
    /* purecov: begin inspected */
    for (auto &next : m_tables) {
      next->deinit();
    }
    m_tables.clear();

    return true;
    /* purecov: end */
  }
  return false;
}

bool Perfschema_module::finalize() {
  unregister_pfs_tables(m_tables);

  for (auto &next : m_tables) {
    next->deinit();
  }
  m_tables.clear();

  return false;
}

}  // namespace perfschema
}  // namespace gr
