/*
  Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/harness/net_ts/impl/callstack.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace net::impl;  // Callstack

struct Executor {
  template <class Func>
  void run(Func f) {
    // add run() to the callstack of this thread
    Callstack<Executor>::Context callstack(this);
    f();
  }
};

// @test if callstack can detect if a func() was called in this threads executor
TEST(NetTS_impl_callstack, contains) {
  Executor executor;
  SCOPED_TRACE("// run check outside the executor");
  ASSERT_FALSE(Callstack<Executor>::contains(&executor));

  SCOPED_TRACE("// run check inside the executor");
  executor.run(
      [&executor] { ASSERT_TRUE(Callstack<Executor>::contains(&executor)); });
}

// @test if callstack can handle complex types like DebugInfo
TEST(NetTS_impl_callstack, debug_info) {
  // lots of ::testing in this test, let's simplify
  using namespace ::testing;

  // capture line number and function-name
  struct DebugInfo {
    int line;
    const char *func;

    DebugInfo(int line, const char *func) : line(line), func(func) {}
  };

  SCOPED_TRACE("// create an first stackframe");
  DebugInfo dbg_info(__LINE__, __func__);
  // add debuginfo to the callstack
  Callstack<DebugInfo>::Context dbg_ctx(&dbg_info);

  // check debuginfo is on the callstack
  ASSERT_THAT(
      Callstack<DebugInfo>(),
      ElementsAre(Property("key", &Callstack<DebugInfo>::Context::key,
                           Field(&DebugInfo::line, Eq(dbg_info.line)))));

  SCOPED_TRACE("// create another stackframe");
  [&outer_dbg_info = dbg_info] {
    DebugInfo inner_dbg_info(__LINE__, __func__);
    Callstack<DebugInfo>::Context dbg_ctx(&inner_dbg_info);

    SCOPED_TRACE("// check debuginfo is on the callstack");
    // [0] { 78, operator() }
    // [1] { 65, TestBody }
    ASSERT_THAT(
        Callstack<DebugInfo>(),
        ElementsAre(
            Property("key", &Callstack<DebugInfo>::Context::key,
                     Field(&DebugInfo::line, Eq(inner_dbg_info.line))),
            Property("key", &Callstack<DebugInfo>::Context::key,
                     Field(&DebugInfo::line, Eq(outer_dbg_info.line)))));
  }();

  SCOPED_TRACE("// 2nd stackframe removes itself again.");
  ASSERT_THAT(
      Callstack<DebugInfo>(),
      ElementsAre(Property("key", &Callstack<DebugInfo>::Context::key,
                           Field(&DebugInfo::line, Eq(dbg_info.line)))));
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
