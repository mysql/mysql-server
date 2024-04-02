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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_TRACK_AUTHORIZE_HANDLER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_TRACK_AUTHORIZE_HANDLER_H_

#include <memory>

#include "mrs/interface/authorize_handler.h"

namespace mrs {
namespace authentication {

template <typename Callback, typename Handler>
class TrackAuthorizeHandler : public Handler {
 public:
  using AuthApp = typename Handler::AuthApp;
  using Session = typename Handler::Session;
  using AuthUser = typename Handler::AuthUser;
  using RequestContext = typename Handler::RequestContext;
  using AuthorizeHandler = mrs::interface::AuthorizeHandler;

  template <typename... T>
  TrackAuthorizeHandler(Callback *cb, T... t) : Handler(t...), cb_{cb} {
    cb_->acquire(this);
  }

  ~TrackAuthorizeHandler() override { cb_->destroy(this); }

  UniversalId get_service_id() const override {
    return Handler::get_service_id();
  }

  bool redirects() const override { return Handler::redirects(); }
  UniversalId get_id() const override { return Handler::get_id(); }

  const AuthApp &get_entry() const override { return Handler::get_entry(); }

  bool is_authorized(Session *session, AuthUser *user) override {
    return Handler::is_authorized(session, user);
  }

  bool authorize(RequestContext &ctxt, Session *session,
                 AuthUser *out_user) override {
    return Handler::authorize(ctxt, session, out_user);
  }

  void pre_authorize_account(AuthorizeHandler *handler,
                             const std::string &account) override {
    cb_->pre_authorize_account(handler, account);
    Handler::pre_authorize_account(handler, account);
  }

 private:
  Callback *cb_;
};

}  // namespace authentication
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_TRACK_AUTHORIZE_HANDLER_H_
