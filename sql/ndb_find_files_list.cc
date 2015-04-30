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


#include "ndb_find_files_list.h"

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include "sql_show.h"       // find_files()
#include "sql_class.h"      // THD::col_access


// Hides the usage of MEM_ROOT and List<> from the interface
struct Ndb_find_files_list_impl {
  // MEM_ROOT which holds the memory of file names and list links
  MEM_ROOT m_mem_root;

  // List of files and it's iterator
  List<LEX_STRING> m_files_list;
  List_iterator_fast<LEX_STRING> m_files_list_it;

  Ndb_find_files_list_impl()
  {
    // Initialize the MEM_ROOT which hold the file names
    // which the list pointers point at.
    init_alloc_root(PSI_NOT_INSTRUMENTED, &m_mem_root, 1024, 0);
  }

  ~Ndb_find_files_list_impl()
  {
    free_root(&m_mem_root, 0);
  }
};


Ndb_find_files_list::Ndb_find_files_list(class THD* thd) :
  m_thd(thd),
  m_impl(NULL)
{
}


Ndb_find_files_list::~Ndb_find_files_list()
{
  delete m_impl;
}


bool
Ndb_find_files_list::find_files_impl(const char *db,
                                     const char *path,
                                     bool dir)
{
  m_impl = new Ndb_find_files_list_impl();
  if (!m_impl)
  {
    return false;
  }

  /*
      The schema distribution participant has full permissions
      to drop or create any database. When determining if a database
      should be dropped on participating mysqld it will thus need
      full permissions also when listing the tables in the database.
      Such permission is controlled by the "magic" THD::col_access variable
      and need to be set high enough so that find_files() returns all
      files in the database(without checking any grants).

      Without full permission no tables would  be returned for databases
      which have special access rights(like performance_schema and
      information_schema). Those would thus appear empty and a faulty
      decision to drop them would be taken.

      Fix by setting the "magic" THD::col_access member in order to skip
      the access control check in find_files().
    */
  const ulong saved_col_access= m_thd->col_access;
  assert(sizeof(saved_col_access) == sizeof(m_thd->col_access));
  m_thd->col_access|= TABLE_ACLS;

  const find_files_result result = find_files(m_thd,
                                              &m_impl->m_files_list,
                                              db, path, NULL, dir,
                                              &m_impl->m_mem_root);
  if (result != FIND_FILES_OK)
  {
    // Restore column access rights
    m_thd->col_access= saved_col_access;

    return false;
  }

  // Restore column access rights
  m_thd->col_access= saved_col_access;

  // Setup the iterator to point at the file list
  m_impl->m_files_list_it.init(m_impl->m_files_list);

  return true;
}


// Return the name of next found file
MYSQL_LEX_STRING* Ndb_find_files_list::next()
{
  return m_impl->m_files_list_it++;
}


// Return the number of found files
uint
Ndb_find_files_list::found_files() const
{
  return m_impl->m_files_list.elements;
}
