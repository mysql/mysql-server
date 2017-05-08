/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GROUP_MEMBERSHIP_LISTENER_H
#define GROUP_MEMBERSHIP_LISTENER_H

#include <mysql/components/service.h>

/**
  A service that listens for notifications about view changes or
  quorum loss.
*/
BEGIN_SERVICE_DEFINITION(group_membership_listener)
  /**
    This function SHALL be called whenever there is a new view
    installed.

    The implementation SHALL consume the notification and
    return false on success, true on failure.

    The implementation MUST NOT block the caller. It MUST
    handle the notification quickly or enqueue it and deal
    with it asynchronously.

    @param view_id The view identifier. This must be copied
                   if the string must outlive the notification
                   lifecycle.

    @return false success, true on failure.
  */
  DECLARE_BOOL_METHOD(notify_view_change, (const char* view_id));

  /**
    This function SHALL be called whenever the state of a member
    changes to UNREACHABLE and that makes the system block.

    The implementation SHALL consume the notification and
    return false on success, true on failure.

    The implementation MUST NOT block the caller. It MUST
    handle the notification quickly or enqueue it and deal
    with it asynchronously.

    @param view_id The view identifier. This must be copied
                   if the string must outlive the notification
                   lifecycle.

    @return false success, true on failure.
  */
  DECLARE_BOOL_METHOD(notify_quorum_loss, (const char* view_id));

END_SERVICE_DEFINITION(group_membership_listener)

#endif /* GROUP_MEMBERSHIP_LISTENER_H */

