/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
#ifndef MYSQL_HARNESS_HEXIFY_INCLUDED
#define MYSQL_HARNESS_HEXIFY_INCLUDED

#include <algorithm>  // copy
#include <array>
#include <charconv>
#include <cstdint>
#include <iterator>  // back_inserter
#include <string>
#include <utility>  // exchange

namespace mysql_harness {

/**
 * hexdump into a string.
 *
 * converts the contents of continous container (has .data() and .size())
 * as hex values in rows of 16 bytes.
 *
 * @param buf a container
 * @return string containing the hexdump
 */
template <class T>
inline std::string hexify(const T &buf) {
  std::string out;

  for (auto cur = reinterpret_cast<const uint8_t *>(buf.data()),
            end = cur + buf.size();
       cur != end;) {
    size_t col{};
    std::array<char, 16L * 3> hexline{
        '.', '.', ' ', '.', '.', ' ', '.', '.', ' ', '.', '.', ' ',
        '.', '.', ' ', '.', '.', ' ', '.', '.', ' ', '.', '.', ' ',
        '.', '.', ' ', '.', '.', ' ', '.', '.', ' ', '.', '.', ' ',
        '.', '.', ' ', '.', '.', ' ', '.', '.', ' ', '.', '.', ' '};
    std::array<char, 16> printable;

    for (auto *hexline_pos = hexline.data(); cur != end && col < 16;
         ++cur, ++col, hexline_pos += 3) {
      const auto ch = *cur;

      const auto res = std::to_chars(hexline_pos, hexline_pos + 2, ch, 16);
      if (res.ptr == hexline_pos + 1) {
        // insert a leading zero, if only one digit was produced.
        hexline_pos[1] = std::exchange(hexline_pos[0], '0');
      }

      printable[col] = std::isprint(ch) ? static_cast<char>(ch) : '.';
    }

    std::copy(hexline.begin(), hexline.end(), std::back_inserter(out));
    out.append(" ");  // separator between hexline and printable

    // only append the chars actually used.
    for (size_t ndx = 0; ndx < col; ++ndx) {
      out.push_back(printable[ndx]);
    }
    out.append("\n");
  }

  return out;
}
}  // namespace mysql_harness

#endif
