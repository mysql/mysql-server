/*
  Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

#include "client/authentication.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

#include <utility>

#include "helper/container/map.h"
#include "helper/http/url.h"
#include "helper/json/error.h"
#include "helper/json/rapid_json_to_struct.h"
#include "helper/json/serializer_to_text.h"
#include "helper/json/text_to.h"
#include "helper/json/to_string.h"
#include "helper/string/hex.h"
#include "helper/string/random.h"

#include "mysql/harness/string_utils.h"
#include "mysqlrouter/base64.h"
#include "mysqlrouter/component/http_auth_method_basic.h"

#include "mrs_client_debug.h"

namespace mrs_client {

namespace {

void debugln_struct(int idx, const Result &result) {
  mrs_debugln("status ", idx, ":", result.status);
  mrs_debugln("Body   ", idx, ":", result.body);

  for (auto h : result.headers) {
    mrs_debugln("header  ", idx, ": key=", h.first, ", value=", h.second);
  }
}

void calculate_sha256(const unsigned char *client_key,
                      unsigned char *stored_key, int size) {
  SHA256(client_key, size, stored_key);
}

void calculate_hmac(const unsigned char *key, int key_len,
                    const unsigned char *data, int data_len,
                    unsigned char *result, unsigned int *result_len) {
  HMAC(EVP_sha256(), key, key_len, data, data_len, result, result_len);
}

void calculate_xor(const unsigned char *a, const unsigned char *b,
                   unsigned char *out, int len) {
  for (int i = 0; i < len; i++) {
    out[i] = a[i] ^ b[i];
  }
}

static std::string string_array(const std::string &s) {
  std::string result;

  for (std::size_t i = 0; i < s.length(); ++i) {
    if (i != 0) result += ",";
    result += std::to_string((int)(unsigned char)s[i]);
  }

  return result;
}

static std::string string_array(const uint8_t *v, std::size_t len) {
  std::string result;

  for (std::size_t i = 0; i < len; ++i) {
    if (i != 0) result += ",";
    result += std::to_string((int)(unsigned char)v[i]);
  }

  return result;
}

std::string compute_client_proof(std::string password, std::string salt,
                                 int iterations, std::string auth_message) {
  unsigned char salted_password[SHA256_DIGEST_LENGTH];
  unsigned char client_key[SHA256_DIGEST_LENGTH];
  unsigned char stored_key[SHA256_DIGEST_LENGTH];
  unsigned char client_signature[SHA256_DIGEST_LENGTH];
  unsigned int len;
  std::string client_proof(SHA256_DIGEST_LENGTH, '\0');
  std::string client_key_str{"Client Key"};

  mrs_debugln("iterations:    ", iterations);
  mrs_debugln("salt:          ", string_array(salt));
  mrs_debugln("auth_message:  ", string_array(auth_message));

  // Generate SaltedPassword using PBKDF2
  if (!PKCS5_PBKDF2_HMAC(password.data(), password.length(),
                         (unsigned char *)salt.data(), salt.length(),
                         iterations, EVP_sha256(), SHA256_DIGEST_LENGTH,
                         salted_password)) {
    throw std::runtime_error("Error generating SaltedPassword.");
  }

  calculate_hmac(salted_password, SHA256_DIGEST_LENGTH,
                 (unsigned char *)client_key_str.data(),
                 client_key_str.length(), client_key, &len);

  mrs_debugln("client_key:    ", string_array(client_key, len));

  calculate_sha256(client_key, stored_key, SHA256_DIGEST_LENGTH);

  calculate_hmac(stored_key, SHA256_DIGEST_LENGTH,
                 (unsigned char *)auth_message.data(), auth_message.length(),
                 client_signature, &len);

  // Compute ClientProof = ClientKey XOR ClientSignature
  calculate_xor(client_key, client_signature,
                (unsigned char *)client_proof.data(), SHA256_DIGEST_LENGTH);

  return client_proof;
}

void add_authorization_header(HttpClientRequest *request,
                              const std::string &user,
                              const std::string &password) {
  HttpAuthMethodBasic basic;
  const char *kAuthorization = "Authorization";
  std::string auth_string{basic.kMethodName};
  auth_string += " " + basic.encode_authorization({user, password});
  request->add_header(kAuthorization, auth_string.c_str());
}

std::string get_authorization_json(
    const std::string &user, const std::string &password, const bool use_jwt,
    const std::map<std::string, std::string> &url_query) {
  helper::json::SerializerToText stt;

  {
    auto obj = stt.add_object();
    obj->member_add_value("sessionType", (use_jwt ? "bearer" : "cookie"));
    obj->member_add_value("username", user);
    obj->member_add_value("password", password);

    for (const auto &[k, v] : url_query) {
      obj->member_add_value(k, v);
    }
  }
  return stt.get_result();
}

struct JsonResponse {
  std::optional<std::string> access_token;
  std::optional<std::string> session_id;
};

struct JsonChallange {
  std::optional<int> iterations;
  std::optional<std::string> nonce;
  std::optional<std::vector<uint8_t>> salt;
};

class ParseJsonResponse
    : public helper::json::RapidReaderHandlerStringValuesToStruct<
          JsonResponse> {
 public:
  void handle_object_value(const std::string &key, std::string &&vt) override {
    if (key == "accessToken") {
      result_.access_token = std::move(vt);
    } else if (key == "sessionId") {
      result_.session_id = std::move(vt);
    }
  }
};

class ParseJsonRawChallenge
    : public helper::json::RapidReaderHandlerStringValuesToStruct<std::string> {
 public:
  void handle_object_value(const std::string &key, std::string &&vt) override {
    if (key == "data") {
      result_ = std::move(vt);
    }
  }
};

class ParseJsonObjectChallenge
    : public helper::json::RapidReaderHandlerStringValuesToStruct<
          JsonChallange> {
 public:
  void handle_object_value(const std::string &key, std::string &&vt) override {
    mrs_debugln("handle_object_value key:", key, ", var:", vt);
    if (key == "iterations") {
      result_.iterations = atoi(vt.c_str());
    } else if (key == "nonce") {
      result_.nonce = std::move(vt);
    }
  }

  void handle_array_value(const std::string &key, std::string &&vt) override {
    mrs_debugln("handle_array_value key:", key, ", var:", vt);
    if (key == "salt.salt") {
      if (!result_.salt.has_value()) result_.salt = std::vector<uint8_t>();

      result_.salt.value().push_back(atoi(vt.c_str()));
    }
  }
};

}  // namespace

Result Authentication::do_basic_flow(
    HttpClientRequest *request, std::string url, const std::string &user,
    const std::string &password, const SessionType st,
    [[maybe_unused]] const std::optional<std::string> &auth_app) {
  assert(!auth_app.has_value() && "Not implemented in this app");
  add_authorization_header(request, user, password);

  if (st == SessionType::kJWT) {
    url = url + "?sessionType=bearer";
  }

  bool set_new_cookies = st == SessionType::kCookie;
  auto result = request->do_request(HttpMethod::Get, url, {}, set_new_cookies);

  debugln_struct(1, result);

  if (result.status == HttpStatusCode::NotFound) return result;

  if (result.status != HttpStatusCode::TemporaryRedirect) {
    return result;
    //    throw std::runtime_error(
    //        "Expected redirection flow, received other status code.");
  }

  const auto location = find_in_headers(result.headers, "Location");
  if (location.empty())
    throw std::runtime_error(
        "HTTP redirect, doesn't contain `Location` header.");

  // Parameter value
  std::string pvalue;
  http::base::Uri u{location};
  std::map<std::string, std::string> parameters;

  helper::http::Url helper_uri(u);

  if (!helper_uri.get_if_query_parameter("login", &pvalue))
    throw std::runtime_error(
        "HTTP redirect, doesn't contain `login` query parameter.");

  if (pvalue != "success") {
    if (pvalue != "fail") {
      throw std::runtime_error(
          "HTTP redirect, has unknown value for \"login\" query parameter.");
    }
    if (helper_uri.get_if_query_parameter("accessToken", &pvalue)) {
      throw std::runtime_error(
          "HTTP redirect, login failed but it contains `accessToken` query "
          "parameter.");
    }

    const auto cookie = find_in_headers(result.headers, "Set-Cookie");
    if (!cookie.empty())
      throw std::runtime_error(
          "Response contains Set-Cookie, but authentication failed.");

    return {HttpStatusCode::Unauthorized, {}, {}};
  }

  if (st == SessionType::kJWT) {
    pvalue.clear();
    if (!helper_uri.get_if_query_parameter("accessToken", &pvalue)) {
      throw std::runtime_error(
          "HTTP redirect, doesn't contain `accessToken` query parameter.");
    }

    if (pvalue.empty())
      throw std::runtime_error(
          "HTTP redirect, doesn't contain valid JWT token.");
    std::string header{"Authorization:Bearer "};
    header += pvalue;
    request->get_session()->add_header(&header[0]);
  } else {
    const auto cookie = find_in_headers(result.headers, "Set-Cookie");
    if (cookie.empty() || cookie.find("session_") == std::string::npos)
      throw std::runtime_error(
          "Response doesn't contains Set-Cookie with proper value.");
  }

  return {HttpStatusCode::Ok, request->get_input_headers(), {}};
}

Result Authentication::do_basic_json_flow(
    HttpClientRequest *request, std::string url, const std::string &user,
    const std::string &password, const SessionType st,
    [[maybe_unused]] const std::optional<std::string> &auth_app) {
  http::base::Uri u{url};
  bool use_jwt = st == SessionType::kJWT;
  bool use_cookies = !use_jwt;

  assert(!auth_app.has_value() && "Not implemented in this app");

  auto body =
      get_authorization_json(user, password, use_jwt, u.get_query_elements());
  auto result = request->do_request(HttpMethod::Post, url, body, use_cookies);

  debugln_struct(1, result);

  if (result.status != HttpStatusCode::Ok) return result;

  auto json = helper::json::text_to_handler<ParseJsonResponse>(result.body);

  if (!json) throw helper::json::ErrorJsonParse();

  auto [access_token, session_id] = *json;

  if (session_id.has_value() && access_token.has_value())
    throw std::runtime_error(
        "Response contains both `session_id` and `access_token` which is not "
        "allowed.");

  if (!session_id.has_value() && !access_token.has_value()) {
    const auto cookie = find_in_headers(result.headers, "Set-Cookie");
    if (cookie.empty() || cookie.find("session_") == std::string::npos)
      throw std::runtime_error(
          "Response doesn't contains neither `session_id` nor `access_token`.");
    session_id = cookie;
  }

  if (use_jwt && session_id.has_value()) {
    throw std::runtime_error(
        "Application requested JWT token, but received a cookie-session-id.");
  }

  if (use_cookies && access_token.has_value()) {
    throw std::runtime_error(
        "Application requested cookie-session-id, but received a JWT token.");
  }

  if (use_jwt) {
    std::string header{"Authorization:Bearer "};
    header += access_token.value();
    request->get_session()->add_header(&header[0]);
  }

  // Ignore the `use_cookies`, the cookie should be already set in headers.
  // The session_id - cookie name is service dependent, thus we would also need
  // to transfer service_id.

  return {HttpStatusCode::Ok, request->get_input_headers(), {}};
}

class Scram {
 public:
  struct Challenge {
    std::string nonce;
    std::string salt;
    std::string iterations;
  };
  using Base64NoPadd =
      Base64Base<Base64Alphabet::Base64Url, Base64Endianess::BIG, true, '='>;
  using Base64Data =
      Base64Base<Base64Alphabet::Base64, Base64Endianess::BIG, true, '='>;

  std::string get_initial_auth_data(const std::string &user) {
    const static std::string kParameterAuthData = "data";
    using namespace std::string_literals;
    initial_nonce_ = generate_nonce(10);
    client_first_ = "n="s + user + ",r=" + initial_nonce_;
    return kParameterAuthData + "=" +
           Base64NoPadd::encode(as_array("n,," + client_first_));
  }

  void parse_auth_data_phase1(const JsonChallange &data) {
    std::string result{};

    result.append("r=").append(data.nonce.value());
    result.append(",s=").append(Base64Data::encode(data.salt.value()));
    result.append(",i=").append(std::to_string(data.iterations.value()));

    parse_auth_data_phase1(result);
  }

  void parse_auth_data_phase1(const std::string &data) {
    auto elements = mysql_harness::split_string(data, ',', true);
    std::map<std::string, std::string> result;
    for (const auto &e : elements) {
      auto idx = e.find("=");
      if (idx == std::string::npos) continue;
      result[e.substr(0, idx)] = e.substr(idx + 1);
    }

    if (!result.count("r"))
      throw std::runtime_error("Challenge response doesn't contain 'r' field.");
    if (!result.count("s"))
      throw std::runtime_error("Challenge response doesn't contain 's' field.");
    if (!result.count("i"))
      throw std::runtime_error("Challenge response doesn't contain 'i' field.");

    challenge_.nonce = result["r"];
    auto a = Base64Data::decode(result["s"]);
    challenge_.salt = std::string{a.begin(), a.end()};
    challenge_.iterations = result["i"];
    server_first_ = data;
    client_final_ = std::string("r=") + challenge_.nonce;
  }

  std::string calculate_proof(const std::string &pass) {
    std::string auth_msg{client_first_ + "," + server_first_ + "," +
                         client_final_};

    proof_ = compute_client_proof(
        pass, challenge_.salt, atoi(challenge_.iterations.c_str()), auth_msg);
    auto proof64 = Base64Data::encode(proof_);
    auto auth_data = client_final_ + ",p=" + proof64;
    return "state=response&data=" + Base64NoPadd::encode(auth_data);
  }

  std::string as_string(const std::vector<unsigned char> &c) {
    return std::string(c.begin(), c.end());
  }

  std::vector<uint8_t> as_array(const std::string &s) {
    return std::vector<uint8_t>(s.begin(), s.end());
  }

  std::string generate_nonce(std::size_t size) {
    return helper::string::hex(
        helper::generate_string<helper::Generator8bitsValues>(size));
  }

  std::string proof_;
  std::string initial_nonce_;
  std::string client_first_;
  std::string server_first_;
  std::string client_final_;
  Challenge challenge_;
};

auto check_bearer_cookies(const Result &result, bool must_have_cookies,
                          bool must_have_token) {
  int found_cookies{0};
  for (auto &kv : result.headers) {
    if (kv.first == "Set-Cookie") {
      ++found_cookies;
    }
  }

  auto json = helper::json::text_to_handler<ParseJsonResponse>(result.body);

  if (!json) throw helper::json::ErrorJsonParse();

  auto [access_token, session_id] = *json;

  if (must_have_cookies && !found_cookies)
    throw std::runtime_error("Expected cookie sets, but there were none.");
  if (!must_have_cookies && found_cookies)
    throw std::runtime_error("Expected no cookie sets, but there " +
                             std::to_string(found_cookies) + ".");

  if (must_have_token && !access_token.has_value())
    throw std::runtime_error("Expected token, but it was not set.");

  if (!must_have_token && access_token.has_value())
    throw std::runtime_error("Expected no token, but it was set.");

  return access_token;
}

Result Authentication::do_scram_post_flow(
    HttpClientRequest *request, std::string url, const std::string &user,
    const std::string &password, const SessionType st,
    const std::optional<std::string> &auth_app) {
  using JsonObject = std::map<std::string, std::string>;
  Scram scram;

  scram.get_initial_auth_data(user);

  JsonObject request_data{
      {"sessionType", (st == SessionType::kJWT ? "bearer" : "cookie")},
      {"username", user},
      {"nonce", scram.initial_nonce_}};

  if (auth_app.has_value()) {
    request_data["authApp"] = auth_app.value();
  }

  // Deadening on this parameter, we will check if there is
  // either the cookie or JWT set. Its done in following calls:
  //
  //     check_bearer_cookies(result, false, false);
  // ..
  //     check_bearer_cookies(result, set_new_cookies, !set_new_cookies);
  //
  bool set_new_cookies = st == SessionType::kCookie;
  auto result = request->do_request(HttpMethod::Post, url,
                                    helper::json::to_string(request_data),
                                    set_new_cookies);

  if (result.status == HttpStatusCode::NotFound) return result;

  if (result.status != HttpStatusCode::Ok &&
      result.status != HttpStatusCode::Unauthorized &&
      result.status != HttpStatusCode::TemporaryRedirect)
    throw std::runtime_error(
        std::to_string(result.status) +
        ", Expected status Ok|Unauthorized with payload, received other status "
        "code.");

  debugln_struct(1, result);

  check_bearer_cookies(result, false, false);

  auto json =
      helper::json::text_to_handler<ParseJsonObjectChallenge>(result.body);

  if (!json) throw helper::json::ErrorJsonParse();

  if (!json->nonce.has_value() || !json->iterations.has_value() ||
      !json->salt.has_value()) {
    throw std::runtime_error(
        "The challenge message is missing required fields.");
  }

  scram.parse_auth_data_phase1(*json);
  scram.calculate_proof(password);

  JsonObject request_continue{{"state", "response"},
                              {"clientProof", scram.proof_},
                              {"nonce", scram.challenge_.nonce}};

  result = request->do_request(HttpMethod::Post, url,
                               helper::json::to_string(request_continue),
                               set_new_cookies);

  debugln_struct(2, result);

  if (result.status != HttpStatusCode::Ok) return result;

  auto access_token =
      check_bearer_cookies(result, set_new_cookies, !set_new_cookies);

  if (!set_new_cookies && access_token.has_value()) {
    std::string header{"Authorization:Bearer "};
    header += access_token.value();
    request->get_session()->add_header(&header[0]);
  }

  return {HttpStatusCode::Ok, request->get_input_headers(), {}};
}

Result Authentication::do_scram_get_flow(
    HttpClientRequest *request, std::string url, const std::string &user,
    const std::string &password, const SessionType st,
    const std::optional<std::string> &auth_app) {
  Scram scram;

  auto url_init = url + "?" + scram.get_initial_auth_data(user);
  if (st == SessionType::kJWT) {
    url_init = url_init + "&sessionType=bearer";
  }

  if (auth_app.has_value()) {
    url_init = url_init + "&app=" + auth_app.value();
  }

  // Deadening on this parameter, we will check if there is
  // either the cookie or JWT set. Its done in following calls:
  //
  //     check_bearer_cookies(result, false, false);
  // ..
  //     check_bearer_cookies(result, set_new_cookies, !set_new_cookies);
  //
  bool set_new_cookies = st == SessionType::kCookie;
  auto result =
      request->do_request(HttpMethod::Get, url_init, {}, set_new_cookies);

  if (result.status == HttpStatusCode::NotFound) return result;

  if (result.status != HttpStatusCode::Unauthorized &&
      result.status != HttpStatusCode::TemporaryRedirect)
    throw std::runtime_error(
        std::to_string(result.status) +
        ", Expected status Unauthorized with payload, received other status "
        "code.");

  debugln_struct(1, result);

  check_bearer_cookies(result, false, false);

  auto json = helper::json::text_to_handler<ParseJsonRawChallenge>(result.body);

  if (!json) throw helper::json::ErrorJsonParse();

  scram.parse_auth_data_phase1(*json);

  auto url_final = url + "?" + scram.calculate_proof(password);

  result = request->do_request(HttpMethod::Get, url_final, {}, set_new_cookies);

  debugln_struct(2, result);

  if (result.status != HttpStatusCode::Ok) return result;

  auto access_token =
      check_bearer_cookies(result, set_new_cookies, !set_new_cookies);

  if (!set_new_cookies && access_token.has_value()) {
    std::string header{"Authorization:Bearer "};
    header += access_token.value();
    request->get_session()->add_header(&header[0]);
  }

  return {HttpStatusCode::Ok, request->get_input_headers(), {}};
}

}  // namespace mrs_client
