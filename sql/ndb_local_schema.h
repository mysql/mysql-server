/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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
     NOTE! This class is now similar to the Ndb_dd_client class,
     they could probably merged into one in order to have only
     one class which interfaces with the DD.
  */

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
    local MySQL Servers DD
  */

  class Table : protected Base
  {
    bool m_has_triggers;

    bool mdl_try_lock_for_rename(const char* new_db,
                                 const char* new_name) const;
    bool mdl_try_lock_exclusive(void) const;

  public:
    Table(); // Not implemented
    Table(const Table&); // Not implemented
    Table(class THD* thd, const char* db, const char* name);

    bool is_local_table(bool* exists) const;
    void remove_table(void) const;
    void rename_table(const char* new_db, const char* new_name,
                      int new_id, int new_version) const;
  };
};

#endif
