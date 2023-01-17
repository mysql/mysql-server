/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef ACL_CHANGE_NOTIFICATION_INCLUDED
#define ACL_CHANGE_NOTIFICATION_INCLUDED

#include "my_sqlcommand.h"    // enum_sql_command
#include "sql/sql_rewrite.h"  // User_params
#include "sql/table.h"        // LEX_USER, LEX_CSTRING, List

class Acl_change_notification {
 public:
  struct Priv : public std::string {
    Priv(const LEX_CSTRING &lex_priv)
        : std::string{lex_priv.str, lex_priv.length} {}
  };
  struct User {
    std::string name;
    std::string host;
    User(const LEX_USER &lex_user)
        : name(lex_user.user.str, lex_user.user.length),
          host(lex_user.host.str, lex_user.host.length) {}
  };
  Acl_change_notification(THD *thd, enum_sql_command op,
                          const List<LEX_USER> *users,
                          std::set<LEX_USER *> *rewrite_params,
                          const List<LEX_CSTRING> *dynamic_privs);

 private:
  const std::string db;
  const enum_sql_command operation;
  const List<LEX_USER> empty_users;
  const List<LEX_USER> &users;
  const User_params rewrite_user_params;
  const List<LEX_CSTRING> empty_dynamic_privs;
  const List<LEX_CSTRING> &dynamic_privs;

 public:
  enum_sql_command get_operation() const { return operation; }
  const std::string &get_db() const { return db; }
  auto &get_user_list() const { return users; }
  auto &get_dynamic_privilege_list() const { return dynamic_privs; }
  const User_params *get_rewrite_params() const {
    if (rewrite_user_params.users == nullptr) return nullptr;
    return &rewrite_user_params;
  }
};

#endif
