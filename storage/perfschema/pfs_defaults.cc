/* Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file storage/perfschema/pfs_defaults.cc
  Default setup (implementation).
*/

#include "my_global.h"
#include "pfs.h"
#include "pfs_defaults.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "pfs_setup_actor.h"
#include "pfs_setup_object.h"

static PSI_thread_key thread_key;
static PSI_thread_info thread_info= { &thread_key, "setup", PSI_FLAG_GLOBAL };

const char* pfs_category= "performance_schema";

void install_default_setup(PSI_bootstrap *boot)
{
  PSI *psi= (PSI*) boot->get_interface(PSI_CURRENT_VERSION);
  if (psi == NULL)
    return;

  psi->register_thread(pfs_category, &thread_info, 1);
  PSI_thread *psi_thread= psi->new_thread(thread_key, NULL, 0);

  if (psi_thread != NULL)
  {
    /* LF_HASH needs a thread, for PINS */
    psi->set_thread(psi_thread);

    String percent("%", 1, &my_charset_utf8_bin);
    /* Enable all users on all hosts by default */
    insert_setup_actor(&percent, &percent, &percent, true, true);

    String mysql_db("mysql", 5, &my_charset_utf8_bin);
    String PS_db("performance_schema", 18, &my_charset_utf8_bin);
    String IS_db("information_schema", 18, &my_charset_utf8_bin);

    /* Disable sp by default in mysql. */
    insert_setup_object(OBJECT_TYPE_EVENT, &mysql_db, &percent, false, false);
    /* Disable sp in performance/information schema. */
    insert_setup_object(OBJECT_TYPE_EVENT, &PS_db, &percent, false, false);
    insert_setup_object(OBJECT_TYPE_EVENT, &IS_db, &percent, false, false);
    /* Enable every other sp. */
    insert_setup_object(OBJECT_TYPE_EVENT, &percent, &percent, true, true);

    /* Disable sp by default in mysql. */
    insert_setup_object(OBJECT_TYPE_FUNCTION, &mysql_db, &percent, false, false);
    /* Disable sp in performance/information schema. */
    insert_setup_object(OBJECT_TYPE_FUNCTION, &PS_db, &percent, false, false);
    insert_setup_object(OBJECT_TYPE_FUNCTION, &IS_db, &percent, false, false);
    /* Enable every other sp. */
    insert_setup_object(OBJECT_TYPE_FUNCTION, &percent, &percent, true, true);

    /* Disable sp by default in mysql. */
    insert_setup_object(OBJECT_TYPE_PROCEDURE, &mysql_db, &percent, false, false);
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
}

