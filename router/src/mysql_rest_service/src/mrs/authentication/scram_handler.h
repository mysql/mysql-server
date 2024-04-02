/*
 Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_SCRAM_HANDLER_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_SCRAM_HANDLER_H_

#include "mrs/authentication/helper/key_stored_informations.h"
#include "mrs/authentication/sasl_handler.h"

namespace mrs {
namespace authentication {

class ScramHandler : public SaslHandler {
 public:
  ScramHandler(const AuthApp &entry, const std::string &random_data);

  bool redirects() const override;

  SessionData *allocate_session_data() override;

  SaslResult client_request_authentication_exchange(
      RequestContext &ctxt, Session *session, AuthUser *out_user) override;
  SaslResult client_response(RequestContext &ctxt, Session *session,
                             AuthUser *out_user, const std::string &auth_data,
                             const bool is_json) override;
  SaslResult client_initial_response(RequestContext &ctxt, Session *session,
                                     AuthUser *out_user,
                                     const std::string &auth_data,
                                     const bool is_json) override;

 private:
  std::string get_salt_for_the_user(const std::string &user_name) const;
  const std::string &random_data_;
};

}  // namespace authentication
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_SCRAM_HANDLER_H_
