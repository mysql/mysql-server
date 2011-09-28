/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifdef HAVE_PSI_INTERFACE
PSI_mutex_key rg_key_mutex_MYRG_INFO_mutex;

static PSI_mutex_info all_myisammrg_mutexes[]=
{
  { &rg_key_mutex_MYRG_INFO_mutex, "MYRG_INFO::mutex", 0}
};

PSI_file_key rg_key_file_MRG;

static PSI_file_info all_myisammrg_files[]=
{
  { &rg_key_file_MRG, "MRG", 0}
};

void init_myisammrg_psi_keys()
{
  const char* category= "myisammrg";
  int count;

  count= array_elements(all_myisammrg_mutexes);
  mysql_mutex_register(category, all_myisammrg_mutexes, count);

  count= array_elements(all_myisammrg_files);
  mysql_file_register(category, all_myisammrg_files, count);
}
#endif /* HAVE_PSI_INTERFACE */

