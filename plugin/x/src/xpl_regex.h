/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PLUGIN_X_SRC_XPL_REGEX_H_
#define PLUGIN_X_SRC_XPL_REGEX_H_

#include <memory>

#include "unicode/regex.h"

namespace xpl {

class Regex {
 public:
  explicit Regex(const char *const pattern);
  bool match(const char *value) const;

 private:
  UErrorCode m_status;
  UParseError m_parse_error;
  /* RegexPatter doesn't have public constructor
   * that allow to initialize the object directly
   * by the text patter. Only static method which
   * returns an object pointer allows initialization
   * text patter.
   *
   * We require here to have an smart-ptr/field
   * instead a object/field.
   */
  std::unique_ptr<icu::RegexPattern> m_pattern;
};
}  // namespace xpl

#endif  // PLUGIN_X_SRC_XPL_REGEX_H_
