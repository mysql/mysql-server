/* Copyright (c) 2024, Oracle and/or its affiliates.

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
#include <cstdint>
#include <limits>
#include "fts0vlc.ic"

using std::uint64_t;

namespace innodb_fts0vlc_unittest {

namespace {
constexpr uint64_t max_64 = std::numeric_limits<uint64_t>::max();
}

TEST(fts0vlc, fts_get_encoded_len) {
  EXPECT_EQ(fts_get_encoded_len(0ULL), 1);
  EXPECT_EQ(fts_get_encoded_len(127ULL), 1);
  EXPECT_EQ(fts_get_encoded_len(128ULL), 2);
  EXPECT_EQ(fts_get_encoded_len((1ULL << 14) - 1), 2);
  EXPECT_EQ(fts_get_encoded_len(1ULL << 14), 3);
  EXPECT_EQ(fts_get_encoded_len((1ULL << 21) - 1), 3);
  EXPECT_EQ(fts_get_encoded_len(1ULL << 21), 4);
  EXPECT_EQ(fts_get_encoded_len((1ULL << 28) - 1), 4);
  EXPECT_EQ(fts_get_encoded_len(1ULL << 28), 5);

  /* max 32-bit unsigned integer */
  EXPECT_EQ(fts_get_encoded_len((1ULL << 32) - 1), 5);

  EXPECT_EQ(fts_get_encoded_len((1ULL << 35) - 1), 5);
  EXPECT_EQ(fts_get_encoded_len(1ULL << 35), 6);
  EXPECT_EQ(fts_get_encoded_len((1ULL << 42) - 1), 6);
  EXPECT_EQ(fts_get_encoded_len(1ULL << 42), 7);
  EXPECT_EQ(fts_get_encoded_len((1ULL << 49) - 1), 7);
  EXPECT_EQ(fts_get_encoded_len(1ULL << 49), 8);
  EXPECT_EQ(fts_get_encoded_len((1ULL << 56) - 1), 8);
  EXPECT_EQ(fts_get_encoded_len(1ULL << 56), 9);
  EXPECT_EQ(fts_get_encoded_len((1ULL << 63) - 1), 9);
  EXPECT_EQ(fts_get_encoded_len(1ULL << 63), 10);
  EXPECT_EQ(fts_get_encoded_len(max_64), 10);
}

