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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>

#include <array>
#include <cstring>

#include "my_byteorder.h"
#include "storage/heap/heapdef.h"

namespace heap_hash_unittest {

class HeapHashTest : public testing::TestWithParam<ha_base_keytype> {
 protected:
  void Init(bool composite = false, bool nullable = false) {
    real_segment = composite ? 1 : 0;
    keydef.seg = segments.data();
    keydef.keysegs = composite ? 3 : 1;
    segments[0].type = HA_KEYTYPE_BINARY;
    segments[0].start = 1;
    segments[0].length = 2;
    auto &real = segments[real_segment];
    real.type = GetParam();
    real.start = composite ? 3 : 1;  // Deliberately unaligned.
    real.length = GetParam() == HA_KEYTYPE_FLOAT ? 4 : 8;
    real.null_bit = nullable ? 1 : 0;
    real.null_pos = 0;
    segments[2].type = HA_KEYTYPE_BINARY;
    segments[2].start = real.start + real.length;
    segments[2].length = 4;
    for (auto &seg : segments) seg.charset = &my_charset_latin1;
    for (auto &record : records) {
      record[1] = 17;
      record[2] = 23;
      record[segments[2].start] = 41;
    }
  }

  void Store(uint i, double value) {
    uchar *pos = records[i].data() + segments[real_segment].start;
    if (GetParam() == HA_KEYTYPE_FLOAT)
      float4store(pos, static_cast<float>(value));
    else
      float8store(pos, value);
  }

  void MakeKeys() {
    for (uint i = 0; i < 2; ++i)
      hp_make_key(&keydef, keys[i].data(), records[i].data());
  }

  void ExpectEqual() {
    MakeKeys();
    EXPECT_EQ(hp_hashnr(&keydef, keys[0].data()),
              hp_hashnr(&keydef, keys[1].data()));
    EXPECT_EQ(hp_rec_hashnr(&keydef, records[0].data()),
              hp_rec_hashnr(&keydef, records[1].data()));
    for (uint i = 0; i < 2; ++i) {
      EXPECT_EQ(hp_hashnr(&keydef, keys[i].data()),
                hp_rec_hashnr(&keydef, records[i].data()));
      for (uint j = 0; j < 2; ++j) {
        EXPECT_EQ(0, hp_key_cmp(&keydef, records[i].data(), keys[j].data()));
        EXPECT_EQ(
            0, hp_rec_key_cmp(&keydef, records[i].data(), records[j].data()));
      }
    }
  }

  void ExpectDifferent() {
    MakeKeys();
    for (uint i = 0; i < 2; ++i) {
      const uint j = 1 - i;
      EXPECT_NE(0, hp_key_cmp(&keydef, records[i].data(), keys[j].data()));
      EXPECT_NE(0,
                hp_rec_key_cmp(&keydef, records[i].data(), records[j].data()));
    }
  }

  // Compare with the unchanged byte-hashing path on canonical +0 bytes.
  // A trailing segment makes an incorrect hash-state update observable.
  void ExpectCanonicalHash() {
    MakeKeys();
    const uint type = segments[real_segment].type;
    segments[real_segment].type = HA_KEYTYPE_BINARY;
    const uint64 expected = hp_rec_hashnr(&keydef, records[0].data());
    segments[real_segment].type = type;
    for (uint i = 0; i < 2; ++i) {
      EXPECT_EQ(expected, hp_hashnr(&keydef, keys[i].data()));
      EXPECT_EQ(expected, hp_rec_hashnr(&keydef, records[i].data()));
    }
  }

  HP_KEYDEF keydef{};
  std::array<HA_KEYSEG, 3> segments{};
  std::array<std::array<uchar, 32>, 2> records{};
  std::array<std::array<uchar, 32>, 2> keys{};
  uint real_segment{};
};

TEST_P(HeapHashTest, SignedZero) {
  Init();
  Store(0, 0.0);
  Store(1, -0.0);
  EXPECT_NE(0, memcmp(records[0].data(), records[1].data(), 32));
  ExpectEqual();
  ExpectCanonicalHash();
}

TEST_P(HeapHashTest, Nonzero) {
  Init();
  for (double value : {1.0, -1.0, 0.5, -0.5}) {
    Store(0, value);
    Store(1, value);
    ExpectEqual();
    ExpectCanonicalHash();
    Store(1, -value);
    ExpectDifferent();
  }
}

TEST_P(HeapHashTest, Composite) {
  Init(true);
  Store(0, 0.0);
  Store(1, -0.0);
  ExpectEqual();
  ExpectCanonicalHash();
  records[1][segments[2].start]++;
  ExpectDifferent();  // Equality must continue after the zero segment.
  records[1][segments[2].start]--;
  records[1][1]++;
  ExpectDifferent();
}

TEST_P(HeapHashTest, NullableComposite) {
  Init(true, true);
  Store(0, 0.0);
  Store(1, -0.0);
  ExpectEqual();
  ExpectCanonicalHash();
  records[1][0] = 1;
  ExpectDifferent();  // NULL is distinct from either zero sign.
  records[0][0] = 1;
  Store(1, 7.0);  // Bytes in a NULL segment must be ignored.
  ExpectEqual();
  records[1][segments[2].start]++;
  ExpectDifferent();
}

INSTANTIATE_TEST_SUITE_P(FloatingPoint, HeapHashTest,
                         testing::Values(HA_KEYTYPE_FLOAT, HA_KEYTYPE_DOUBLE));

}  // namespace heap_hash_unittest
