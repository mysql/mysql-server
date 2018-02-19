/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <string>

/**
  A convenience context class used to share information between
  the event handlers and the notifier. It contains flags stating
  which notifications should be triggered and also the view
  identifier.
 */
class Notification_context {
 private:
  bool m_member_role_changed;
  bool m_member_state_changed;
  bool m_quorum_lost;
  bool m_view_changed;
  std::string m_view_id;

 public:
  Notification_context()
      : m_member_role_changed(false),
        m_member_state_changed(false),
        m_quorum_lost(false),
        m_view_changed(false),
        m_view_id("") {}

  void reset() {
    m_member_role_changed = false;
    m_member_state_changed = false;
    m_view_changed = false;
    m_quorum_lost = false;
  }
  void set_member_role_changed() { m_member_role_changed = true; }
  void set_member_state_changed() { m_member_state_changed = true; }
  void set_quorum_lost() { m_quorum_lost = true; }
  void set_view_changed() { m_view_changed = true; }
  void set_view_id(const std::string &v) { m_view_id.assign(v); }

  bool get_member_role_changed() const { return m_member_role_changed; }
  bool get_member_state_changed() const { return m_member_state_changed; }
  bool get_quorum_lost() const { return m_quorum_lost; }
  bool get_view_changed() const { return m_view_changed; }
  const std::string &get_view_id() const { return m_view_id; }
};

/**
  This function SHALL trigger the notifications based on the
  notification context provided as an argument. Different
  notifications SHALL be emitted depending on what is flagged.

  It calls out into those services that have registered
  themselves in the server service registry as listeners for
  the notifications such as view changes, or member state
  changes.

  If an error is returned it can be that part of the notifications
  have succeeded and part have not.

  @note The assumption is that this function is fast, i.e.,
  consumers SHALL implement a notification handling system
  that will release the caller thread immediately after they
  receive the notification.

  @param ctx The notification context.
 */
bool notify_and_reset_ctx(Notification_context &ctx);

#endif /* LISTENER_SERVICES_NOTIFICATION_H */
