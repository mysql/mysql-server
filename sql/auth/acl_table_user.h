/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#ifndef ACL_TABLE_USER_INCLUDED
#define ACL_TABLE_USER_INCLUDED

#include "my_config.h"

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <sys/types.h>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "my_alloc.h"
#include "sql-common/json_dom.h"
#include "sql/auth/acl_table_base.h"
#include "sql/auth/partial_revokes.h"
#include "sql/auth/sql_mfa.h" /* I_multi_factor_auth */
#include "sql/auth/user_table.h"

class ACL_USER;
class RowIterator;
class THD;
class User_table_schema;
struct LEX_USER;
struct TABLE;

namespace acl_table {
enum class User_attribute_type {
  ADDITIONAL_PASSWORD = 0,
  RESTRICTIONS,
  PASSWORD_LOCKING,
  METADATA,
  COMMENT,
  MULTI_FACTOR_AUTHENTICATION_DATA
};

struct Password_lock {
  /**
     read from the user config. The number of days to keep the account locked
  */
  int password_lock_time_days{};
  /**
    read from the user config. The number of failed login attempts before the
    account is locked
  */
  uint failed_login_attempts{};

  Password_lock();

  Password_lock &operator=(const Password_lock &other);

  Password_lock &operator=(Password_lock &&other);

  Password_lock(const Password_lock &other);

  Password_lock(Password_lock &&other);
};

/**
  Class to handle information stored in mysql.user.user_attributes
*/
class Acl_user_attributes {
 public:
  /**
    Default constructor.
  */
  Acl_user_attributes(MEM_ROOT *mem_root, bool read_restrictions,
                      Auth_id &auth_id, Access_bitmask global_privs);

  Acl_user_attributes(MEM_ROOT *mem_root, bool read_restrictions,
                      Auth_id &auth_id, Restrictions *m_restrictions,
                      I_multi_factor_auth *mfa);

  ~Acl_user_attributes();

 public:
  /**
    Obtain info from JSON representation of user attributes

    @param [in] json_object JSON object that holds user attributes

    @returns status of parsing json_object
      @retval false Success
      @retval true  Error parsing the JSON object
  */
  bool deserialize(const Json_object &json_object);

  /**
    Create JSON object from user attributes

    @param [out] json_object Object to store serialized user attributes

    @returns status of serialization
      @retval false Success
      @retval true  Error serializing user attributes
  */
  bool serialize(Json_object &json_object) const;

  /**
    Update second password for user. We replace existing one if any.

    @param [in] credential Second password

    @returns status of password update
      @retval false Success
      @retval true  Error. Second password is empty
  */
  bool update_additional_password(std::string &credential);

  /**
    Discard second password.
  */
  void discard_additional_password();

  /**
    Get second password

    @returns second password
  */
  const std::string get_additional_password() const;

  /**
    Get the restriction list for the user

    @returns Restriction list
  */
  Restrictions get_restrictions() const;

  void update_restrictions(const Restrictions &restricitions);

  auto get_failed_login_attempts() const {
    return m_password_lock.failed_login_attempts;
  }
  auto get_password_lock_time_days() const {
    return m_password_lock.password_lock_time_days;
  }
  auto get_password_lock() const { return m_password_lock; }
  void set_password_lock(Password_lock password_lock) {
    m_password_lock = password_lock;
  }

  I_multi_factor_auth *get_mfa() { return m_mfa; }
  void set_mfa(I_multi_factor_auth *mfa) { m_mfa = mfa; }

  /**
    Take over ownership of the json pointer.
    @return Error state
      @retval true An error occurred
      @retval false Success
  */
  bool consume_user_attributes_json(Json_dom_ptr json);

 private:
  void report_and_remove_invalid_db_restrictions(
      DB_restrictions &db_restrictions, Access_bitmask mask,
      enum loglevel level, ulonglong errcode);
  bool deserialize_password_lock(const Json_object &json_object);
  bool deserialize_multi_factor(const Json_object &json_object);

