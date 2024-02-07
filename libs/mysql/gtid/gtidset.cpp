/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "mysql/gtid/gtidset.h"
#include <map>
#include <string>

namespace mysql::gtid {

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

bool Gtid_set::operator==(const Gtid_set &other) const {
  const auto &other_set = other.get_gtid_set();
  const auto &this_set = m_gtid_set;

  if (other_set.size() != this_set.size()) return false;

  for (auto const &[other_uuid, other_tag_map] : other_set) {
    auto tag_map_it = this_set.find(other_uuid);
    // uuid does not exist in this set
    if (tag_map_it == this_set.end()) {
      return false;
    }

    const auto &this_tag_map = tag_map_it->second;

    // the number of tags
    if (this_tag_map.size() != other_tag_map.size()) {
      return false;
    }

    for (auto const &[other_tag, other_intervals] : other_tag_map) {
      auto it = this_tag_map.find(other_tag);
      // tsid does not exist in this set
      if (it == this_tag_map.end()) {
        return false;
      }

      // tsid exists, lets check if the set of intervals match
      const auto &this_intervals = it->second;
      if (this_intervals.size() != other_intervals.size()) return false;

      // check each interval for this tsid
      for (const auto &interval : this_intervals) {
        if (other_intervals.find(interval) == other_intervals.end())
          return false;
      }
    }
  }

  return true;
}

const Gtid_set::Tsid_interval_map &Gtid_set::get_gtid_set() const {
  return m_gtid_set;
}

bool Gtid_set::do_add(const Tsid &tsid, const Gno_interval &interval) {
  return this->do_add(tsid.get_uuid(), tsid.get_tag(), interval);
}

bool Gtid_set::do_add(const Uuid &uuid, const Tag &tag,
                      const Gno_interval &interval) {
  const auto &sid = uuid;
  auto gtid_set_it = m_gtid_set.find(sid);
  if (gtid_set_it == m_gtid_set.end()) {
    auto ret = m_gtid_set.insert(std::make_pair(sid, Tag_interval_map()));
    gtid_set_it = ret.first;
  }

  auto it = gtid_set_it->second.find(tag);
  if (it == gtid_set_it->second.end()) {
    std::set<Gno_interval> intervals;
    intervals.insert(interval);
    gtid_set_it->second.insert(std::make_pair(tag, intervals));
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

bool Gtid_set::add(const Tsid &tsid, const Gno_interval &interval) {
  return do_add(tsid, interval);
}

bool Gtid_set::add(const Gtid_set &other) {
  for (auto const &[uuid, tag_map] : other.m_gtid_set) {
    for (auto const &[tag, intervals] : tag_map) {
      for (auto &interval : intervals) {
        if (do_add(uuid, tag, interval)) return true;
      }
    }
  }

  return false;
}

bool Gtid_set::add(const Gtid &gtid) {
  return do_add(gtid.get_tsid(), Gno_interval{gtid.get_gno(), gtid.get_gno()});
}

std::string Gtid_set::to_string() const {
  if (m_gtid_set.empty()) {
    return empty_gtid_set_str;
  }

  std::stringstream ss;
  for (auto const &[uuid, tag_map] : m_gtid_set) {
    ss << uuid.to_string();
    ss << Gtid::separator_gtid;
    assert(!tag_map.empty());
    for (auto const &[tag, intervals] : tag_map) {
      if (tag.is_empty() == false) {
        ss << tag.to_string() << Gtid::separator_gtid;
      }
      assert(!intervals.empty());
      for (auto &interval : intervals) {
        ss << interval.to_string() << separator_interval;
      }
      ss.seekp(-1, std::ios_base::end);
      ss << Gtid::separator_gtid;
    }
    ss.seekp(-1, std::ios_base::end);
    ss << separator_uuid_set;
  }
  return std::string(ss.str(), 0, ss.str().size() - 1);
}

bool Gtid_set::contains(const Gtid &gtid) const {
  auto tag_map_it = m_gtid_set.find(gtid.get_uuid());
  if (tag_map_it == m_gtid_set.end()) return false;

  auto it = tag_map_it->second.find(gtid.get_tag());
  if (it == tag_map_it->second.end()) return false;

  for (const auto &interval : it->second) {
    if (gtid.get_gno() >= interval.get_start() &&
        gtid.get_gno() <= interval.get_end())
      return true;
  }
  return false;
}

std::size_t Gtid_set::get_num_tsids() const {
  std::size_t tsid_num = 0;
  for (auto const &[uuid, tag_map] : m_gtid_set) {
    tsid_num += tag_map.size();
  }
  return tsid_num;
}

bool Gtid_set::is_empty() const { return m_gtid_set.empty(); }

std::size_t Gtid_set::count() const {
  std::size_t count{0};

  for (auto const &[uuid, tag_map] : m_gtid_set) {
    for (auto const &[tag, intervals] : tag_map) {
      for (auto &interval : intervals) {
        count += interval.count();
      }
    }
  }

  return count;
}

void Gtid_set::reset() { m_gtid_set.clear(); }

Gtid_format Gtid_set::get_gtid_set_format() const {
  for (auto const &[uuid, tag_map] : m_gtid_set) {
    for (auto const &[tag, intervals] : tag_map) {
      if (tag.is_defined()) {
        return Gtid_format::tagged;
      }
    }
  }
  return Gtid_format::untagged;
}

}  // namespace mysql::gtid
