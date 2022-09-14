/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.
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

#ifndef SQL_MFA_INCLUDED
#define SQL_MFA_INCLUDED

#include <string>
#include <vector>

#include "sql-common/json_dom.h"  // Json_array
#include "sql/auth/user_table.h"
#include "sql/mem_root_allocator.h"
#include "sql/sql_class.h"
#include "sql/table.h"

enum class nthfactor { NONE = 1, SECOND_FACTOR, THIRD_FACTOR };

class Multi_factor_auth_list;
class Multi_factor_auth_info;

/**
  An interface to access information about Multi factor authentication
  methods. This interface represents a chain of authentication plugins
  for a given user account.
*/
class I_multi_factor_auth {
 public:
  virtual ~I_multi_factor_auth() = default;
  /**
    Helper methods to verify and update ALTER USER sql
    when altering Multi factor authentication methods.
  */
  virtual bool is_alter_allowed(THD *, LEX_USER *) { return false; }
  virtual void alter_mfa(I_multi_factor_auth *) {}
  /**
    Helper method to validate Multi factor authentication methods.
  */
  virtual bool validate_plugins_in_auth_chain(THD *thd) = 0;
  /**
    Helper method to validate Multi factor authentication methods are
    correct compared to authentication policy.
  */
  virtual bool validate_against_authentication_policy(THD *) { return false; }
  /**
    method to add/delete Multi factor authentication methods in user_attributes
    column.
  */
  virtual bool update_user_attributes() = 0;
  virtual void add_factor(I_multi_factor_auth *m [[maybe_unused]]) {}
  /**
    Helper methods to convert this interface into a valid JSON object
    and vice versa.
  */
  virtual bool serialize(Json_array &mfa_arr) = 0;
  virtual bool deserialize(uint f, Json_dom *mfa_dom) = 0;
  /**
    Helper methods to do registration step.
  */
  virtual bool init_registration(THD *, uint) = 0;
  virtual bool finish_registration(THD *, LEX_USER *, uint) = 0;
  virtual bool is_passwordless() = 0;

  /**
    Fill needed info in LEX_USER::mfa_list for query rewrite
  */
  virtual void get_info_for_query_rewrite(THD *, LEX_USER *) = 0;
  /**
    Fill in generated passwords from respective Multi factor authentication
    methods
  */
  virtual void get_generated_passwords(Userhostpassword_list &gp, const char *u,
                                       const char *h) = 0;
  /**
    Fill in server challenge generated as part of initiate registration step.
  */
  virtual void get_server_challenge(std::vector<std::string> &sc) = 0;
  /**
    Get methods.
  */
  Multi_factor_auth_list *get_multi_factor_auth_list() {
    return down_cast<Multi_factor_auth_list *>(this);
  }

  Multi_factor_auth_info *get_multi_factor_auth_info() {
    return down_cast<Multi_factor_auth_info *>(this);
  }
};

template <typename T>
using my_vector = std::vector<T, Mem_root_allocator<T>>;

class Multi_factor_auth_list : public I_multi_factor_auth {
 private:
  /* multi_factor_auth hierarchy */
  my_vector<I_multi_factor_auth *> m_factor;

 public:
  Multi_factor_auth_list(MEM_ROOT *);
  ~Multi_factor_auth_list() override;
  my_vector<I_multi_factor_auth *> &get_mfa_list();
  size_t get_mfa_list_size();
  bool is_alter_allowed(THD *, LEX_USER *) override;
  void alter_mfa(I_multi_factor_auth *) override;
  bool validate_plugins_in_auth_chain(THD *thd) override;
  bool validate_against_authentication_policy(THD *thd) override;
  bool update_user_attributes() override;
  void add_factor(I_multi_factor_auth *m) override;
  bool serialize(Json_array &mfa_arr) override;
  bool deserialize(uint f, Json_dom *mfa_dom) override;
  bool init_registration(THD *, uint) override;
  bool finish_registration(THD *, LEX_USER *, uint) override;
  bool is_passwordless() override;
  void get_info_for_query_rewrite(THD *, LEX_USER *) override;
  void get_generated_passwords(Userhostpassword_list &gp, const char *u,
                               const char *h) override;
  void get_server_challenge(std::vector<std::string> &sc) override;

 private:
  /*
    This methods ensures that hierarchy of m_factor is always
    2FA followed by 3FA.
  */
  void sort_mfa();
};

/*
  This class represents each individual factor from chain of
  authentication plugins for a given user account.
*/
class Multi_factor_auth_info : public I_multi_factor_auth {
 private:
  MEM_ROOT *m_mem_root;
  LEX_MFA *m_multi_factor_auth;
  acl_table::Pod_user_what_to_update m_update;

 public:
  Multi_factor_auth_info(MEM_ROOT *mem_root);
  Multi_factor_auth_info(MEM_ROOT *mem_root, LEX_MFA *m);
  ~Multi_factor_auth_info() override {}
  /* validate Multi factor authentication plugins during ACL DDL */
  bool validate_plugins_in_auth_chain(THD *thd) override;
  /* update user attributes */
  bool update_user_attributes() override;
  /* construct json object out of user attributes column */
  bool serialize(Json_array &mfa_arr) override;
  bool deserialize(uint f, Json_dom *mfa_dom) override;
  /* helper methods to do registration */
  bool init_registration(THD *, uint) override;
  bool finish_registration(THD *, LEX_USER *, uint) override;
  bool is_passwordless() override;
  void get_info_for_query_rewrite(THD *, LEX_USER *) override;
  void get_generated_passwords(Userhostpassword_list &gp, const char *u,
                               const char *h) override;
  void get_server_challenge(std::vector<std::string> &sc) override;

  /* during ALTER USER copy attributes from ACL_USER */
  Multi_factor_auth_info &operator=(Multi_factor_auth_info &new_af);

 private:
  /*
    validate Multi factor authentication attributes read from row of
    mysql.user table
  */
  bool validate_row();

 public:
  bool is_identified_by();
  bool is_identified_with();
  LEX_CSTRING &plugin_name();

  const char *get_auth_str();
  size_t get_auth_str_len();

  const char *get_plugin_str();
  size_t get_plugin_str_len();

  const char *get_generated_password_str();
  size_t get_generated_password_len();

  nthfactor get_factor();
  unsigned int get_nth_factor();
  bool is_add_factor();
  bool is_drop_factor();
  bool is_modify_factor();

  bool get_init_registration();
  bool get_finish_registration();
  bool get_requires_registration();
  bool get_unregister();
  LEX_MFA *get_lex_mfa();

  void set_auth_str(const char *, size_t);
  void set_plugin_str(const char *, size_t);
  void set_generated_password(const char *, size_t);
  void set_factor(nthfactor f);
  void set_passwordless(int v);
  void set_init_registration(bool v);
  void set_finish_registration(bool v);
  void set_requires_registration(int v);

  std::string get_command_string(enum_sql_command sql_command);
};

#endif /* SQL_MFA_INCLUDED */
