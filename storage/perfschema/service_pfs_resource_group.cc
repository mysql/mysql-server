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

/**
  @file storage/perfschema/service_pfs_resource_group.cc
  The performance schema implementation of the resource group service.
*/

#include <mysql/components/my_service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/plugin.h>

#include "storage/perfschema/pfs_server.h"
#include "storage/perfschema/pfs_services.h"
#include "template_utils.h"

extern int pfs_set_thread_resource_group_vc(const char *group_name,
                                            int group_name_len,
                                            void *user_data);
extern int pfs_set_thread_resource_group_by_id_vc(PSI_thread *thread,
                                                  ulonglong thread_id,
                                                  const char *group_name,
                                                  int group_name_len,
                                                  void *user_data);
extern int pfs_get_thread_system_attrs_vc(PSI_thread_attrs *thread_attrs);

extern int pfs_get_thread_system_attrs_by_id_vc(PSI_thread *thread,
                                                ulonglong thread_id,
                                                PSI_thread_attrs *thread_attrs);

int impl_pfs_set_thread_resource_group(const char *group_name,
                                       int group_name_len, void *user_data) {
  return pfs_set_thread_resource_group_vc(group_name, group_name_len,
                                          user_data);
}

int impl_pfs_set_thread_resource_group_by_id(PSI_thread *thread,
                                             ulonglong thread_id,
                                             const char *group_name,
                                             int group_name_len,
                                             void *user_data) {
  return pfs_set_thread_resource_group_by_id_vc(thread, thread_id, group_name,
                                                group_name_len, user_data);
}

int impl_pfs_get_thread_system_attrs(PSI_thread_attrs *thread_attrs) {
  return pfs_get_thread_system_attrs_vc(thread_attrs);
}

int impl_pfs_get_thread_system_attrs_by_id(PSI_thread *thread,
                                           ulonglong thread_id,
                                           PSI_thread_attrs *thread_attrs) {
  return pfs_get_thread_system_attrs_by_id_vc(thread, thread_id, thread_attrs);
}

SERVICE_TYPE(pfs_resource_group_v3)
SERVICE_IMPLEMENTATION(mysql_server, pfs_resource_group_v3) = {
    impl_pfs_set_thread_resource_group,
    impl_pfs_set_thread_resource_group_by_id, impl_pfs_get_thread_system_attrs,
    impl_pfs_get_thread_system_attrs_by_id};
