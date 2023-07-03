/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include "my_alloc.h"
#include "sql/join_optimizer/overflow_bitset.h"

using std::vector;

TEST(OverflowBitsetTest, ZeroInitialize) {
  OverflowBitset s;
  if (sizeof(void *) == 8) {
    EXPECT_EQ(63, s.capacity());
  } else {
    EXPECT_EQ(31, s.capacity());
  }
  for (unsigned i = 0; i < s.capacity(); ++i) {
    EXPECT_FALSE(IsBitSet(i, s));
  }
  EXPECT_TRUE(s.is_inline());
}

TEST(OverflowBitsetTest, InitializeFromInt) {
  OverflowBitset s{0x7eadbeef};
  for (int i = 0; i < 31; ++i) {
    EXPECT_EQ(IsBitSet(i, s), IsBitSet(i, 0x7eadbeef));
  }
}

TEST(OverflowBitsetTest, TrivialCopy) {
  OverflowBitset s{0x7eadbeef};
  OverflowBitset t(s);
  for (int i = 0; i < 31; ++i) {
    EXPECT_EQ(IsBitSet(i, s), IsBitSet(i, t));
  }
}

TEST(OverflowBitsetTest, MutateInline) {
  MEM_ROOT mem_root;
  MutableOverflowBitset s{&mem_root, 30};
  for (int i = 0; i < 30; ++i) {
    if (i % 3 == 0) {
      s.SetBit(i);
    }
  }
  s.ClearBits(2, 9);
  s.ClearBit(27);

  OverflowBitset cs = std::move(s);
  for (int i = 0; i < 30; ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(i % 3 == 0 && (i < 2 || i >= 9) && (i != 27), IsBitSet(i, cs));
  }
}

TEST(OverflowBitsetTest, MutateOverflow) {
  MEM_ROOT mem_root;
  MutableOverflowBitset s{&mem_root, 200};
  for (int i = 0; i < 200; ++i) {
    if (i % 3 == 0) {
      s.SetBit(i);
    }
  }
  s.ClearBits(2, 9);
  s.ClearBits(60, 150);
  s.ClearBit(42);

  OverflowBitset cs = std::move(s);
  for (int i = 0; i < 200; ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(
        i % 3 == 0 && (i < 2 || i >= 9) && (i < 60 || i >= 150) && (i != 42),
        IsBitSet(i, cs));
  }
}

TEST(OverflowBitsetTest, AndOrXor) {  // Also tests Clone.
  for (int size : {63, 64, 200}) {
    SCOPED_TRACE(size);

    MEM_ROOT mem_root;
    MutableOverflowBitset s1{&mem_root, static_cast<size_t>(size)};
    MutableOverflowBitset s2{&mem_root, static_cast<size_t>(size)};
    for (int i = 0; i < size; ++i) {
      if (i % 3 == 0) {
        s1.SetBit(i);
      }
      if (i % 5 == 0) {
        s2.SetBit(i);
      }
    }

    OverflowBitset ors =
        OverflowBitset::Or(&mem_root, s1.Clone(&mem_root), s2.Clone(&mem_root));
    OverflowBitset ands = OverflowBitset::And(&mem_root, s1.Clone(&mem_root),
                                              s2.Clone(&mem_root));
    OverflowBitset xors = OverflowBitset::Xor(&mem_root, s1.Clone(&mem_root),
                                              s2.Clone(&mem_root));

    for (int i = 0; i < size; ++i) {
      const bool v1 = (i % 3 == 0);
      const bool v2 = (i % 5 == 0);

      SCOPED_TRACE(i);
      EXPECT_EQ(v1 | v2, IsBitSet(i, ors));
      EXPECT_EQ(v1 & v2, IsBitSet(i, ands));
      EXPECT_EQ(v1 ^ v2, IsBitSet(i, xors));
    }
  }
}

TEST(OverflowBitsetTest, BitsSetInInline) {
  OverflowBitset s{0x1005};
  vector<int> ret;
  for (int bit_num : BitsSetIn(s)) {
    ret.push_back(bit_num);
  }
  EXPECT_THAT(ret, testing::ElementsAre(0, 2, 12));
}

TEST(OverflowBitsetTest, BitsSetInOverflow) {
  MEM_ROOT mem_root;
  MutableOverflowBitset s{&mem_root, 200};
  s.SetBit(100);
  s.SetBit(180);
  s.SetBit(181);
  s.SetBit(199);

  vector<int> ret;
  for (int bit_num : BitsSetIn(std::move(s))) {
    ret.push_back(bit_num);
  }
  EXPECT_THAT(ret, testing::ElementsAre(100, 180, 181, 199));
}

TEST(OverflowBitsetTest, BitsSetInBothInline) {
  OverflowBitset s{0x1005}, t{0x1204};
  vector<int> ret;
  for (int bit_num : BitsSetInBoth(s, t)) {
    ret.push_back(bit_num);
  }
  EXPECT_THAT(ret, testing::ElementsAre(2, 12));
}

TEST(OverflowBitsetTest, BitsSetInBothOverflow) {
  MEM_ROOT mem_root;
  MutableOverflowBitset s{&mem_root, 200};
  s.SetBit(100);
  s.SetBit(180);
  s.SetBit(181);
  s.SetBit(199);
  MutableOverflowBitset t{&mem_root, 200};
  t.SetBit(100);
  t.SetBit(181);

  vector<int> ret;
  for (int bit_num : BitsSetInBoth(std::move(s), std::move(t))) {
    ret.push_back(bit_num);
  }
  EXPECT_THAT(ret, testing::ElementsAre(100, 181));
}

