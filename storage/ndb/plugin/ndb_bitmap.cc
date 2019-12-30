/*
   Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.

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
