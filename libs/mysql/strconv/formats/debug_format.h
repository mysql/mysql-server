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

#ifndef MYSQL_STRCONV_FORMATS_DEBUG_FORMAT_H
#define MYSQL_STRCONV_FORMATS_DEBUG_FORMAT_H

/// @file
/// Experimental API header

#include "mysql/strconv/formats/format.h"       // Format_base
#include "mysql/strconv/formats/text_format.h"  // Text_format

/// @addtogroup GroupLibsMysqlStrconv
/// @{

namespace mysql::strconv {

/// Format tag to identify debug format.
///
/// A request to format in Debug_format will fall back to Text_format in case no
/// `Debug_format` implementation exists for the object type.
///
/// This is intended only for formatting, not for parsing.
struct Debug_format : public Format_base {
  [[nodiscard]] auto parent() const { return Text_format{}; }
};

}  // namespace mysql::strconv

// addtogroup GroupLibsMysqlStrconv
/// @}

#endif  // ifndef MYSQL_STRCONV_FORMATS_DEBUG_FORMAT_H
