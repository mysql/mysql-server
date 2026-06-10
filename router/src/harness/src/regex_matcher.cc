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

#include "mysql/harness/regex_matcher.h"

#include <unicode/regex.h>
#include <unicode/unistr.h>
#include <unicode/utypes.h>

namespace mysql_harness {

MatcherInterface::~MatcherInterface() = default;

class RegexMatcher::Impl {
 public:
  Impl(const std::string &pattern) {
    status_ = U_ZERO_ERROR;
    pattern_ = pattern;

    std::unique_ptr<icu::RegexPattern> regex_pattern(icu::RegexPattern::compile(
        icu::UnicodeString::fromUTF8(pattern_.c_str()),
        URegexpFlag::UREGEX_CASE_INSENSITIVE, status_));

    if (U_FAILURE(status_)) {
      return;
    }

    regex_pattern_ = std::move(regex_pattern);
  }

  bool matches(const icu::UnicodeString &input) const {
    if (U_FAILURE(status_)) {
      return false;
    }

    UErrorCode out_status = U_ZERO_ERROR;
    std::unique_ptr<icu::RegexMatcher> regex_matcher(
        regex_pattern_->matcher(input, out_status));

    return regex_matcher->matches(out_status) != 0;
  }

  bool find(const icu::UnicodeString &input) const {
    if (U_FAILURE(status_)) {
      return false;
    }

    UErrorCode out_status = U_ZERO_ERROR;
    std::unique_ptr<icu::RegexMatcher> regex_matcher(
        regex_pattern_->matcher(input, out_status));

    return regex_matcher->find(out_status) != 0;
  }

  std::string replace_all(icu::UnicodeString &input,
                          const icu::UnicodeString &replacement) const {
    if (!U_FAILURE(status_)) {
      UErrorCode out_status = U_ZERO_ERROR;
      std::unique_ptr<icu::RegexMatcher> regex_matcher(
          regex_pattern_->matcher(input, out_status));

      input = regex_matcher->replaceAll(replacement, out_status);
    }

    std::string result;
    input.toUTF8String(result);
    return result;
  }

  UErrorCode status_;
  std::string pattern_;
  std::unique_ptr<icu::RegexPattern> regex_pattern_;
};

RegexMatcher::RegexMatcher(const std::string &pattern) {
  impl_ = std::make_unique<Impl>(pattern);
}

stdx::expected<void, std::string> RegexMatcher::is_valid() const {
  if (impl_ && U_FAILURE(impl_->status_)) {
    return stdx::unexpected(u_errorName(impl_->status_));
  }

  return {};
}

bool RegexMatcher::find(const std::string &input) const {
  if (!impl_) return false;

  const icu::UnicodeString us_input(input.data(), input.size());

  return impl_->find(us_input);
}

bool RegexMatcher::matches(const std::string &input) const {
  if (!impl_) return false;

  const icu::UnicodeString us_input(input.data(), input.size());

  return impl_->matches(us_input);
}

std::string RegexMatcher::replace_all(std::string input,
                                      const std::string &replacement) const {
  if (!impl_) return input;

  icu::UnicodeString us_input(input.data(), input.size());
  const icu::UnicodeString us_replacement(replacement.data(),
                                          replacement.size());

  if (impl_->find(us_input)) {
    return impl_->replace_all(us_input, us_replacement);
  }

  return input;
}

}  // namespace mysql_harness
