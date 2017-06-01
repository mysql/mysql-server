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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#ifndef PFS_NOTIFY_SERVICE_H
#define PFS_NOTIFY_SERVICE_H

#include <mysql/components/service.h>
#include <mysql/psi/mysql_thread.h>

/**
  @page PAGE_COMPONENTS_PFS_SERVICE Performance Schema Notification service
  @section Introduction
*/

BEGIN_SERVICE_DEFINITION(pfs_notification)
  register_notification_v1_t register_notification;
  unregister_notification_v1_t unregister_notification;
END_SERVICE_DEFINITION(pfs_notification)

#endif
