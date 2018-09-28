/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ACL_TABLE_USER_INCLUDED
#define ACL_TABLE_USER_INCLUDED

#include "sql/auth/acl_table_base.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"
#include "sql/auth/auth_internal.h"
#include "sql/auth/sql_auth_cache.h"
#include "sql/json_dom.h"

namespace acl_table {

typedef std::pair<Table_op_error_code, struct timeval>
    acl_table_user_writer_status;

/**
  mysql.user table writer. It updates or drop a one single row from the table.
*/

class Acl_table_user_writer_status {
 public:
  Acl_table_user_writer_status();
  Acl_table_user_writer_status(bool skip, ulong rights, Table_op_error_code err,
                               struct timeval pwd_timestamp, std::string cred)
      : skip_cache_update(skip),
        updated_rights(rights),
        error(err),
        password_change_timestamp(pwd_timestamp),
        second_cred(cred) {}
  bool skip_cache_update;
  ulong updated_rights;
  Table_op_error_code error;
  struct timeval password_change_timestamp;
  std::string second_cred;
};

class Acl_table_user_writer : public Acl_table {
 public:
  Acl_table_user_writer(THD *thd, TABLE *table, LEX_USER *combo, ulong rights,
                        bool revoke_grant, bool can_create_user,
                        Pod_user_what_to_update what_to_update);
  virtual ~Acl_table_user_writer();
  virtual Acl_table_op_status finish_operation(Table_op_error_code &error);
  Acl_table_user_writer_status driver();

  bool setup_table(int &error, bool &builtin_password);

  /* Set of functions to set user table data */
  bool update_authentication_info(Acl_table_user_writer_status &return_value);
  bool update_privileges(Acl_table_user_writer_status &return_value);
  bool update_ssl_properties();
  bool update_user_resources();
  bool update_password_expiry();
  bool update_account_locking();
  bool update_password_history();
  bool update_password_reuse();
  bool update_password_require_current();
  bool update_user_attributes(std::string &current_password,
                              Acl_table_user_writer_status &return_value);

  ulong get_user_privileges();
  std::string get_current_credentials();

 private:
  LEX_USER *m_combo;
  ulong m_rights;
  bool m_revoke_grant;
  bool m_can_create_user;
  Pod_user_what_to_update m_what_to_update;
  User_table_schema *m_table_schema;
};

/**
  mysql.user table reader. It reads all raws from table and create in-memory
  cache.
*/

class Acl_table_user_reader : public Acl_table {
 public:
  Acl_table_user_reader(THD *thd, TABLE *table);
  ~Acl_table_user_reader();
  bool driver();
  bool setup_table(bool &is_old_db_layout);
  bool read_row(bool &is_old_db_layout, bool &super_users_with_empty_plugin);
  virtual Acl_table_op_status finish_operation(Table_op_error_code &error);

  /* Set of function to read user table data */
  void reset_acl_user(ACL_USER &user);
  void read_account_name(ACL_USER &user);
  bool read_authentication_string(ACL_USER &user);
  void read_privileges(ACL_USER &user);
  void read_ssl_fields(ACL_USER &user);
  void read_user_resources(ACL_USER &user);
  bool read_plugin_info(ACL_USER &user, bool &super_users_with_empty_plugin,
                        bool &is_old_db_layout);
  bool read_password_expiry(ACL_USER &user, bool &password_expired);
  void read_password_locked(ACL_USER &user);
  void read_password_last_changed(ACL_USER &user);
  void read_password_lifetime(ACL_USER &user);
  void read_password_history_fields(ACL_USER &user);
  void read_password_reuse_time_fields(ACL_USER &user);
  void read_password_require_current(ACL_USER &user);
  bool read_user_attributes(ACL_USER &user);
  void add_row_to_acl_users(ACL_USER &user);

 private:
  User_table_schema *m_table_schema;
  READ_RECORD m_read_record_info;
  MEM_ROOT m_mem_root;
};

}  // namespace acl_table
#endif /* ACL_TABLE_USER_INCLUDED */
