/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_ALTER_TABLE_H
#define SQL_ALTER_TABLE_H

/**
  Alter_table_common represents the common properties of the ALTER TABLE
  statements.
  @todo move Alter_info and other ALTER generic structures from Lex here.
*/
class Alter_table_common : public Sql_statement
{
protected:
  /**
    Constructor.
    @param lex the LEX structure for this statement.
  */
  Alter_table_common(LEX *lex)
    : Sql_statement(lex)
  {}

  virtual ~Alter_table_common()
  {}

};

/**
  Alter_table_statement represents the generic ALTER TABLE statement.
  @todo move Alter_info and other ALTER specific structures from Lex here.
*/
class Alter_table_statement : public Alter_table_common
{
public:
  /**
    Constructor, used to represent a ALTER TABLE statement.
    @param lex the LEX structure for this statement.
  */
  Alter_table_statement(LEX *lex)
    : Alter_table_common(lex)
  {}

  ~Alter_table_statement()
  {}

  /**
    Execute a ALTER TABLE statement at runtime.
    @param thd the current thread.
    @return false on success.
  */
  bool execute(THD *thd);
};

#endif
