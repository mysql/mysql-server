/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#include "mysql/harness/net_ts/impl/resolver.h"

#include <array>
#include <system_error>

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>

#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"

TEST(NetTS_impl_resolver, gethostname_buffer_empty) {
  std::array<char, 1> name{};
  const auto res = net::impl::resolver::gethostname(name.data(), 0);

#if defined(_WIN32)
  EXPECT_THAT(res, stdx::make_unexpected(
                       std::error_code(WSAEFAULT, std::system_category())));
#else
  EXPECT_THAT(res, stdx::make_unexpected(
                       make_error_code(std::errc::filename_too_long)));

#endif
}

TEST(NetTS_impl_resolver, gethostname_buffer_too_short) {
  // it is implementation defined what happens on truncation
  //
  // either,
  // - ENAMETOOLONG
  // - truncated hostname
  std::array<char, 1> name = {{0x01}};
  const auto res = net::impl::resolver::gethostname(name.data(), name.size());

  // glibc returns ENAMETOOLONG
  // glibc-2.1 returns EINVAL
  // macosx succeeds
#if defined(_WIN32)
  EXPECT_THAT(res, stdx::make_unexpected(
                       std::error_code(WSAEFAULT, std::system_category())));
#else
  EXPECT_THAT(res, ::testing::AnyOf(
                       // macosx returns success leading to a non-detectable
                       // hostname truncation.
                       stdx::expected<void, std::error_code>{},
                       // all others fail
                       stdx::make_unexpected(
                           make_error_code(std::errc::filename_too_long))));
#endif

  if (res) {
    // macosx returns success on trucation.
    EXPECT_THAT(name, ::testing::Contains('\0'));
  }
}

TEST(NetTS_impl_resolver, gethostname) {
  std::array<char, 255> name{};
  const auto res = net::impl::resolver::gethostname(name.data(), name.size());

  EXPECT_THAT(res, ::testing::Truly([](auto v) { return bool(v); }));
}

TEST(NetTS_impl_resolver, getnameinfo) {
  std::array<char, NI_MAXHOST> name{};
  std::array<char, NI_MAXSERV> serv{};
  struct sockaddr_in saddr {};

  saddr.sin_family = AF_INET;

  const auto res = net::impl::resolver::getnameinfo(
      reinterpret_cast<sockaddr *>(&saddr), sizeof(saddr), name.data(),
      name.size(), serv.data(), serv.size(), NI_NUMERICSERV | NI_NUMERICHOST);

  ASSERT_THAT(res, ::testing::Truly([](auto v) { return bool(v); }));
}

TEST(NetTS_impl_resolver, getnameinfo_invalid_sockaddr_size) {
  std::array<char, NI_MAXHOST> name{};
  std::array<char, NI_MAXSERV> serv{};
  struct sockaddr_in saddr {};

  saddr.sin_family = AF_INET;

  const auto res = net::impl::resolver::getnameinfo(
      reinterpret_cast<sockaddr *>(&saddr),
      sizeof(saddr.sin_family),  // if the size of the sockaddr is too small,
                                 // 'bad_family' should be triggered
      name.data(), name.size(), serv.data(), serv.size(),
      NI_NUMERICSERV | NI_NUMERICHOST);

  // windows: WSAEFAULT
  // solaris: resolver:4 (EAI_FAIL)
  // others: EAI_FAMILY
  // wine: WSAEAFNOSUPPORT
#if defined(_WIN32)
  EXPECT_THAT(res, ::testing::AnyOf(stdx::make_unexpected(make_error_code(
                                        net::ip::resolver_errc::bad_family)),
                                    stdx::make_unexpected(std::error_code(
                                        WSAEFAULT, std::system_category()))));
#else
  EXPECT_THAT(res, ::testing::AnyOf(stdx::make_unexpected(make_error_code(
                                        net::ip::resolver_errc::bad_family)),
                                    stdx::make_unexpected(make_error_code(
                                        net::ip::resolver_errc::fail))));
#endif
}

#if !defined(_WIN32) && !defined(__FreeBSD__) && !defined(__APPLE__)
// windows doesn't check for bad-flags, but returns ERROR_INSUFFICIENT_BUFFER
// freebsd, macosx doesn't check for bad-flags, but returns EAI_NONAME
TEST(NetTS_impl_resolver, getnameinfo_badflags) {
  std::array<char, NI_MAXHOST> name{};
  std::array<char, NI_MAXSERV> serv{};
  struct sockaddr_in saddr {};

  saddr.sin_family = AF_INET;

  const auto res = net::impl::resolver::getnameinfo(
      reinterpret_cast<sockaddr *>(&saddr), sizeof(saddr), name.data(),
      name.size(), serv.data(), serv.size(),
      0xffff  // bad-flags
  );

  EXPECT_EQ(res, stdx::make_unexpected(
                     make_error_code(net::ip::resolver_errc::bad_flags)));
}
#endif

