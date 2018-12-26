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
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "NdbInstance.h"
#include "debug.h"

/* ------------------------------------------
   ------------- NdbInstance ----------------
   ------------------------------------------ */
 
NdbInstance::NdbInstance(Ndb_cluster_connection *c, int ntransactions) :
  next(0), wqitem(0), ndb_owner(false)
{
  if(c) {
    ndb_owner = true;
    db = new Ndb(c);
    db->init(ntransactions);
    db->setCustomData(this);
  }
  else {
    /* "dummy" NdbInstance used as a placeholder in linked lists */
    db = 0;
  }
}


NdbInstance::NdbInstance(Ndb *_db, workitem *_item) :
  db(_db), wqitem(_item), ndb_owner(false)
{
  wqitem->ndb_instance = this;
}


NdbInstance::~NdbInstance() {
  if(ndb_owner) delete db;
}

