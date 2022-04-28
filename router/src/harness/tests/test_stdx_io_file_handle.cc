/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include "mysql/harness/stdx/io/file_handle.h"

#include <gmock/gmock.h>

#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/expected_ostream.h"

namespace std {
std::ostream &operator<<(std::ostream &os, const std::error_condition &ec) {
  os << ec.message();
  return os;
}
}  // namespace std

namespace stdx {
namespace io {
std::ostream &operator<<(std::ostream &os, const file_handle &fh) {
  os << "handle: " << fh.native_handle();
  return os;
}
}  // namespace io
}  // namespace stdx

TEST(StdxIoFilehandleTest, uniquely_named_file) {
  SCOPED_TRACE("// create tmpfile");
  auto open_res = stdx::io::file_handle::uniquely_named_file(
      {}, stdx::io::mode::write, stdx::io::caching::temporary,
      stdx::io::flag::unlink_on_first_close);
  ASSERT_THAT(open_res,
              ::testing::Truly([](auto const &p) { return bool(p); }));
  auto fh = std::move(open_res.value());

  EXPECT_NE(fh.native_handle(), stdx::io::file_handle::invalid_handle);

  ASSERT_THAT(fh.close(),
              ::testing::Truly([](auto const &p) { return bool(p); }));

  EXPECT_EQ(fh.native_handle(), stdx::io::file_handle::invalid_handle);
}

TEST(StdxIoFilehandleTest, release) {
  SCOPED_TRACE("// create tmpfile");
  auto open_res = stdx::io::file_handle::uniquely_named_file(
      {}, stdx::io::mode::write, stdx::io::caching::temporary,
      stdx::io::flag::unlink_on_first_close);
  ASSERT_THAT(open_res,
              ::testing::Truly([](auto const &p) { return bool(p); }));
  auto fh = std::move(open_res.value());

  EXPECT_NE(fh.native_handle(), stdx::io::file_handle::invalid_handle);

  SCOPED_TRACE("// capture the filehandle to automatic close at test-end");
  auto fd = fh.release();
  EXPECT_NE(fd, stdx::io::file_handle::invalid_handle);

  stdx::io::file_handle cleanup(fd, 0, 0, fh.kernel_caching(), fh.flags());

  EXPECT_EQ(fh.native_handle(), stdx::io::file_handle::invalid_handle);

  SCOPED_TRACE("// releasing a release file-descriptor");
  EXPECT_EQ(fh.release(), stdx::io::file_handle::invalid_handle);
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
