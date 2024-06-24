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

#include <openssl/hmac.h>
#include <cassert>
#include <iostream>
#include <vector>

#include "helper/container/generic.h"
#include "helper/error.h"
#include "helper/json/rapid_json_interator.h"
#include "helper/json/text_to.h"
#include "helper/json/to_string.h"
#include "helper/token/jwt.h"

#include "mysql/harness/string_utils.h"
#include "mysqlrouter/base64.h"

namespace helper {

std::string as_string(const std::vector<unsigned char> &c) {
  return std::string(c.begin(), c.end());
}

std::vector<uint8_t> as_array(const std::string &s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

template <typename Document>
void doc_set_member(Document &doc, std::string_view name,
                    std::string_view value) {
  rapidjson::Value jname{name.data(), name.size(), doc.GetAllocator()};
  rapidjson::Value jvalue{value.data(), value.size(), doc.GetAllocator()};
  doc.AddMember(jname, jvalue, doc.GetAllocator());
}

using Base64NoPadd =
    Base64Base<Base64Alphabet::Base64Url, Base64Endianess::BIG, false, '='>;
void Jwt::parse(const std::string &token, JwtHolder *out) {
  auto parts = mysql_harness::split_string(token, '.');

  if (!(parts.size() > 1 && parts.size() < 4))
    throw Error("Invalid number of parts ", std::to_string(parts.size()));

  try {
    out->parts[0] = parts[0];
    out->parts[1] = parts[1];
    if (parts.size() == 3) {
      out->parts[2] = parts[2];
    }
    out->header = as_string(Base64NoPadd::decode(parts[0]));
    out->payload = as_string(Base64NoPadd::decode(parts[1]));
    if (parts.size() == 3) {
      out->signature = as_string(Base64NoPadd::decode(parts[2]));
    }
  } catch (const std::exception &) {
    throw Error("Exception while decoding JWT base64 parts");
  }
}

const std::string kHeaderClaimAlgorithm{"alg"};
const std::string kHeaderClaimType{"typ"};

Jwt Jwt::create(const JwtHolder &holder) {
  Jwt result;

  if (!helper::json::text_to(&result.header_, holder.header))
    throw Error("JWT header is not JSON");
  auto header_keys = get_payload_names(result.header_);
  if (!helper::json::text_to(&result.payload_, holder.payload))
    throw Error("JWT payload is not JSON");

  header_keys = get_payload_names(result.header_);
  if (!helper::container::has(header_keys, kHeaderClaimAlgorithm))
    throw Error("JWT header doesn't specifies the algorithm");
  if (!helper::container::has(header_keys, kHeaderClaimType))
    throw Error("JWT header doesn't specifies the type");

  if (result.get_header_claim_type() != "JWT")
    throw Error("JWT header type is not supported \"",
                result.get_header_claim_type(), "\"");

  result.holder_ = holder;
  result.signature_ = holder.signature;
  result.valid_ = true;

  return result;
}

Jwt Jwt::create(const std::string &algorithm, Document &payload) {
  using namespace rapidjson;
  Jwt result;
  // No other supported.
  assert(algorithm == "HS256" || algorithm == "none");
  result.header_.SetObject();
  doc_set_member(result.header_, kHeaderClaimType, "JWT");
  doc_set_member(result.header_, kHeaderClaimAlgorithm, algorithm);
  result.payload_.CopyFrom(payload, result.payload_.GetAllocator());

  result.holder_.parts[0] = (Base64NoPadd::encode(
      as_array(helper::json::to_string(&result.header_))));
  result.holder_.parts[1] = (Base64NoPadd::encode(
      as_array(helper::json::to_string(&result.payload_))));

  if (algorithm != "none" && algorithm != "HS256") return {};

  return result;
}

std::string Jwt::get_header_claim_algorithm() const {
  if (!header_.HasMember(kHeaderClaimAlgorithm.c_str())) return {};
  const auto &value = header_[kHeaderClaimAlgorithm.c_str()];

  if (!value.IsString()) return {};

  return value.GetString();
}

std::string Jwt::get_header_claim_type() const {
  if (!header_.HasMember(kHeaderClaimType.c_str())) return {};
  const auto &value = header_[kHeaderClaimType.c_str()];

  if (!value.IsString()) return {};

  return value.GetString();
}

std::vector<std::string> Jwt::get_payload_names(const Value &v) {
  std::vector<std::string> result;
  result.reserve(v.MemberCount());

  for (auto field : helper::json::member_iterator(v)) {
    result.push_back(field.first);
  }

  return result;
}

std::vector<std::string> Jwt::get_payload_claim_names() const {
  return get_payload_names(payload_);
}

const Jwt::Value *Jwt::get_payload_claim_custom(const std::string &name) const {
  if (!payload_.HasMember(name.c_str())) return {};
  const auto &value = payload_[name.c_str()];

  return &value;
}

bool Jwt::is_valid() const { return valid_; }

std::string encode_HS256(const std::string &secret,
                         const std::string &message) {
  unsigned char buffer[EVP_MAX_MD_SIZE];
  unsigned int buffer_size = sizeof(buffer);

  if (nullptr == HMAC(EVP_sha256(), secret.c_str(), secret.length(),
                      reinterpret_cast<const unsigned char *>(message.c_str()),
                      message.length(), buffer, &buffer_size)) {
    return {};
  }
  return std::string{reinterpret_cast<char *>(buffer), buffer_size};
}

bool Jwt::verify(const std::string &secret) const {
  if (get_header_claim_algorithm() == "none") return true;
  if (signature_.empty()) return false;

  // This class only supports HS256 !
  if (get_header_claim_algorithm() != "HS256") return false;
  auto message = holder_.parts[0] + "." + holder_.parts[1];
  auto result = encode_HS256(secret, message);

  if (result.length() != holder_.signature.length()) return false;

  for (unsigned int i = 0; i < result.length(); ++i)
    if (result[i] != holder_.signature[i]) {
      return false;
    }

  return true;
}

std::string Jwt::sign(const std::string &secret) const {
  auto message = holder_.parts[0] + "." + holder_.parts[1];
  if (get_header_claim_algorithm() == "none") return message;

  // This class only supports HS256 !
  if (get_header_claim_algorithm() != "HS256") return {};
  auto result = encode_HS256(secret, message);
  result = Base64NoPadd::encode(as_array(result));
  return message + "." + result;
}

}  // namespace helper
