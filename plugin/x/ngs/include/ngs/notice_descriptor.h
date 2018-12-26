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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_NOTICE_DESCRIPTOR_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_NOTICE_DESCRIPTOR_H_

#include <array>
#include <map>
#include <string>

namespace ngs {

/**
  Internal identifiers for notices

  Purpose of this enum is to group all identifiers for configurable notices.
  Configurable means that user can request to receive an report about an
  event, where notice is the report.
*/
enum class Notice_type {
  k_warning = 0,
  k_group_replication_quorum_loss,
  k_group_replication_view_changed,
  k_group_replication_member_role_changed,
  k_group_replication_member_state_changed,
  k_xplugin_deprecation,
  k_last_element
};

/**
  Structure that describes a notice
 */
struct Notice_descriptor {
  /**
    Notice identifiers which can be dispatched

    Array containing global notices that can be dispatched by broker
    and placed inside per session queue, which later on will be delivered
    the client.
  */
  const static std::array<Notice_type, 4> dispatchables;

  Notice_descriptor(const Notice_type notice_type, const std::string &payload);

  Notice_type m_notice_type;
  std::string m_payload;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_NOTICE_DESCRIPTOR_H_
