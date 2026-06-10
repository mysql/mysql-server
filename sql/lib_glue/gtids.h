/* Copyright (c) 2011, 2026, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <string_view>                  // string_view
#include "mysql/utils/return_status.h"  // Return_status

namespace mysql::gtids {
class Gtid_set;
}  // namespace mysql::gtids

/// Parse the GTID set from a specification in text format, reporting errors on
/// failure.
///
/// @param input_text The text to parse.
///
/// @param gtid_set The GTID set to parse into.
///
/// @retval ok Success.
///
/// @retval error Error. The error has been reported using BINLOG_ERROR.
mysql::utils::Return_status gtid_set_decode_text_report_errors(
    const std::string_view &input_text, mysql::gtids::Gtid_set &gtid_set);
