/*
  Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#include "designator.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#define DO_DEBUG 0

#if DO_DEBUG
void Designator::trace(const std::string &where) const {
  fprintf(stderr, "[%20s]: %s\n", where.c_str(),
          std::string(cur_, input_.end()).c_str());
}
#endif

inline std::string::value_type Designator::peek() const {
  if (cur_ == input_.end()) return '\0';  // Return NUL character if end.
  return *cur_;
}

inline std::string::value_type Designator::next() {
  if (cur_ == input_.end()) parse_error("Unexpected end of input");
  return *++cur_;
}

[[noreturn]] void Designator::parse_error(const std::string &prefix) const {
  std::string message(prefix + " at '" + std::string(cur_, input_.end()) + "'");
  throw std::runtime_error(message);
}

void Designator::skip_space() {
#if DO_DEBUG
  trace(__func__);
#endif
  while (isspace(peek())) ++cur_;
}

long Designator::parse_number() {
#if DO_DEBUG
  trace(__func__);
#endif
  skip_space();
  std::string::const_iterator start = cur_;
  while (::isdigit(peek())) ++cur_;
  if (std::distance(start, cur_) == 0) parse_error("Expected number");
  return strtol(std::string(start, cur_).c_str(), nullptr, 10);
}

void Designator::parse_plugin() {
#if DO_DEBUG
  trace(__func__);
#endif
  skip_space();
  std::string::const_iterator start = cur_;
  if (!::isalpha(peek()) && peek() != '_')
    parse_error("Invalid start of module name");
  while (::isalnum(peek()) || peek() == '_') ++cur_;
  plugin.assign(start, cur_);
}

void Designator::parse_version_list() {
#if DO_DEBUG
  trace(__func__);
#endif
  while (true) {
    skip_space();
    Relation rel = parse_relation();
    Version ver = parse_version();
    constraint.push_back(std::make_pair(rel, ver));
#if DO_DEBUG
    trace(__func__);
#endif
    skip_space();
    if (peek() != ',') break;
    ++cur_;
  }
}

Designator::Relation Designator::parse_relation() {
#if DO_DEBUG
  trace(__func__);
#endif
  switch (peek()) {
    case '<':
      switch (next()) {
        case '=':
          ++cur_;
          return LESS_EQUAL;

        case '<':
          ++cur_;
          return LESS_THEN;
      }
      --cur_;
      break;

    case '>':
      switch (next()) {
        case '=':
          ++cur_;
          return GREATER_EQUAL;

        case '>':
          ++cur_;
          return GREATER_THEN;
      }
      --cur_;
      break;

    case '!':
      switch (next()) {
        case '=':
          ++cur_;
          return NOT_EQUAL;
      }
      --cur_;
      break;

    case '=':
      switch (next()) {
        case '=':
          ++cur_;
          return EQUAL;
      }
      --cur_;
      break;
  }
  parse_error("Expected operator");
  return EQUAL;  // Needs returning something
}

Version Designator::parse_version() {
#if DO_DEBUG
  trace(__func__);
#endif
  Version version;

  version.ver_major = parse_number();
  if (peek() != '.') return version;
  ++cur_;
  version.ver_minor = parse_number();

  if (peek() != '.') return version;
  ++cur_;
  version.ver_patch = parse_number();

  return version;
}

void Designator::parse_root() {
#if DO_DEBUG
  trace(__func__);
#endif
  parse_plugin();
#if DO_DEBUG
  trace(__func__);
#endif
  skip_space();
  switch (peek()) {
    case '(':
      ++cur_;
      parse_version_list();
      skip_space();
      if (peek() != ')') parse_error("Expected end of version list");
      ++cur_;
      break;

    case 0:
      break;

    default:
      parse_error("Expected start of version list");
  }
}

bool Designator::version_good(const Version &version) const {
  for (auto &check : constraint) {
    switch (check.first) {
      case LESS_THEN:
        if (!(version < check.second)) return false;
        break;

      case LESS_EQUAL:
        if (!(version <= check.second)) return false;
        break;

      case GREATER_THEN:
        if (!(version > check.second)) return false;
        break;

      case GREATER_EQUAL:
        if (!(version >= check.second)) return false;
        break;

      case EQUAL:
        if (!(version == check.second)) return false;
        break;

      case NOT_EQUAL:
        if (!(version != check.second)) return false;
        break;

      default:  // Should not be reached
        throw std::runtime_error("Bad relation operator for constraint");
    }
  }
  return true;
}

Designator::Designator(const std::string &str)
    : input_(str), cur_(input_.begin()) {
  parse_root();
  skip_space();  // Trailing space allowed
  if (cur_ != input_.end()) {
    std::string trailing(cur_, input_.end());
    throw std::runtime_error("Trailing input: '" + trailing + "'");
  }
}
