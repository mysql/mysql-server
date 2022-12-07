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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_TRACK_AUTHORIZE_HANDLER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_TRACK_AUTHORIZE_HANDLER_H_

#include <memory>

#include "mrs/interface/authorize_handler.h"

namespace mrs {
namespace authentication {

template <typename Ctxt, typename Callback>
class TrackAuthorizeHandler : public interface::AuthorizeHandler {
 public:
  using AuthorizeHandlerPtr = std::shared_ptr<interface::AuthorizeHandler>;
  TrackAuthorizeHandler(AuthorizeHandlerPtr handler, Ctxt ctxt, Callback cb)
      : handler_{handler}, ctxt_{ctxt}, cb_{cb} {
    cb_->acquire(this);
  }

  ~TrackAuthorizeHandler() override { cb_->destroy(this); }

  UniversalId get_service_id() const override {
    return handler_->get_service_id();
  }

  UniversalId get_id() const override { return handler_->get_id(); }

  const AuthApp &get_entry() const override { return handler_->get_entry(); }

  bool is_authorized(Session *session, AuthUser *user) override {
    return handler_->is_authorized(session, user);
  }

  bool authorize(Session *session, http::Url *url,
                 SqlSessionCached *sql_session, HttpHeaders &input_headers,
                 AuthUser *out_user) override {
    return handler_->authorize(session, url, sql_session, input_headers,
                               out_user);
  }

 private:
  AuthorizeHandlerPtr handler_;
  Ctxt ctxt_;
  Callback cb_;
};

}  // namespace authentication
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_TRACK_AUTHORIZE_HANDLER_H_
