/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_HARNESS_INCLUDE_MYSQL_HARNESS_REGEX_MATCHER_H_
#define ROUTER_SRC_HARNESS_INCLUDE_MYSQL_HARNESS_REGEX_MATCHER_H_

#include "matcher_interface.h"

#include <memory>

#include "harness_export.h"

namespace mysql_harness {

class HARNESS_EXPORT RegexMatcher : public MatcherInterface {
 public:
  RegexMatcher(const std::string &pattern);

  stdx::expected<void, std::string> is_valid() const override;
  bool matches(const std::string &input) const override;
  bool find(const std::string &input) const override;
  std::string replace_all(std::string input,
                          const std::string &replacement) const override;

 private:
  class Impl;
  std::shared_ptr<Impl> impl_;
};

}  // namespace mysql_harness

#endif /* ROUTER_SRC_HARNESS_INCLUDE_MYSQL_HARNESS_REGEX_MATCHER_H_ */
