/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_GTID_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_GTID_H_

#include <iostream>
#include <list>
#include <optional>
#include <string>
#include <vector>

#include "helper/string/from.h"
#include "mrs/database/entry/universal_id.h"
#include "mysql/harness/string_utils.h"

namespace mrs {
namespace database {

namespace inner {

class GTIDuuid : public entry::UniversalId {
 public:
  class DefaultHex {
   public:
    void operator()(std::ostream &os) {
      if (insert_separator(index++)) os << "-";
      os << std::setfill('0') << std::setw(2) << std::hex << std::uppercase;
    }

    int operator()(const uint8_t v) { return v; }

    bool insert_separator(int pos) {
      switch (pos) {
        case 4:
        case 6:
        case 8:
        case 10:
          return true;
        default:
          return false;
      }
    }
    int index{0};
  };

  std::string to_string() const {
    return helper::string::hex<decltype(raw), DefaultHex>(raw);
  }
};

template <typename ValueType>
ValueType abs(const ValueType v1, const ValueType v2) {
  if (v1 < v2) return v2 - v1;
  if (v1 > v2) return v1 - v2;

  return 0;
}

template <typename ValueType>
ValueType value(const std::optional<ValueType> &v) {
  return v.value();
}

template <typename ValueType>
inline ValueType value(const ValueType &v) {
  return v;
}

template <typename ValueType>
bool has_value(const std::optional<ValueType> &v) {
  return v.has_value();
}

template <typename ValueType>
inline bool has_value(const ValueType &) {
  return true;
}

template <typename ValueType, typename X>
ValueType max(X &&first) {
  return value(first);
}

template <typename ValueType, typename X, typename... Args>
ValueType max(X &&first, Args &&... rest) {
  ValueType rest_value = inner::max<ValueType>(rest...);
  if (!has_value(first)) return rest_value;
  if (rest_value > value(first)) return rest_value;
  return value(first);
}

class GtidRange {
 public:
  GtidRange(uint64_t start = 0, std::optional<uint64_t> end = {})
      : start_{start}, end_{end} {}

  GtidRange(const GtidRange &other) : start_{other.start_}, end_{other.end_} {}

  GtidRange &operator=(const GtidRange &other) {
    start_ = other.start_;
    end_ = other.end_;
    return *this;
  }

  bool operator==(const GtidRange &other) const {
    if (start_ != other.start_) return false;
    if (end_.has_value() != other.end_.has_value()) return false;
    if (!end_.has_value()) return true;

    return end_.value() == other.end_.value();
  }

  bool contains(const GtidRange &other) const {
    if (start_ > other.start_) return false;

    if (end_.has_value()) {
      if (end_.value() < other.start_) return false;

      if (!other.end_.has_value()) return true;

      return other.end_.value() <= end_.value();
    }

    if (other.end_.has_value() && other.end_.value() != start_) return false;

    return other.start_ == start_;
  }

  bool is_point() const { return !end_.has_value(); }

  bool parse(const std::string &value) {
    auto args = mysql_harness::split_string(value, '-', false);
    switch (args.size()) {
      case 2:
        end_ = helper::to_uint64(args[1]);
        [[fallthrough]];
      case 1:
        start_ = helper::to_uint64(args[0]);
        break;
      default:
        return false;
    }

    if (0 == start_) return false;
    if (end_.has_value() && 0 == end_.value()) return false;

    return true;
  }

  bool try_merge(const GtidRange &other) {
    // Not optimal implementation
    if (contains(other)) return true;
    if (other.contains(*this)) {
      *this = other;
      return true;
    }

    if (other.is_point()) {
      if (1 == inner::abs(start_, other.start_)) {
        start_ = std::min(start_, other.start_);
        end_ = inner::max<uint64_t>(start_, end_, other.start_);

        return true;
      } else if (end_.has_value() && end_.value() == (other.start_ - 1)) {
        end_ = other.start_;
        return true;
      }
      return false;
    }

    if (is_point()) {
      if (1 == inner::abs(start_, other.start_)) {
        start_ = std::min(start_, other.start_);
        end_ = inner::max<uint64_t>(start_, other.start_, other.end_);

        return true;
      } else if (other.end_.has_value() && start_ - 1 == other.end_) {
        start_ = other.start_;
        end_ = start_;
        return true;
      }
      return false;
    }

    // This and other have two points and
    // both ranges, doesn't contain each other
    if (is_between(other.start_)) {
      end_ = other.end_;
      return true;
    }

    if (other.is_between(start_)) {
      start_ = other.start_;
      return true;
    }

    if (1 == static_cast<int64_t>(other.start_ - end_.value())) {
      end_ = other.end_;
      return true;
    }

    if (1 == static_cast<int64_t>(start_ - other.end_.value())) {
      start_ = other.start_;
      return true;
    }

    return false;
  }

  std::string to_string() const {
    std::string result = ":" + std::to_string(start_);

    if (end_.has_value()) {
      result += "-" + std::to_string(end_.value());
    }

    return result;
  }
  uint64_t get_start() const { return start_; }
  const std::optional<uint64_t> &get_end() const { return end_; }

  GtidRange *begin() { return this; }
  GtidRange *end() { return this + 1; }
  const GtidRange *begin() const { return this; }
  const GtidRange *end() const { return this + 1; }
  uint64_t size() const { return 1; }

 private:
  bool is_between(const uint64_t value) const {
    if (start_ <= value) {
      if (end_.has_value() && end_.value() >= value) return true;
    }
    return false;
  }
  uint64_t start_;
  std::optional<uint64_t> end_;
};

class GtidSetOfRanges {
 public:
  GtidSetOfRanges() {}
  GtidSetOfRanges(const GtidSetOfRanges &other) : ranges_{other.ranges_} {}

