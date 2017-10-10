/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_PLUGIN_REFERENCE_H
#define NDB_PLUGIN_REFERENCE_H

#include "sql_plugin_ref.h"

/*
  RAII style class for locking the "ndbcluster plugin" and accessing
  it's handle
*/

class Ndb_plugin_reference
{
  plugin_ref plugin;
public:
  Ndb_plugin_reference();

  bool lock();
  st_plugin_int* handle() const;
  ~Ndb_plugin_reference();
};

#endif
