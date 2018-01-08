/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef X_TESTS_DRIVER_CONSOLE_H_
#define X_TESTS_DRIVER_CONSOLE_H_

#include <ostream>
#include <set>
#include <string>
#include <utility>

#include "plugin/x/client/mysqlxclient/xerror.h"
#include "plugin/x/client/mysqlxclient/xprotocol.h"


std::ostream &operator<<(std::ostream &os, const xcl::XError &err);
std::ostream &operator<<(std::ostream &os, const std::exception &exc);
std::ostream &operator<<(std::ostream &os, const std::set<int> &value);
std::ostream &operator<<(std::ostream &os,
                         const xcl::XProtocol::Message &message);

class Console {
 public:
  struct Options {
    bool m_use_color{false};
    bool m_be_verbose{false};
  };

 public:
  explicit Console(const Options &options);
  Console(const Options &options,
          std::ostream *out,
          std::ostream *err);

  template <typename T>
  void print(const T &obj) const {
    (*m_out) << obj;
  }

  template <typename T, typename... R>
  void print(const T &first, R &&... rest) const {
    print(first);
    print(std::forward<R>(rest)...);
  }

  template <typename... T>
  void print_verbose(T &&... args) const {
    if (m_options.m_be_verbose) print(std::forward<T>(args)...);
  }

  template <typename T>
  void print_error(const T &obj) const {
    (*m_err) << obj;
  }

  template <typename T, typename... R>
  void print_error(const T &first, R &&... rest) const {
    print_error(first);
    print_error(std::forward<R>(rest)...);
  }

  template <typename... T>
  void print_error_red(T &&... values) const {
#ifndef _WIN32
    if (m_options.m_use_color)
      print_error(k_red, values..., k_clear);
    else
#endif
      print_error(values...);
  }

 private:
  class Color {
   public:
    explicit Color(const char *const v) : m_value(v) {}

   private:
    const char *const m_value;

    friend std::ostream &operator<<(std::ostream &os, const Color &c) {
      return os << c.m_value;
    }
  };

  static const Color k_red;
  static const Color k_clear;
  const Options m_options;
  std::ostream *m_out;
  std::ostream *m_err;
};

#endif  // X_TESTS_DRIVER_CONSOLE_H_
