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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_HTTP_LOCAL_SESSION_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_HTTP_LOCAL_SESSION_H_

#include <cassert>
#include <string>

#include "mysqlrouter/http_request.h"

#include "mrs/http/cookie.h"
#include "mrs/http/session_manager.h"

namespace mrs {
namespace http {

template <typename Data>
class LocalSessionManager {
 public:
  LocalSessionManager(const std::string &service_path,
                      const std::string &session_key, SessionManager *sm)
      : service_path_{service_path}, session_key_{session_key}, sm_{sm} {}

  template <typename... Args>
  Data *new_session_data(HttpRequest *request, Args &&... args) {
    auto session = sm_->new_session();
    http::Cookie::set(request, session_key_, session->get_id(),
                      sm_->get_timeout(), service_path_);
    Data *data;
    session->set_data(data = new Data{args...});

    return data;
  }

  void remove_session_data(HttpRequest *request) {
    auto data = get_session_data(request);
    if (data) remove_session_data(data);
    Cookie::clear(request, session_key_.c_str());
  }

  Data *get_session_data(HttpRequest *request) const {
    auto session = get_session(request);
    if (!session) return nullptr;
    return session->template get_data<Data>();
  }

  void remove_session_data(Data *data) { sm_->remove_session(data); }

 private:
  using Session = SessionManager::Session;

  Session *get_session(HttpRequest *request) const {
    auto id = get_session_id(request);

    if (id.empty()) return nullptr;

    return sm_->get_session(id);
  }

  std::string get_session_id(HttpRequest *request) const {
    return Cookie::get(request, session_key_.c_str());
  }

  std::string service_path_;
  std::string session_key_;
  SessionManager *sm_;
};

}  // namespace http
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_HTTP_LOCAL_SESSION_H_
