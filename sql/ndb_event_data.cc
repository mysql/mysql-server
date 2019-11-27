/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "ndb_event_data.h"

#include <table.h>


Ndb_event_data::Ndb_event_data(NDB_SHARE *the_share) :
  shadow_table(NULL),
  share(the_share)
{
  ndb_value[0]= NULL;
  ndb_value[1]= NULL;
}


Ndb_event_data::~Ndb_event_data()
{
  if (shadow_table)
    closefrm(shadow_table, 1);
  shadow_table= NULL;
  free_root(&mem_root, MYF(0));
  share= NULL;
  /*
    ndbvalue[] allocated with my_multi_malloc -> only
    first pointer need to be freed
  */
  my_free(ndb_value[0]);
}


void Ndb_event_data::print(const char* where, FILE* file) const
{
  fprintf(file,
          "%s shadow_table: %p '%s.%s'\n",
          where,
          shadow_table, shadow_table->s->db.str,
          shadow_table->s->table_name.str);
}
