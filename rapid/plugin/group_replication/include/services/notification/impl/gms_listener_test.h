/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

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
class group_membership_listener_example_impl
{
public:
  /**
  @c notify_view_change(const char*)
  */
  static DEFINE_BOOL_METHOD(notify_view_change, (const char*));

  /**
  @c notify_quorum_lost(const char*)
  */
  static DEFINE_BOOL_METHOD(notify_quorum_lost, (const char*));

};

/**
  An example implementation of the group_member_status_listener
  service. It is actually used for testing purposes.
*/
class group_member_status_listener_example_impl
{
public:
  /**
  @c notify_member_role_change(const char*)
  */
  static DEFINE_BOOL_METHOD(notify_member_role_change, (const char*));

  /**
  @c notify_member_state_change(const char*)
  */
  static DEFINE_BOOL_METHOD(notify_member_state_change, (const char*));

};

#endif /* GMS_LISTENER_TEST_H */

