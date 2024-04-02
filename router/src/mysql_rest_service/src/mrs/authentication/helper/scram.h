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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_HELPER_SCRAM_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_HELPER_SCRAM_H_

#include <memory>
#include <string>
#include <vector>

namespace mrs {
namespace authentication {

struct ScramServerAuthChallange {
  std::vector<uint8_t> salt;
  uint32_t iterations;
  std::string nonce_ex;
};

struct ScramClientAuthInitial {
  std::string user;
  std::string nonce;
};

struct ScramClientAuthContinue {
  std::string client_proof;
  std::string nonce;
  std::string session;
};

class ScramParser {
 public:
  virtual ~ScramParser() = default;

  virtual ScramClientAuthInitial set_initial_request(
      const std::string &auth_data) = 0;
  virtual std::string set_challange(const ScramServerAuthChallange &challange,
                                    const std::string &session_id) = 0;
  virtual ScramClientAuthContinue set_continue(
      const std::string &auth_data) = 0;

  std::string get_auth_message() const {
    return auth_message_auth_init + "," + auth_message_challange + "," +
           auth_message_continue;
  }

  virtual bool is_json() const = 0;

 protected:
  std::string auth_message_auth_init;
  std::string auth_message_challange;
  std::string auth_message_continue;
};

std::unique_ptr<ScramParser> create_scram_parser(const bool is_json);

}  // namespace authentication
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_AUTHENTICATION_HELPER_SCRAM_H_
