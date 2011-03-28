/*
   Copyright (C) 2003, 2005, 2006 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#include <Ndb.hpp>
#include "NdbPoolImpl.hpp"
#include <NdbPool.hpp>

static NdbPool* m_pool = 0;

bool
create_instance(Ndb_cluster_connection* cc,
		Uint32 max_ndb_objects,
                Uint32 no_conn_obj,
                Uint32 init_no_ndb_objects)
{
  if (m_pool != NULL) {
    return false;
  }
  m_pool = NdbPool::create_instance(cc, 
				    max_ndb_objects,
                                    no_conn_obj,
                                    init_no_ndb_objects);
  if (m_pool == NULL) {
    return false;
  }
  return true;
}

void
drop_instance()
{
  if (m_pool == NULL) {
    return;
  }
  NdbPool::drop_instance();
  m_pool = NULL;
}

Ndb*
get_ndb_object(Uint32 &hint_id,
               const char* a_catalog_name,
               const char* a_schema_name)
{
  if (m_pool == NULL) {
    return NULL;
  }
  return m_pool->get_ndb_object(hint_id, a_catalog_name, a_schema_name);
}

void
return_ndb_object(Ndb* returned_object, Uint32 id)
{
  if (m_pool == NULL) {
    return;
  }
  m_pool->return_ndb_object(returned_object, id);
}

