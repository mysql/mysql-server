/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "pfs.h"
#include "pfs_defaults.h"
#include "pfs_instr.h"
#include "pfs_setup_actor.h"

static PSI_thread_key key;
static PSI_thread_info info= { &key, "setup", PSI_FLAG_GLOBAL };

void install_default_setup(PSI_bootstrap *boot)
{
  PSI *psi= (PSI*) boot->get_interface(PSI_CURRENT_VERSION);
  if (psi == NULL)
    return;

  psi->register_thread("performance_schema", &info, 1);
  PSI_thread *psi_thread= psi->new_thread(key, NULL, 0);
  if (psi_thread == NULL)
    return;

  /* LF_HASH needs a thread, for PINS */
  psi->set_thread(psi_thread);

  String percent("%", 1, &my_charset_utf8_bin);
  /* Enable all users on all hosts by default */
  insert_setup_actor(&percent, &percent, &percent);

  psi->delete_current_thread();
}

