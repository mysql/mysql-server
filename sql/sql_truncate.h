#ifndef SQL_TRUNCATE_INCLUDED
#define SQL_TRUNCATE_INCLUDED

/* Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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

#include <memory>
#include <string>

#include "my_sqlcommand.h"
#include "sql/dd/types/table.h"
#include "sql/sql_cmd_ddl.h"

class MDL_ticket;
class THD;
class Table_ref;
struct handlerton;

using Up_table = std::unique_ptr<dd::Table>;

/**
  Sql_cmd_truncate_table represents the TRUNCATE statement.
*/
class Sql_cmd_truncate_table : public Sql_cmd_ddl {
  /** Set if a lock must be downgraded after truncate is done. */
  MDL_ticket *m_ticket_downgrade = nullptr;

  /** Track error status from functions called. */
  bool m_error = true;

 public:
  /**
    Constructor, used to represent a TRUNCATE statement.
  */
  Sql_cmd_truncate_table() = default;

  ~Sql_cmd_truncate_table() override = default;

  bool execute(THD *) override;

  enum_sql_command sql_command_code() const override { return SQLCOM_TRUNCATE; }

 private:
  /* Handle locking a base table for truncate. */
  bool lock_table(THD *, Table_ref *);

  /*
    Optimized delete of all rows by doing a full regenerate of the table.
    Depending on the storage engine, it can be accomplished through a
    drop and recreate or via the handler truncate method.
  */
  void truncate_base(THD *, Table_ref *);
  void truncate_temporary(THD *, Table_ref *);

  void end_transaction(THD *, bool, bool);
  void cleanup_base(THD *, const handlerton *);
  void cleanup_temporary(THD *, handlerton *, const Table_ref &, Up_table *,
                         const std::string &);
};

#endif
