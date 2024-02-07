/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SERVICE_PFS_NOTIFICATION_H
#define SERVICE_PFS_NOTIFICATION_H

struct PSI_notification_v3;

int pfs_register_notification(const struct PSI_notification_v3 *callbacks,
                              bool with_ref_count);
int pfs_unregister_notification(int handle);
void pfs_notify_thread_create(PSI_thread *thread);
void pfs_notify_thread_destroy(PSI_thread *thread);
void pfs_notify_session_connect(PSI_thread *thread);
void pfs_notify_session_disconnect(PSI_thread *thread);
void pfs_notify_session_change_user(PSI_thread *thread);

#endif
