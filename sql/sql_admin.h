/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_TABLE_MAINTENANCE_H
#define SQL_TABLE_MAINTENANCE_H

#include <stddef.h>

#include "lex_string.h"
#include "my_dbug.h"
#include "my_sqlcommand.h"
#include "mysql/mysql_lex_string.h"
#include "sql_cmd.h"       // Sql_cmd

class THD;
struct TABLE_LIST;
template <class T> class List;

typedef struct st_key_cache KEY_CACHE;
typedef struct st_lex_user LEX_USER;

/* Must be able to hold ALTER TABLE t PARTITION BY ... KEY ALGORITHM = 1 ... */
#define SQL_ADMIN_MSG_TEXT_SIZE 128 * 1024

bool mysql_assign_to_keycache(THD* thd, TABLE_LIST* table_list,
                              LEX_STRING *key_cache_name);
bool mysql_preload_keys(THD* thd, TABLE_LIST* table_list);

/**
  Sql_cmd_analyze_table represents the ANALYZE TABLE statement.
*/
class Sql_cmd_analyze_table : public Sql_cmd
{
public:
  /**
    Constructor, used to represent a ANALYZE TABLE statement.
  */
  Sql_cmd_analyze_table()
  {}

  ~Sql_cmd_analyze_table()
  {}

  bool execute(THD *thd);

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_ANALYZE;
  }
};



/**
  Sql_cmd_check_table represents the CHECK TABLE statement.
*/
class Sql_cmd_check_table : public Sql_cmd
{
public:
  /**
    Constructor, used to represent a CHECK TABLE statement.
  */
  Sql_cmd_check_table()
  {}

  ~Sql_cmd_check_table()
  {}

  bool execute(THD *thd);

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_CHECK;
  }
};


/**
  Sql_cmd_optimize_table represents the OPTIMIZE TABLE statement.
*/
class Sql_cmd_optimize_table : public Sql_cmd
{
public:
  /**
    Constructor, used to represent a OPTIMIZE TABLE statement.
  */
  Sql_cmd_optimize_table()
  {}

  ~Sql_cmd_optimize_table()
  {}

  bool execute(THD *thd);

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_OPTIMIZE;
  }
};



/**
  Sql_cmd_repair_table represents the REPAIR TABLE statement.
*/
class Sql_cmd_repair_table : public Sql_cmd
{
public:
  /**
    Constructor, used to represent a REPAIR TABLE statement.
  */
  Sql_cmd_repair_table()
  {}

  ~Sql_cmd_repair_table()
  {}

  bool execute(THD *thd);

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_REPAIR;
  }
};


/**
  Sql_cmd_shutdown represents the SHUTDOWN statement.
*/
class Sql_cmd_shutdown : public Sql_cmd
{
public:
  virtual bool execute(THD *thd);
  virtual enum_sql_command sql_command_code() const { return SQLCOM_SHUTDOWN; }
};


enum role_enum
{
  ROLE_NONE, ROLE_DEFAULT, ROLE_ALL, ROLE_NAME
};

/**
  Sql_cmd_set_role represetns the SET ROLE ... statement.
*/
class Sql_cmd_set_role : public Sql_cmd
{
  friend class PT_set_role;

  const role_enum role_type;
  const List<LEX_USER> *role_list;
  const List<LEX_USER> *except_roles;

public:
  Sql_cmd_set_role(role_enum role_type_arg,
                   const List<LEX_USER> *except_roles_arg)
  : role_type(role_type_arg), role_list(NULL), except_roles(except_roles_arg)
  {
    DBUG_ASSERT(role_type == ROLE_NONE || role_type == ROLE_DEFAULT ||
                role_type == ROLE_ALL);
    DBUG_ASSERT(role_type == ROLE_ALL || except_roles == NULL);
  }
  explicit Sql_cmd_set_role(const List<LEX_USER> *role_arg)
  : role_type(ROLE_NAME), role_list(role_arg)
  {}

  virtual bool execute(THD *thd);
  virtual enum_sql_command sql_command_code() const { return SQLCOM_SET_ROLE; }
};


/**
  Sql_cmd_create_role represetns the CREATE ROLE ... statement.
*/
class Sql_cmd_create_role : public Sql_cmd
{
  friend class PT_create_role;

  const bool if_not_exists;
  const List<LEX_USER> *roles;

public:
  explicit Sql_cmd_create_role(bool if_not_exists_arg,
                               const List<LEX_USER> *roles_arg)
  : if_not_exists(if_not_exists_arg), roles(roles_arg)
  {}

  virtual bool execute(THD *thd);
  virtual enum_sql_command sql_command_code() const
  { return SQLCOM_CREATE_ROLE; }
};


