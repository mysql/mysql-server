/* Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

/**
  @file storage/perfschema/pfs_defaults.cc
  Default setup (implementation).
*/

#include "storage/perfschema/pfs_defaults.h"

#include <stddef.h>

#include "storage/perfschema/pfs.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_setup_actor.h"
#include "storage/perfschema/pfs_setup_object.h"

void install_default_setup(PSI_thread_bootstrap *thread_boot) {
  void *service = thread_boot->get_interface(PSI_CURRENT_THREAD_VERSION);
  if (service == nullptr) {
    return;
  }

#ifdef HAVE_PSI_THREAD_INTERFACE
  static PSI_thread_key thread_key;
  static PSI_thread_info thread_info = {&thread_key, "setup",
                                        "pfs_setup", PSI_FLAG_SINGLETON,
                                        0,           PSI_DOCUMENT_ME};

  const char *pfs_category = "performance_schema";

  PSI_thread_service_t *psi = (PSI_thread_service_t *)service;

  psi->register_thread(pfs_category, &thread_info, 1);
  PSI_thread *psi_thread = psi->new_thread(thread_key, 0, nullptr, 0);

  if (psi_thread != nullptr) {
    /* LF_HASH needs a thread, for PINS */
    psi->set_thread(psi_thread);

    PFS_user_name any_user;
    PFS_host_name any_host;
    PFS_role_name any_role;
    PFS_schema_name any_schema;
    PFS_object_name any_table_object;
    PFS_object_name any_routine_object;
    PFS_schema_name mysql_db;
    PFS_schema_name ps_db;
    PFS_schema_name is_db;
    any_user.set("%", 1);
    any_host.set("%", 1);
    any_role.set("%", 1);
    any_schema.set("%", 1);
    any_table_object.set_as_table("%", 1);
    any_routine_object.set_as_routine("%", 1);
    mysql_db.set("mysql", 5);
    ps_db.set("performance_schema", 18);
    is_db.set("information_schema", 18);

    /* Enable all users on all hosts by default */
    insert_setup_actor(&any_user, &any_host, &any_role, true, true);

    /* Disable sp by default in mysql. */
    insert_setup_object(OBJECT_TYPE_EVENT, &mysql_db, &any_routine_object,
                        false, false);
    /* Disable sp in performance/information schema. */
    insert_setup_object(OBJECT_TYPE_EVENT, &ps_db, &any_routine_object, false,
                        false);
    insert_setup_object(OBJECT_TYPE_EVENT, &is_db, &any_routine_object, false,
                        false);
    /* Enable every other sp. */
    insert_setup_object(OBJECT_TYPE_EVENT, &any_schema, &any_routine_object,
                        true, true);

    /* Disable sp by default in mysql. */
    insert_setup_object(OBJECT_TYPE_FUNCTION, &mysql_db, &any_routine_object,
                        false, false);
    /* Disable sp in performance/information schema. */
    insert_setup_object(OBJECT_TYPE_FUNCTION, &ps_db, &any_routine_object,
                        false, false);
    insert_setup_object(OBJECT_TYPE_FUNCTION, &is_db, &any_routine_object,
                        false, false);
    /* Enable every other sp. */
    insert_setup_object(OBJECT_TYPE_FUNCTION, &any_schema, &any_routine_object,
                        true, true);

    /* Disable sp by default in mysql. */
    insert_setup_object(OBJECT_TYPE_PROCEDURE, &mysql_db, &any_routine_object,
                        false, false);
    /* Disable sp in performance/information schema. */
    insert_setup_object(OBJECT_TYPE_PROCEDURE, &ps_db, &any_routine_object,
                        false, false);
    insert_setup_object(OBJECT_TYPE_PROCEDURE, &is_db, &any_routine_object,
                        false, false);
    /* Enable every other sp. */
    insert_setup_object(OBJECT_TYPE_PROCEDURE, &any_schema, &any_routine_object,
                        true, true);

    /* Disable system tables by default */
    insert_setup_object(OBJECT_TYPE_TABLE, &mysql_db, &any_table_object, false,
                        false);
    /* Disable performance/information schema tables. */
    insert_setup_object(OBJECT_TYPE_TABLE, &ps_db, &any_table_object, false,
                        false);
    insert_setup_object(OBJECT_TYPE_TABLE, &is_db, &any_table_object, false,
                        false);
    /* Enable every other tables */
    insert_setup_object(OBJECT_TYPE_TABLE, &any_schema, &any_table_object, true,
                        true);

    /* Disable sp by default in mysql. */
    insert_setup_object(OBJECT_TYPE_TRIGGER, &mysql_db, &any_routine_object,
                        false, false);
    /* Disable sp in performance/information schema. */
    insert_setup_object(OBJECT_TYPE_TRIGGER, &ps_db, &any_routine_object, false,
                        false);
    insert_setup_object(OBJECT_TYPE_TRIGGER, &is_db, &any_routine_object, false,
                        false);
    /* Enable every other sp. */
    insert_setup_object(OBJECT_TYPE_TRIGGER, &any_schema, &any_routine_object,
                        true, true);
  }

  psi->delete_current_thread();
#endif /* HAVE_PSI_THREAD_INTERFACE */
}