TEST(fts0vlc, fts_encode_int_decode) {
  /* Variable-length integer coding packs consecutive groups of 7 bits,
    starting with most-significant byte, into subsequent bytes of
    the buffer, using the most-significant bit to mark the end of
    the encoded sequence
  */

  byte buf[12]{};
  byte *bufptr{buf};
  constexpr byte filler = 0xa5;
  for (byte &elem : buf) {
    elem = filler;
  }

  EXPECT_EQ(fts_encode_int(0ULL, bufptr), 1);
  EXPECT_EQ(bufptr[0], 0x80);
  EXPECT_EQ(bufptr[1], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), 0ULL);
  EXPECT_EQ(bufptr, buf + 1);

  EXPECT_EQ(fts_encode_int(10ULL, bufptr), 1);
  EXPECT_EQ(buf[1], 0x8a);
  EXPECT_EQ(buf[2], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), 10ULL);
  EXPECT_EQ(bufptr, buf + 2);

  EXPECT_EQ(fts_encode_int(127ULL, bufptr), 1);
  EXPECT_EQ(buf[2], 0xff);
  EXPECT_EQ(buf[3], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), 127ULL);
  EXPECT_EQ(bufptr, buf + 3);

  EXPECT_EQ(fts_encode_int(128ULL, bufptr), 2);
  EXPECT_EQ(buf[3], 0x01);
  EXPECT_EQ(buf[4], 0x80);
  EXPECT_EQ(buf[5], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), 128ULL);
  EXPECT_EQ(bufptr, buf + 5);

  EXPECT_EQ(fts_encode_int(130ULL, bufptr), 2);
  EXPECT_EQ(buf[5], 0x01);
  EXPECT_EQ(buf[6], 0x82);
  EXPECT_EQ(buf[7], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), 130ULL);
  EXPECT_EQ(bufptr, buf + 7);

  EXPECT_EQ(fts_encode_int((1ULL << 14) - 1, bufptr), 2);
  EXPECT_EQ(buf[7], 0x7f);
  EXPECT_EQ(buf[8], 0xff);
  EXPECT_EQ(buf[9], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), (1ULL << 14) - 1);
  EXPECT_EQ(bufptr, buf + 9);

  EXPECT_EQ(fts_encode_int(1ULL << 14, bufptr), 3);
  EXPECT_EQ(buf[9], 0x01);
  EXPECT_EQ(buf[10], 0x00);
  EXPECT_EQ(buf[11], 0x80);
  EXPECT_EQ(fts_decode_vlc(&bufptr), 1ULL << 14);
  EXPECT_EQ(bufptr, buf + 12);

  bufptr = buf;
  for (byte &elem : buf) {
    elem = filler;
  }

  EXPECT_EQ(fts_encode_int((1ULL << 14) + 256, bufptr), 3);
  EXPECT_EQ(buf[0], 0x01);
  EXPECT_EQ(buf[1], 0x02);
  EXPECT_EQ(buf[2], 0x80);
  EXPECT_EQ(buf[3], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), (1ULL << 14) + 256);
  EXPECT_EQ(bufptr, buf + 3);

  EXPECT_EQ(fts_encode_int((1ULL << 21) - 1, bufptr), 3);
  EXPECT_EQ(buf[3], 0x7f);
  EXPECT_EQ(buf[4], 0x7f);
  EXPECT_EQ(buf[5], 0xff);
  EXPECT_EQ(buf[6], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), (1ULL << 21) - 1);
  EXPECT_EQ(bufptr, buf + 6);

  EXPECT_EQ(fts_encode_int(1ULL << 21, bufptr), 4);
  EXPECT_EQ(buf[6], 0x01);
  EXPECT_EQ(buf[7], 0x00);
  EXPECT_EQ(buf[8], 0x00);
  EXPECT_EQ(buf[9], 0x80);
  EXPECT_EQ(buf[10], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), 1ULL << 21);
  EXPECT_EQ(bufptr, buf + 10);

  bufptr = buf;
  for (byte &elem : buf) {
    elem = filler;
  }

  EXPECT_EQ(fts_encode_int((1ULL << 28) - 1, bufptr), 4);
  EXPECT_EQ(buf[0], 0x7f);
  EXPECT_EQ(buf[1], 0x7f);
  EXPECT_EQ(buf[2], 0x7f);
  EXPECT_EQ(buf[3], 0xff);
  EXPECT_EQ(buf[4], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), (1ULL << 28) - 1);
  EXPECT_EQ(bufptr, buf + 4);

  EXPECT_EQ(fts_encode_int(1ULL << 28, bufptr), 5);
  EXPECT_EQ(buf[4], 0x01);
  EXPECT_EQ(buf[5], 0x00);
  EXPECT_EQ(buf[6], 0x00);
  EXPECT_EQ(buf[7], 0x00);
  EXPECT_EQ(buf[8], 0x80);
  EXPECT_EQ(buf[9], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), 1ULL << 28);
  EXPECT_EQ(bufptr, buf + 9);

  bufptr = buf;
  for (byte &elem : buf) {
    elem = filler;
  }

  /* maximum 32-bit integer */
  EXPECT_EQ(fts_encode_int((1ULL << 32) - 1, bufptr), 5);
  EXPECT_EQ(buf[0], 0x0f);
  EXPECT_EQ(buf[1], 0x7f);
  EXPECT_EQ(buf[2], 0x7f);
  EXPECT_EQ(buf[3], 0x7f);
  EXPECT_EQ(buf[4], 0xff);
  EXPECT_EQ(buf[5], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), (1ULL << 32) - 1);
  EXPECT_EQ(bufptr, buf + 5);

  EXPECT_EQ(fts_encode_int((1ULL << 35) - 1, bufptr), 5);
  EXPECT_EQ(buf[5], 0x7f);
  EXPECT_EQ(buf[6], 0x7f);
  EXPECT_EQ(buf[7], 0x7f);
  EXPECT_EQ(buf[8], 0x7f);
  EXPECT_EQ(buf[9], 0xff);
  EXPECT_EQ(buf[10], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), (1ULL << 35) - 1);
  EXPECT_EQ(bufptr, buf + 10);

  bufptr = buf;
  for (byte &elem : buf) {
    elem = filler;
  }

  EXPECT_EQ(fts_encode_int(1ULL << 35, bufptr), 6);
  EXPECT_EQ(buf[0], 0x01);
  EXPECT_EQ(buf[1], 0x00);
  EXPECT_EQ(buf[2], 0x00);
  EXPECT_EQ(buf[3], 0x00);
  EXPECT_EQ(buf[4], 0x00);
  EXPECT_EQ(buf[5], 0x80);
  EXPECT_EQ(buf[6], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), 1ULL << 35);
  EXPECT_EQ(bufptr, buf + 6);

  EXPECT_EQ(fts_encode_int((1ULL << 42) - 1, bufptr), 6);
  EXPECT_EQ(buf[6], 0x7f);
  EXPECT_EQ(buf[7], 0x7f);
  EXPECT_EQ(buf[8], 0x7f);
  EXPECT_EQ(buf[9], 0x7f);
  EXPECT_EQ(buf[10], 0x7f);
  EXPECT_EQ(buf[11], 0xff);
  EXPECT_EQ(fts_decode_vlc(&bufptr), (1ULL << 42) - 1);
  EXPECT_EQ(bufptr, buf + 12);

  bufptr = buf;
  for (byte &elem : buf) {
    elem = filler;
  }

  EXPECT_EQ(fts_encode_int(1ULL << 42, bufptr), 7);
  EXPECT_EQ(buf[0], 0x01);
  EXPECT_EQ(buf[1], 0x00);
  EXPECT_EQ(buf[2], 0x00);
  EXPECT_EQ(buf[3], 0x00);
  EXPECT_EQ(buf[4], 0x00);
  EXPECT_EQ(buf[5], 0x00);
  EXPECT_EQ(buf[6], 0x80);
  EXPECT_EQ(buf[7], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), 1ULL << 42);
  EXPECT_EQ(bufptr, buf + 7);

  bufptr = buf;
  for (byte &elem : buf) {
    elem = filler;
  }

  EXPECT_EQ(fts_encode_int((1ULL << 49) - 1, bufptr), 7);
  EXPECT_EQ(buf[0], 0x7f);
  EXPECT_EQ(buf[1], 0x7f);
  EXPECT_EQ(buf[2], 0x7f);
  EXPECT_EQ(buf[3], 0x7f);
  EXPECT_EQ(buf[4], 0x7f);
  EXPECT_EQ(buf[5], 0x7f);
  EXPECT_EQ(buf[6], 0xff);
  EXPECT_EQ(buf[7], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), (1ULL << 49) - 1);
  EXPECT_EQ(bufptr, buf + 7);

  bufptr = buf;
  for (byte &elem : buf) {
    elem = filler;
  }

  EXPECT_EQ(fts_encode_int(1ULL << 49, bufptr), 8);
  EXPECT_EQ(buf[0], 0x01);
  EXPECT_EQ(buf[1], 0x00);
  EXPECT_EQ(buf[2], 0x00);
  EXPECT_EQ(buf[3], 0x00);
  EXPECT_EQ(buf[4], 0x00);
  EXPECT_EQ(buf[5], 0x00);
  EXPECT_EQ(buf[6], 0x00);
  EXPECT_EQ(buf[7], 0x80);
  EXPECT_EQ(buf[8], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), 1ULL << 49);
  EXPECT_EQ(bufptr, buf + 8);

  bufptr = buf;
  for (byte &elem : buf) {
    elem = filler;
  }

  EXPECT_EQ(fts_encode_int((1ULL << 56) - 1, bufptr), 8);
  EXPECT_EQ(buf[0], 0x7f);
  EXPECT_EQ(buf[1], 0x7f);
  EXPECT_EQ(buf[2], 0x7f);
  EXPECT_EQ(buf[3], 0x7f);
  EXPECT_EQ(buf[4], 0x7f);
  EXPECT_EQ(buf[5], 0x7f);
  EXPECT_EQ(buf[6], 0x7f);
  EXPECT_EQ(buf[7], 0xff);
  EXPECT_EQ(buf[8], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), (1ULL << 56) - 1);
  EXPECT_EQ(bufptr, buf + 8);

  bufptr = buf;
  for (byte &elem : buf) {
    elem = filler;
  }

  EXPECT_EQ(fts_encode_int(1ULL << 56, bufptr), 9);
  EXPECT_EQ(buf[0], 0x01);
  EXPECT_EQ(buf[1], 0x00);
  EXPECT_EQ(buf[2], 0x00);
  EXPECT_EQ(buf[3], 0x00);
  EXPECT_EQ(buf[4], 0x00);
  EXPECT_EQ(buf[5], 0x00);
  EXPECT_EQ(buf[6], 0x00);
  EXPECT_EQ(buf[7], 0x00);
  EXPECT_EQ(buf[8], 0x80);
  EXPECT_EQ(buf[9], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), 1ULL << 56);
  EXPECT_EQ(bufptr, buf + 9);

  bufptr = buf;
  for (byte &elem : buf) {
    elem = filler;
  }

  EXPECT_EQ(fts_encode_int((1ULL << 63) - 1, bufptr), 9);
  EXPECT_EQ(buf[0], 0x7f);
  EXPECT_EQ(buf[1], 0x7f);
  EXPECT_EQ(buf[2], 0x7f);
  EXPECT_EQ(buf[3], 0x7f);
  EXPECT_EQ(buf[4], 0x7f);
  EXPECT_EQ(buf[5], 0x7f);
  EXPECT_EQ(buf[6], 0x7f);
  EXPECT_EQ(buf[7], 0x7f);
  EXPECT_EQ(buf[8], 0xff);
  EXPECT_EQ(buf[9], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), (1ULL << 63) - 1);
  EXPECT_EQ(bufptr, buf + 9);

  bufptr = buf;
  for (byte &elem : buf) {
    elem = filler;
  }

  EXPECT_EQ(fts_encode_int(1ULL << 63, bufptr), 10);
  EXPECT_EQ(buf[0], 0x01);
  EXPECT_EQ(buf[1], 0x00);
  EXPECT_EQ(buf[2], 0x00);
  EXPECT_EQ(buf[3], 0x00);
  EXPECT_EQ(buf[4], 0x00);
  EXPECT_EQ(buf[5], 0x00);
  EXPECT_EQ(buf[6], 0x00);
  EXPECT_EQ(buf[7], 0x00);
  EXPECT_EQ(buf[8], 0x00);
  EXPECT_EQ(buf[9], 0x80);
  EXPECT_EQ(buf[10], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), 1ULL << 63);
  EXPECT_EQ(bufptr, buf + 10);

  bufptr = buf;
  for (byte &elem : buf) {
    elem = filler;
  }

  EXPECT_EQ(fts_encode_int(max_64, bufptr), 10);
  EXPECT_EQ(buf[0], 0x01);
  EXPECT_EQ(buf[1], 0x7f);
  EXPECT_EQ(buf[2], 0x7f);
  EXPECT_EQ(buf[3], 0x7f);
  EXPECT_EQ(buf[4], 0x7f);
  EXPECT_EQ(buf[5], 0x7f);
  EXPECT_EQ(buf[6], 0x7f);
  EXPECT_EQ(buf[7], 0x7f);
  EXPECT_EQ(buf[8], 0x7f);
  EXPECT_EQ(buf[9], 0xff);
  EXPECT_EQ(buf[10], filler);
  EXPECT_EQ(fts_decode_vlc(&bufptr), max_64);
  EXPECT_EQ(bufptr, buf + 10);
}

}  // namespace innodb_fts0vlc_unittest