 private:
  /** Mem root */
  MEM_ROOT *m_mem_root;
  /** Operation for restrictions */
  bool m_read_restrictions;
  /** Auth ID */
  Auth_id m_auth_id;
  /** Second password for user */
  std::string m_additional_password;
  /** Restrictions_list on certain databases for user */
  Restrictions m_restrictions;
  /** Global static privileges */
  Access_bitmask m_global_privs;
  /** password locking */
  Password_lock m_password_lock;
  /** multi factor auth info */
  I_multi_factor_auth *m_mfa;
  /** Save the original json object */
  Json_dom_ptr m_user_attributes_json;
};

// Forward and alias declarations
using acl_table_user_writer_status =
    std::pair<Table_op_error_code, struct timeval>;

/**
  mysql.user table writer. It updates or drop a one single row from the table.
*/

class Acl_table_user_writer_status {
 public:
  Acl_table_user_writer_status();
  Acl_table_user_writer_status(bool skip, Access_bitmask rights,
                               Table_op_error_code err,
                               my_timeval pwd_timestamp, std::string cred,
                               Password_lock &password_lock,
                               I_multi_factor_auth *multi_factor)
      : skip_cache_update(skip),
        updated_rights(rights),
        error(err),
        password_change_timestamp(pwd_timestamp),
        second_cred(cred),
        restrictions(),
        password_lock(password_lock),
        multi_factor(multi_factor) {}

  bool skip_cache_update;
  Access_bitmask updated_rights;
  Table_op_error_code error;
  my_timeval password_change_timestamp;
  std::string second_cred;
  Restrictions restrictions;
  Password_lock password_lock;
  I_multi_factor_auth *multi_factor;
};

class Acl_table_user_writer : public Acl_table {
 public:
  Acl_table_user_writer(THD *thd, TABLE *table, LEX_USER *combo,
                        Access_bitmask rights, bool revoke_grant,
                        bool can_create_user,
                        Pod_user_what_to_update what_to_update,
                        Restrictions *restrictions, I_multi_factor_auth *mfa);
  ~Acl_table_user_writer() override;
  Acl_table_op_status finish_operation(Table_op_error_code &error) override;
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

  void replace_user_application_user_metadata(
      std::function<bool(TABLE *table)> const &update);
  Access_bitmask get_user_privileges();
  std::string get_current_credentials();

 private:
  bool update_user_application_user_metadata();
  bool write_user_attributes_column(const Acl_user_attributes &user_attributes);
  bool m_has_user_application_user_metadata;
  LEX_USER *m_combo;
  Access_bitmask m_rights;
  bool m_revoke_grant;
  bool m_can_create_user;
  Pod_user_what_to_update m_what_to_update;
  User_table_schema *m_table_schema;
  Restrictions *m_restrictions;
  I_multi_factor_auth *m_mfa;
  std::function<bool(TABLE *table)> m_user_application_user_metadata;
};

/**
  mysql.user table reader. It reads all raws from table and create in-memory
  cache.
*/

class Acl_table_user_reader : public Acl_table {
 public:
  Acl_table_user_reader(THD *thd, TABLE *table);
  ~Acl_table_user_reader() override;
  bool driver();
  bool setup_table(bool &is_old_db_layout);
  bool read_row(bool &super_users_with_empty_plugin);
  Acl_table_op_status finish_operation(Table_op_error_code &error) override;

  /* Set of function to read user table data */
  void reset_acl_user(ACL_USER &user);
  void read_account_name(ACL_USER &user);
  bool read_authentication_string(ACL_USER &user);
  void read_privileges(ACL_USER &user);
  void read_ssl_fields(ACL_USER &user);
  void read_user_resources(ACL_USER &user);
  bool read_plugin_info(ACL_USER &user, bool &super_users_with_empty_plugin);
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
  User_table_schema *m_table_schema = nullptr;
  unique_ptr_destroy_only<RowIterator> m_iterator;
  MEM_ROOT m_mem_root{PSI_NOT_INSTRUMENTED, ACL_ALLOC_BLOCK_SIZE};
  Restrictions *m_restrictions = nullptr;
  Json_object *m_user_application_user_metadata_json = nullptr;
};

}  // namespace acl_table
#endif /* ACL_TABLE_USER_INCLUDED */
