/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_MQ_NOTICE_CONFIGURATION_H_
#define PLUGIN_X_SRC_MQ_NOTICE_CONFIGURATION_H_

#include <algorithm>
#include <map>

#include "my_dbug.h"

#include "plugin/x/ngs/include/ngs/interface/notice_configuration_interface.h"
#include "plugin/x/ngs/include/ngs/notice_descriptor.h"

namespace xpl {

class Notice_configuration : public ngs::Notice_configuration_interface {
 public:
  using Notice_type = ::ngs::Notice_type;

 public:
  Notice_configuration() {
    set_notice(Notice_type::k_warning, true);
    set_notice(Notice_type::k_xplugin_deprecation, true);
  }

  bool get_name_by_notice_type(const Notice_type notice_type,
                               std::string *out_name) const override {
    const auto &notice_name_to_type = get_map_of_notice_names();

    for (const auto &entry : notice_name_to_type) {
      if (entry.second == notice_type) {
        *out_name = entry.first;
        return true;
      }
    }

    return false;
  }

  bool get_notice_type_by_name(const std::string &name,
                               Notice_type *out_notice_type) const override {
    const auto &notice_name_to_type = get_map_of_notice_names();
    const auto type = notice_name_to_type.find(name);

    if (notice_name_to_type.end() == type) return false;

    if (nullptr != out_notice_type) *out_notice_type = (*type).second;

    return true;
  }

  bool is_notice_enabled(const Notice_type notice_type) const override {
    return m_notices[static_cast<int>(notice_type)];
  }

  void set_notice(const Notice_type notice_type,
                  const bool should_be_enabled) override {
    DBUG_ASSERT(notice_type != Notice_type::k_last_element);
    m_notices[static_cast<int>(notice_type)] = should_be_enabled;

    m_is_dispatchable_enabled = std::any_of(
        ::ngs::Notice_descriptor::dispatchables.begin(),
        ::ngs::Notice_descriptor::dispatchables.end(),
        [this](const Notice_type type) { return is_notice_enabled(type); });
  }

  bool is_any_dispatchable_notice_enabled() const override {
    return m_is_dispatchable_enabled;
  }

 private:
  const std::map<std::string, Notice_type> &get_map_of_notice_names() const {
    const static std::map<std::string, Notice_type> notice_name_to_type{
        {"warnings", Notice_type::k_warning},
        {"group_replication/membership/quorum_loss",
         Notice_type::k_group_replication_quorum_loss},
        {"group_replication/membership/view",
         Notice_type::k_group_replication_view_changed},
        {"group_replication/status/role_change",
         Notice_type::k_group_replication_member_role_changed},
        {"group_replication/status/state_change",
         Notice_type::k_group_replication_member_state_changed}};

    return notice_name_to_type;
  }

  bool m_notices[(int)Notice_type::k_last_element]{false};
  bool m_is_dispatchable_enabled{false};
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_MQ_NOTICE_CONFIGURATION_H_
