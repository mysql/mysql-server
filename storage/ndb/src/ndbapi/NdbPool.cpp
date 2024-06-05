/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <Ndb.hpp>
#include <NdbPool.hpp>
#include "NdbPoolImpl.hpp"

static NdbPool *m_pool = nullptr;

bool create_instance(Ndb_cluster_connection *cc, Uint32 max_ndb_objects,
                     Uint32 no_conn_obj, Uint32 init_no_ndb_objects) {
  if (m_pool != nullptr) {
    return false;
  }
  m_pool = NdbPool::create_instance(cc, max_ndb_objects, no_conn_obj,
                                    init_no_ndb_objects);
  if (m_pool == nullptr) {
    return false;
  }
  return true;
}

void drop_instance() {
  if (m_pool == nullptr) {
    return;
  }
  NdbPool::drop_instance();
  m_pool = nullptr;
}

Ndb *get_ndb_object(Uint32 &hint_id, const char *a_catalog_name,
                    const char *a_schema_name) {
  if (m_pool == nullptr) {
    return nullptr;
  }
  return m_pool->get_ndb_object(hint_id, a_catalog_name, a_schema_name);
}

void return_ndb_object(Ndb *returned_object, Uint32 id) {
  if (m_pool == nullptr) {
    return;
  }
  m_pool->return_ndb_object(returned_object, id);
}
