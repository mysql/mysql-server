/*
   Copyright (C) 2003, 2005-2007 MySQL AB
    Use is subject to license terms.

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

class Ndb;
class NdbPool;

bool
create_instance(Ndb_cluster_connection* cc,
                Uint32 max_ndb_objects,
                Uint32 no_conn_obj,
                Uint32 init_no_ndb_objects);

void
drop_instance();

Ndb*
get_ndb_object(Uint32 &hint_id,
               const char* a_catalog_name,
               const char* a_schema_name);

void
return_ndb_object(Ndb* returned_object, Uint32 id);

