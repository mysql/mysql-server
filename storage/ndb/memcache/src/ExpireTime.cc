/*
 Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.
 
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

#include "my_config.h"
#include <time.h>

#include "memcached/types.h"
#include "memcached/server_api.h"

#include <NdbApi.hpp>

#include "ExpireTime.h"
#include "Operation.h"


ExpireTime::ExpireTime(workitem *i) :
  item(i),
  ndb_expire_time(0),
  is_expired(false)
{
  SERVER_CORE_API * SERVER = item->pipeline->engine->server.core;
  current_time = SERVER->get_current_time();

  /*** NOTE regarding items read from NDB and then cached locally.
   *** Expire times in the local cache are currently HARD-CODED, HERE.
   *** Data retrieved from memory expires after 5 seconds.
   *** Data retrieved from disk expires after 300 seconds.
   ***/
  if(item->plan->hasDataOnDisk()) 
    local_cache_expire_time = current_time + 300;
  else
    local_cache_expire_time = current_time + 5;
}


/*  stored_item_has_expired() 
    If the timestamp is a MySQL 5.6 fractional-second timestamp,
    only the integer part is considered.
*/
bool ExpireTime::stored_item_has_expired(Operation &op) {  
  SERVER_CORE_API * SERVER = item->pipeline->engine->server.core;
  time_t stored_exptime;
  
  if(item->prefix_info.has_expire_col && ! op.isNull(COL_STORE_EXPIRES)) {
    stored_exptime = op.getIntValue(COL_STORE_EXPIRES);
    ndb_expire_time = SERVER->realtime(stored_exptime);

    if(ndb_expire_time > 0) {
      /* Has the item already expired? */
      if(ndb_expire_time < current_time)
        is_expired = true;
    
      /* Don't keep it in memory for longer than it is valid in the db */
      if(ndb_expire_time < local_cache_expire_time)
        local_cache_expire_time = ndb_expire_time;
    }
  }
  return is_expired;
}