TEST(OverflowBitsetTest, OverlapsInline) {
  OverflowBitset s1{0x1005};
  OverflowBitset s2{0x0150};
  OverflowBitset s3{0xffff};

  EXPECT_FALSE(Overlaps(s1, s2));
  EXPECT_TRUE(Overlaps(s2, s3));
  EXPECT_TRUE(Overlaps(s1, s3));

  // Nothing overlaps with the empty set.
  OverflowBitset s4{0};
  EXPECT_FALSE(Overlaps(s1, s4));
  EXPECT_FALSE(Overlaps(s2, s4));
  EXPECT_FALSE(Overlaps(s3, s4));
  EXPECT_FALSE(Overlaps(s4, s1));
  EXPECT_FALSE(Overlaps(s4, s2));
  EXPECT_FALSE(Overlaps(s4, s3));
}

TEST(OverflowBitsetTest, OverlapsOverflow) {
  MEM_ROOT mem_root;
  MutableOverflowBitset s1_tmp{&mem_root, 200};
  MutableOverflowBitset s2_tmp{&mem_root, 200};
  MutableOverflowBitset s3_tmp{&mem_root, 200};

  s1_tmp.SetBit(1);
  s1_tmp.SetBit(100);
  s2_tmp.SetBit(60);
  s2_tmp.SetBit(160);
  s3_tmp.SetBit(1);
  s3_tmp.SetBit(160);

  OverflowBitset s1 = std::move(s1_tmp);
  OverflowBitset s2 = std::move(s2_tmp);
  OverflowBitset s3 = std::move(s3_tmp);
  EXPECT_FALSE(Overlaps(s1, s2));
  EXPECT_TRUE(Overlaps(s2, s3));
  EXPECT_TRUE(Overlaps(s1, s3));
}

TEST(OverflowBitsetTest, IsSubsetInline) {
  OverflowBitset s1{0x1005};
  OverflowBitset s2{0x0150};
  OverflowBitset s3{0xffff};

  EXPECT_TRUE(IsSubset(s1, s1));
  EXPECT_FALSE(IsSubset(s1, s2));
  EXPECT_TRUE(IsSubset(s1, s3));

  EXPECT_FALSE(IsSubset(s2, s1));
  EXPECT_TRUE(IsSubset(s2, s2));
  EXPECT_TRUE(IsSubset(s2, s3));

  EXPECT_FALSE(IsSubset(s3, s1));
  EXPECT_FALSE(IsSubset(s3, s2));
  EXPECT_TRUE(IsSubset(s3, s3));
}

TEST(OverflowBitsetTest, IsSubsetOverflow) {
  MEM_ROOT mem_root;
  MutableOverflowBitset s1_tmp{&mem_root, 200};
  MutableOverflowBitset s2_tmp{&mem_root, 200};
  MutableOverflowBitset s3_tmp{&mem_root, 200};

  s1_tmp.SetBit(1);
  s1_tmp.SetBit(100);

  s2_tmp.SetBit(60);
  s2_tmp.SetBit(160);

  s3_tmp.SetBit(1);
  s3_tmp.SetBit(60);
  s3_tmp.SetBit(100);
  s3_tmp.SetBit(160);

  OverflowBitset s1 = std::move(s1_tmp);
  OverflowBitset s2 = std::move(s2_tmp);
  OverflowBitset s3 = std::move(s3_tmp);

  EXPECT_TRUE(IsSubset(s1, s1));
  EXPECT_FALSE(IsSubset(s1, s2));
  EXPECT_TRUE(IsSubset(s1, s3));

  EXPECT_FALSE(IsSubset(s2, s1));
  EXPECT_TRUE(IsSubset(s2, s2));
  EXPECT_TRUE(IsSubset(s2, s3));

  EXPECT_FALSE(IsSubset(s3, s1));
  EXPECT_FALSE(IsSubset(s3, s2));
  EXPECT_TRUE(IsSubset(s3, s3));
}

TEST(OverflowBitsetTest, IsEmptyInline) {
  EXPECT_TRUE(IsEmpty(OverflowBitset{}));
  EXPECT_TRUE(IsEmpty(OverflowBitset{0}));
  EXPECT_FALSE(IsEmpty(OverflowBitset{1}));
}

TEST(OverflowBitsetTest, IsEmptyOverflow) {
  MEM_ROOT mem_root;
  MutableOverflowBitset s1{&mem_root, 200};
  EXPECT_TRUE(IsEmpty(std::move(s1)));

  MutableOverflowBitset s2{&mem_root, 200};
  s2.SetBit(186);
  EXPECT_FALSE(IsEmpty(std::move(s2)));
}

TEST(OverflowBitsetTest, PopulationCountInline) {
  MEM_ROOT mem_root;
  MutableOverflowBitset s{&mem_root, 30};
  for (int i = 0; i < 30; ++i) {
    if (i % 3 == 0) {
      s.SetBit(i);
    }
  }

  EXPECT_EQ(10, PopulationCount(std::move(s)));
}

TEST(OverflowBitsetTest, PopulationCountOverflow) {
  MEM_ROOT mem_root;
  MutableOverflowBitset s{&mem_root, 200};
  for (int i = 0; i < 200; ++i) {
    if (i % 3 == 0) {
      s.SetBit(i);
    }
  }
  EXPECT_EQ(67, PopulationCount(std::move(s)));
}
