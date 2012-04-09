/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "ndb_thd_ndb.h"

/*
  Default value for max number of transactions createable against NDB from
  the handler. Should really be 2 but there is a transaction to much allocated
  when lock table is used, and one extra to used for global schema lock.
*/
static const int MAX_TRANSACTIONS= 4;


Thd_ndb*
Thd_ndb::seize(THD* thd)
{
  DBUG_ENTER("seize_thd_ndb");

  Thd_ndb* thd_ndb= new Thd_ndb(thd);
  if (thd_ndb == NULL)
    return NULL;

  if (thd_ndb->ndb->init(MAX_TRANSACTIONS) != 0)
  {
    DBUG_PRINT("error", ("Ndb::init failed, eror: %d  message: %s",
                         thd_ndb->ndb->getNdbError().code,
                         thd_ndb->ndb->getNdbError().message));
    
    delete thd_ndb;
    thd_ndb= NULL;
  }
  DBUG_RETURN(thd_ndb);
}


void
Thd_ndb::release(Thd_ndb* thd_ndb)
{
  DBUG_ENTER("release_thd_ndb");
  delete thd_ndb;
  DBUG_VOID_RETURN;
}


bool
Thd_ndb::recycle_ndb(THD* thd)
{
  DBUG_ENTER("recycle_ndb");
  DBUG_PRINT("enter", ("ndb: 0x%lx", (long)ndb));

  DBUG_ASSERT(global_schema_lock_trans == NULL);
  DBUG_ASSERT(trans == NULL);

  delete ndb;
  if ((ndb= new Ndb(connection, "")) == NULL)
  {
    DBUG_PRINT("error",("failed to allocate Ndb object"));
    DBUG_RETURN(false);
  }

  if (ndb->init(MAX_TRANSACTIONS) != 0)
  {
    delete ndb;
    ndb= NULL;
    DBUG_PRINT("error", ("Ndb::init failed, %d  message: %s",
                         ndb->getNdbError().code,
                         ndb->getNdbError().message));
    DBUG_RETURN(false);
  }
  DBUG_RETURN(true);
}


bool
Thd_ndb::valid_ndb(void)
{
  // The ndb object should be valid as long as a
  // global schema lock transaction is ongoing
  if (global_schema_lock_trans)
    return true;

  // The ndb object should be valid as long as a
  // transaction is ongoing
  if (trans)
    return true;

  if (unlikely(m_connect_count != connection->get_connect_count()))
    return false;

  return true;
}


void
Thd_ndb::init_open_tables()
{
  count= 0;
  m_error= FALSE;
  my_hash_reset(&open_tables);
}
