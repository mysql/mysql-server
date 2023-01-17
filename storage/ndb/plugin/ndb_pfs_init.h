/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_PFS_INIT_H
#define NDB_PFS_INIT_H

#include "mysql/psi/mysql_memory.h"

/*
  @brief Acquire service handles and create proxy tables

  @return false on success, true on failure
*/
bool ndb_pfs_init();

/*
  @brief Release service handles and delete proxy tables

  @return void
*/
void ndb_pfs_deinit();

// Keys registered for instrumented memory
extern PSI_memory_key key_memory_thd_ndb_batch_mem_root;

#endif
