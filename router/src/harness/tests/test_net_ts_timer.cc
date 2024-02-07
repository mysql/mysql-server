/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#include "mysql/harness/net_ts/timer.h"

#include <chrono>

#include <gmock/gmock.h>
#include <gtest/gtest-typed-test.h>

#include "mysql/harness/net_ts/impl/socket.h"
#include "mysql/harness/stdx/expected_ostream.h"

template <class T>
class NetTS_timer : public ::testing::Test {};

using TimerTypes = ::testing::Types<net::system_timer, net::steady_timer>;
TYPED_TEST_SUITE(NetTS_timer, TimerTypes);

TYPED_TEST(NetTS_timer, timer_default_construct) {
  using clock_type = typename TypeParam::clock_type;

  net::io_context io_ctx;
  TypeParam timer(io_ctx);

  EXPECT_EQ(timer.expiry(), typename clock_type::time_point{});
}

TYPED_TEST(NetTS_timer, timer_expires_after) {
  using clock_type = typename TypeParam::clock_type;

  net::io_context io_ctx;
  TypeParam timer(io_ctx);

  using namespace std::chrono_literals;
  const auto wait_duration = 100ms;

  timer.expires_after(wait_duration);
  auto before = clock_type::now();
  timer.wait();

  EXPECT_GE(clock_type::now() - before, wait_duration);
}

TYPED_TEST(NetTS_timer, timer_expires_after_async) {
  using clock_type = typename TypeParam::clock_type;

  net::io_context io_ctx;

  ASSERT_TRUE(io_ctx.open_res()) << io_ctx.open_res().error();

  TypeParam timer(io_ctx);

  using namespace std::chrono_literals;
  const auto wait_duration = 100ms;

  timer.expires_after(wait_duration);
  auto before = clock_type::now();

  typename clock_type::time_point after{};

  timer.async_wait([&](std::error_code ec) {
    if (ec == std::errc::operation_canceled) {
      return;
    }

    after = clock_type::now();
  });

  EXPECT_THAT(io_ctx.run_one(), 1);

  // it changed
  EXPECT_NE(after, typename clock_type::time_point{});

  // it is after before
  EXPECT_GE(after, before);
  EXPECT_GE(clock_type::now() - before, wait_duration);
}

// check behaviour with sub-milliseconds timeouts
TYPED_TEST(NetTS_timer, timer_expires_after_async_short) {
  using clock_type = typename TypeParam::clock_type;

  net::io_context io_ctx;

  ASSERT_TRUE(io_ctx.open_res()) << io_ctx.open_res().error();

  TypeParam timer(io_ctx);

  using namespace std::chrono_literals;
  const auto wait_duration = 900us;

  timer.expires_after(wait_duration);
  auto before = clock_type::now();

  typename clock_type::time_point after{};

  timer.async_wait([&](std::error_code ec) {
    if (ec == std::errc::operation_canceled) {
      return;
    }

    after = clock_type::now();
  });

  EXPECT_THAT(io_ctx.run_one(), 1);

  // it changed
  EXPECT_NE(after, typename clock_type::time_point{});

  // it is after before
  EXPECT_GE(after, before);
  EXPECT_GE(clock_type::now() - before, wait_duration);
}

#if !defined(__cpp_lib_chrono) || __cpp_lib_chrono < 201611L
// __cpp_lib_chrono is defined in c++17 and later
//
// operator<< is defined in <chrono> with C++20
namespace std {
namespace chrono {
template <class CharT, class Traits, class Rep, class Period>
std::basic_ostream<CharT, Traits> &operator<<(
    std::basic_ostream<CharT, Traits> &os,
    const std::chrono::duration<Rep, Period> &dur) {
  os << dur.count();

  if (std::is_same<typename Period::type, std::nano>::value) {
    os << "ns";
  } else if (std::is_same<typename Period::type, std::micro>::value) {
    os << "us";
  } else if (std::is_same<typename Period::type, std::milli>::value) {
    os << "ms";
  } else if (std::is_same<typename Period::type, std::ratio<1>>::value) {
    os << "s";
  }

  return os;
}
}  // namespace chrono
}  // namespace std
#endif

TYPED_TEST(NetTS_timer, timer_expires_after_async_retry) {
  using clock_type = typename TypeParam::clock_type;

  net::io_context io_ctx;

  ASSERT_TRUE(io_ctx.open_res()) << io_ctx.open_res().error();

  TypeParam timer(io_ctx);

  using namespace std::chrono_literals;

  for (int ndx{}; ndx < 10; ++ndx) {
    io_ctx.restart();

    SCOPED_TRACE(std::to_string(ndx));
    const auto wait_duration = 2ms;

    timer.expires_after(wait_duration);

    auto before = clock_type::now();
    typename clock_type::time_point after{};

    timer.async_wait([&](std::error_code ec) {
      if (ec == std::errc::operation_canceled) {
        return;
      }

      after = clock_type::now();
    });

    EXPECT_THAT(io_ctx.run_one(), 1);

    // it changed
    EXPECT_NE(after, typename TypeParam::time_point{});

    // it is after before
    EXPECT_GE(after, before);
    EXPECT_GE(clock_type::now() - before, wait_duration);
  }
}

TYPED_TEST(NetTS_timer, timer_expires_at) {
  using clock_type = typename TypeParam::clock_type;

  net::io_context io_ctx;
  TypeParam timer(io_ctx);

  using namespace std::chrono_literals;
  const auto wait_duration = 100ms;

  timer.expires_at(clock_type::now() + wait_duration);
  auto before = clock_type::now();
  timer.wait();

  EXPECT_GE(clock_type::now() - before, wait_duration);
}

int main(int argc, char *argv[]) {
  net::impl::socket::init();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
