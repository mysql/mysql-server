/*
 Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.
 
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
#ifndef NDBMEMCACHE_NDBINSTANCE_H
#define NDBMEMCACHE_NDBINSTANCE_H

#ifndef __cplusplus
#error "This file is for C++ only"
#endif

#include <pthread.h>
#include "NdbApi.hpp"

#include "ndbmemcache_config.h"
#include "KeyPrefix.h"
#include "QueryPlan.h"
#include "workitem.h"

#define VPSZ sizeof(void *)
#define TOTAL_SZ ((3 * VPSZ) + sizeof(int))
#define PADDING (64 - TOTAL_SZ)


class NdbInstance {
public:
  /* Public Methods */
  NdbInstance(Ndb_cluster_connection *, int);
  NdbInstance(Ndb *, workitem *);
  ~NdbInstance();
  void link_workitem(workitem *);
  void unlink_workitem(workitem *);

  /* Public Instance Variables */  
  int id;
  Ndb *db;
  NdbInstance *next;
  workitem *wqitem;
  bool ndb_owner;

private:
  char cache_line_padding[PADDING];
};


inline void NdbInstance::link_workitem(workitem *item) {
  assert(item->ndb_instance == NULL);
  assert(wqitem == NULL);
  
  item->ndb_instance = this;
  wqitem = item;
}


inline void NdbInstance::unlink_workitem(workitem *item) {
  assert(wqitem == item);
  wqitem->ndb_instance = NULL;
  wqitem = NULL;
}


#endif