/**
  Sql_cmd_drop_role represetns the DROP ROLE ... statement.
*/
class Sql_cmd_drop_role : public Sql_cmd
{
  friend class PT_drop_role;

  bool ignore_errors;
  const List<LEX_USER> *roles;

public:
  explicit Sql_cmd_drop_role(bool ignore_errors_arg,
                             const List<LEX_USER> *roles_arg)
  : ignore_errors(ignore_errors_arg), roles(roles_arg)
  {}

  virtual bool execute(THD *thd);
  virtual enum_sql_command sql_command_code() const { return SQLCOM_DROP_ROLE; }
};


/**
  Sql_cmd_grant_roles represents the GRANT role-list TO ... statement.
*/
class Sql_cmd_grant_roles : public Sql_cmd
{
  const List<LEX_USER> *roles;
  const List<LEX_USER> *users;
  const bool with_admin_option;

public:
  explicit Sql_cmd_grant_roles(const List<LEX_USER> *roles_arg,
                               const List<LEX_USER> *users_arg,
                               bool with_admin_option_arg)
  : roles(roles_arg), users(users_arg), with_admin_option(with_admin_option_arg)
  {}

  virtual bool execute(THD *thd);
  virtual enum_sql_command sql_command_code() const
  { return SQLCOM_GRANT_ROLE; }
};


/**
  Sql_cmd_revoke_roles represents the REVOKE [role list] TO ... statement.
*/
class Sql_cmd_revoke_roles : public Sql_cmd
{
  const List<LEX_USER> *roles;
  const List<LEX_USER> *users;

public:
  explicit Sql_cmd_revoke_roles(const List<LEX_USER> *roles_arg,
                               const List<LEX_USER> *users_arg)
  : roles(roles_arg), users(users_arg)
  {}

  virtual bool execute(THD *thd);
  virtual enum_sql_command sql_command_code() const
  { return SQLCOM_REVOKE_ROLE; }
};


/**
  Sql_cmd_alter_user_default_role ALTER USER ... DEFAULT ROLE ... statement.
*/
class Sql_cmd_alter_user_default_role : public Sql_cmd
{
  friend class PT_alter_user_default_role;

  const bool if_exists;
  const List<LEX_USER> *users;
  const List<LEX_USER> *roles;
  const role_enum role_type;
public:
  explicit Sql_cmd_alter_user_default_role(bool if_exists_arg,
                                           const List<LEX_USER> *users_arg,
                                           const List<LEX_USER> *roles_arg,
                                           const role_enum role_type_arg)
  : if_exists(if_exists_arg), users(users_arg), roles(roles_arg),
    role_type(role_type_arg)
  {}

  virtual bool execute(THD *thd);
  virtual enum_sql_command sql_command_code() const
  { return SQLCOM_ALTER_USER_DEFAULT_ROLE; }
};


/**
  Sql_cmd_show_privileges SHOW PRIVILEGES ... statement.
*/
class Sql_cmd_show_privileges: public Sql_cmd
{
  friend class PT_show_privileges;

  const LEX_USER *for_user;
  const List<LEX_USER> *using_users;

public:
  explicit Sql_cmd_show_privileges(const LEX_USER *for_user_arg,
                                   const List<LEX_USER> *using_users_arg)
  : for_user(for_user_arg), using_users(using_users_arg)
  { }

  virtual bool execute(THD *thd);
  virtual enum_sql_command sql_command_code() const
  { return SQLCOM_SHOW_PRIVILEGES; }
};



enum alter_instance_action_enum
{
  ROTATE_INNODB_MASTER_KEY,
  LAST_MASTER_KEY                       /* Add new master key type before this */
};


/**
  Sql_cmd_alter_instance represents the ROTATE alter_instance_action MASTER KEY statement.
*/
class Alter_instance;

class Sql_cmd_alter_instance : public Sql_cmd
{
  friend class PT_alter_instance;
  const enum alter_instance_action_enum alter_instance_action;
  Alter_instance *alter_instance;
public:
  explicit Sql_cmd_alter_instance(enum alter_instance_action_enum alter_instance_action_arg)
  : alter_instance_action(alter_instance_action_arg),
    alter_instance(NULL)
  {}

  virtual bool execute(THD *thd);
  virtual enum_sql_command sql_command_code() const { return SQLCOM_ALTER_INSTANCE; }
};


/**
  Sql_cmd_show represents the SHOW COLUMNS/SHOW INDEX statements.
*/
class Sql_cmd_show : public Sql_cmd
{
public:
  Sql_cmd_show(enum_sql_command sql_command)
    : m_sql_command(sql_command)
  {}
  virtual bool execute(THD *thd);
  virtual enum_sql_command sql_command_code() const { return m_sql_command; }
  virtual bool prepare(THD *thd);

private:
  enum_sql_command m_sql_command;
};
#endif
