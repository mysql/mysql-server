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

#include <string>

#include "gtest/gtest.h"

#include "destination_ssl_context.h"

namespace {

TEST(DestinationTlsContextTest,
     FailedReloadRetainsCacheHitWhileCacheMissUsesConfiguredSettings) {
  DestinationTlsContext context{/* session_cache_mode */ true,
                                /* ssl_session_cache_size */ 0,
                                /* ssl_session_cache_timeout */ 0};
  const auto cached_context = context.get("destination", "localhost");
  ASSERT_NE(nullptr, cached_context);

  context.ciphers("not-a-valid-cipher-list");
  std::string out_error;
  EXPECT_FALSE(context.reload(out_error));
  EXPECT_FALSE(out_error.empty());

  EXPECT_EQ(cached_context, context.get("destination", "localhost"));
  EXPECT_EQ(nullptr, context.get("new-destination", "localhost"));
}

TEST(DestinationTlsContextTest, ReloadRejectsInvalidKeyAndCrl) {
  DestinationTlsContext invalid_key{/* session_cache_mode */ true,
                                    /* ssl_session_cache_size */ 0,
                                    /* ssl_session_cache_timeout */ 0};
  invalid_key.client_key_and_cert_file("missing-key.pem", "missing-cert.pem");

  std::string out_error;
  EXPECT_FALSE(invalid_key.reload(out_error));
  EXPECT_FALSE(out_error.empty());

  DestinationTlsContext invalid_crl{/* session_cache_mode */ true,
                                    /* ssl_session_cache_size */ 0,
                                    /* ssl_session_cache_timeout */ 0};
  invalid_crl.verify(SslVerify::kVerifyCa);
  invalid_crl.ca_file(SSL_TEST_DATA_DIR "/cacert.pem");
  invalid_crl.crl_file("missing.crl");

  out_error.clear();
  EXPECT_FALSE(invalid_crl.reload(out_error));
  EXPECT_FALSE(out_error.empty());
}

}  // namespace

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
