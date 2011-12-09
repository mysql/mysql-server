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

#include "ndb_thd.h"
#include "ndb_thd_ndb.h"


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
    if (!thd_ndb->recycle_ndb(thd))
      return NULL;
  }

  return thd_ndb->ndb;
}
