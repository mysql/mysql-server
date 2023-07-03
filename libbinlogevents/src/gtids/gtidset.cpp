/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include "libbinlogevents/include/gtids/gtidset.h"
#include <map>
#include <string>

namespace binary_log::gtids {

std::size_t Gno_interval::count() const {
  return m_next_gno_after_end - m_start;
}

Gno_interval::Gno_interval(gno_t start, gno_t end)
    : m_start(start), m_next_gno_after_end(end + 1) {}

Gno_interval::Gno_interval(const Gno_interval &other)
    : m_start(other.get_start()), m_next_gno_after_end(other.get_end() + 1) {}

Gno_interval &Gno_interval::operator=(const Gno_interval &other) {
  m_start = other.get_start();
  m_next_gno_after_end = other.get_end() + 1;
  return *this;
}

bool Gno_interval::operator==(const Gno_interval &other) const {
  return ((other.get_end() + 1) == m_next_gno_after_end) &&
         other.get_start() == m_start;
}

bool Gno_interval::operator<(const Gno_interval &other) const {
  if (m_start < other.get_start()) return true;
  if (m_start > other.get_start()) return false;
  if ((m_next_gno_after_end - 1) < other.get_end()) return true;

  return false;
}

bool Gno_interval::intersects(const Gno_interval &other) const {
  bool other_starts_in_this_interval =
      (other.get_start() >= m_start &&
       other.get_start() < m_next_gno_after_end);
  bool this_starts_in_other_interval =
      (m_start >= other.get_start() && m_start <= other.get_end());

  return other_starts_in_this_interval || this_starts_in_other_interval;
}

bool Gno_interval::contiguous(const Gno_interval &other) const {
  if (other.get_start() == m_next_gno_after_end) return true;
  if (other.get_end() + 1 == m_start) return true;
  return false;
}

bool Gno_interval::intersects_or_contiguous(const Gno_interval &other) const {
  return intersects(other) || contiguous(other);
}

gno_t Gno_interval::get_start() const { return m_start; }
gno_t Gno_interval::get_end() const { return m_next_gno_after_end - 1; }

bool Gno_interval::add(const Gno_interval &other) {
  if (intersects_or_contiguous(other)) {
    m_next_gno_after_end = std::max(other.get_end() + 1, m_next_gno_after_end);
    m_start = std::min(other.get_start(), m_start);
    return false;
  }
  return true;
}

std::string Gno_interval::to_string() const {
  std::stringstream ss;
  if (m_start == get_end())
    ss << m_start;
  else
    ss << m_start << SEPARATOR_GNO_START_END << get_end();
  return ss.str();
}

bool Gno_interval::is_valid() const {
  return m_start < m_next_gno_after_end && m_start > 0;
}

Gtid_set::~Gtid_set() = default;

Gtid_set &Gtid_set::operator=(const Gtid_set &other) {
  reset();
  add(other);

  return *this;
}

Gtid_set::Gtid_set(const Gtid_set &other) { *this = other; }

bool Gtid_set::operator==(const Gtid_set &other) const {
  const auto &other_interval_list = other.get_gtid_set();
  const auto &this_interval_list = m_gtid_set;

  if (other_interval_list.size() != this_interval_list.size()) return false;

  for (auto const &[uuid, other_intervals] : other_interval_list) {
    auto it = this_interval_list.find(uuid);

    // uuid does not exist in this set
    if (it == this_interval_list.end()) return false;

    // uuid exists, lets check if the set of intervals match
    const auto &this_intervals = it->second;
    if (this_intervals.size() != other_intervals.size()) return false;

    // check each interval for this uuid
    for (const auto &interval : this_intervals) {
      if (other_intervals.find(interval) == other_intervals.end()) return false;
    }
  }

  return true;
}

bool Gtid_set::Uuid_comparator::operator()(const Uuid &lhs,
                                           const Uuid &rhs) const {
  return memcmp(rhs.bytes, lhs.bytes, Uuid::BYTE_LENGTH) > 0;
}

const Gtid_set::Gno_interval_list &Gtid_set::get_gtid_set() const {
  return m_gtid_set;
}

bool Gtid_set::do_add(const Uuid &uuid, const Gno_interval &interval) {
  auto it = m_gtid_set.find(uuid);
  if (it == m_gtid_set.end()) {
    std::set<Gno_interval> intervals;
    intervals.insert(interval);
    m_gtid_set.insert(std::pair<Uuid, std::set<Gno_interval>>(uuid, intervals));
    return false;
  }

  auto &intervals = it->second;

  /*
   * Iterate over the list of intervals and whenever we find a contiguous
   * interval remove it, merge the intervals and move on to the next interval.
   *
   * Eventually at the end, we will have the interval adjusted to merge.
   */
  auto current = interval;
  for (auto iterator = intervals.begin(); iterator != intervals.end();) {
    auto &next = *iterator;
    if (next.intersects_or_contiguous(current)) {
      current.add(next);
      /* returns the next element after the one erased */
      iterator = intervals.erase(iterator);
    } else {
      ++iterator;
    }
  }

  intervals.insert(current);

  return false;
}

bool Gtid_set::add(const Uuid &uuid, const Gno_interval &interval) {
  return do_add(uuid, interval);
}

bool Gtid_set::add(const Gtid_set &other) {
  for (auto const &[uuid, intervals] : other.m_gtid_set) {
    for (auto &interval : intervals) {
      if (do_add(uuid, interval)) return true;
    }
  }

  return false;
}

bool Gtid_set::add(const Gtid &gtid) {
  return do_add(gtid.get_uuid(), Gno_interval{gtid.get_gno(), gtid.get_gno()});
}

std::string Gtid_set::to_string() const {
  if (m_gtid_set.empty()) {
    return EMPTY_GTID_SET;
  }

  std::stringstream ss;
  for (auto &[uuid, intervals] : m_gtid_set) {
    ss << uuid.to_string() << Gtid::SEPARATOR_UUID_SEQNO;
    assert(!intervals.empty());

    for (auto &interval : intervals) {
      ss << interval.to_string() << SEPARATOR_SEQNO_INTERVALS;
    }
    ss.seekp(-1, std::ios_base::end);
    ss << SEPARATOR_UUID_SETS;
  }

  ss.seekp(-1, std::ios_base::end);
  return std::string(ss.str(), 0, ss.str().size() - 1);
}

bool Gtid_set::contains(const Gtid &gtid) const {
  auto it = m_gtid_set.find(gtid.get_uuid());
  if (it == m_gtid_set.end()) return false;

  for (const auto &interval : it->second) {
    if (gtid.get_gno() >= interval.get_start() &&
        gtid.get_gno() <= interval.get_end())
      return true;
  }
  return false;
}

bool Gtid_set::is_empty() const { return m_gtid_set.empty(); }

std::size_t Gtid_set::count() const {
  std::size_t count{0};

  for (auto const &[uuid, intervals] : m_gtid_set) {
    for (auto &interval : intervals) {
      count += interval.count();
    }
  }

  return count;
}

void Gtid_set::reset() { m_gtid_set.clear(); }

}  // namespace binary_log::gtids
