// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_STRCONV_CONV_TEXT_SOURCE_LOCATION_H
#define MYSQL_STRCONV_CONV_TEXT_SOURCE_LOCATION_H

/// @file
/// Experimental API header

#include <source_location>                       // source_location
#include "mysql/strconv/encode/string_target.h"  // Is_string_target
#include "mysql/strconv/formats/text_format.h"   // Text_format

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {
/// Enable conversion from std::source_location to string
void encode_impl(const Text_format &format, Is_string_target auto &target,
                 const std::source_location &source_location) {
  target.concat(format, source_location.file_name(), ":",
                source_location.line(), ":", source_location.function_name());
}
}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_CONV_TEXT_SOURCE_LOCATION_H
