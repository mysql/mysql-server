/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysql_priv.h"
#include "rpl_info_fields.h"

bool Rpl_info_fields::configure()
{
  DBUG_ENTER("Rpl_info_fields::configure");

  if (!field && !(field= new info_fields[ninfo]))
      DBUG_RETURN(TRUE);

  for (int pos= 0; field && pos < ninfo; pos++)
  {
    field[pos].saved.str= field[pos].use.str= 0;
    field[pos].size= 0;
  }

  for (int pos= 0; field && pos < ninfo; pos++)
  {
    if (!(field[pos].saved.str= static_cast<char*>
         (my_malloc(FN_REFLEN, MYF(MY_WME)))))
      DBUG_RETURN(TRUE);
    field[pos].size= FN_REFLEN;
    field[pos].use.str= field[pos].saved.str;
  }

  DBUG_RETURN(FALSE);
}

bool Rpl_info_fields::resize(int needed_size, int pos)
{
  char *buffer= field[pos].saved.str;
  
  DBUG_ENTER("Rpl_info_fields::resize");

  if (field[pos].size < needed_size)
  {
    buffer=
      (char *) my_realloc(buffer, needed_size, MYF(MY_WME));

    if (!buffer)
      DBUG_RETURN(TRUE);

    field[pos].saved.str= field[pos].use.str= buffer;
    field[pos].size= needed_size;
  }

  DBUG_RETURN(FALSE);
}

Rpl_info_fields::~Rpl_info_fields()
{
  if (field)
  {
    for (int pos= 0; pos < ninfo; pos++)
    {
      if (field[pos].saved.str)
      {
        my_free(field[pos].saved.str, MYF(0));
      }
    }
    delete [] field;
  }
}
