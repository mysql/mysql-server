/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/rpl_info_values.h"

#include "my_dbug.h"
#include "my_inttypes.h"
#include "sql_string.h"       // String


Rpl_info_values::Rpl_info_values(int param_ninfo): value(0),
    ninfo(param_ninfo) { }

/**
  Initializes a sequence of values to be read from or stored into a repository.
  The number of values created and initialized are determined by the property
  @c ninfo which is set while calling the constructor. Each value is created
  with the default size of @c FN_REFLEN.

  @retval false No error
  @retval true Failure
*/
bool Rpl_info_values::init()
{
  DBUG_ENTER("Rpl_info_values::init");

  if (!value && !(value= new String[ninfo]))
      DBUG_RETURN(true);

  DBUG_RETURN(false);
}

Rpl_info_values::~Rpl_info_values()
{
  delete [] value;
}
