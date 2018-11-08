/*
   Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "sql/ndb_thd.h"

#include "my_dbug.h"
#include "mysql/thread_type.h"
#include "sql/ndb_thd_ndb.h"
#include "sql/sql_class.h"

/*
  Make sure THD has a Thd_ndb struct allocated and associated

  - validate_ndb, check if the Ndb object need to be recycled
*/

Ndb* check_ndb_in_thd(THD* thd, bool validate_ndb)
{
  Thd_ndb *thd_ndb= get_thd_ndb(thd);
  if (!thd_ndb)
  {
    if (!(thd_ndb= Thd_ndb::seize(thd)))
      return NULL;
    thd_set_thd_ndb(thd, thd_ndb);
  }

  else if (validate_ndb && !thd_ndb->valid_ndb())
  {
    if (!thd_ndb->recycle_ndb())
      return NULL;
  }

  DBUG_ASSERT(thd_ndb->is_slave_thread() == thd->slave_thread);

  return thd_ndb->ndb;
}


bool
applying_binlog(const THD* thd)
{
  if (thd->slave_thread)
  {
    DBUG_PRINT("info", ("THD is slave thread"));
    return true;
  }

  if (thd->rli_fake)
  {
    /*
      Thread is in "pseudo_slave_mode" which is entered implicitly when the
      first BINLOG statement is executed (see 'mysql_client_binlog_statement')
      and explicitly ended when SET @pseudo_slave_mode=0 is finally executed.
    */
    DBUG_PRINT("info", ("THD is in pseduo slave mode"));
    return true;
  }

  return false;
}

extern ulong opt_server_id_mask;

uint32
thd_unmasked_server_id(const THD* thd)
{
  const uint32 unmasked_server_id = thd->unmasked_server_id;
  assert(thd->server_id == (thd->unmasked_server_id & opt_server_id_mask));
  return unmasked_server_id;
}

const char* ndb_thd_query(const THD* thd) { return thd->query().str; }

size_t ndb_thd_query_length(const THD* thd) { return thd->query().length; }

bool ndb_thd_is_binlog_thread(const THD* thd)
{
  return thd->system_thread == SYSTEM_THREAD_NDBCLUSTER_BINLOG;
}

bool ndb_thd_is_background_thread(const THD* thd)
{
  return thd->system_thread == SYSTEM_THREAD_BACKGROUND;
}
