/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_ALTER_INSTANCE_INCLUDED
#define SQL_ALTER_INSTANCE_INCLUDED

#include <my_inttypes.h>

class THD;
/*
  Base class for execution control for ALTER INSTANCE ... statement
*/
class Alter_instance {
 protected:
  THD *m_thd;

 public:
  explicit Alter_instance(THD *thd) : m_thd(thd) {}
  virtual bool execute() = 0;
  bool log_to_binlog();
  virtual ~Alter_instance() = default;
};

class Rotate_innodb_master_key : public Alter_instance {
 public:
  explicit Rotate_innodb_master_key(THD *thd) : Alter_instance(thd) {}

  bool execute() override;
  ~Rotate_innodb_master_key() override = default;
};

class Rotate_binlog_master_key : public Alter_instance {
 public:
  explicit Rotate_binlog_master_key(THD *thd) : Alter_instance(thd) {}

  /**
    Executes master key rotation by calling Rpl_encryption api.

    @retval False on success
    @retval True on error
  */
  bool execute() override;
  ~Rotate_binlog_master_key() override = default;
};

/** Alter Innodb redo log properties. */
class Innodb_redo_log : public Alter_instance {
 public:
  /**
    @param[in]  thd     server THD
    @param[in]  enable  enable or disable redo logging
  */
  Innodb_redo_log(THD *thd, bool enable)
      : Alter_instance(thd), m_enable(enable) {}

  bool execute() override;

 private:
  /** Enable or disable redo logging. */
  bool m_enable;
};

class Reload_keyring : public Alter_instance {
 public:
  explicit Reload_keyring(THD *thd) : Alter_instance(thd) {}

  /**
    Execute keyring reload operation by calling required APIs

    @returns status of the operation
      @retval false Success
      @retval true  Error
  */
  bool execute() override;
  virtual ~Reload_keyring() override = default;

 private:
  const static size_t s_error_message_length;
};

#endif /* SQL_ALTER_INSTANCE_INCLUDED */
