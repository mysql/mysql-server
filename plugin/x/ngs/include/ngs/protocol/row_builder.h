/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _NGS_ROW_BUILDER_H_
#define _NGS_ROW_BUILDER_H_

#include <sys/types.h>
#include <set>
#include <string>

#include "decimal.h"
#include "m_ctype.h"
#include "my_inttypes.h"
#include "mysql_com.h"
#include "mysql_time.h"
#include "plugin/x/ngs/include/ngs/protocol/message_builder.h"
#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"

namespace ngs {
class Output_buffer;

class Row_builder : public Message_builder {
  typedef ::google::protobuf::io::CodedOutputStream CodedOutputStream;

 public:
  Row_builder();
  ~Row_builder();

  void start_row(Output_buffer *out_buffer);
  void abort_row();
  void end_row();

  void add_null_field();
  void add_longlong_field(longlong value, bool unsigned_flag);
  void add_decimal_field(const decimal_t *value);
  void add_decimal_field(const char *const value, size_t length);
  void add_double_field(double value);
  void add_float_field(float value);
  void add_date_field(const MYSQL_TIME *value);
  void add_time_field(const MYSQL_TIME *value, uint decimals);
  void add_datetime_field(const MYSQL_TIME *value, uint decimals);
  void add_string_field(const char *const value, size_t length,
                        const CHARSET_INFO *const valuecs);
  void add_set_field(const char *const value, size_t length,
                     const CHARSET_INFO *const valuecs);
  void add_bit_field(const char *const value, size_t length,
                     const CHARSET_INFO *const valuecs);

  inline size_t get_num_fields() const {
    return m_row_processing ? m_num_fields : 0;
  }

 private:
  // how many fields were already stored in the buffer for the row being
  // currently processed (since start_row())
  size_t m_num_fields;
  // true if currently the row is being built (there was start_row() with no
  // closing end_row())
  bool m_row_processing;

  static size_t get_time_size(const MYSQL_TIME *value);
  static void append_time_values(const MYSQL_TIME *value,
                                 CodedOutputStream *out_stream);
};
}  // namespace ngs

#endif  //  _NGS_ROW_BUILDER_H_
