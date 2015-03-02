#ifndef SQL_TRUNCATE_INCLUDED
#define SQL_TRUNCATE_INCLUDED
/* Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include "sql_cmd_dml.h"          // Sql_cmd_dml

class MDL_ticket;
class THD;
struct TABLE_LIST;

/**
  Sql_cmd_truncate_table represents the TRUNCATE statement.
*/
class Sql_cmd_truncate_table : public Sql_cmd_dml
{
private:
  /* Set if a lock must be downgraded after truncate is done. */
  MDL_ticket *m_ticket_downgrade;

public:
  /**
    Constructor, used to represent a TRUNCATE statement.
  */
  Sql_cmd_truncate_table()
  {}

  virtual ~Sql_cmd_truncate_table()
  {}

  /**
    Execute a TRUNCATE statement at runtime.
    @param thd the current thread.
    @return false on success.
  */
  bool execute(THD *thd);

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_TRUNCATE;
  }

  virtual bool prepared_statement_test(THD *) { return false; }
  virtual bool prepare(THD *) { return false; }
  virtual void cleanup(THD *) {}

protected:
  enum truncate_result{
    TRUNCATE_OK=0,
    TRUNCATE_FAILED_BUT_BINLOG,
    TRUNCATE_FAILED_SKIP_BINLOG
  };

  /** Handle locking a base table for truncate. */
  bool lock_table(THD *, TABLE_LIST *, bool *);

  /** Truncate table via the handler method. */
  enum truncate_result handler_truncate(THD *, TABLE_LIST *, bool);

  /**
    Optimized delete of all rows by doing a full regenerate of the table.
    Depending on the storage engine, it can be accomplished through a
    drop and recreate or via the handler truncate method.
  */
  bool truncate_table(THD *, TABLE_LIST *);
};

#endif
