/*
   Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_GLOBAL_SCHEMA_LOCK_GUARD_H
#define NDB_GLOBAL_SCHEMA_LOCK_GUARD_H

#include <mysql/plugin.h>

class Ndb_global_schema_lock_guard
{
public:
  Ndb_global_schema_lock_guard(THD *thd);
  ~Ndb_global_schema_lock_guard();
  int lock(bool no_lock_queue=false,
           bool report_cluster_disconnected=true);
private:
  THD* m_thd;
  bool m_locked;
};

#endif

