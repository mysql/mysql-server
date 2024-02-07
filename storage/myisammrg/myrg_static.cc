/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Static variables for pisam library. All defined here for easy making of
  a shared library
*/

#ifndef stdin
#include "storage/myisammrg/myrg_def.h"
#endif
#include "my_macros.h"
#include "my_psi_config.h"
#include "mysql/psi/mysql_file.h"
#include "mysql/psi/mysql_memory.h"
#include "nulls.h"
#include "template_utils.h"
#include "typelib.h"

LIST *myrg_open_list = nullptr;
static const char *merge_insert_methods[] = {"FIRST", "LAST", NullS};
TYPELIB merge_insert_method = {array_elements(merge_insert_methods) - 1, "",
                               merge_insert_methods, nullptr};

PSI_memory_key rg_key_memory_MYRG_INFO;
PSI_memory_key rg_key_memory_children;

#ifdef HAVE_PSI_INTERFACE
PSI_mutex_key rg_key_mutex_MYRG_INFO_mutex;

static PSI_mutex_info all_myisammrg_mutexes[] = {
    {&rg_key_mutex_MYRG_INFO_mutex, "MYRG_INFO::mutex", 0, 0, PSI_DOCUMENT_ME}};

PSI_file_key rg_key_file_MRG;

static PSI_file_info all_myisammrg_files[] = {
    {&rg_key_file_MRG, "MRG", 0, 0, PSI_DOCUMENT_ME}};

static PSI_memory_info all_myisammrg_memory[] = {
    {&rg_key_memory_MYRG_INFO, "MYRG_INFO", 0, 0, PSI_DOCUMENT_ME},
    {&rg_key_memory_children, "children", 0, 0, PSI_DOCUMENT_ME}};

void init_myisammrg_psi_keys() {
  const char *category = "myisammrg";
  int count;

  count = array_elements(all_myisammrg_mutexes);
  mysql_mutex_register(category, all_myisammrg_mutexes, count);

  count = array_elements(all_myisammrg_files);
  mysql_file_register(category, all_myisammrg_files, count);

  count = array_elements(all_myisammrg_memory);
  mysql_memory_register(category, all_myisammrg_memory, count);
}
#endif /* HAVE_PSI_INTERFACE */
