/* Copyright (c) 2009, 2013, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/pfs_check.cc
  Check the performance schema table structure.
  The code in this file is implemented in pfs_check.cc
  instead of pfs_server.cc, to separate dependencies to server
  structures (THD, ...) in a dedicated file.
  This code organization helps a lot maintenance of the unit tests.
*/

#include "my_global.h"
#include "pfs_server.h"
#include "pfs_engine_table.h"

/*
*/

/**
  Check that the performance schema tables
  have the expected structure.
  Discrepancies are written in the server log,
  but are not considered fatal, so this function does not
  return an error code:
  - some differences are compatible, and should not cause a failure
  - some differences are not compatible, but then the DBA needs an operational
  server to be able to DROP+CREATE the tables with the proper structure,
  as part of the initial server installation or during an upgrade.
  In case of discrepancies, later attempt to perform DML against
  the performance schema will be rejected with an error.
*/
void check_performance_schema()
{
  DBUG_ENTER("check_performance_schema");

  THD *thd= new THD();
  if (thd == NULL)
    DBUG_VOID_RETURN;

  thd->thread_stack= (char*) &thd;
  thd->store_globals();

  PFS_engine_table_share::check_all_tables(thd);

  thd->restore_globals();
  delete thd;
  DBUG_VOID_RETURN;
}

