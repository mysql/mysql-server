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

#include "mrs/users/user_manager.h"
#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace users {

using AuthUser = UserManager::AuthUser;

namespace {

bool should_update_db_entry(const AuthUser &provided, const AuthUser &db) {
  //  assert(!provided.has_user_id);

  // The user manager can update, only following fields.
  if (provided.name != db.name) return true;
  if (provided.email != db.email) return true;
  if (provided.vendor_user_id != db.vendor_user_id) return true;

  return false;
}

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
  UserIndex idx(*out_user);

  result = user_cache_.get_cached_value(idx);
  if (result) return result;

  if (out_user->email.empty() && out_user->name.empty()) return nullptr;

  auto &container = user_cache_.get_container();
  log_debug("input: %s", to_string(*out_user).c_str());
  for (auto &kv : container) {
    auto &value = kv.second;
    bool is_different = false;

    if (!out_user->email.empty()) {
      if (out_user->email == value.email)
        result = &value;
      else
        is_different = true;
    }

    if (!out_user->name.empty()) {
      if (out_user->name == value.name)
        result = &value;
      else
        is_different = true;
    }

    if (result && out_is_different) {
      *out_is_different = is_different;
      break;
    }
  }

  return result;
}

bool UserManager::user_get_by_id(UserId user_id, AuthUser *out_user,
                                 SqlSessionCache *out_cache) {
  bool needs_update = false;
  ReadLock lock{mutex_user_cache_};
  out_user->has_user_id = true;
  out_user->user_id = user_id;
  auto found_user = cache_get(out_user, &needs_update);

  if (!found_user) {
    found_user = query_user(out_cache, out_user, &needs_update);
  }

  if (!found_user || !found_user->login_permitted) return false;

  *out_user = *found_user;

  return true;
}

bool UserManager::user_get(AuthUser *out_user, SqlSessionCache *out_cache,
                           const bool update_changed) {
  assert(!out_user->has_user_id &&
         "Search shouldn't be done by ID. The class provides other methods to "
         "achieve this.");
  AuthUser tmp_user;
  AuthUser *found_user = nullptr;
  bool needs_update = false;
  bool *needs_update_ptr = (update_changed ? &needs_update : nullptr);

  log_debug("user_get %s, update_changed=%s", to_string(*out_user).c_str(),
            (update_changed ? "true" : "false"));
  {
    ReadLock lock{mutex_user_cache_};
    found_user = cache_get(out_user, needs_update_ptr);
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
    found_user = cache_get(out_user, needs_update_ptr);

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
    found_user = query_user(out_cache, out_user, needs_update_ptr);
    if (found_user) {
      log_debug("found in DB");
      if (!needs_update && found_user->login_permitted) {
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
    out_user->auth_string = found_user->auth_string;
    out_user->groups = found_user->groups;
    out_user->options = found_user->options;

    log_debug("Updating user from %s to %s", to_string(*found_user).c_str(),
              to_string(*out_user).c_str());
    return query_update_user(out_cache, found_user->user_id, out_user);
  }

  if (limit_to_existing_users_) {
    return false;
  }

  // TODO(lkotula): It will bring trouble (Shouldn't be in review)
  std::size_t p = out_user->vendor_user_id.find("@");
  if (std::string::npos != p && out_user->name.empty()) {
    out_user->name = out_user->vendor_user_id.substr(0, p);
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

  if (is_different)
    *is_different = should_update_db_entry(*out_user, user_query.get_user());

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
