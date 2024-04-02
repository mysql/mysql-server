/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_USERS_USER_MANAGIER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_USERS_USER_MANAGIER_H_

#include <shared_mutex>

#include "helper/cache/cache.h"
#include "helper/cache/policy/lru.h"
#include "mrs/database/entry/auth_user.h"
#include "mrs/database/query_entry_auth_user.h"
#include "mrs/interface/authorize_handler.h"

namespace mrs {
namespace users {

class UserManager {
 public:
  using PolicyLru = helper::cache::policy::Lru;
  using AuthUser = database::entry::AuthUser;
  using UserId = AuthUser::UserId;
  using UserIndex = AuthUser::UserIndex;
  using Cache = helper::cache::Cache<UserIndex, AuthUser, 100, PolicyLru>;
  using Handler = mrs::interface::AuthorizeHandler;
  using SqlSessionCache = Handler::SqlSessionCached;

 public:
  UserManager(const bool limit_to_existing_users,
              const helper::Optional<UniversalId> &default_role_id)
      : limit_to_existing_users_{limit_to_existing_users},
        default_role_id_{default_role_id} {}

  bool user_get_by_id(UserId user_id, AuthUser *out_user,
                      SqlSessionCache *out_cache);
  /**
   * Find the user data inside a cache or DB.
   *
   * If the user entry provided to the function differs from
   * the entry found (cache/db), then the DB entry is updated.
   * This behavior is provided for account that are
   * imported/managed by other sources like in case of OAUTH2.
   */
  bool user_get(AuthUser *out_user, SqlSessionCache *out_cache,
                const bool update_changed = true);
  void user_invalidate(const UserId id);

 private:
  bool query_update_user(SqlSessionCache *out_cache, const UserId user_id,
                         AuthUser *out_user);
  bool query_insert_user(SqlSessionCache *out_cache, AuthUser *out_user);
  AuthUser *query_user(SqlSessionCache *out_cache, AuthUser *out_user,
                       bool *out_is_different);
  AuthUser *cache_get(AuthUser *out_user, bool *out_is_different);

  std::shared_mutex mutex_query_database_;
  std::shared_mutex mutex_user_cache_;
  Cache user_cache_;
  bool limit_to_existing_users_;
  const helper::Optional<UniversalId> default_role_id_;
};

}  // namespace users
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_USERS_USER_MANAGIER_H_
