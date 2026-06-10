// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_STRCONV_CONV_DEBUG_PARSE_STATUS_H
#define MYSQL_STRCONV_CONV_DEBUG_PARSE_STATUS_H

/// @file
/// Experimental API header

#include "mysql/strconv/decode/parse_status.h"   // Parse_status
#include "mysql/strconv/encode/string_target.h"  // Is_string_target
#include "mysql/strconv/formats/debug_format.h"  // Debug_format

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

/// Enable encode(Debug_format, Parse_status), to help debugging.
void encode_impl(const Debug_format &format, Is_string_target auto &target,
                 const detail::Parse_status &status) {
  switch (status) {
    case detail::Parse_status::ok:
      target.write(format, "ok");
      break;
    case detail::Parse_status::fullmatch_error:
      target.write(format, "fullmatch_error");
      break;
    case detail::Parse_status::parse_error:
      target.write(format, "parse_error");
      break;
    case detail::Parse_status::store_error:
      target.write(format, "store_error");
      break;
    default:
      target.write(format, "unknown");
      break;
  }
}

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_CONV_DEBUG_PARSE_STATUS_H
