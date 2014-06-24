/*
   Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "dynamic_ids.h"

Server_ids::Server_ids()
  : dynamic_ids(PSI_NOT_INSTRUMENTED)
{
}

bool Server_ids::unpack_dynamic_ids(char *param_dynamic_ids)
{
  char *token= NULL, *last= NULL;
  uint num_items= 0;
 
  DBUG_ENTER("Server_ids::unpack_dynamic_ids");

  token= my_strtok_r(param_dynamic_ids, " ", &last);

  if (token == NULL)
    DBUG_RETURN(TRUE);

  num_items= atoi(token);
  for (uint i=0; i < num_items; i++)
  {
    token= my_strtok_r(NULL, " ", &last);
    if (token == NULL)
      DBUG_RETURN(TRUE);
    else
    {
      ulong val= atol(token);
      dynamic_ids.insert_unique(val);
    }
  }
  DBUG_RETURN(FALSE);
}

bool Server_ids::pack_dynamic_ids(String *buffer)
{
  DBUG_ENTER("Server_ids::pack_dynamic_ids");

  if (buffer->set_int(dynamic_ids.size(), FALSE, &my_charset_bin))
    DBUG_RETURN(TRUE);

  for (ulong i= 0;
       i < dynamic_ids.size(); i++)
  {
    ulong s_id= dynamic_ids[i];
    if (buffer->append(" ") ||
        buffer->append_ulonglong(s_id))
      DBUG_RETURN(TRUE);
  }

  DBUG_RETURN(FALSE);
}

