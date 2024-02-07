/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include <algorithm>

#include <gtest/gtest.h>

#include "unittest/gunit/benchmark.h"
#include "unittest/gunit/mysys_util.h"

#include "my_checksum.h"
// Unit tests for my_checksum function implemented with zlib and hardware
// intrinsics where supported.
using namespace mycrc32;

namespace mysys_my_checksum {
std::uint32_t VerifyChecksumFuncs(const unsigned char *buf,
                                  std::size_t length) {
  std::uint32_t crc_seed = 0xbadcafe;
  std::uint32_t expected_crc = crc32_z(crc_seed, buf, length);

  EXPECT_EQ(expected_crc, my_checksum(crc_seed, buf, length));
  EXPECT_EQ(expected_crc, PunnedCrc32<std::uint64_t>(crc_seed, buf, length));
  return expected_crc;
}

TEST(MysysMyChecksum, EmptyBuffer) {
  unsigned char b[1] = {'0'};
  EXPECT_EQ(0xbadcafe, VerifyChecksumFuncs(b, 0));
}

TEST(MysysMyChecksum, TenBytesZero) {
  unsigned char b[10] = {'0', '0', '0', '0', '0', '0', '0', '0', '0', '0'};
  EXPECT_EQ(272755629U, VerifyChecksumFuncs(b, 10));
}

TEST(MysysMyChecksum, TenBytesFF) {
  unsigned char b[10] = {0xff, 0xff, 0xff, 0xff, 0xff,
                         0xff, 0xff, 0xff, 0xff, 0xff};
  EXPECT_EQ(533143559U, VerifyChecksumFuncs(b, 10));
}

TEST(MysysMyChecksum, ThirtyOneBytes) {
  alignas(alignof(std::int64_t)) unsigned char b[] = {
      0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, 0x77, 0x66, 0x55,
      0x44, 0x33, 0x22, 0x11, 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60,
      0x70, 0x80, 0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0};
  EXPECT_EQ(2359828439U, VerifyChecksumFuncs(b, sizeof(b)));
  EXPECT_EQ(1093230115U, VerifyChecksumFuncs(b + 1, sizeof(b) - 1));
  EXPECT_EQ(3891498923U, VerifyChecksumFuncs(b + 4, sizeof(b) - 4));
  EXPECT_EQ(561217492U, VerifyChecksumFuncs(b + 7, sizeof(b) - 7));
}

TEST(MysysMyChecksum, IntegerCrc32_8bit) {
  unsigned char b = 0xba;
  std::uint32_t crc = 0xff;
  std::uint32_t zres = crc32_z(~crc, &b, 1U);
  EXPECT_EQ(IntegerCrc32(crc, b), ~zres);
}

TEST(MysysMyChecksum, IntegerCrc32_16bit) {
  unsigned char value_bytes[] = {0xaa, 0xbb};

  std::uint32_t crc = 0xbadcafe;
  std::uint32_t zres = crc32_z(~crc, value_bytes, sizeof(value_bytes));

  std::uint16_t value;
  memcpy(&value, value_bytes, sizeof(value_bytes));

  EXPECT_EQ(IntegerCrc32(crc, value), ~zres);
}

TEST(MysysMyChecksum, IntegerCrc32_32bit) {
  unsigned char value_bytes[] = {0xaa, 0xbb, 0xcc, 0xdd};
  std::uint32_t crc = 0xbadcafe;
  std::uint32_t zres = crc32_z(~crc, value_bytes, sizeof(value_bytes));
  std::uint32_t value;
  memcpy(&value, value_bytes, sizeof(value_bytes));
  EXPECT_EQ(IntegerCrc32(crc, value), ~zres);
}

TEST(MysysMyChecksum, IntegerCrc32_double_32bit) {
  unsigned char value_bytes[] = {0x99, 0x11, 0xaa, 0xbb,
                                 0xcc, 0xdd, 0xee, 0xff};
  std::uint32_t crc = 0xbadcafe;
  std::uint32_t zres = crc32_z(~crc, value_bytes, sizeof(value_bytes));
  std::uint32_t value1, value2;
  memcpy(&value1, value_bytes, sizeof(value1));
  memcpy(&value2, value_bytes + sizeof(value1), sizeof(value2));

  std::uint32_t crc1 = IntegerCrc32(crc, value1);
  EXPECT_EQ(IntegerCrc32(crc1, value2), ~zres);
}

TEST(MysysMyChecksum, IntegerCrc32_64bit) {
  unsigned char value_bytes[] = {0x11, 0x22, 0x33, 0x44,
                                 0x55, 0x66, 0x77, 0x88};
  std::uint64_t value;
  memcpy(&value, value_bytes, sizeof(value));
  std::uint32_t crc = 0xbadcafe;
  std::uint32_t zres = crc32_z(~crc, value_bytes, sizeof(value_bytes));
  EXPECT_EQ(IntegerCrc32(crc, value), ~zres);
}

static volatile std::uint32_t do_not_optimize = 0;
// 50k buffer, 8-byte using crc32_z directly
static void BM_crc32_z_50k(size_t num_iterations) {
  StopBenchmarkTiming();

  alignas(alignof(std::uint64_t)) unsigned char buf[50000];
  unsigned char v = 0xda;
  std::generate(buf, buf + sizeof(buf), [&] { return ++v; });

  std::uint32_t crc = 0xdeadcafe;
  StartBenchmarkTiming();

  for (size_t i = 0; i < num_iterations; ++i) {
    crc = crc32_z(crc, buf, sizeof(buf) - 1);
  }

  StopBenchmarkTiming();
  do_not_optimize = crc;
}
BENCHMARK(BM_crc32_z_50k)

// 50k buffer, 8-byte using my_checksum (will use intrinsics on ARM)
//
static void BM_my_checksum_50k(size_t num_iterations) {
  StopBenchmarkTiming();

  alignas(alignof(std::uint64_t)) unsigned char buf[50000];
  unsigned char v = 0xda;
  std::generate(buf, buf + sizeof(buf), [&] { return ++v; });

  std::uint32_t crc = 0xdeadcafe;
  StartBenchmarkTiming();

  for (size_t i = 0; i < num_iterations; ++i) {
    crc = my_checksum(crc, buf, sizeof(buf) - 1);
  }

  StopBenchmarkTiming();
  do_not_optimize = crc;
}
BENCHMARK(BM_my_checksum_50k)

#ifdef HAVE_ARMV8_CRC32_INTRINSIC

// Baseline 8 bit integer using crc32_z
static void BM_crc32_z_8bit(size_t num_iterations) {
  StopBenchmarkTiming();

  std::uint32_t crc = 0xdeadcafe;
  StartBenchmarkTiming();

  for (size_t i = 0; i < num_iterations; ++i) {
    std::uint8_t b = static_cast<std::uint8_t>(i);
    crc = crc32_z(crc, &b, 1);
  }

  StopBenchmarkTiming();
  do_not_optimize = crc;
}
BENCHMARK(BM_crc32_z_8bit)

// 8-bit integer using intrinsic wrapper overload
static void BM_IntegerCrc32_8bit(size_t num_iterations) {
  StopBenchmarkTiming();

  std::uint32_t crc = 0xdeadcafe;
  StartBenchmarkTiming();

  for (size_t i = 0; i < num_iterations; ++i) {
    crc = IntegerCrc32(crc, static_cast<std::uint8_t>(i));
  }

  StopBenchmarkTiming();
  do_not_optimize = crc;
}
BENCHMARK(BM_IntegerCrc32_8bit)

// Baseline 64-bit integer suing crc32_z
static void BM_crc32_z_64bit(size_t num_iterations) {
  StopBenchmarkTiming();

  std::uint32_t crc = 0xdeadcafe;
  StartBenchmarkTiming();

  for (size_t i = 0; i < num_iterations; ++i) {
    unsigned char buf[8];
    memcpy(buf, &i, sizeof(i));
    crc = crc32_z(crc, buf, 8);
  }

  StopBenchmarkTiming();
  do_not_optimize = crc;
}
BENCHMARK(BM_crc32_z_64bit)

// 64-bit integer using intrinsic wrapper overload
static void BM_IntegerCrc32_64bit(size_t num_iterations) {
  StopBenchmarkTiming();

  std::uint32_t crc = 0xdeadcafe;
  StartBenchmarkTiming();

  for (size_t i = 0; i < num_iterations; ++i) {
    std::uint64_t v = i;
    crc = IntegerCrc32(crc, v);
  }

  StopBenchmarkTiming();
  do_not_optimize = crc;
}
BENCHMARK(BM_IntegerCrc32_64bit)

// PunnedCrc32 algo 50k with 8-byte slices
static void BM_PunnedCrc32_50k(size_t num_iterations) {
  StopBenchmarkTiming();

  alignas(alignof(std::uint64_t)) unsigned char buf[50000];
  unsigned char v = 0xda;
  std::generate(buf, buf + sizeof(buf), [&] { return ++v; });

  std::uint32_t crc = 0xdeadcafe;
  StartBenchmarkTiming();

  for (size_t i = 0; i < num_iterations; ++i) {
    crc = PunnedCrc32<std::uint64_t>(crc, buf, sizeof(buf) - 1);
  }

  StopBenchmarkTiming();
  do_not_optimize = crc;
}
BENCHMARK(BM_PunnedCrc32_50k)

#endif /* HAVE_ARMV8_CRC32_INTRINSIC */

}  // namespace mysys_my_checksum
