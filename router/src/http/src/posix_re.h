/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQLROUTER_POSIX_RE_INCLUDED
#define MYSQLROUTER_POSIX_RE_INCLUDED

// Posix (Extended) Regular Expression
//
// C++11 has std::regex, by gcc-4.x throws exceptions when it
// it used. Instead we build on a subset of std::regex
//
//

#if ((defined(__GNUC__) && __GNUC__ < 5) && !defined(__clang__))
// detect broken gcc 4.x, but ignore clang which announces itself as GCC 4.2.1
#if !defined(_WIN32)
#define USE_POSIX_RE_IMPL
#else
#error "GCC 4.x or older, on Windows isn't supported"
#endif
#endif

#ifdef USE_POSIX_RE_IMPL
#include <regex.h>
#else
#include <regex>
#endif

#include <memory>

class PosixRE_constants {
 public:
#ifdef USE_POSIX_RE_IMPL
  using syntax_option_type = int;
  using match_flag_type = int;
  using error_type = int;
  static constexpr auto match_default = 0;
  static constexpr auto basic = 0;
  static constexpr auto extended = REG_EXTENDED;
  static constexpr auto icase = REG_ICASE;
  static constexpr auto nosubs = REG_NOSUB;

  static constexpr auto match_not_bol = REG_NOTBOL;
  static constexpr auto match_not_eol = REG_NOTEOL;
#else
  using syntax_option_type = std::regex_constants::syntax_option_type;
  using error_type = std::regex_constants::error_type;
  using match_flag_type = std::regex_constants::match_flag_type;
  static constexpr auto match_default = std::regex_constants::match_default;
  static constexpr auto basic = std::regex_constants::basic;
  static constexpr auto extended = std::regex_constants::extended;
  static constexpr auto icase = std::regex_constants::icase;
  static constexpr auto nosubs = std::regex_constants::nosubs;

  static constexpr auto match_not_bol = std::regex_constants::match_not_bol;
  static constexpr auto match_not_eol = std::regex_constants::match_not_eol;
#endif
};

class PosixREError : public std::runtime_error {
  PosixRE_constants::error_type code_;

 public:
  PosixREError(PosixRE_constants::error_type c, const char *w)
      : std::runtime_error{w}, code_{c} {}

  PosixRE_constants::error_type code() const noexcept { return code_; }
};

class PosixRE {
 public:
  using flag_type = PosixRE_constants::syntax_option_type;
  using match_flag_type = PosixRE_constants::match_flag_type;
  static constexpr auto match_default = PosixRE_constants::match_default;

  static constexpr auto basic = PosixRE_constants::basic;
  static constexpr auto extended = PosixRE_constants::extended;
  static constexpr auto icase = PosixRE_constants::icase;
  static constexpr auto nosubs = PosixRE_constants::nosubs;

  static constexpr auto match_not_eol = PosixRE_constants::match_not_eol;
  static constexpr auto match_not_bol = PosixRE_constants::match_not_bol;

  PosixRE(const std::string &regex_str, flag_type syntax_options = extended)
      :
#ifdef USE_POSIX_RE_IMPL
        reg_ {
    new regex_t
  }
#else
        reg_ {
    regex_str, syntax_options
  }
#endif
  {
#ifdef USE_POSIX_RE_IMPL
    int error_code;
    if (0 !=
        (error_code = regcomp(reg_.get(), regex_str.c_str(), syntax_options))) {
      char errbuf[256];
      regerror(error_code, reg_.get(), errbuf, sizeof(errbuf));

      throw PosixREError(error_code, errbuf);
    }
#endif
  }

  PosixRE(const char *regex_str, flag_type syntax_options = extended)
      :
#ifdef USE_POSIX_RE_IMPL
        reg_ {
    new regex_t
  }
#else
        reg_ {
    regex_str, syntax_options
  }
#endif
  {
#ifdef USE_POSIX_RE_IMPL
    int error_code;
    if (0 != (error_code = regcomp(reg_.get(), regex_str, syntax_options))) {
      char errbuf[256];
      regerror(error_code, reg_.get(), errbuf, sizeof(errbuf));

      throw PosixREError(error_code, errbuf);
    }
#endif
  }

  // no copy, move only
  PosixRE(const PosixRE &) = default;
  PosixRE &operator=(const PosixRE &) = default;

  PosixRE(PosixRE &&) = default;
  PosixRE &operator=(PosixRE &&) = default;

  ~PosixRE() {
#ifdef USE_POSIX_RE_IMPL
    auto r = reg_.get();
    if (r) regfree(r);
#endif
  }

  /**
   * search entire line for match.
   */
  bool search(const std::string &line,
              match_flag_type match_flags = match_default) const {
#ifdef USE_POSIX_RE_IMPL
    if (0 != regexec(reg_.get(), line.c_str(), 0, NULL, match_flags)) {
      return false;
    }

    return true;
#else
    return std::regex_search(line, reg_, match_flags);
#endif
  }

 private:
#ifdef USE_POSIX_RE_IMPL
  std::unique_ptr<regex_t> reg_;
#else
  std::regex reg_;
#endif
};

#endif
