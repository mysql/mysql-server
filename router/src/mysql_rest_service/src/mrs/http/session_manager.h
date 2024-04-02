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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_REST_SESSION_MANAGER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_REST_SESSION_MANAGER_H_

#include <chrono>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

#include "mrs/database/entry/auth_user.h"
#include "mrs/database/entry/universal_id.h"

namespace mrs {
namespace http {

class SessionManager {
 public:
  using AuthUser = mrs::database::entry::AuthUser;
  using SessionId = std::string;
  using system_clock = std::chrono::system_clock;
  using AuthorizationHandlerId = mrs::database::entry::UniversalId;

  enum Allocation { OnlyExisting = 0, CreateWhenNotExisting = 1 };

  class Session {
   public:
    class SessionData {
     public:
      virtual ~SessionData() = default;
      Session *internal_session{nullptr};
    };

    enum State {
      kUninitialized,
      kWaitingForCode,
      kGettingTokken,
      kTokenVerified,
      kUserVerified
    };

   public:
    Session(const SessionId id, const AuthorizationHandlerId &authorization);

    template <typename Derived>
    Derived *get_data() {
      static_assert(std::is_base_of<SessionData, Derived>::value);

      return dynamic_cast<Derived *>(data_.get());
    }

    void set_data(SessionData *data) {
      data_.reset(data);
      data_->internal_session = this;
    }

    AuthorizationHandlerId get_authorization_handler_id() const {
      return authorization_handler_id_;
    }

    const SessionId &get_session_id() const { return id_; }

    system_clock::time_point get_access_time() const { return access_time_; }

    system_clock::time_point update_access_time() {
      return access_time_ = system_clock::now();
    }

    bool has_timeouted(system_clock::duration timeout) const {
      return access_time_ + timeout <= system_clock::now();
    }

    bool generate_token{false};
    State state{kUninitialized};
    std::string users_on_complete_url_redirection;
    std::string users_on_complete_timeout;
    std::string handler_name;
    AuthUser user;

   private:
    std::unique_ptr<SessionData> data_;
    SessionId id_;
    system_clock::time_point access_time_;
    AuthorizationHandlerId authorization_handler_id_{0};
  };

  using SessionPtr = std::unique_ptr<Session>;

 public:
  Session *get_session(const SessionId &id);
  Session *new_session(const AuthorizationHandlerId id);
  Session *new_session(const SessionId &session_id);

  void remove_session(const Session::SessionData *session_data);
  void remove_session(const Session *session);
  bool remove_session(const SessionId session);
  void remove_timeouted();
  system_clock::duration get_timeout() { return timeout_; }

 private:
  // Methods with postfix "_impl" at end of method name, marks that the methods
  // doesn't use mutexes, thus it should be used after locking `mutex_` object.
  Session *get_session_impl(const SessionId &id);
  SessionId generate_session_id_impl();
  void remove_timeouted_impl();

  std::vector<SessionPtr> sessions_;
  // TODO(lkotula): Temporary, remove after proper implementation of generate_id
  // (Shouldn't be in review)
  uint64_t last_session_id_{0};
  std::mutex mutex_;
  system_clock::time_point oldest_session_;
  // TODO(lkotula): Make the `timeout_` a configurable value by user (Shouldn't
  // be in review)
  system_clock::duration timeout_{std::chrono::minutes(15)};
};

}  // namespace http
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_REST_SESSION_MANAGER_H_
