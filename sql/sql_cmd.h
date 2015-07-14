/* Copyright (c) 2009, 2015, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file Representation of an SQL command.
*/

#ifndef SQL_CMD_INCLUDED
#define SQL_CMD_INCLUDED

#include "my_sqlcommand.h"
#include "sql_alloc.h"
class THD;

/**
  @class Sql_cmd - Representation of an SQL command.

  This class is an interface between the parser and the runtime.
  The parser builds the appropriate derived classes of Sql_cmd
  to represent a SQL statement in the parsed tree.
  The execute() method in the derived classes of Sql_cmd contain the runtime
  implementation.
  Note that this interface is used for SQL statements recently implemented,
  the code for older statements tend to load the LEX structure with more
  attributes instead.
  Implement new statements by sub-classing Sql_cmd, as this improves
  code modularity (see the 'big switch' in dispatch_command()), and decreases
  the total size of the LEX structure (therefore saving memory in stored
  programs).
  The recommended name of a derived class of Sql_cmd is Sql_cmd_<derived>.

  Notice that the Sql_cmd class should not be confused with the Statement class.
  Statement is a class that is used to manage an SQL command or a set 
  of SQL commands. When the SQL statement text is analyzed, the parser will
  create one or more Sql_cmd objects to represent the actual SQL commands.
*/
class Sql_cmd : public Sql_alloc
{
private:
  Sql_cmd(const Sql_cmd &);         // No copy constructor wanted
  void operator=(Sql_cmd &);        // No assignment operator wanted

public:
  /**
    @brief Return the command code for this statement
  */
  virtual enum_sql_command sql_command_code() const = 0;

  /**
    Execute this SQL statement.
    @param thd the current thread.
    @retval false on success.
    @retval true on error
  */
  virtual bool execute(THD *thd) = 0;

  /**
    Command-specific reinitialization before execution of prepared statement

    @see reinit_stmt_before_use()

    @note Currently this function is overloaded for INSERT/REPLACE stmts only.

    @param thd  Current THD.
  */
  virtual void cleanup(THD *thd) {}

protected:
  Sql_cmd()
  {}

  virtual ~Sql_cmd()
  {
    /*
      Sql_cmd objects are allocated in thd->mem_root.
      In MySQL, the C++ destructor is never called, the underlying MEM_ROOT is
      simply destroyed instead.
      Do not rely on the destructor for any cleanup.
    */
    DBUG_ASSERT(FALSE);
  }
};

#endif // SQL_CMD_INCLUDED
