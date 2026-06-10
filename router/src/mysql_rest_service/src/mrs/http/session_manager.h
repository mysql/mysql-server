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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_REST_SESSION_MANAGER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_REST_SESSION_MANAGER_H_

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <vector>

#include "collector/mysql_fixed_pool_manager.h"
#include "mrs/database/entry/auth_user.h"
#include "mrs/database/entry/universal_id.h"

namespace mrs {

// The following timeout constants are expressed in minutes.
const uint64_t k_maximum_expire_timeout{43200};
const uint64_t k_maximum_inactivity_timeout{43200};
const uint64_t k_default_expire_timeout{15};

const uint32_t k_default_passthrough_max_sessions_per_user{10};

namespace http {

class SessionManager {
 public:
  using AuthUser = mrs::database::entry::AuthUser;
  using SessionId = std::string;
  using system_clock = std::chrono::system_clock;
  using AuthorizationHandlerId = mrs::database::entry::UniversalId;
  using minutes = std::chrono::minutes;
  using CachedSession = collector::MysqlFixedPoolManager::CachedObject;

  class Configuration {
   public:
    minutes expire_timeout{k_default_expire_timeout};
    std::optional<minutes> inactivity_timeout{};

    uint32_t max_passthrough_sessions_per_user{
        k_default_passthrough_max_sessions_per_user};
  };

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
    Session(SessionManager *owner, const SessionId id,
            const AuthorizationHandlerId &authorization,
            const std::string &holder_name);

    template <typename Derived>
    Derived *get_data() const {
      static_assert(std::is_base_of<SessionData, Derived>::value);

      return dynamic_cast<Derived *>(data_.get());
    }

    void set_data(SessionData *data) {
      data_.reset(data);
      data_->internal_session = this;
    }

    void set_data(std::unique_ptr<SessionData> &&data) {
      data_ = std::move(data);
      data_->internal_session = this;
    }

    AuthorizationHandlerId get_authorization_handler_id() const {
      return authorization_handler_id_;
    }

    const SessionId &get_session_id() const { return id_; }

    const std::string &get_holder_name() const { return holder_name_; }

    system_clock::time_point get_access_time() const { return access_time_; }
    system_clock::time_point get_create_time() const { return create_time_; }

    system_clock::time_point update_access_time() {
      return access_time_ = system_clock::now();
    }

    bool has_access_timeout(system_clock::duration timeout) const {
      return access_time_ + timeout <= system_clock::now();
    }

    bool is_expired(system_clock::duration timeout) const {
      return create_time_ + timeout <= system_clock::now();
    }

    void enable_db_session_pool(uint32_t passthrough_pool_size);

    SessionManager *owner{nullptr};
    bool generate_token{false};
    State state{kUninitialized};
    std::optional<std::string> users_on_complete_url_redirection;
    std::string users_on_complete_timeout;
    std::string handler_name;
    std::string handler_secondary_id;
    AuthUser user;
    std::string proto;
    std::string host;

    std::shared_ptr<collector::MysqlFixedPoolManager> db_session_pool;

   private:
    friend class SessionManager;
    std::unique_ptr<SessionData> data_;
    SessionId id_;
    system_clock::time_point access_time_;
    system_clock::time_point create_time_;
    AuthorizationHandlerId authorization_handler_id_{0};
    std::string holder_name_;
  };

  using SessionPtr = std::shared_ptr<Session>;

 public:
  SessionManager();

  void configure(const Configuration &config);
  const Configuration &configuration() const { return config_; }

  SessionPtr get_session_secondary_id(const SessionId &id);
  SessionPtr get_session(const SessionId &id);
  SessionPtr new_session(const AuthorizationHandlerId id,
                         const std::string &holder_name);
  SessionPtr new_session(const SessionId &session_id);
  bool change_session_id(SessionPtr session, const SessionId &new_session_id);

  template <class Generator>
  void set_unique_session_secondary_id(Session *session, const Generator &g) {
    std::lock_guard<std::mutex> lck{mutex_};
    std::string id;
    do {
      id = g();
    } while (get_session_handler_specific_id_impl(id));

    session->handler_secondary_id = id;
  }

  void remove_session(const Session::SessionData *session_data);
  bool remove_session(const SessionPtr &session);
  bool remove_session(const SessionId session);
  void remove_timeouted();

  std::function<void(const SessionPtr &)> on_session_delete;

 private:
  bool remove_session_impl(const Session *session);
  // Methods with postfix "_impl" at end of method name, marks that the methods
  // doesn't use mutexes, thus it should be used after locking `mutex_` object.
  SessionPtr get_session_impl(const SessionId &id);
  SessionPtr get_session_handler_specific_id_impl(const SessionId &id);

  SessionId generate_session_id_impl();
  void remove_timeouted_impl();
  void remove_inactive_impl(const system_clock::time_point &now);
  void remove_expired_impl(const system_clock::time_point &now);

  std::vector<SessionPtr> sessions_;
  std::mutex mutex_;
  system_clock::time_point oldest_inactive_session_;
  system_clock::time_point oldest_session_;
  Configuration config_;
};

}  // namespace http
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_REST_SESSION_MANAGER_H_
