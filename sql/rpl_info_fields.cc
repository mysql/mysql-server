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

#include "sql_priv.h"
#include "my_sys.h"
#include "rpl_info_fields.h"

/**
  Initializes a sequence of fields to be read from or stored to a repository.
  The number of fields created and initialized are determined by the property
  @c ninfo which is set while calling the constructor. Each field is created
  with the default size of @c FN_REFLEN.

  @retval FALSE No error
  @retval TRUE Failure
*/
bool Rpl_info_fields::init()
{
  DBUG_ENTER("Rpl_info_fields::configure");

  if (!field && !(field= new info_field[ninfo]))
      DBUG_RETURN(TRUE);

  for (int pos= 0; field && pos < ninfo; pos++)
  {
    field[pos].saved.str= field[pos].use.str= 0;
    field[pos].size= 0;
  }

  for (int pos= 0; field && pos < ninfo; pos++)
  {
    if (!(field[pos].saved.str= static_cast<char*>
         (my_malloc(FN_REFLEN, MYF(0)))))
      DBUG_RETURN(TRUE);
    field[pos].size= FN_REFLEN;
    field[pos].use.str= field[pos].saved.str;
  }

  DBUG_RETURN(FALSE);
}

/**
  Resize a field if necessary by calling my_realloc.

  @param[in] needed_size The value required.
  @param[in] pos The field that may be resized.

  @retval FALSE No error
  @retval TRUE Failure
*/
bool Rpl_info_fields::resize(int needed_size, int pos)
{
  char *buffer= field[pos].saved.str;
  
  DBUG_ENTER("Rpl_info_fields::resize");

  if (field[pos].size < needed_size)
  {
    buffer=
      (char *) my_realloc(buffer, needed_size, MYF(0));

    if (!buffer)
      DBUG_RETURN(TRUE);

    field[pos].saved.str= field[pos].use.str= buffer;
    field[pos].size= needed_size;
  }

  DBUG_RETURN(FALSE);
}

/**
  Restore the pointer from "saved" to "use". This member function must be
  called if the "use" points to another space in memory that it is not the
  original one initially allocated. For instance, this may happen when
  a the repository is a table and one reads from it.
*/
void Rpl_info_fields::restore()
{
  for (int pos= 0; field && pos < ninfo; pos++)
    field[pos].use.str= field[pos].saved.str;
}

Rpl_info_fields::~Rpl_info_fields()
{
  if (field)
  {
    for (int pos= 0; pos < ninfo; pos++)
    {
      if (field[pos].saved.str)
      {
        my_free(field[pos].saved.str);
      }
    }
    delete [] field;
  }
}
