/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include "sql/xa/xid_extract.h"
#include <regex>
#include "unhex.h"

namespace xa::extractor {
/**
  Converts an array of bytes in hexadecimal format to their raw
  counterpart.

  @param hexed The array of bytes in hexadecimal format.

  @return the array of raw bytes.
 */
std::string unhex(std::string const &hexed);
}  // namespace xa::extractor

xa::XID_extractor::XID_extractor(std::string const &source,
                                 size_t max_extractions) {
  this->extract(source, max_extractions);
}

size_t xa::XID_extractor::extract(std::string const &source,
                                  size_t max_extractions) {
  static const std::regex xid_regex{
      "X'((?:[0-9a-fA-F][0-9a-fA-F]){1,64})?'"  // GTRID
      "(?:[[:space:]]*),(?:[[:space:]]*)"       // white-space and comma
      "X'((?:[0-9a-fA-F][0-9a-fA-F]){1,64})?'"  // BQUAL
      "(?:[[:space:]]*),(?:[[:space:]]*)"       // white-space and comma
      "(0|[1-9][0-9]{0,19})",                   // FORMATID
      std::regex::optimize};

  this->m_xids.clear();

  std::smatch tokenizer;
  auto begin = source.begin();
  auto end = source.end();

  while (std::regex_search(begin, end, tokenizer, xid_regex)) {
    if (tokenizer.size() >= 4) {
      try {
        auto format_id = std::stol(tokenizer[3]);
        if (format_id >= 0) {
          auto gtrid = xa::extractor::unhex(tokenizer[1]);
          auto bqual = xa::extractor::unhex(tokenizer[2]);
          auto &xid = this->m_xids.emplace_back();
          xid.set(format_id, gtrid.data(), gtrid.length(), bqual.data(),
                  bqual.length());
        }
      } catch (...) {
      }

      if (max_extractions != this->m_xids.size() &&
          tokenizer.suffix().length() != 0) {
        begin = source.end() - tokenizer.suffix().length();
        continue;
      }
    }
    break;
  }

  return this->m_xids.size();
}

xa::XID_extractor::xid_list::iterator xa::XID_extractor::begin() {
  return this->m_xids.begin();
}

xa::XID_extractor::xid_list::iterator xa::XID_extractor::end() {
  return this->m_xids.end();
}

size_t xa::XID_extractor::size() { return this->m_xids.size(); }

xid_t &xa::XID_extractor::operator[](size_t idx) {
  assert(idx < this->m_xids.size());
  return this->m_xids[idx];
}

std::string xa::extractor::unhex(std::string const &hexed) {
  if (hexed.length() == 0) {
    return "";
  }
  constexpr size_t MAX_LEN{65};
  char buffer[MAX_LEN] = {0};
  assert(hexed.length() <= MAX_LEN * 2);
  ::unhex(hexed.data(), hexed.data() + hexed.length(), buffer);
  return std::string{buffer, hexed.length() / 2};
}
