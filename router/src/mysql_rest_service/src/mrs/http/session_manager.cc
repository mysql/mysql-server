/*
 Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "mrs/http/session_manager.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace mrs {
namespace http {

static std::string current_timestamp() {
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
  return ss.str();
}

using Session = SessionManager::Session;
using SessionIdType = SessionManager::SessionId;

SessionManager::Session::Session(const SessionId id,
                                 const AuthorizationHandlerId &authorization_id)
    : id_{id},
      access_time_{system_clock::now()},
      authorization_handler_id_{authorization_id} {}

Session *SessionManager::new_session(
    const AuthorizationHandlerId authorize_handler_id) {
  std::lock_guard<std::mutex> lck{mutex_};
  sessions_.push_back(std::make_unique<Session>(generate_session_id_impl(),
                                                authorize_handler_id));
  return sessions_.back().get();
}

Session *SessionManager::new_session(const SessionId &session_id) {
  std::lock_guard<std::mutex> lck{mutex_};
  sessions_.push_back(
      std::make_unique<Session>(session_id, AuthorizationHandlerId{}));
  return sessions_.back().get();
}

Session *SessionManager::get_session(const SessionId &id) {
  std::lock_guard<std::mutex> lck{mutex_};
  auto result = get_session_impl(id);
  if (result) {
    auto tp = result->update_access_time();
    if (tp < oldest_session_) oldest_session_ = tp;
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
  remove_session(session_data->internal_session);
}

void SessionManager::remove_session(const Session *session) {
  std::lock_guard<std::mutex> lck{mutex_};
  auto it = std::find_if(
      sessions_.begin(), sessions_.end(),
      [session](const auto &item) { return item.get() == session; });

  if (it != sessions_.end()) sessions_.erase(it);
}

Session *SessionManager::get_session_impl(const SessionId &id) {
  Session *result{nullptr};

  remove_timeouted_impl();

  for (auto &session : sessions_) {
    if (session->get_session_id() == id) {
      result = session.get();
      break;
    }
  }

  return result;
}

std::string SessionManager::generate_session_id_impl() {
  // TODO(lkotula): We need something random here, that user can't guess.
  // (Shouldn't be in review)
  SessionId new_id;
  do {
    new_id = current_timestamp() + "-" + std::to_string(last_session_id_++);
  } while (get_session_impl(new_id));

  return new_id;
}

void SessionManager::remove_timeouted_impl() {
  using namespace std::chrono_literals;

  auto time = system_clock::now();

  if (time - oldest_session_ >= timeout_) {
    oldest_session_ = time;

    auto it = sessions_.begin();
    while (it != sessions_.end()) {
      if ((*it)->has_timeouted(timeout_)) {
        it = sessions_.erase(it);
        continue;
      }

      if (oldest_session_ > (*it)->get_access_time())
        oldest_session_ = (*it)->get_access_time();
      ++it;
    }
  }
}

}  // namespace http
}  // namespace mrs
