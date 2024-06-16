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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/** @file include/ut0log.h Base of InnoDB utilities. */

#ifndef ut0core_h
#define ut0core_h

#include <string.h>
#include <sstream>

#include "ut0dbg.h"

namespace ut {
struct Location {
  const char *filename;
  size_t line;
  std::string str() const;
  std::string basename() const;
  std::string to_json() const;
  std::ostream &print(std::ostream &out) const {
    out << "[Location: file=" << filename << ", line=" << line << "]";
    return out;
  }
};

inline std::string Location::basename() const {
  const std::string path(filename);
  auto pos = path.find_last_of('/');
  return path.substr(pos);
}

inline std::string Location::to_json() const {
  std::ostringstream sout;
  sout << "{type: Location, basename: " << basename() << ", line: " << line
       << "}";
  return sout.str();
}

inline std::string Location::str() const {
  std::ostringstream sout;
  (void)print(sout);
  return sout.str();
}

}  // namespace ut

inline std::ostream &operator<<(std::ostream &out, const ut::Location &obj) {
  return obj.print(out);
}

#define UT_LOCATION_HERE (ut::Location{__FILE__, __LINE__})

namespace ib {

#ifdef UNIV_DEBUG
/** Finds the first format specifier in `fmt` format string
@param[in]   fmt  The format string
@return Either the longest suffix of `fmt` which starts with format specifier,
or `nullptr` if could not find any format specifier inside `fmt`.
*/
static inline const char *get_first_format(const char *fmt) {
  const char *pos = strchr(fmt, '%');
  if (pos != nullptr && pos[1] == '%') {
    return (get_first_format(pos + 2));
  }
  return (pos);
}

/** Verifies that the `fmt` format string does not require any arguments
@param[in]   fmt  The format string
@return true if and only if there is no format specifier inside `fmt` which
requires passing an argument */
static inline bool verify_fmt_match(const char *fmt) {
  return (get_first_format(fmt) == nullptr);
}

/** Verifies that the `fmt` format string contains format specifiers which match
the type and order of the arguments
@param[in]  fmt   The format string
@param[in]  head  The first argument
@param[in]  tail  Others (perhaps none) arguments
@return true if and only if the format specifiers found in `fmt` correspond to
types of head, tail...
*/
template <typename Head, typename... Tail>
static bool verify_fmt_match(const char *fmt, Head &&head [[maybe_unused]],
                             Tail &&...tail) {
  using H =
      typename std::remove_cv<typename std::remove_reference<Head>::type>::type;
  const char *pos = get_first_format(fmt);
  if (pos == nullptr) {
    return (false);
  }
  /* We currently only handle :
  %[-0-9.*]*(d|ld|lld|u|lu|llu|zu|zx|zd|s|x|i|f|c|X|p|lx|llx|lf)
  Feel free to extend the parser, if you need something more, as the parser is
  not intended to be any stricter than real printf-format parser, and if it does
  not handle something, it is only to keep the code simpler. */
  const std::string skipable("-+ #0123456789.*");

  pos++;
  while (*pos != '\0' && skipable.find_first_of(*pos) != std::string::npos) {
    pos++;
  }
  if (*pos == '\0') {
    return (false);
  }
  bool is_ok = true;
  if (pos[0] == 'l') {
    if (pos[1] == 'l') {
      if (pos[2] == 'd') {
        is_ok = std::is_same<H, long long int>::value;
      } else if (pos[2] == 'u' || pos[2] == 'x') {
        is_ok = std::is_same<H, unsigned long long int>::value;
      } else {
        is_ok = false;
      }
    } else if (pos[1] == 'd') {
      is_ok = std::is_same<H, long int>::value;
    } else if (pos[1] == 'u') {
      is_ok = std::is_same<H, unsigned long int>::value;
    } else if (pos[1] == 'x') {
      is_ok = std::is_same<H, unsigned long int>::value;
    } else if (pos[1] == 'f') {
      is_ok = std::is_same<H, double>::value;
    } else {
      is_ok = false;
    }
  } else if (pos[0] == 'd') {
    is_ok = std::is_same<H, int>::value;
  } else if (pos[0] == 'u') {
    is_ok = std::is_same<H, unsigned int>::value;
  } else if (pos[0] == 'x') {
    is_ok = std::is_same<H, unsigned int>::value;
  } else if (pos[0] == 'X') {
    is_ok = std::is_same<H, unsigned int>::value;
  } else if (pos[0] == 'i') {
    is_ok = std::is_same<H, int>::value;
  } else if (pos[0] == 'f') {
    is_ok = std::is_same<H, float>::value;
  } else if (pos[0] == 'c') {
    is_ok = std::is_same<H, char>::value;
  } else if (pos[0] == 'p') {
    is_ok = std::is_pointer<H>::value;
  } else if (pos[0] == 's') {
    is_ok = (std::is_same<H, char *>::value ||
             std::is_same<H, char const *>::value ||
             (std::is_array<H>::value &&
              std::is_same<typename std::remove_cv<
                               typename std::remove_extent<H>::type>::type,
                           char>::value));
  } else if (pos[0] == 'z') {
    if (pos[1] == 'u') {
      is_ok = std::is_same<H, size_t>::value;
    } else if (pos[1] == 'x') {
      is_ok = std::is_same<H, size_t>::value;
    } else if (pos[1] == 'd') {
      is_ok = std::is_same<H, ssize_t>::value;
    } else {
      is_ok = false;
    }
  } else {
    is_ok = false;
  }
  return (is_ok && verify_fmt_match(pos + 1, std::forward<Tail>(tail)...));
}
#endif /* UNIV_DEBUG */

/** This is a wrapper class, used to print any unsigned integer type
in hexadecimal format.  The main purpose of this data type is to
overload the global operator<<, so that we can print the given
wrapper value in hex. */
struct hex {
  explicit hex(uintmax_t t) : m_val(t) {}
  const uintmax_t m_val;
};

/** This is an overload of the global operator<< for the user defined type
ib::hex.  The unsigned value held in the ib::hex wrapper class will be printed
into the given output stream in hexadecimal format.
@param[in,out]  lhs     the output stream into which rhs is written.
@param[in]      rhs     the object to be written into lhs.
@retval reference to the output stream. */
inline std::ostream &operator<<(std::ostream &lhs, const hex &rhs) {
  std::ios_base::fmtflags ff = lhs.flags();
  lhs << std::showbase << std::hex << rhs.m_val;
  lhs.setf(ff);
  return (lhs);
}

}  // namespace ib

#endif /* ut0core_h */
