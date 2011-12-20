/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_LOCAL_SCHEMA_H
#define NDB_LOCAL_SCHEMA_H

class Ndb_local_schema
{
  /*
    Base functionality for working with local schema
    objects like tables, triggers and databases
  */
  class Base {
    bool m_have_mdl_lock;
    bool m_push_warnings;

    bool mdl_try_lock(void) const;
    void mdl_unlock(void);

  protected:
    class THD* m_thd;
    const char* m_db;
    const char* m_name;

    void log_warning(const char* fmt, ...) const;

    Base(); // Not implemented
    Base(const Base&); // Not implemented
    Base(class THD* thd, const char* db, const char* name);
    ~Base();

    bool have_mdl_lock(void) const { return m_have_mdl_lock; }
  };

public:

  /*
    Class used for working with a table in the
    local MySQL Servers "dictionary"
  */

  class Table : protected Base
  {
    bool m_frm_file_exist;
    bool m_ndb_file_exist;
    bool m_has_triggers;

    bool file_exists(const char* ext) const;
    bool remove_file(const char* ext) const;
    bool rename_file(const char* new_db, const char* new_name,
                     const char* ext) const;

    // Read the engine type from .frm and return true if it says NDB
    bool frm_engine_is_ndb(void) const;

  public:
    Table(); // Not implemented
    Table(const Table&); // Not implemented
    Table(class THD* thd, const char* db, const char* name);

    bool is_local_table(void) const;
    void remove_table(void) const;
    void rename_table(const char* new_db, const char* new_name) const;
  };
};

#endif
