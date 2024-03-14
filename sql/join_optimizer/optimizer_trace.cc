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

#include "sql/join_optimizer/optimizer_trace.h"
#include "sql/opt_trace.h"

std::streambuf::int_type TraceBuffer::overflow(int_type ch) {
  Segment &segment{[&]() -> Segment & {
    if (m_segments.size() < static_cast<size_t>(m_max_segments)) {
      // We did not exceed m_max_segments, so add another segment.
      return m_segments.emplace_back();
    }

    if (m_excess_segment == nullptr) {
      // Allocate one additional segment that is repeatedly overwritten with
      // all subsequent text.
      m_excess_segment = std::make_unique<Segment>();
    } else {
      ++m_full_excess_segments;
    }

    return *m_excess_segment;
  }()};

  segment[0] = ch;
  setp(std::to_address(segment.begin()) + 1, std::to_address(segment.end()));

  // Anything but EOF means 'ok'.
  return traits_type::not_eof(ch);
}
