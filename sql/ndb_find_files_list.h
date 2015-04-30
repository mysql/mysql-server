/*
   Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_FIND_FILES_LIST_H
#define NDB_FIND_FILES_LIST_H

#include <my_global.h>
#include <mysql/mysql_lex_string.h>


/**
  Ndb_find_files_list

  Utility class which encapsulates the find_files() function
  and its returned list of files.

  The find_files() function return a list consisting of
  pointers to file names, the file names themself are allocated
  in a specified MEM_ROOT. In order to avoid that the default
  MEM_ROOT of the caller is used, this class allocates a memory
  root which holds the file name pointers and the file name strings
  until the list goes out of scope.

  The find_files() function also has a special quirk in that
  it looks at the THD::col_access member variable to determine which
  files and directories the caller has permission to see. This class
  make sure that caller has full access to see all files by applying
  the proper values for THD::col_access(see inline comment below).

*/

class Ndb_find_files_list {
  class THD* const m_thd;
  struct Ndb_find_files_list_impl* m_impl;

  bool find_files_impl(const char *db,
                       const char *path,
                       bool dir);
public:
  explicit Ndb_find_files_list(class THD* thd);
  ~Ndb_find_files_list();

  bool find_databases(const char *path)
  {
    return find_files_impl(NULL, path, true);
  }

  bool find_tables(const char *db,
                   const char *path)
  {
    return find_files_impl(db, path, false);
  }

  // Return the name of next found file
  MYSQL_LEX_STRING* next();

  // Return the number of found files
  uint found_files() const;
};

#endif
