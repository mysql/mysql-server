/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <gtest/gtest.h>

#include "libbinlogevents/include/codecs/binary.h"
#include "libbinlogevents/include/compression/zstd_comp.h"
#include "libbinlogevents/include/debug_vars.h"
#include "libbinlogevents/include/event_reader.h"
#include "libbinlogevents/include/uuid.h"

namespace unittests {

TEST(CompatibilityTest, TestCompression) {
  binary_log::transaction::compression::type test_var;
  static_cast<void>(test_var);
}

TEST(CompatibilityTest, TestEvent) {
  binary_log::Event_reader test_var(nullptr, 0);
  static_cast<void>(test_var);
}

TEST(CompatibilityTest, TestGtid) {
  binary_log::gtids::Uuid test_var;
  static_cast<void>(test_var);
}

TEST(CompatibilityTest, TestDebug) {
  bool test_var = binary_log_debug::debug_checksum_test;
  static_cast<void>(test_var);
}

TEST(CompatibilityTest, TestCodecs) {
  binary_log::codecs::binary::Transaction_payload::fields test_var;
  static_cast<void>(test_var);
}

}  // namespace unittests
