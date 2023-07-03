/*
 * Copyright (c) 2018, 2022, Oracle and/or its affiliates.
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

#include "plugin/x/src/ngs/notice_descriptor.h"

#include <algorithm>
#include <array>

namespace ngs {

Notice_descriptor::Notice_descriptor(const Notice_type notice_type)
    : m_notice_type(notice_type) {}

Notice_descriptor::Notice_descriptor(const Notice_type notice_type,
                                     const std::string &payload)
    : m_notice_type(notice_type), m_payload(payload) {}

bool Notice_descriptor::is_dispatchable(const Notice_type notice_type) {
  /**
    Notice identifiers which can be dispatched

    Array containing global notices that can be dispatched by broker
    and placed inside per session queue, which later on will be delivered
    the client.
  */
  static const std::array<Notice_type, 5> k_dispatchables{{
      Notice_type::k_group_replication_quorum_loss,
      Notice_type::k_group_replication_view_changed,
      Notice_type::k_group_replication_member_role_changed,
      Notice_type::k_group_replication_member_state_changed,
      Notice_type::k_warning,
  }};

  return std::any_of(
      k_dispatchables.begin(), k_dispatchables.end(),
      [notice_type](const Notice_type type) { return notice_type == type; });
}

}  // namespace ngs
