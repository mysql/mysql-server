/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "mrs/users/user_manager.h"
#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace users {

using AuthUser = UserManager::AuthUser;

namespace {

class NoLock {
 public:
  NoLock(std::mutex &) {}
  NoLock(std::shared_mutex &) {}
};

using WriteLock = std::unique_lock<std::shared_mutex>;
using ReadLock = std::shared_lock<std::shared_mutex>;

}  // namespace

void UserManager::user_invalidate(const UserId id) {
  WriteLock lock(mutex_user_cache_);
  user_cache_.remove({id});
}

AuthUser *UserManager::cache_get(AuthUser *out_user, bool *out_is_different) {
  AuthUser *result = nullptr;

  result = user_cache_.get_cached_value(UserIndex(*out_user));
  if (result) return result;

  auto &container = user_cache_.get_container();
  log_debug("input: %s", to_string(*out_user).c_str());
  for (auto &kv : container) {
    auto &value = kv.second;
    log_debug("C: %s", to_string(value).c_str());
    if (out_user->match_other_fields(value)) {
      *out_is_different = true;
      return &value;
    }
  }

  return nullptr;
}

bool UserManager::user_get(AuthUser *out_user, SqlSessionCache *out_cache) {
  AuthUser tmp_user;
  AuthUser *found_user = nullptr;
  bool needs_update = false;

  log_debug("user_get %s", to_string(*out_user).c_str());
  {
    ReadLock lock{mutex_user_cache_};
    found_user = cache_get(out_user, &needs_update);
    if (found_user) {
      if (!needs_update) {
        *out_user = *found_user;
        return true;
      }

      // We are releasing read-lock at the block end,
      // thus we need to copy the data.
      tmp_user = *found_user;
      found_user = &tmp_user;
    }
  }

  WriteLock lock(mutex_user_cache_);
  if (!found_user) {
    log_debug("user not found in the cache");
    found_user = cache_get(out_user, &needs_update);

    if (found_user) {
      if (!needs_update && found_user->login_permitted) {
        log_debug("second attempt, found in the cache");
        *out_user = *found_user;
        return true;
      }
    }
  }

  if (!found_user) {
    log_debug("Looking inside DB");
    found_user = query_user(out_cache, out_user, &needs_update);

    if (found_user) {
      if (!needs_update && found_user->login_permitted) {
        log_debug("found in DB");
        *out_user = *found_user;
        return true;
      }
    }
  }

  if (found_user && !found_user->login_permitted) {
    log_debug("User not permitted to login");
    return false;
  }

  if (needs_update) {
    // Copy/preserve data that are not provided by remote
    out_user->login_permitted = found_user->login_permitted;
    out_user->privileges = found_user->privileges;

    log_debug("Updating user from %s to %s", to_string(*found_user).c_str(),
              to_string(*out_user).c_str());
    return query_update_user(out_cache, found_user->user_id, out_user);
  }

  if (limit_to_existing_users_) {
    return false;
  }

  log_debug("Inserting user");
  return query_insert_user(out_cache, out_user);
}

AuthUser *UserManager::query_user(SqlSessionCache *out_cache,
                                  AuthUser *out_user, bool *is_different) {
  database::QueryEntryAuthUser user_query;
  if (!user_query.query_user(out_cache->get(), out_user)) return nullptr;

  auto &user = user_query.get_user();
  auto result = user_cache_.set(user, user);

  *is_different = !(*out_user == user_query.get_user());

  return result;
}

bool UserManager::query_update_user(SqlSessionCache *out_cache, const UserId id,
                                    AuthUser *user) {
  user->has_user_id = true;
  user->user_id = id;

  database::QueryEntryAuthUser user_query;
  return user_query.update_user(out_cache->get(), user);
}

bool UserManager::query_insert_user(SqlSessionCache *out_cache,
                                    AuthUser *user) {
  database::QueryEntryAuthUser user_query;
  auto user_id =
      user_query.insert_user(out_cache->get(), user, default_role_id_);
  user->has_user_id = true;
  user->user_id = user_id;

  if (default_role_id_) {
    bool different;
    auto u = query_user(out_cache, user, &different);

    if (!u) return false;
    *user = *u;
    return true;
  }

  user_cache_.set(UserIndex(*user), *user);

  return true;
}

}  // namespace users
}  // namespace mrs
