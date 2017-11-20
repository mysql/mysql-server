/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "formatters/console.h"

#include <algorithm>
#include <iostream>
#include <set>

#include "formatters/message_formatter.h"


const Console::Color Console::k_red("\033[1;31m");
const Console::Color Console::k_clear("\033[0m");

Console::Console(const Options& options)
    : m_options(options),
      m_out(&std::cout),
      m_err(&std::cerr) {}

Console::Console(const Options& options, std::ostream* out, std::ostream* err)
    : m_options(options),
      m_out(out),
      m_err(err) {}


std::ostream &operator<<(std::ostream &os, const xcl::XError &err) {
  return os << err.what() << " (code " << err.error() << ")";
}

std::ostream &operator<<(std::ostream &os, const std::exception &exc) {
  return os << exc.what();
}

std::ostream& operator<<(std::ostream& os,
                         const xcl::XProtocol::Message& message) {
  return os << formatter::message_to_text(message);
}

std::ostream &operator<<(std::ostream &os, const std::set<int> &value) {
  std::copy(value.begin(),
            value.end(),
            std::ostream_iterator<int>(os, " "));
  return os;
}
