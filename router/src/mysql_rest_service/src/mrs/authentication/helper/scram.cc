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

#include "mrs/authentication/helper/scram.h"

#include <cassert>
#include <map>
#include <vector>

#include "helper/container/generic.h"
#include "helper/json/rapid_json_to_struct.h"
#include "helper/json/serializer_to_text.h"
#include "helper/json/text_to.h"
#include "helper/string/contains.h"
#include "mrs/authentication/helper/http_result.h"

#include "mysql/harness/string_utils.h"
#include "mysqlrouter/base64.h"

namespace mrs {
namespace authentication {

template <typename T>
std::string as_string(const std::vector<T> &v) {
  return std::string(v.begin(), v.end());
}

int64_t as_int64(const char *s) {
  int64_t i;
  if (1 == sscanf(s, "%" SCNd64, &i)) {
    return i;
  }

  return 0;
}

std::string scram_concat(const std::vector<std::string> &params) {
  std::string result;

  for (const auto &p : params) {
    if (!result.empty()) result += ",";
    result += p;
  }

  return result;
}

std::string scram_pack(const ScramClientAuthContinue &data) {
  // Pack without client_proof
  std::string result{"r="};

  result.append(data.nonce);

  return result;
}

std::string scram_pack(const ScramClientAuthInitial &data) {
  std::string result{"n="};

  result.append(data.user);
  result.append(",r=").append(data.nonce);

  return result;
}

std::string scram_pack(const ScramServerAuthChallange &data) {
  std::string result{};

  result.append("r=").append(data.nonce_ex);
  result.append(",s=").append(Base64::encode(data.salt));
  result.append(",i=").append(std::to_string(data.iterations));

  return result;
}

std::map<std::string, std::string> convert_to_map(
    const std::vector<std::string> &values) {
  std::map<std::string, std::string> map;
  for (const auto &v : values) {
    auto eq = v.find("=");
    if (v.npos == eq) continue;
    map[v.substr(0, eq)] = v.substr(eq + 1);
  }

  return map;
}

ScramClientAuthInitial scram_unpack_initial(const std::string &auth_data) {
  auto params = mysql_harness::split_string(auth_data, ',');
  if (!params.size())
    throw get_problem_description(HttpStatusCode::Unauthorized,
                                  "Authorization data, not provided");
  if (params[0] != "n")
    throw get_problem_description(HttpStatusCode::Unauthorized,
                                  "Authorization data, has wrong format");

  auto map = convert_to_map(params);

  if (!map.count("n") || !map.count("r"))
    throw get_problem_description(
        HttpStatusCode::Unauthorized,
        "Authorization data, doesn't contains required attributes");

  ScramClientAuthInitial result;

  result.user = map["n"];
  result.nonce = map["r"];

  return result;
}

ScramClientAuthContinue scram_unpack_continue(const std::string &auth_data) {
  auto params = mysql_harness::split_string(auth_data, ',');
  if (!params.size())
    throw get_problem_description(HttpStatusCode::Unauthorized,
                                  "Authorization data, not provided");
  auto map = convert_to_map(params);

  if (!map.count("r") || !map.count("p"))
    throw get_problem_description(
        HttpStatusCode::Unauthorized,
        "Authorization data, doesn't contains required attributes");

  ScramClientAuthContinue result;

  result.nonce = map["r"];
  result.client_proof = as_string(Base64::decode(map["p"]));

  return result;
}

std::string scram_remove_proof(const std::string &auth_data) {
  std::string result;

  auto params = mysql_harness::split_string(auth_data, ',');
  helper::container::remove_if(params,
                               [](auto v) { return 0 == v.find("p="); });

  return scram_concat(params);

  return result;
}

std::string scram_remove_gs2_header(const std::string &auth_data) {
  auto params = mysql_harness::split_string(auth_data, ',');
  int i = 2;
  while (i && !params.empty()) {
    params.erase(params.begin());
    --i;
  }

  return scram_concat(params);
}

class ScramStandardParser : public ScramParser {
 public:
  ScramClientAuthInitial set_initial_request(
      const std::string &auth_data) override {
    auto result = scram_unpack_initial(auth_data);
    auth_message_auth_init = scram_remove_gs2_header(auth_data);

    return result;
  }

  std::string set_challange(const ScramServerAuthChallange &challange,
                            const std::string &) override {
    auth_message_challange = scram_pack(challange);

    return auth_message_challange;
  }

  ScramClientAuthContinue set_continue(const std::string &auth_data) override {
    auth_message_continue = scram_remove_proof(auth_data);
    auto result = scram_unpack_continue(auth_data);
    return result;
  }

  bool is_json() const override { return false; }
};

class JsonAuthInitRequest
    : public helper::json::RapidReaderHandlerToStruct<ScramClientAuthInitial> {
 public:
  bool String(const Ch *cstr, rapidjson::SizeType clength, bool) override {
    if (!is_object_path()) return true;
    if ("user" == get_current_key())
      result_.user.assign(cstr, clength);
    else if ("nonce" == get_current_key())
      result_.nonce.assign(cstr, clength);

    return true;
  }
};

class JsonAuthContinue
    : public helper::json::RapidReaderHandlerToStruct<ScramClientAuthContinue> {
 public:
  bool String(const Ch *cstr, rapidjson::SizeType clength, bool) override {
    if (!is_object_path()) {
      return true;
    }
    if ("clientProof" == get_current_key())
      result_.client_proof.assign(cstr, clength);
    else if ("nonce" == get_current_key())
      result_.nonce.assign(cstr, clength);
    else if ("session" == get_current_key())
      result_.session.assign(cstr, clength);

    return true;
  }

  bool RawNumber(const Ch *c, rapidjson::SizeType, bool) override {
    if (is_object_path()) return true;
    if (!helper::starts_with(get_current_key(), "clientProof.")) return true;

    uint8_t v = static_cast<uint8_t>(as_int64(c));
    result_.client_proof.append(reinterpret_cast<char *>(&v), 1);
    return true;
  }
};

class ScramJsonParser : public ScramParser {
 public:
  ScramClientAuthInitial set_initial_request(
      const std::string &auth_data) override {
    auto result = helper::json::text_to_handler<JsonAuthInitRequest>(auth_data);

    auth_message_auth_init = scram_pack(result);

    return result;
  }

  std::string set_challange(const ScramServerAuthChallange &challange,
                            const std::string &session_id) override {
    helper::json::SerializerToText stt;
    auth_message_challange = scram_pack(challange);
    {
      auto obj = stt.add_object();
      obj->member_add_value("session", session_id);
      obj->member_add_value("iterations", challange.iterations);
      obj->member_add_value("nonce", challange.nonce_ex);
      {
        auto arr = obj->member_add_array("salt");
        arr.add(challange.salt);
      }
    }
    return stt.get_result();
  }

  ScramClientAuthContinue set_continue(const std::string &auth_data) override {
    auto result = helper::json::text_to_handler<JsonAuthContinue>(auth_data);
    auth_message_continue = scram_pack(result);
    return result;
  }

  bool is_json() const override { return true; }
};

std::unique_ptr<ScramParser> create_scram_parser(const bool is_json) {
  if (!is_json) return std::make_unique<ScramStandardParser>();

  return std::make_unique<ScramJsonParser>();
}

}  // namespace authentication
}  // namespace mrs