TEST(NetTS_impl_resolver, getnameinfo_overflow) {
  std::array<char, 1> name{};  // buffer too small to resolve the address
  struct sockaddr_in saddr {};

  saddr.sin_family = AF_INET;
  saddr.sin_port = htons(80);

  const auto res = net::impl::resolver::getnameinfo(
      reinterpret_cast<sockaddr *>(&saddr), sizeof(saddr), name.data(),
      name.size(), nullptr, 0, 0);

  // glibc-2.12 (EL6): ENOSPC
  // glibc-2.27 (U18.04): EAI_OVERFLOW
  // glibc-2.31 (U20.04): EAI_AGAIN
  // freebsd: EAI_MEMORY
  // macosx: EAI_OVERFLOW
  // solaris: ENOSPC
  // windows: ERROR_INSUFFICIENT_BUFFER
  // wine: WSATRY_AGAIN
#if defined(_WIN32)
  EXPECT_THAT(res, ::testing::AnyOf(
                       stdx::make_unexpected(std::error_code(
                           ERROR_INSUFFICIENT_BUFFER, std::system_category())),
                       stdx::make_unexpected(std::error_code(make_error_code(
                           net::ip::resolver_errc::try_again)))));
#else
  EXPECT_THAT(
      res,
      ::testing::AnyOf(
          stdx::make_unexpected(make_error_code(std::errc::no_space_on_device)),
          stdx::make_unexpected(
              make_error_code(net::ip::resolver_errc::out_of_memory)),
          stdx::make_unexpected(
              make_error_code(net::ip::resolver_errc::try_again))
#if defined(EAI_OVERFLOW)
              ,
          stdx::make_unexpected(
              make_error_code(net::ip::resolver_errc::overflow))
#endif
              ));
#endif
}

TEST(NetTS_impl_resolver, getaddrinfo_numerichost_ipv4_loopback) {
  std::string name("127.0.0.1");
  struct addrinfo hints {};

  hints.ai_flags = AI_NUMERICHOST;

  const auto res =
      net::impl::resolver::getaddrinfo(name.data(), nullptr, &hints);

  ASSERT_TRUE(res);

  const auto *ainfo = res->get();

  // getaddrinfo succeeded, we should have a value
  ASSERT_NE(ainfo, nullptr);

  EXPECT_EQ(ainfo->ai_family, AF_INET);
  ASSERT_EQ(ainfo->ai_addr->sa_family, AF_INET);
}

TEST(NetTS_impl_resolver, getaddrinfo_numerichost_ipv4_mapped_ipv6) {
  std::string name("::ffff:127.0.0.1");
  struct addrinfo hints {};

  hints.ai_flags = AI_NUMERICHOST;

  const auto res =
      net::impl::resolver::getaddrinfo(name.data(), nullptr, &hints);

  ASSERT_TRUE(res) << res.error();

  const auto *ainfo = res->get();

  // getaddrinfo succeeded, we should have a value
  ASSERT_NE(ainfo, nullptr);

  // solaris: AF_INET
  // others: AF_INET6
  EXPECT_THAT(ainfo->ai_family, ::testing::AnyOf(AF_INET6, AF_INET));
  ASSERT_EQ(ainfo->ai_addr->sa_family, ainfo->ai_family);
}

TEST(NetTS_impl_resolver, getaddrinfo_numerichost_fail) {
  struct addrinfo hints {};

  hints.ai_flags = AI_NUMERICHOST;
  hints.ai_family = AF_INET;

  const auto res =
      net::impl::resolver::getaddrinfo("localhost", nullptr, &hints);

  EXPECT_EQ(res, stdx::make_unexpected(
                     make_error_code(net::ip::resolver_errc::host_not_found)));
}

