/* Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

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
  if (service == NULL) {
    return;
  }

#ifdef HAVE_PSI_THREAD_INTERFACE
  static PSI_thread_key thread_key;
  static PSI_thread_info thread_info = {&thread_key, "setup",
                                        PSI_FLAG_SINGLETON, 0, PSI_DOCUMENT_ME};

  const char *pfs_category = "performance_schema";

  PSI_thread_service_t *psi = (PSI_thread_service_t *)service;

  psi->register_thread(pfs_category, &thread_info, 1);
  PSI_thread *psi_thread = psi->new_thread(thread_key, NULL, 0);

  if (psi_thread != NULL) {
    /* LF_HASH needs a thread, for PINS */
    psi->set_thread(psi_thread);

    String percent("%", 1, &my_charset_utf8mb4_bin);
    /* Enable all users on all hosts by default */
    insert_setup_actor(&percent, &percent, &percent, true, true);

    String mysql_db("mysql", 5, &my_charset_utf8mb4_bin);
    String PS_db("performance_schema", 18, &my_charset_utf8mb4_bin);
    String IS_db("information_schema", 18, &my_charset_utf8mb4_bin);

    /* Disable sp by default in mysql. */
    insert_setup_object(OBJECT_TYPE_EVENT, &mysql_db, &percent, false, false);
    /* Disable sp in performance/information schema. */
    insert_setup_object(OBJECT_TYPE_EVENT, &PS_db, &percent, false, false);
    insert_setup_object(OBJECT_TYPE_EVENT, &IS_db, &percent, false, false);
    /* Enable every other sp. */
    insert_setup_object(OBJECT_TYPE_EVENT, &percent, &percent, true, true);

    /* Disable sp by default in mysql. */
    insert_setup_object(OBJECT_TYPE_FUNCTION, &mysql_db, &percent, false,
                        false);
    /* Disable sp in performance/information schema. */
    insert_setup_object(OBJECT_TYPE_FUNCTION, &PS_db, &percent, false, false);
    insert_setup_object(OBJECT_TYPE_FUNCTION, &IS_db, &percent, false, false);
    /* Enable every other sp. */
    insert_setup_object(OBJECT_TYPE_FUNCTION, &percent, &percent, true, true);

    /* Disable sp by default in mysql. */
    insert_setup_object(OBJECT_TYPE_PROCEDURE, &mysql_db, &percent, false,
                        false);
    /* Disable sp in performance/information schema. */
    insert_setup_object(OBJECT_TYPE_PROCEDURE, &PS_db, &percent, false, false);
    insert_setup_object(OBJECT_TYPE_PROCEDURE, &IS_db, &percent, false, false);
    /* Enable every other sp. */
    insert_setup_object(OBJECT_TYPE_PROCEDURE, &percent, &percent, true, true);

    /* Disable system tables by default */
    insert_setup_object(OBJECT_TYPE_TABLE, &mysql_db, &percent, false, false);
    /* Disable performance/information schema tables. */
    insert_setup_object(OBJECT_TYPE_TABLE, &PS_db, &percent, false, false);
    insert_setup_object(OBJECT_TYPE_TABLE, &IS_db, &percent, false, false);
    /* Enable every other tables */
    insert_setup_object(OBJECT_TYPE_TABLE, &percent, &percent, true, true);

    /* Disable sp by default in mysql. */
    insert_setup_object(OBJECT_TYPE_TRIGGER, &mysql_db, &percent, false, false);
    /* Disable sp in performance/information schema. */
    insert_setup_object(OBJECT_TYPE_TRIGGER, &PS_db, &percent, false, false);
    insert_setup_object(OBJECT_TYPE_TRIGGER, &IS_db, &percent, false, false);
    /* Enable every other sp. */
    insert_setup_object(OBJECT_TYPE_TRIGGER, &percent, &percent, true, true);
  }

  psi->delete_current_thread();
#endif /* HAVE_PSI_THREAD_INTERFACE */
}
