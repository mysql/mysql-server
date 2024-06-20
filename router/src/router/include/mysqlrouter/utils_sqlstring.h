/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _UTILS_SQLSTRING_H_
#define _UTILS_SQLSTRING_H_

#include "mysqlrouter/router_mysql_export.h"

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>
#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include <stdexcept>

namespace mysqlrouter {

enum SqlStringFlags {
  QuoteOnlyIfNeeded = 1 << 0,
  UseAnsiQuotes = 1 << 1,

  EndOfInput = 1 << 7
};

std::string ROUTER_MYSQL_EXPORT escape_sql_string(const char *s,
                                                  bool wildcards = false);
std::string ROUTER_MYSQL_EXPORT escape_sql_string(const char *s, int len,
                                                  bool wildcards = false);
std::string ROUTER_MYSQL_EXPORT
escape_sql_string(const std::string &string,
                  bool wildcards = false);  // "strings" or 'strings'
std::string ROUTER_MYSQL_EXPORT escape_backticks(const char *s,
                                                 int length);  // `identifier`
std::string ROUTER_MYSQL_EXPORT
escape_backticks(const std::string &string);  // `identifier`
std::string ROUTER_MYSQL_EXPORT quote_identifier(const std::string &identifier,
                                                 const char quote_char);
std::string ROUTER_MYSQL_EXPORT
quote_identifier_if_needed(const std::string &ident, const char quote_char);

class ROUTER_MYSQL_EXPORT sqlstring {
 public:
  struct sqlstringformat {
    int _flags;
    sqlstringformat(const int flags) : _flags(flags) {}
  };

  /**
   * Iterator wrapper, for serializing arrays of structures to string.
   *
   * This class forwards some operators that are defined in `Iterator` class,
   *  still the class that derives from `CustomContainerIterator`, must define
   * `operator*` to change the structure to string or other simple type.
   */
  template <typename Iterator, typename Derived>
  class CustomContainerIterator {
   public:
    CustomContainerIterator(const Iterator &it) : it_{it} {}
    CustomContainerIterator(const CustomContainerIterator &other)
        : it_{other.it_} {}
    CustomContainerIterator(Iterator &&it) : it_{it} {}
    virtual ~CustomContainerIterator() = default;

    CustomContainerIterator &operator=(CustomContainerIterator &&other) {
      it_ = std::move(other.it_);
      return *this;
    }

    CustomContainerIterator &operator=(const CustomContainerIterator &other) {
      it_ = other.it_;
      return *this;
    }

    Derived &operator++() {
      ++it_;
      return *dynamic_cast<Derived *>(this);
    }

    bool operator!=(const CustomContainerIterator &other) const {
      return it_ != other.it_;
    }

    static std::pair<Derived, Derived> from_iterators(Iterator begin,
                                                      Iterator end);

    template <typename Container>
    static std::pair<Derived, Derived> from_container(Container &c);

   protected:
    Iterator it_;
  };

 private:
  std::string _formatted;
  std::string _format_string_left;
  sqlstringformat _format;
  int _locked_escape{0};

  std::string consume_until_next_escape();
  int next_escape();
  void lock_escape(int esc);
  void unlock_escape();

  sqlstring &append(const std::string &s);

  sqlstring &format(int esc, const char *v, int length) {
    if (esc == '!') {
      std::string escaped = escape_backticks(v, length);
      if ((_format._flags & QuoteOnlyIfNeeded) != 0)
        append(quote_identifier_if_needed(escaped, '`'));
      else
        append(quote_identifier(escaped, '`'));
    } else if (esc == '?') {
      if (v) {
        if (_format._flags & UseAnsiQuotes)
          append("\"").append(escape_sql_string(v, length)).append("\"");
        else
          append("'").append(escape_sql_string(v, length)).append("'");
      } else {
        append("NULL");
      }
    } else  // shouldn't happen
      throw std::invalid_argument(
          "Error formatting SQL query: internal error, expected ? or ! escape "
          "got something else");

    return *this;
  }

 public:
  static const sqlstring empty;
  static const sqlstring null;
  static const sqlstring end;

  sqlstring();
  sqlstring(const char *format_string, const sqlstringformat format = 0);
  sqlstring(const sqlstring &copy);
  sqlstring &operator=(const sqlstring &) = default;
  bool done() const;

  void reset(const char *format_string, const sqlstringformat format = 0);

