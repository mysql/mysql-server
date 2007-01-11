/* Copyright (C) 2000-2001, 2003, 2005 MySQL AB

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


#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"

list_node end_of_list;

void free_list(I_List <i_string_pair> *list)
{
  i_string_pair *tmp;
  while ((tmp= list->get()))
    delete tmp;
}


void free_list(I_List <i_string> *list)
{
  i_string *tmp;
  while ((tmp= list->get()))
    delete tmp;
}
