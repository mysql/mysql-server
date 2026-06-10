/*
 Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "mrs/http/session_manager.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "helper/generate_uuid.h"

namespace mrs {
namespace http {

using Session = SessionManager::Session;
using SessionIdType = SessionManager::SessionId;
using SessionPtr = SessionManager::SessionPtr;

SessionManager::Session::Session(SessionManager *session_manager,
                                 const SessionId id,
                                 const AuthorizationHandlerId &authorization_id,
                                 const std::string &holder_name)
    : owner{session_manager},
      id_{id},
      access_time_{system_clock::now()},
      create_time_{access_time_},
      authorization_handler_id_{authorization_id},
      holder_name_{holder_name} {}

void SessionManager::Session::enable_db_session_pool(
    uint32_t passthrough_pool_size) {
  db_session_pool =
      std::make_shared<collector::MysqlFixedPoolManager>(passthrough_pool_size);
}

SessionManager::SessionManager()
    : oldest_inactive_session_{system_clock::now()},
      oldest_session_{oldest_inactive_session_} {
  on_session_delete = [](const SessionPtr &) {};
}

void SessionManager::configure(const Configuration &config) {
  std::lock_guard<std::mutex> lck{mutex_};
  config_ = config;
}

SessionPtr SessionManager::new_session(
    const AuthorizationHandlerId authorize_handler_id,
    const std::string &holder_name) {
  std::lock_guard<std::mutex> lck{mutex_};
  sessions_.push_back(std::make_unique<Session>(
      this, generate_session_id_impl(), authorize_handler_id, holder_name));
  return sessions_.back();
}

SessionPtr SessionManager::new_session(const SessionId &session_id) {
  std::lock_guard<std::mutex> lck{mutex_};
  sessions_.push_back(std::make_unique<Session>(
      this, session_id, AuthorizationHandlerId{}, std::string()));
  return sessions_.back();
}

bool SessionManager::change_session_id(SessionPtr session,
                                       const SessionId &new_session_id) {
  std::lock_guard<std::mutex> lck{mutex_};

  if (get_session_impl(new_session_id)) return false;

  session->id_ = new_session_id;

  return true;
}

SessionPtr SessionManager::get_session_secondary_id(const SessionId &id) {
  std::lock_guard<std::mutex> lck{mutex_};
  auto result = get_session_handler_specific_id_impl(id);

  return result;
}

SessionPtr SessionManager::get_session(const SessionId &id) {
  std::lock_guard<std::mutex> lck{mutex_};
  auto result = get_session_impl(id);
  if (result) {
    auto tp = result->update_access_time();
    if (tp < oldest_inactive_session_) oldest_inactive_session_ = tp;
  }

  return result;
}

bool SessionManager::remove_session(const SessionId session_id) {
  std::lock_guard<std::mutex> lck{mutex_};
  auto it = std::find_if(sessions_.begin(), sessions_.end(),
                         [&session_id](const auto &item) {
                           return item.get()->get_session_id() == session_id;
                         });

  if (it != sessions_.end()) {
    on_session_delete(*it);
    sessions_.erase(it);
    return true;
  }

  return false;
}

void SessionManager::remove_timeouted() {
  std::lock_guard<std::mutex> lck{mutex_};
  remove_timeouted_impl();
}

void SessionManager::remove_session(const Session::SessionData *session_data) {
  assert(session_data->internal_session);
  remove_session_impl(session_data->internal_session);
}

bool SessionManager::remove_session(const SessionPtr &session) {
  return remove_session_impl(session.get());
}

bool SessionManager::remove_session_impl(const Session *session) {
  std::lock_guard<std::mutex> lck{mutex_};
  auto it = std::find_if(
      sessions_.begin(), sessions_.end(),
      [session](const auto &item) { return item.get() == session; });

  if (it != sessions_.end()) {
    on_session_delete(*it);
    sessions_.erase(it);
    return true;
  }

  return false;
}

SessionPtr SessionManager::get_session_handler_specific_id_impl(
    const SessionId &id) {
  remove_timeouted_impl();

  for (auto &session : sessions_) {
    if (session->handler_secondary_id == id) {
      return session;
    }
  }

  return {};
}

SessionPtr SessionManager::get_session_impl(const SessionId &id) {
  remove_timeouted_impl();

  for (auto &session : sessions_) {
    if (session->get_session_id() == id) {
      return session;
    }
  }

  return {};
}

std::string SessionManager::generate_session_id_impl() {
  SessionId new_id;
  do {
    new_id = helper::to_uuid_string(helper::generate_uuid_v4());
  } while (get_session_impl(new_id));

  return new_id;
}

void SessionManager::remove_timeouted_impl() {
  using namespace std::chrono_literals;

  auto time = system_clock::now();
  remove_inactive_impl(time);
  remove_expired_impl(time);
}

void SessionManager::remove_inactive_impl(const system_clock::time_point &now) {
  if (config_.inactivity_timeout.has_value()) {
    if (now - oldest_inactive_session_ >= config_.inactivity_timeout.value()) {
      auto it = sessions_.begin();
      while (it != sessions_.end()) {
        if ((*it)->has_access_timeout(config_.inactivity_timeout.value())) {
          on_session_delete(*it);
          it = sessions_.erase(it);
          continue;
        }

        ++it;
      }

      oldest_inactive_session_ = now;
      for (const auto &s : sessions_) {
        if (oldest_inactive_session_ > s->get_access_time())
          oldest_inactive_session_ = s->get_access_time();
      }
    }
  }
}

void SessionManager::remove_expired_impl(const system_clock::time_point &now) {
  if (now - oldest_session_ >= config_.expire_timeout) {
    oldest_session_ = now;
    auto it = sessions_.begin();
    while (it != sessions_.end()) {
      if ((*it)->is_expired(config_.expire_timeout)) {
        on_session_delete(*it);
        it = sessions_.erase(it);
        continue;
      }
      ++it;
    }

    oldest_session_ = now;
    for (const auto &s : sessions_) {
      if (oldest_session_ > s->get_create_time())
        oldest_session_ = s->get_create_time();
    }
  }
}

}  // namespace http
}  // namespace mrs
