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

#ifndef SQL_PARTITION_ADMIN_H
#define SQL_PARTITION_ADMIN_H

#ifndef WITH_PARTITION_STORAGE_ENGINE

/**
  Stub class that returns a error if the partition storage engine is
  not supported.
*/
class Partition_statement_unsupported : public Sql_statement
{
public:
  Partition_statement_unsupported(LEX *lex)
    : Sql_statement(lex)
  {}

  ~Partition_statement_unsupported()
  {}

  bool execute(THD *thd);
};


class Alter_table_analyze_partition_statement :
  public Partition_statement_unsupported
{
public:
  Alter_table_analyze_partition_statement(LEX *lex)
    : Partition_statement_unsupported(lex)
  {}

  ~Alter_table_analyze_partition_statement()
  {}
};


class Alter_table_check_partition_statement :
  public Partition_statement_unsupported
{
public:
  Alter_table_check_partition_statement(LEX *lex)
    : Partition_statement_unsupported(lex)
  {}

  ~Alter_table_check_partition_statement()
  {}
};


class Alter_table_optimize_partition_statement :
  public Partition_statement_unsupported
{
public:
  Alter_table_optimize_partition_statement(LEX *lex)
    : Partition_statement_unsupported(lex)
  {}

  ~Alter_table_optimize_partition_statement()
  {}
};


class Alter_table_repair_partition_statement :
  public Partition_statement_unsupported
{
public:
  Alter_table_repair_partition_statement(LEX *lex)
    : Partition_statement_unsupported(lex)
  {}

  ~Alter_table_repair_partition_statement()
  {}
};


class Alter_table_truncate_partition_statement :
  public Partition_statement_unsupported
{
public:
  Alter_table_truncate_partition_statement(LEX *lex)
    : Partition_statement_unsupported(lex)
  {}

  ~Alter_table_truncate_partition_statement()
  {}
};


#else

/**
  Class that represents the ALTER TABLE t1 ANALYZE PARTITION p statement.
*/
class Alter_table_analyze_partition_statement : public Analyze_table_statement
{
public:
  /**
    Constructor, used to represent a ALTER TABLE ANALYZE PARTITION statement.
    @param lex the LEX structure for this statement.
  */
  Alter_table_analyze_partition_statement(LEX *lex)
    : Analyze_table_statement(lex)
  {}

  ~Alter_table_analyze_partition_statement()
  {}

  /**
    Execute a ALTER TABLE ANALYZE PARTITION statement at runtime.
    @param thd the current thread.
    @return false on success.
  */
  bool execute(THD *thd);
};


/**
  Class that represents the ALTER TABLE t1 CHECK PARTITION p statement.
*/
class Alter_table_check_partition_statement : public Check_table_statement
{
public:
  /**
    Constructor, used to represent a ALTER TABLE CHECK PARTITION statement.
    @param lex the LEX structure for this statement.
  */
  Alter_table_check_partition_statement(LEX *lex)
    : Check_table_statement(lex)
  {}

  ~Alter_table_check_partition_statement()
  {}

  /**
    Execute a ALTER TABLE CHECK PARTITION statement at runtime.
    @param thd the current thread.
    @return false on success.
  */
  bool execute(THD *thd);
};


/**
  Class that represents the ALTER TABLE t1 OPTIMIZE PARTITION p statement.
*/
class Alter_table_optimize_partition_statement : public Optimize_table_statement
{
public:
  /**
    Constructor, used to represent a ALTER TABLE OPTIMIZE PARTITION statement.
    @param lex the LEX structure for this statement.
  */
  Alter_table_optimize_partition_statement(LEX *lex)
    : Optimize_table_statement(lex)
  {}

  ~Alter_table_optimize_partition_statement()
  {}

  /**
    Execute a ALTER TABLE OPTIMIZE PARTITION statement at runtime.
    @param thd the current thread.
    @return false on success.
  */
  bool execute(THD *thd);
};


/**
  Class that represents the ALTER TABLE t1 REPAIR PARTITION p statement.
*/
class Alter_table_repair_partition_statement : public Repair_table_statement
{
public:
  /**
    Constructor, used to represent a ALTER TABLE REPAIR PARTITION statement.
    @param lex the LEX structure for this statement.
  */
  Alter_table_repair_partition_statement(LEX *lex)
    : Repair_table_statement(lex)
  {}

  ~Alter_table_repair_partition_statement()
  {}

  /**
    Execute a ALTER TABLE REPAIR PARTITION statement at runtime.
    @param thd the current thread.
    @return false on success.
  */
  bool execute(THD *thd);
};


/**
  Class that represents the ALTER TABLE t1 TRUNCATE PARTITION p statement.
*/
class Alter_table_truncate_partition_statement : public Sql_statement
{
public:
  /**
    Constructor, used to represent a ALTER TABLE TRUNCATE PARTITION statement.
    @param lex the LEX structure for this statement.
  */
  Alter_table_truncate_partition_statement(LEX *lex)
    : Sql_statement(lex)
  {}

  virtual ~Alter_table_truncate_partition_statement()
  {}

  /**
    Execute a ALTER TABLE TRUNCATE PARTITION statement at runtime.
    @param thd the current thread.
    @return false on success.
  */
  bool execute(THD *thd);
};

#endif /* WITH_PARTITION_STORAGE_ENGINE */
#endif /* SQL_PARTITION_ADMIN_H */
