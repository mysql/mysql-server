/*
   Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/dynamic_ids.h"

#include <stdlib.h>
#include <sys/types.h>

#include "m_ctype.h"
#include "m_string.h"        // my_strtok_r
#include "my_dbug.h"
#include "my_inttypes.h"
#include "mysql/psi/psi_base.h"
#include "sql_string.h"      // String

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

