/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "my_dbug.h"
#include "ndb_thd.h"
#include "ndb_thd_ndb.h"

#include <sql_class.h>
#include "mysqld.h"         // opt_server_id_mask

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


uint32
thd_unmasked_server_id(const THD* thd)
{
  const uint32 unmasked_server_id = thd->unmasked_server_id;
  assert(thd->server_id == (thd->unmasked_server_id & opt_server_id_mask));
  return unmasked_server_id;
}