TEST(NetTS_impl_resolver, getaddrinfo_numericserv) {
  struct addrinfo hints {};

  hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;

  const auto res = net::impl::resolver::getaddrinfo("127.0.0.1", "80", &hints);

#if !defined(__sun)
  // solaris returns EAI_NONAME if AI_NUMERICSERV and serv is a numeric string
  // even though it is documented as working.
  //
  // it works if AI_NUMERICSERV is not specified.
  ASSERT_TRUE(res) << res.error();

  const auto *ainfo = res->get();

  // getaddrinfo succeeded, we should have a value
  ASSERT_NE(ainfo, nullptr);

  EXPECT_EQ(ainfo->ai_family, AF_INET);
  ASSERT_EQ(ainfo->ai_addr->sa_family, AF_INET);
  EXPECT_EQ(reinterpret_cast<sockaddr_in *>(ainfo->ai_addr)->sin_family,
            AF_INET);
  EXPECT_EQ(reinterpret_cast<sockaddr_in *>(ainfo->ai_addr)->sin_port,
            ntohs(80));
#endif
}

TEST(NetTS_impl_resolver, getaddrinfo_numericserv_fail) {
  struct addrinfo hints {};

  hints.ai_flags = AI_NUMERICSERV;

  const auto res =
      net::impl::resolver::getaddrinfo("127.0.0.1", "http", &hints);

  EXPECT_EQ(res, stdx::make_unexpected(
                     make_error_code(net::ip::resolver_errc::host_not_found)));
}

TEST(NetTS_impl_resolver, getaddrinfo_fail_empty_host) {
  const auto res = net::impl::resolver::getaddrinfo(nullptr, nullptr, nullptr);

  EXPECT_EQ(res, stdx::make_unexpected(
                     make_error_code(net::ip::resolver_errc::host_not_found)));
}

TEST(NetTS_impl_resolver, inetntop_nospace) {
  struct sockaddr_in saddr {};

  saddr.sin_family = AF_INET;

  std::array<char, 1> name{};

  const auto res =
      net::impl::resolver::inetntop(AF_INET, &saddr, name.data(), name.size());

#if defined(_WIN32)
  // msdn says: ERROR_INVALID_PARAMETER (87)
  // wine's ws2_32 returns: STATUS_INVALID_PARAMETER (0xc000000d)
  // windows returns: WSAEINVAL (10022)
  EXPECT_THAT(res,
              ::testing::AnyOf(stdx::make_unexpected(std::error_code{
                                   static_cast<int>(STATUS_INVALID_PARAMETER),
                                   std::system_category()}),
                               stdx::make_unexpected(make_error_condition(
                                   std::errc::invalid_argument))));
#else
  EXPECT_THAT(res, stdx::make_unexpected(
                       make_error_condition(std::errc::no_space_on_device)));
#endif
}

TEST(NetTS_impl_resolver, inetntop_ipv4) {
  struct in_addr addr {};

  std::array<char, INET_ADDRSTRLEN> name{};

  const auto res =
      net::impl::resolver::inetntop(AF_INET, &addr, name.data(), name.size());

  ASSERT_TRUE(res) << res.error();
  EXPECT_STREQ(res.value(), "0.0.0.0");
}

TEST(NetTS_impl_resolver, inetntop_ipv6) {
  struct in6_addr addr {};

  std::array<char, INET6_ADDRSTRLEN> name{};

  const auto res =
      net::impl::resolver::inetntop(AF_INET6, &addr, name.data(), name.size());

  ASSERT_TRUE(res) << res.error();
  EXPECT_STREQ(res.value(), "::");
}

TEST(NetTS_impl_resolver, inetntop_fail_invalid_protocol) {
  struct in6_addr addr {};

  std::array<char, INET6_ADDRSTRLEN> name{};

  const auto res =
      net::impl::resolver::inetntop(AF_UNIX, &addr, name.data(), name.size());

  EXPECT_EQ(res, stdx::make_unexpected(make_error_condition(
                     std::errc::address_family_not_supported)));
}

TEST(NetTS_impl_resolver, inetntop_fail_empty_buffer) {
  struct in6_addr addr {};

  std::array<char, INET6_ADDRSTRLEN> name{};

  const auto res =
      net::impl::resolver::inetntop(AF_INET6, &addr, name.data(), 0);

#if defined(_WIN32)
  // msdn says: ERROR_INVALID_PARAMETER (87)
  // wine's ws2_32 returns: STATUS_INVALID_PARAMETER (0xc000000d)
  // windows returns: WSAEINVAL (10022)
  EXPECT_THAT(res,
              ::testing::AnyOf(stdx::make_unexpected(std::error_code{
                                   static_cast<int>(STATUS_INVALID_PARAMETER),
                                   std::system_category()}),
                               stdx::make_unexpected(make_error_condition(
                                   std::errc::invalid_argument))));
#else
  EXPECT_THAT(res, stdx::make_unexpected(
                       make_error_condition(std::errc::no_space_on_device)));
#endif
}

int main(int argc, char *argv[]) {
  net::impl::socket::init();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
