/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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