  operator std::string() const;
  std::string str() const;
  bool is_empty() const;

  inline bool operator==(const sqlstring &other) const {
    return str() == other.str();
  }

  //! modifies formatting options
  sqlstring &operator<<(const sqlstringformat);
  //! replaces a ? in the format string with a float numeric value
  sqlstring &operator<<(const float val) { return operator<<((double)val); }
  //! replaces a ? in the format string with a double numeric value
  sqlstring &operator<<(const double);
  //! replaces a ? in the format string with a NULL value.
  sqlstring &operator<<(const std::nullptr_t);
  //! replaces a ? in the format string with a quoted string value or ! with a
  //! back-quoted identifier value
  sqlstring &operator<<(const std::string &);
  //! replaces a ? in the format string with a quoted string value or ! with a
  //! back-quoted identifier value is the value is NULL, ? will be replaced with
  //! a NULL. ! will raise an exception
  sqlstring &operator<<(const char *);
  //! replaces a ? or ! with the content of the other string verbatim
  sqlstring &operator<<(const sqlstring &);
  //! replaces a ? with an array of bytes
  sqlstring &operator<<(const std::vector<uint8_t> &v);

  //! appends a pre-formatted sqlstring to a pre-formatted sqlstring
  sqlstring &append_preformatted(const sqlstring &s);

  sqlstring &append_preformatted_sep(const std::string &separator,
                                     const sqlstring &s);

  /**
   * Replace `?` or `!` with multiple values.
   *
   * Each element of the container, is going to be applied to parameter type
   * fetched at the start (either `?` or `!`). Each iteam is going to be
   * separated by comma.
   *
   * Example 1:
   *
   *      sqlstring s{"First=(?) Second=(!)"};
   *      s << std::vector<std::string>{"1","2","3"} << "a";
   *
   *  The resulting query: First=("1","2","3") Second=(A)
   *
   * Example 2:
   *
   *      sqlstring s{"First=(!) Second=(?)"};
   *      s << std::vector<std::string>{"1","2","3"} << "a";
   *
   *  The resulting query: First=(1,2,3) Second=("A")
   *
   */
  template <typename T>
  sqlstring &operator<<(const std::pair<T, T> &iterators) {
    auto esc = next_escape();
    lock_escape(esc);
    bool first = true;

    T it = iterators.first;
    T end = iterators.second;
    for (; it != end; ++it) {
      if (!first) {
        append(",");
      }
      *this << *it;

      first = false;
    }
    unlock_escape();
    append(consume_until_next_escape());

    return *this;
  }

  template <typename T>
  sqlstring &operator<<(const std::vector<T> &values) {
    using const_iterator = typename std::vector<T>::const_iterator;
    *this << std::make_pair<const_iterator, const_iterator>(values.begin(),
                                                            values.end());

    return *this;
  }

  template <typename T>
  sqlstring &operator<<(const std::set<T> &values) {
    using const_iterator = typename std::set<T>::const_iterator;
    *this << std::make_pair<const_iterator, const_iterator>(values.begin(),
                                                            values.end());

    return *this;
  }

  //! replaces a ? in the format string with any integer numeric value
  template <typename T>
  sqlstring &operator<<(const T value) {
    /**
     * Import `to_string` function which enables serialization to string, for
     * standard types.
     *
     * User can define `to_string` function for serialization of custom type.
     * Both the function and the type must be located in the same `namespace`.
     * Following this schema enables `argument-dependent-lookup/ADL` in stream
     * operator of `sqlstring` (which uses the `to_string`).
     */
    using std::to_string;
    int esc = next_escape();
    if (esc != '?')
      throw std::invalid_argument(
          "Error formatting SQL query: invalid escape for numeric argument");
    // Uses ADL here
    append(to_string(value));
    append(consume_until_next_escape());
    return *this;
  }
};

template <typename Iterator, typename Derived>
template <typename Container>
std::pair<Derived, Derived>
sqlstring::CustomContainerIterator<Iterator, Derived>::from_container(
    Container &c) {
  return std::make_pair<Derived, Derived>(c.begin(), c.end());
}

template <typename Iterator, typename Derived>
std::pair<Derived, Derived>
sqlstring::CustomContainerIterator<Iterator, Derived>::from_iterators(
    Iterator begin, Iterator end) {
  return std::make_pair<Derived, Derived>(begin, end);
}

}  // namespace mysqlrouter

#endif