  GtidSetOfRanges &operator=(const GtidRange &other) {
    ranges_.clear();
    ranges_.push_back(other);
    return *this;
  }

  GtidSetOfRanges &operator=(const GtidSetOfRanges &other) {
    ranges_ = other.ranges_;
    return *this;
  }

  bool operator==(const GtidSetOfRanges &other) const {
    if (other.ranges_.size() != ranges_.size()) return false;

    for (auto &r : ranges_) {
      if (!other.has(r)) return false;
    }

    return true;
  }

  bool has(const GtidRange &other) const {
    for (auto &r : ranges_) {
      if (r == other) return true;
    }
    return false;
  }

  bool contains(const GtidRange &other) const {
    for (auto &r : ranges_) {
      if (r.contains(other)) return true;
    }
    return false;
  }

  bool contains(const GtidSetOfRanges &other) const {
    for (auto &r : other.ranges_) {
      if (!contains(r)) return false;
    }
    return true;
  }

  bool parse(const std::vector<std::string> &values) {
    for (const auto &v : values) {
      auto range = ranges_.emplace(ranges_.end());
      if (!(*range).parse(v)) return false;
    }
    return true;
  }

  std::string to_string() const {
    std::string result;
    for (const auto &r : ranges_) {
      result += r.to_string();
    }
    return result;
  }

  void insert(const GtidRange &other) {
    auto it = ranges_.begin();

    do {
      if (other.get_start() < (*it).get_start()) {
        ranges_.insert(it, other);
        return;
      }
      ++it;
    } while (it != ranges_.end());

    ranges_.insert(it, other);
  }

  auto begin() { return ranges_.begin(); }
  auto end() { return ranges_.end(); }
  auto begin() const { return ranges_.begin(); }
  auto end() const { return ranges_.end(); }
  uint64_t size() const { return ranges_.size(); }

  std::list<GtidRange> ranges_;
};

template <typename Range>
class Gtid {
 public:
  template <typename... T>
  Gtid(const GTIDuuid &uid, T &&... v) : uid_{uid}, range_{v...} {}

  bool operator==(const Gtid &other) const {
    if (uid_ != other.uid_) return false;

    return range_ == other.range_;
  }

  template <typename OtherRange>
  bool contains(const Gtid<OtherRange> &other) const {
    if (uid_ != other.uid_) return false;
    for (const auto &r : range_) {
      bool all_matched = false;
      for (const auto &orange : other.range_) {
        if (!r.contains(orange)) break;
        all_matched = true;
      }
      if (all_matched) return true;
    }
    return false;
  }

  bool parse(const std::string &gtid) {
    auto result =
        helper::string::unhex<std::string, helper::string::get_unhex_character>(
            gtid);
    if (result.size() != GTIDuuid::k_size) return false;

    GTIDuuid::from_raw(&uid_, result.data());
    return true;
  }

  std::string to_string() const {
    auto result = uid_.to_string();
    result += range_.to_string();
    return result;
  }

  bool try_merge(const GtidRange &range) {
    for (auto &r : range_) {
      if (r.try_merge(range)) return true;
    }
    return false;
  }

  template <typename SomeRange>
  bool try_merge(const Gtid<SomeRange> &gtid) {
    if (uid_ != gtid.uid_) return false;

    return try_merge(gtid.range_);
  }

  template <typename SomeRange>
  bool insert(const Gtid<SomeRange> &other) {
    if (!(other.uid_ == uid_)) return false;

    range_.insert(other.range_);

    return true;
  }

  template <typename SomeRange>
  void set(const Gtid<SomeRange> &other) {
    uid_ = other.uid_;
    range_ = other.range_;
  }

  uint64_t size() const { return range_.size(); }

  const GTIDuuid &get_uid() const { return uid_; }
  const Range &get_range() const { return range_; }

 protected:
  template <typename SomeRange>
  friend class Gtid;
  GTIDuuid uid_;
  Range range_;
};

}  // namespace inner

using GTIDuuid = inner::GTIDuuid;

class Gtid : public inner::Gtid<inner::GtidRange> {
 public:
  using Parent = inner::Gtid<inner::GtidRange>;

  Gtid() : Parent(GTIDuuid{}) {}
  Gtid(const GTIDuuid &uid, const inner::GtidRange &r) : Parent(uid, r) {}
  Gtid(const std::string &v) : Parent(GTIDuuid{}) {
    if (!parse(v)) throw std::runtime_error("Invalid GTID");
  }

 public:
  bool parse(const std::string &v) {
    auto gtid_parts = mysql_harness::split_string(v, ':', false);
    if (gtid_parts.size() != 2) return false;
    if (!Parent::parse(gtid_parts[0])) return false;
    return range_.parse(gtid_parts[1]);
  }
};

class GtidSet : public inner::Gtid<inner::GtidSetOfRanges> {
 public:
  using Parent = Gtid<inner::GtidSetOfRanges>;

  GtidSet() : Gtid(GTIDuuid{}) {}

  GtidSet(const std::string &v) : Gtid(GTIDuuid{}) {
    if (!parse(v)) throw std::runtime_error("Invalid GTID-set");
  }

 public:
  bool parse(const std::string &v) {
    auto gtid_parts = mysql_harness::split_string(v, ':', false);
    if (gtid_parts.size() < 2) return false;
    if (!Parent::parse(gtid_parts[0])) return false;
    gtid_parts.erase(gtid_parts.begin());
    return range_.parse(gtid_parts);
  }
};

using Gtids = std::vector<Gtid>;
using GtidSets = std::vector<GtidSet>;

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_HELPER_GTID_H_
