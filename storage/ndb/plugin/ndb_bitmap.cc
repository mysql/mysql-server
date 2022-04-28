/*
   Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "storage/ndb/plugin/ndb_bitmap.h"

#include <iomanip>
#include <sstream>

std::string ndb_bitmap_to_hex_string(const MY_BITMAP *bitmap) {
  std::ostringstream os;
  os << "{";

  const char *separator = "";
  // The MY_BITMAP buffer size is always rounded up to 32 bit words, print
  // word by word
  for (size_t i = no_words_in_map(bitmap); i-- > 0;) {
    os << separator << std::hex << std::setw(8) << std::setfill('0')
       << bitmap->bitmap[i];
    separator = " ";
  }
  os << "}";
  return os.str();
}

#ifdef TEST_NDB_BITMAP
#include <NdbTap.hpp>

TAPTEST(NdbBitmap) {
  Ndb_bitmap_buf<1> buf1;
  OK(buf1.size_in_bytes() == 4);

  Ndb_bitmap_buf<16> buf16;
  OK(buf16.size_in_bytes() == 4);

  Ndb_bitmap_buf<31> buf31;
  OK(buf31.size_in_bytes() == 4);

  Ndb_bitmap_buf<32> buf32;
  OK(buf32.size_in_bytes() == 4);
  {
    MY_BITMAP b32;
    ndb_bitmap_init(&b32, buf32, 32);
    bitmap_set_bit(&b32, 0);
    bitmap_set_bit(&b32, 1);
    bitmap_set_bit(&b32, 31);
    OK(bitmap_is_set(&b32, 0));
    OK(bitmap_is_set(&b32, 1));
    OK(bitmap_is_set(&b32, 31));
  }

  Ndb_bitmap_buf<33> buf33;
  OK(buf33.size_in_bytes() == 8);

  Ndb_bitmap_buf<510> buf510;
  OK(buf510.size_in_bytes() == 64);

  Ndb_bitmap_buf<511> buf511;
  OK(buf511.size_in_bytes() == 64);

  Ndb_bitmap_buf<512> buf512;
  OK(buf512.size_in_bytes() == 64);
  {
    MY_BITMAP b512;
    ndb_bitmap_init(&b512, buf512, 512);
    bitmap_set_bit(&b512, 0);
    bitmap_set_bit(&b512, 1);
    bitmap_set_bit(&b512, 1);
    bitmap_set_bit(&b512, 511);
    OK(bitmap_is_set(&b512, 0));
    OK(bitmap_is_set(&b512, 1));
    OK(bitmap_is_set(&b512, 511));
  }

  return 1;  // OK
}
#endif
