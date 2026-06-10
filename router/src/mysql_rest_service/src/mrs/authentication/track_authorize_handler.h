/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
  using SessionPtr = typename Handler::SessionPtr;
  using AuthUser = typename Handler::AuthUser;
  using RequestContext = typename Handler::RequestContext;
  using AuthorizeHandler = mrs::interface::AuthorizeHandler;

  template <typename... T>
  TrackAuthorizeHandler(Callback *cb, T... t) : Handler(t...), cb_{cb} {}

  std::set<UniversalId> get_service_ids() const override {
    return Handler::get_service_ids();
  }

  bool redirects(RequestContext &ctxt) const override {
    return Handler::redirects(ctxt);
  }
  UniversalId get_id() const override { return Handler::get_id(); }

  const AuthApp &get_entry() const override { return Handler::get_entry(); }

  bool authorize(RequestContext &ctxt, const SessionPtr &session,
                 AuthUser *out_user) override {
    return Handler::authorize(ctxt, session, out_user);
  }

  void pre_authorize_account(AuthorizeHandler *handler,
                             const std::string &account) override {
    log_debug("TrackAuthorizeHandler::pre_authorize_account %s",
              account.c_str());
    cb_->pre_authorize_account(handler, account);
  }

 private:
  Callback *cb_;
};

}  // namespace authentication
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_TRACK_AUTHORIZE_HANDLER_H_
