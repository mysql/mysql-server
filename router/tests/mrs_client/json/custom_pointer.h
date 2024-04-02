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

#ifndef ROUTER_TESTS_MRS_CLIENT_JSON_CUSTOM_POINTER_H_
#define ROUTER_TESTS_MRS_CLIENT_JSON_CUSTOM_POINTER_H_

#include <string>
#include <vector>

#include "mysql/harness/string_utils.h"

namespace json {

class CustomPointer {
 public:
  class Entry {
   public:
    Entry(const std::string &name) : name_{name} {
      if (name_ == CustomPointer::path_accept_all()) accept_all_ = true;
    }

    bool matches(const std::string &element) {
      if (accept_all_) return true;
      return element == name_;
    }

    std::string get_name() { return name_; }

   private:
    std::string name_;
    bool accept_all_{false};
  };

  using Entries = std::vector<Entry>;
  using EntryIt = Entries::iterator;

 public:
  CustomPointer(const std::string &pointer) : name_{pointer} { parse(pointer); }

  static constexpr char k_path_serparator = '/';
  static const std::string &path_accept_all() {
    const static std::string k_path_accept_all = "*";
    return k_path_accept_all;
  }

  EntryIt begin() { return entries_.begin(); }
  EntryIt end() { return entries_.end(); }

  void mark() { mark_ = true; }
  bool is_marked() { return mark_; }
  std::string get_name() { return name_; }

 private:
  bool mark_{false};
  const std::string name_;
  Entries entries_;

  void parse(std::string pointer) {
    if (pointer.empty()) return;
    if (k_path_serparator == pointer[0]) pointer.erase(0, 1);
    auto elements =
        mysql_harness::split_string(pointer, k_path_serparator, true);

    for (const auto &e : elements) {
      entries_.emplace_back(e);
    }
  }
};

}  // namespace json

#endif  // ROUTER_TESTS_MRS_CLIENT_JSON_CUSTOM_POINTER_H_
