/* Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#ifndef GMS_LISTENER_TEST_H
#define GMS_LISTENER_TEST_H

#include <mysql/components/service_implementation.h>

#define GMS_LISTENER_EXAMPLE_NAME "group_membership_listener.gr_example"
#define GMST_LISTENER_EXAMPLE_NAME "group_member_status_listener.gr_example"

void register_listener_service_gr_example();
void unregister_listener_service_gr_example();

/**
  An example implementation of the group_membership_listener
  service. It is actually used for testing purposes.
*/
class group_membership_listener_example_impl {
 public:
  /**
  @c notify_view_change(const char*)
  */
  static DEFINE_BOOL_METHOD(notify_view_change, (const char *));

  /**
  @c notify_quorum_lost(const char*)
  */
  static DEFINE_BOOL_METHOD(notify_quorum_lost, (const char *));
};

/**
  An example implementation of the group_member_status_listener
  service. It is actually used for testing purposes.
*/
class group_member_status_listener_example_impl {
 public:
  /**
  @c notify_member_role_change(const char*)
  */
  static DEFINE_BOOL_METHOD(notify_member_role_change, (const char *));

  /**
  @c notify_member_state_change(const char*)
  */
  static DEFINE_BOOL_METHOD(notify_member_state_change, (const char *));
};

#endif /* GMS_LISTENER_TEST_H */
