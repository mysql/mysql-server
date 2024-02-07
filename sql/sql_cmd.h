/* Copyright (c) 2009, 2024, Oracle and/or its affiliates.

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

/**
  @file sql/sql_cmd.h
  Representation of an SQL command.
*/

#ifndef SQL_CMD_INCLUDED
#define SQL_CMD_INCLUDED

#include <assert.h>

#include "my_sqlcommand.h"
#include "sql/select_lex_visitor.h"

class THD;
class Prepared_statement;
struct handlerton;
struct MYSQL_LEX_STRING;
struct MYSQL_LEX_CSTRING;

/**
  What type of Sql_cmd we're dealing with (DML, DDL, ...).

  "Other" may be used for commands that are neither DML or DDL, such as
  shutdown.

  Theoretically, a command can run through code paths of both DDL and DML
  (e.g. CREATE TABLE ... AS SELECT ...), but at this point, a command
  must identify as only one thing.
*/
enum enum_sql_cmd_type {
  SQL_CMD_UNDETERMINED = 0,
  SQL_CMD_DDL = 1,
  SQL_CMD_DML = 2,
  SQL_CMD_DCL = 4,
  SQL_CMD_OTHER = 8
};

/**
  Representation of an SQL command.

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
class Sql_cmd {
 private:
  Sql_cmd(const Sql_cmd &);   // No copy constructor wanted
  void operator=(Sql_cmd &);  // No assignment operator wanted

 public:
  /**
    @brief Return the command code for this statement
  */
  virtual enum_sql_command sql_command_code() const = 0;

  /**
    @return true if object represents a preparable statement, ie. a query
    that is prepared with a PREPARE statement and executed with an EXECUTE
    statement. False is returned for regular statements (non-preparable
    statements) that are executed directly. Also false if statement is part
    of a stored procedure.
  */
  bool needs_explicit_preparation() const {
    return m_owner != nullptr && !m_part_of_sp;
  }
  /**
    @return true if statement is regular, ie not prepared statement and not
    part of stored procedure.
  */
  bool is_regular() const { return m_owner == nullptr && !m_part_of_sp; }

  /// @return true if this statement is prepared
  bool is_prepared() const { return m_prepared; }

  /**
    Prepare this SQL statement.

    param thd the current thread

    @returns false if success, true if error
  */
  virtual bool prepare(THD *) {
    // Default behavior for a statement is to have no preparation code.
    /* purecov: begin inspected */
    assert(!is_prepared());
    set_prepared();
    return false;
    /* purecov: end */
  }

  /**
    Execute this SQL statement.
    @param thd the current thread.
    @returns false if success, true if error
  */
  virtual bool execute(THD *thd) = 0;

  /**
    Command-specific reinitialization before execution of prepared statement

    param thd  Current THD.
  */
  virtual void cleanup(THD *) { m_secondary_engine = nullptr; }

  /// Set the owning prepared statement
  void set_owner(Prepared_statement *stmt) {
    assert(!m_part_of_sp);
    m_owner = stmt;
  }

  /// Get the owning prepared statement
  Prepared_statement *owner() const { return m_owner; }

  /**
    Mark statement as part of procedure. Such statements can be executed
    multiple times, the first execute() call will also prepare it.
  */
  void set_as_part_of_sp() {
    assert(!m_part_of_sp && m_owner == nullptr);
    m_part_of_sp = true;
  }
  /// @returns true if statement is part of a stored procedure
  bool is_part_of_sp() const { return m_part_of_sp; }

  /// @return SQL command type (DML, DDL, ... -- "undetermined" by default)
  virtual enum enum_sql_cmd_type sql_cmd_type() const {
    return SQL_CMD_UNDETERMINED;
  }

  /// @return true if implemented as single table plan, DML statement only
  virtual bool is_single_table_plan() const {
    /* purecov: begin inspected */
    assert(sql_cmd_type() == SQL_CMD_DML);
    return false;
    /* purecov: end */
  }

  virtual bool accept(THD *, Select_lex_visitor *) { return false; }

  /**
    Is this statement of a type and on a form that makes it eligible
    for execution in a secondary storage engine?

    @return the name of the secondary storage engine, or nullptr if
    the statement is not eligible for execution in a secondary storage
    engine
  */
  virtual const MYSQL_LEX_CSTRING *eligible_secondary_storage_engine(
      THD *) const {
    return nullptr;
  }

  /** @return true if the operation is BULK LOAD. */
  virtual bool is_bulk_load() const { return false; }

  /**
    Disable use of secondary storage engines in this statement. After
    a call to this function, the statement will not try to use a
    secondary storage engine until it is reprepared.
  */
  void disable_secondary_storage_engine() {
    assert(m_secondary_engine == nullptr);
    m_secondary_engine_enabled = false;
  }

  void enable_secondary_storage_engine() { m_secondary_engine_enabled = true; }

  /**
    Has use of secondary storage engines been disabled for this statement?
  */
  bool secondary_storage_engine_disabled() const {
    return !m_secondary_engine_enabled;
  }

  /**
  Mark the current statement as using a secondary storage engine.
  This function must be called before the statement starts opening
  tables in a secondary engine.
  */
  void use_secondary_storage_engine(const handlerton *hton) {
    assert(m_secondary_engine_enabled);
    m_secondary_engine = hton;
  }

  /**
    Is this statement using a secondary storage engine?
    @note that this is reliable during optimization and afterwards; during
    preparation, if this is an explicit preparation (SQL PREPARE, C API
    PREPARE, and automatic repreparation), it may be false as RAPID tables have
    not yet been opened. Therefore, during preparation, it is safer to test
    THD::secondary_engine_optimization().
  */
  bool using_secondary_storage_engine() const {
    return m_secondary_engine != nullptr;
  }

  /**
    Get the handlerton of the secondary engine that is used for
    executing this statement, or nullptr if a secondary engine is not
    used.
  */
  const handlerton *secondary_engine() const { return m_secondary_engine; }

  void set_optional_transform_prepared(bool value) {
    m_prepared_with_optional_transform = value;
  }

  bool is_optional_transform_prepared() {
    return m_prepared_with_optional_transform;
  }

 protected:
  Sql_cmd() : m_owner(nullptr), m_part_of_sp(false), m_prepared(false) {}

  virtual ~Sql_cmd() {
    /*
      Sql_cmd objects are allocated in thd->mem_root.
      In MySQL, the C++ destructor is never called, the underlying MEM_ROOT is
      simply destroyed instead.
      Do not rely on the destructor for any cleanup.
    */
    assert(false);
  }

  /// Set this statement as prepared
  void set_prepared() { m_prepared = true; }

 private:
  Prepared_statement *m_owner;  /// Owning prepared statement, NULL if non-prep.
  bool m_part_of_sp;            /// True when statement is part of stored proc.
  bool m_prepared;              /// True when statement has been prepared

  /**
    Tells if a secondary storage engine can be used for this
    statement. If it is false, use of a secondary storage engine will
    not be considered for executing this statement.
  */
  bool m_secondary_engine_enabled{true};

  /**
    Keeps track of whether the statement was prepared optional
    transformation.
  */
  bool m_prepared_with_optional_transform{false};

  /**
    The secondary storage engine to use for execution of this
    statement, if any, or nullptr if the primary engine is used.
    This property is reset at the start of each execution.
  */
  const handlerton *m_secondary_engine{nullptr};
};

#endif  // SQL_CMD_INCLUDED
