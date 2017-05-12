/*
   Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_THD_H
#define NDB_THD_H

#include "mysql/plugin.h"

extern handlerton* ndbcluster_hton;

  
/* Get Thd_ndb pointer from THD */
static inline
class Thd_ndb*
get_thd_ndb(THD* thd)
{
  return (class Thd_ndb *) thd_get_ha_data(thd, ndbcluster_hton);
}


/* Set Thd_ndb pointer for THD */
static inline
void
thd_set_thd_ndb(THD *thd, class Thd_ndb *thd_ndb)
{
  thd_set_ha_data(thd, ndbcluster_hton, thd_ndb);
}


/* Make sure THD has a Thd_ndb struct assigned */
class Ndb* check_ndb_in_thd(THD* thd, bool validate_ndb= false);

/*
  Determine if THD is applying binlog. ie. either marked as
  slave thread or being in "pseudo slave mode"
*/
bool
applying_binlog(const THD* thd);


/*
  Return the THD's unmasked server id
*/
uint32 thd_unmasked_server_id(const THD* thd);

#endif
