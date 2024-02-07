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

#include "mysql/harness/net_ts/impl/file.h"

#include <gmock/gmock.h>

#include "mysql/harness/stdx/expected_ostream.h"

/**
 * @test closing an invalid file-descriptor should result in bad_file_desc.
 */
TEST(NetTS_impl_file, close_invalid_handle) {
#if !defined(_WIN32)
  // TODO(jkneschk) investigate why windows returns 'true' in this case
  // according to the docs it shouldn't

  const auto expected_ec =
#ifdef _WIN32
      std::error_code(ERROR_INVALID_HANDLE, std::system_category())
#else
      make_error_code(std::errc::bad_file_descriptor)
#endif
      ;

  EXPECT_EQ(net::impl::file::close(net::impl::file::kInvalidHandle),
            stdx::unexpected(expected_ec));
#endif
}

/**
 * @test ensure pipe() returns two fds which can be closed without error.
 */
TEST(NetTS_impl_file, pipe) {
  auto pipe_ec = net::impl::file::pipe();

  ASSERT_TRUE(pipe_ec) << pipe_ec.error();

  auto pipe_fds = std::move(pipe_ec.value());

  EXPECT_TRUE(net::impl::file::close(pipe_fds.first));
  EXPECT_TRUE(net::impl::file::close(pipe_fds.second));
}

#ifndef _WIN32
// TODO(jkneschk) how to get file flags like O_RDONLY and O_NONBLOCK on windows

/**
 * @test ensure pipe() returns two fds which can be closed without error.
 */
TEST(NetTS_impl_file, pipe_with_flags) {
  int flags{0};
#ifndef _WIN32
  flags = O_NONBLOCK;
#endif
  auto pipe_ec = net::impl::file::pipe(flags);

  ASSERT_TRUE(pipe_ec);

  auto pipe_fds = std::move(pipe_ec.value());

  auto fcntl_res = net::impl::file::fcntl(pipe_fds.first,
                                          net::impl::file::get_file_status());

  ASSERT_TRUE(fcntl_res);
  // freebsd returns O_NONBLOCK | O_RDWR
  // linux returns O_NONBLOCK | O_RDONLY
  //
  EXPECT_THAT(*fcntl_res, ::testing::AnyOf(flags | O_RDONLY, flags | O_RDWR));

  fcntl_res = net::impl::file::fcntl(pipe_fds.second,
                                     net::impl::file::get_file_status());
  ASSERT_TRUE(fcntl_res);
  // freebsd returns O_NONBLOCK | O_RDWR
  // linux returns O_NONBLOCK | O_RDONLY
  EXPECT_THAT(*fcntl_res, ::testing::AnyOf(flags | O_WRONLY, flags | O_RDWR));

  EXPECT_TRUE(net::impl::file::close(pipe_fds.first));
  EXPECT_TRUE(net::impl::file::close(pipe_fds.second));
}
#endif

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
