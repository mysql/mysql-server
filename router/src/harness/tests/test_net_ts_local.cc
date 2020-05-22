/*
  Copyright (c) 2020, Oracle and/or its affiliates.

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

#include "mysql/harness/net_ts/local.h"

#include <gmock/gmock.h>

#include "mysql/harness/net_ts/socket.h"  // net::impl::socket::init

#include "mysql/harness/stdx/expected_ostream.h"
#include "test/helpers.h"  // TmpDir

#ifndef _WIN32

template <class T>
class LocalProtocolTest : public ::testing::Test {
 public:
};

using LocalProtocolTypes =
    ::testing::Types<local::stream_protocol, local::datagram_protocol,
                     local::seqpacket_protocol>;

TYPED_TEST_SUITE(LocalProtocolTest, LocalProtocolTypes);

template <class T>
class LocalTwoWayProtocolTest : public ::testing::Test {
 public:
};

using LocalTwoWayProtocolTypes =
    ::testing::Types<local::stream_protocol, local::seqpacket_protocol>;

TYPED_TEST_SUITE(LocalTwoWayProtocolTest, LocalTwoWayProtocolTypes);

TYPED_TEST(LocalProtocolTest, endpoint_construct_default) {
  using protocol_type = TypeParam;
  using endpoint_type = typename protocol_type::endpoint;

  endpoint_type endpoint;

  // sizeof(sa_family_t) on Linux [2], larger on others
  EXPECT_GT(endpoint.size(), 0);
  EXPECT_EQ(endpoint.path().size(), 0);
  EXPECT_EQ(endpoint.path(), std::string());
  EXPECT_EQ(endpoint.capacity(), sizeof(sockaddr_un));
}

TYPED_TEST(LocalProtocolTest, endpoint_construct_pathname) {
  using protocol_type = TypeParam;
  using endpoint_type = typename protocol_type::endpoint;

  endpoint_type endpoint("/foo/bar");

  // at least the sizeof(family)
  EXPECT_GT(endpoint.size(), 8);
  EXPECT_EQ(endpoint.path().size(), 8);
  EXPECT_EQ(endpoint.path(), "/foo/bar");
}

TYPED_TEST(LocalProtocolTest, endpoint_construct_pathname_truncated) {
  using protocol_type = TypeParam;
  using endpoint_type = typename protocol_type::endpoint;

  endpoint_type endpoint(
      "/foo/bar/some/very/long/path/name/that/is/longer/than/108/or/so/chars/"
      "to/test/that/truncation/works/and/nothing/gets/overridden");

  // at least the sizeof(family)
  EXPECT_EQ(endpoint.size(), endpoint.capacity());
  EXPECT_LT(endpoint.path().size(), endpoint.capacity());
  EXPECT_THAT(endpoint.path(), ::testing::StartsWith("/foo/bar"));
}

TYPED_TEST(LocalProtocolTest, endpoint_resize_zero) {
  using endpoint_type = typename TypeParam::endpoint;

  endpoint_type endpoint("/foo/bar");

  EXPECT_GT(endpoint.size(), 8);

  endpoint.resize(0);

  // at least the sizeof(family)
  EXPECT_GT(endpoint.size(), 0);
  EXPECT_EQ(endpoint.path().size(), 0);
  EXPECT_EQ(endpoint.path(), std::string{});
}

TYPED_TEST(LocalProtocolTest, endpoint_resize_non_zero) {
  using protocol_type = TypeParam;
  using endpoint_type = typename protocol_type::endpoint;

  endpoint_type endpoint("/foo/bar");

  EXPECT_GT(endpoint.size(), 8);

  // cut away last char
  endpoint.resize(endpoint.size() - 1);

  // at least the sizeof(family)
  EXPECT_GT(endpoint.size(), 7);
  EXPECT_EQ(endpoint.path().size(), 7);
  EXPECT_EQ(endpoint.path(), "/foo/ba");
}

TYPED_TEST(LocalProtocolTest, endpoint_construct_abstract) {
  using protocol_type = TypeParam;
  using endpoint_type = typename protocol_type::endpoint;

  // workaround dev-studio's broken ""s for string-literals
#define S(x) std::string((x), sizeof(x) - 1)
  endpoint_type endpoint(S("\0/foo/bar"));

  // at least the size of the path + sa_family_t
  EXPECT_GT(endpoint.size(), 8);
  EXPECT_EQ(endpoint.path(), S("\0/foo/bar"));
#undef S
}

// check constexpr
static_assert(local::stream_protocol().family() != AF_UNSPEC, "");
static_assert(local::datagram_protocol().family() != AF_UNSPEC, "");
static_assert(local::seqpacket_protocol().family() != AF_UNSPEC, "");

static_assert(local::stream_protocol::endpoint().size() > 0, "");
static_assert(local::stream_protocol::endpoint().capacity() > 0, "");
static_assert(local::datagram_protocol::endpoint().size() > 0, "");
static_assert(local::datagram_protocol::endpoint().capacity() > 0, "");
static_assert(local::seqpacket_protocol::endpoint().size() > 0, "");
static_assert(local::seqpacket_protocol::endpoint().capacity() > 0, "");

// in C++20, this could succeed
// static_assert(local::stream_protocol::endpoint("foo").size() > 0, "");
#endif

int main(int argc, char *argv[]) {
  net::impl::socket::init();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
