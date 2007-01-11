/* Copyright (C) 2000-2001, 2004 MySQL AB

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

/*
  Static variables for pisam library. All definied here for easy making of
  a shared library
*/

#ifndef stdin
#include "myrg_def.h"
#endif

LIST	*myrg_open_list=0;
static const char *merge_insert_methods[] =
{ "FIRST", "LAST", NullS };
TYPELIB merge_insert_method= { array_elements(merge_insert_methods)-1,"",
			       merge_insert_methods, 0};
