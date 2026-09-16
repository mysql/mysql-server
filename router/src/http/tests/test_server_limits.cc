/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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

#include <limits>
#include <optional>

#include <gtest/gtest.h>

#include "http/base/headers.h"
#include "http/server/connection.h"
#include "http/server/server.h"
#include "mysql/harness/net_ts/internet.h"
#include "mysql/harness/tls_server_context.h"
#include "mysqlrouter/io_thread.h"

namespace {

class ServerLimitsTest : public ::testing::Test {
 protected:
  TlsServerContext tls_ctx_;
  std::list<IoThread> io_threads_;
};

TEST_F(ServerLimitsTest, MaxHttpConnectionsPrecedence) {
  http::server::Server server(&tls_ctx_, &io_threads_, nullptr, nullptr, 5);
  EXPECT_EQ(server.effective_max_http_connections(), 5);

  server.set_max_http_connections(7);
  EXPECT_EQ(server.effective_max_http_connections(), 7);

  server.set_max_http_connections(std::nullopt);
  EXPECT_EQ(server.effective_max_http_connections(), 5);
}

TEST_F(ServerLimitsTest, MaxRequestBodySizePrecedence) {
  http::server::Server server(&tls_ctx_, &io_threads_, nullptr, nullptr, 5, 64);
  EXPECT_EQ(server.effective_max_request_body_size(), 64);

  server.set_max_request_body_size(128);
  EXPECT_EQ(server.effective_max_request_body_size(), 128);

  server.set_max_request_body_size(0);
  EXPECT_EQ(server.effective_max_request_body_size(), 0);

  server.set_max_request_body_size(std::nullopt);
  EXPECT_EQ(server.effective_max_request_body_size(), 64);
}

TEST_F(ServerLimitsTest, MaxResponseBodySizePrecedence) {
  http::server::Server server(&tls_ctx_, &io_threads_, nullptr, nullptr, 5, 64,
                              256);
  EXPECT_EQ(server.effective_max_response_body_size(), 256);

  server.set_max_response_body_size(128);
  EXPECT_EQ(server.effective_max_response_body_size(), 128);

  server.set_max_response_body_size(std::nullopt);
  EXPECT_EQ(server.effective_max_response_body_size(), 256);
}

TEST_F(ServerLimitsTest, ClearLimitOverridesRestoresConfiguredValues) {
  http::server::Server server(&tls_ctx_, &io_threads_, nullptr, nullptr, 5, 64,
                              256);

  server.set_max_http_connections(7);
  server.set_max_request_body_size(128);
  server.set_max_response_body_size(512);

  server.clear_overrides();

  EXPECT_EQ(server.effective_max_http_connections(), 5);
  EXPECT_EQ(server.effective_max_request_body_size(), 64);
  EXPECT_EQ(server.effective_max_response_body_size(), 256);
}

TEST_F(ServerLimitsTest, RuntimeOverridesAreNotConfigBound) {
  http::server::Server server(&tls_ctx_, &io_threads_, nullptr, nullptr, 5, 64,
                              256);
  constexpr uint64_t kMax = std::numeric_limits<uint64_t>::max();

  // The runtime API is policy-free; callers exposing user/config input own
  // bounds validation.
  server.set_max_http_connections(kMax);
  server.set_max_request_body_size(kMax);
  server.set_max_response_body_size(kMax);

  EXPECT_EQ(server.effective_max_http_connections(), kMax);
  EXPECT_EQ(server.effective_max_request_body_size(), kMax);
  EXPECT_EQ(server.effective_max_response_body_size(), kMax);

  server.set_max_http_connections(0);
  EXPECT_EQ(server.effective_max_http_connections(), 0);
}

using ServerConnection = http::server::ServerConnection<net::ip::tcp::socket>;
struct ServerConnectionTestAccess : ServerConnection {
  using ServerConnection::invalid_request_body_headers_rejection_reason;
};

TEST(HttpServerConnectionLimitsTest, ParseContentLength) {
  EXPECT_EQ(ServerConnection::parse_content_length("0"),
            std::optional<uint64_t>(0));
  EXPECT_EQ(ServerConnection::parse_content_length("128"),
            std::optional<uint64_t>(128));
  EXPECT_FALSE(ServerConnection::parse_content_length("-1").has_value());
  EXPECT_FALSE(ServerConnection::parse_content_length("12x").has_value());
}

TEST(HttpServerConnectionLimitsTest, ContentLengthLimitCheck) {
  EXPECT_TRUE(ServerConnection::body_size_exceeds_limit(10, 0, 11));
  EXPECT_FALSE(ServerConnection::body_size_exceeds_limit(10, 0, 10));
  EXPECT_TRUE(ServerConnection::body_size_exceeds_limit(0, 0, 1));
}

TEST(HttpServerConnectionLimitsTest, StreamingBodyLimitCheck) {
  EXPECT_FALSE(ServerConnection::body_size_exceeds_limit(10, 4, 6));
  EXPECT_TRUE(ServerConnection::body_size_exceeds_limit(10, 4, 7));
}

TEST(HttpServerConnectionLimitsTest, ResponseBodyLimitCheck) {
  EXPECT_FALSE(ServerConnection::body_size_exceeds_limit(10, 10));
  EXPECT_TRUE(ServerConnection::body_size_exceeds_limit(10, 11));
}

TEST(HttpServerConnectionLimitsTest, ValidateBodyHeadersContentLengthAndTE) {
  ServerConnection::BodyHeadersValidationResult result{};
  ServerConnection::update_body_related_header("Content-Length", "4", result);
  ServerConnection::update_body_related_header("Transfer-Encoding", "chunked",
                                               result);
  EXPECT_TRUE(result.has_transfer_encoding);
  EXPECT_TRUE(result.has_content_length);
  EXPECT_FALSE(result.invalid_content_length);
  EXPECT_EQ(result.content_length, std::optional<uint64_t>(4));
}

TEST(HttpServerConnectionLimitsTest, ValidateBodyHeadersInvalidContentLength) {
  ServerConnection::BodyHeadersValidationResult result{};
  ServerConnection::update_body_related_header("Content-Length", "invalid",
                                               result);
  EXPECT_FALSE(result.has_transfer_encoding);
  EXPECT_TRUE(result.has_content_length);
  EXPECT_TRUE(result.invalid_content_length);
  EXPECT_FALSE(result.content_length.has_value());
}

TEST(HttpServerConnectionLimitsTest,
     ValidateBodyHeadersConflictingDuplicateContentLength) {
  ServerConnection::BodyHeadersValidationResult result;
  ServerConnection::update_body_related_header("Content-Length", "5", result);
  ServerConnection::update_body_related_header("Content-Length", "6", result);

  EXPECT_TRUE(result.invalid_content_length);
}

TEST(HttpServerConnectionLimitsTest,
     InvalidRequestBodyHeadersReasonUsesCnoErrorDetail) {
  const cno_error_t invalid_content_length{
      CNO_ERRNO_PROTOCOL, CNO_ERROR_DETAIL_INVALID_CONTENT_LENGTH,
      "diagnostic text may change"};
  EXPECT_EQ(
      ServerConnectionTestAccess::invalid_request_body_headers_rejection_reason(
          &invalid_content_length),
      std::optional<std::string_view>("invalid Content-Length"));

  const cno_error_t multiple_content_lengths{
      CNO_ERRNO_PROTOCOL, CNO_ERROR_DETAIL_MULTIPLE_CONTENT_LENGTHS,
      "another diagnostic text"};
  EXPECT_EQ(
      ServerConnectionTestAccess::invalid_request_body_headers_rejection_reason(
          &multiple_content_lengths),
      std::optional<std::string_view>("invalid Content-Length"));

  const cno_error_t protocol_error_without_detail{
      CNO_ERRNO_PROTOCOL, CNO_ERROR_DETAIL_NONE, "invalid content-length"};
  EXPECT_FALSE(
      ServerConnectionTestAccess::invalid_request_body_headers_rejection_reason(
          &protocol_error_without_detail)
          .has_value());
}

}  // namespace

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
