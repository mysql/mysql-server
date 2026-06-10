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

#include "sql/lib_glue/gtids.h"

#include "mysql/gtids/gtids.h"          // Gtid_set
#include "mysql/strconv/strconv.h"      // decode
#include "mysql/utils/return_status.h"  // Return_status
#include "mysqld_error.h"               // ER_MALFORMED_GTID_SET_SPECIFICATION
#include "sql/rpl_gtid.h"               // BINLOG_ERROR

mysql::utils::Return_status gtid_set_decode_text_report_errors(
    const std::string_view &input_text, mysql::gtids::Gtid_set &gtid_set) {
  using mysql::utils::Return_status;
  // ==== Parse ====
  auto ret = mysql::strconv::decode(mysql::strconv::Text_format{}, input_text,
                                    gtid_set);
  if (ret.is_ok()) return Return_status::ok;
  // ==== Report error ====
  // This buffer size is hard-coded in share/messages_to_clients.txt
  static constexpr std::size_t max_length = 200;
  char message[max_length + 1];
  std::size_t length = max_length;
  if (mysql::strconv::compute_encoded_length_text(ret) <= max_length) {
    // Generate a useful error message, if that fits in the buffer size.
    mysql::strconv::encode_text(
        mysql::strconv::out_str_fixed_z(message, length), ret);
  } else {
    // Otherwise, fall back to using a prefix of the input string.
    mysql::strconv::encode_text(
        mysql::strconv::out_str_fixed_z(message, length),
        input_text.substr(0, max_length));
  }
  BINLOG_ERROR(("Malformed GTID set specification: '%.200s'.", message),
               (ER_MALFORMED_GTID_SET_SPECIFICATION, MYF(0), message));
  return Return_status::error;
}
