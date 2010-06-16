/* Copyright (c) 2010 Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_PARTITION_ADMIN_H
#define SQL_PARTITION_ADMIN_H

#ifndef WITH_PARTITION_STORAGE_ENGINE

/**
  Stub class that returns a error if the partition storage engine is
  not supported.
*/
class Partition_statement_unsupported : public Alter_table_common
{
public:
  Partition_statement_unsupported(LEX *lex)
    : Alter_table_common(lex)
  {}

  ~Partition_statement_unsupported()
  {}

  bool execute(THD *thd);
};

class Alter_table_exchange_partition_statement :
  public Partition_statement_unsupported
{
public:
  Alter_table_exchange_partition_statement(LEX *lex)
    : Partition_statement_unsupported(lex)
  {}

  ~Alter_table_exchange_partition_statement()
  {}
};

#else

/**
  Class that represents the ALTER TABLE t1 EXCHANGE PARTITION p
                            WITH TABLE t2 statement.
*/
class Alter_table_exchange_partition_statement : public Alter_table_common
{
public:
  /**
    Constructor, used to represent a ALTER TABLE EXCHANGE PARTITION statement.
    @param lex the LEX structure for this statement.
  */
  Alter_table_exchange_partition_statement(LEX *lex)
    : Alter_table_common(lex)
  {}

  ~Alter_table_exchange_partition_statement()
  {}

  /**
    Execute a ALTER TABLE EXCHANGE PARTITION statement at runtime.
    @param thd the current thread.
    @return false on success.
  */
  bool execute(THD *thd);

private:
  bool exchange_partition(THD *thd, TABLE_LIST *, Alter_info *);
};

#endif /* WITH_PARTITION_STORAGE_ENGINE */
#endif /* SQL_PARTITION_ADMIN_H */
