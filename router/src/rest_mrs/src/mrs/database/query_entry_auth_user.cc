/*
 Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "mrs/database/query_entry_auth_user.h"

#include <cassert>

#include "helper/mysql_row.h"
#include "mrs/database/query_entries_auth_privileges.h"
#include "mrs/database/query_user_groups.h"

namespace mrs {
namespace database {

using AuthUser = QueryEntryAuthUser::AuthUser;

bool QueryEntryAuthUser::query_user(MySQLSession *session,
                                    const AuthUser *user_data) {
  loaded_ = 0;
  // TODO(lkotula): do a proper Query building, that resistant to SQL injection
  // (Shouldn't be in review)
  query_ = {
      "SELECT id, auth_app_id, name, email, vendor_user_id, login_permitted "
      "FROM mysql_rest_service_metadata.auth_user WHERE !=? ?"};

  query_ << (user_data->has_user_id ? "id" : "auth_app_id");
  query_ << (user_data->has_user_id ? user_data->user_id : user_data->app_id);

  do {
    //    if (user_data->has_user_id) {
    //      query_ << (mysqlrouter::sqlstring("and id=? ") <<
    //      user_data->user_id); break;
    //    }

    if (!user_data->vendor_user_id.empty()) {
      query_ << (mysqlrouter::sqlstring("and vendor_user_id=? ")
                 << user_data->vendor_user_id);
      break;
    }

    if (!user_data->email.empty()) {
      query_ << (mysqlrouter::sqlstring("and email=? ") << user_data->email);
      break;
    }

    if (!user_data->name.empty()) {
      query_ << (mysqlrouter::sqlstring("and name=? ") << user_data->name);
      break;
    }

    query_ << mysqlrouter::sqlstring{""};
  } while (false);

  query(session);

  /* What do do when loaded_ is greater than 1 ?*/
  if (loaded_ == 0) return false;

  QueryEntriesAuthPrivileges auth_privileges;
  auth_privileges.query_user(session, user_data_.user_id,
                             &user_data_.privileges);

  QueryUserGroups groups;
  groups.query_groups(session, user_data_.user_id, &user_data_.groups);

  return true;
}

uint64_t QueryEntryAuthUser::insert_user(
    MySQLSession *session, const AuthUser *user,
    const helper::Optional<uint64_t> &default_role_id) {
  assert(!user->has_user_id);
  query_ = {
      "INSERT INTO mysql_rest_service_metadata.auth_user(auth_app_id, name, "
      "email, vendor_user_id, login_permitted) VALUES(?, ?, ?, ?, ?)"};

  query_ << user->app_id << user->name << user->email << user->vendor_user_id
         << user->login_permitted;

  query(session);

  auto user_id = session->last_insert_id();

  if (default_role_id) {
    query_ = {
        "INSERT INTO mysql_rest_service_metadata.user_has_role(auth_user_id, "
        "auth_role_id, comments) VALUES(?, ?, \"Default role.\")"};
    query_ << user_id << default_role_id.value();
    query(session);
  }

  return user_id;
}

bool QueryEntryAuthUser::update_user(MySQLSession *session,
                                     const AuthUser *user) {
  assert(user->has_user_id);

  query_ = {
      "UPDATE mysql_rest_service_metadata.auth_user SET auth_app_id=?,name=?, "
      "email=?, vendor_user_id=?, login_permitted=? WHERE id=?"};

  query_ << user->app_id << user->name << user->email << user->vendor_user_id
         << user->login_permitted << user->user_id;

  query(session);
  return true;
}

void QueryEntryAuthUser::on_row(const Row &row) {
  if (row.size() < 1) return;

  helper::MySQLRow mysql_row(row);

  user_data_.has_user_id = true;
  mysql_row.unserialize(&user_data_.user_id);
  mysql_row.unserialize(&user_data_.app_id);
  mysql_row.unserialize(&user_data_.name);
  mysql_row.unserialize(&user_data_.email);
  mysql_row.unserialize(&user_data_.vendor_user_id);
  mysql_row.unserialize(&user_data_.login_permitted);

  ++loaded_;
}

const AuthUser &QueryEntryAuthUser::get_user() { return user_data_; }

}  // namespace database
}  // namespace mrs
