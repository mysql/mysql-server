/*
 Copyright (c) 2011 Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

#ifndef NDBMEMCACHE_EXPIRETIME_H
#define NDBMEMCACHE_EXPIRETIME_H

#include "workitem.h"
#include "Operation.h"

class ExpireTime {
public:
  ExpireTime(workitem *);
  bool stored_item_has_expired(Operation &);

  workitem *item;
  rel_time_t current_time;
  rel_time_t ndb_expire_time;
  rel_time_t local_cache_expire_time;
  bool is_expired;
};


#endif

