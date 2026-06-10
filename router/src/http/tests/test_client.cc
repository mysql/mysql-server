/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

// there is a another base64.h in the server's code
#include "http/client/client.h"

#include <stdexcept>
#include <tuple>

#include <gtest/gtest.h>

using Endpoint = http::client::Client::Endpoint;

class HttpClientTest : public ::testing::Test {
 public:
  Endpoint make_https(const std::string &host, const uint16_t port) {
    return {true, port, host};
  }

  Endpoint make_http(const std::string &host, const uint16_t port) {
    return {false, port, host};
  }

  Endpoint get_endpoint_from(const std::string &txt_uri) {
    http::base::Uri uri{txt_uri};

    auto e = http::client::Client::get_endpoint_from(uri);
    return {e.is_tls, e.port, e.host};
  }

  const uint16_t k_port_https = 443;
  const uint16_t k_port_http = 80;
};

TEST_F(HttpClientTest, validate_http_with_explicit_port) {
  ASSERT_EQ(make_http("host", 443), get_endpoint_from("http://host:443"));
  ASSERT_EQ(make_http("host", 443), get_endpoint_from("http://host:443/"));
  ASSERT_EQ(make_http("host", 443), get_endpoint_from("http://host:443/path"));
  ASSERT_EQ(make_http("other.com", 80),
            get_endpoint_from("http://other.com:80/path"));
  ASSERT_EQ(make_http("127.0.0.1", 2002),
            get_endpoint_from("http://127.0.0.1:2002/path"));
  ASSERT_EQ(make_http("127.0.0.1", 2002),
            get_endpoint_from("http://usr:pass@127.0.0.1:2002/path"));
}

TEST_F(HttpClientTest, validate_https_with_explicit_port) {
  ASSERT_EQ(make_https("host", 443), get_endpoint_from("https://host:443"));
  ASSERT_EQ(make_https("host", 443), get_endpoint_from("https://host:443/"));
  ASSERT_EQ(make_https("host", 443),
            get_endpoint_from("https://host:443/path"));
  ASSERT_EQ(make_https("other.com", 80),
            get_endpoint_from("https://other.com:80/path"));
  ASSERT_EQ(make_https("127.0.0.1", 2002),
            get_endpoint_from("https://127.0.0.1:2002/path"));
  ASSERT_EQ(make_https("127.0.0.1", 2002),
            get_endpoint_from("https://usr:pass@127.0.0.1:2002/path"));
}

TEST_F(HttpClientTest, validate_http_without_port) {
  ASSERT_EQ(make_http("host", 80), get_endpoint_from("http://host/"));
  ASSERT_EQ(make_https("host", 443), get_endpoint_from("https://host/"));
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
