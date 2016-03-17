/*
* Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; version 2 of the
* License.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
* 02110-1301  USA
*/

#ifndef _NGS_ROW_BUILDER_H_
#define _NGS_ROW_BUILDER_H_

#include "m_ctype.h"
#include "mysql_time.h"
#include "decimal.h"
#include "mysql_com.h"
#include "ngs/protocol/message_builder.h"
#include "ngs_common/protocol_protobuf.h"
#include <string>
#include <set>

namespace ngs
{
  class Output_buffer;

  class Row_builder: public Message_builder
  {
    typedef ::google::protobuf::io::CodedOutputStream CodedOutputStream;
  public:
    Row_builder();
    ~Row_builder();

    void start_row(Output_buffer* out_buffer);
    void abort_row();
    void end_row();

    void add_null_field();
    void add_longlong_field(longlong value, my_bool unsigned_flag);
    void add_decimal_field(const decimal_t * value);
    void add_decimal_field(const char * const value, size_t length);
    void add_double_field(double value);
    void add_float_field(float value);
    void add_date_field(const MYSQL_TIME * value);
    void add_time_field(const MYSQL_TIME * value, uint decimals);
    void add_datetime_field(const MYSQL_TIME * value, uint decimals);
    void add_string_field(const char * const value, size_t length,
      const CHARSET_INFO * const valuecs);
    void add_set_field(const char * const value, size_t length,
      const CHARSET_INFO * const valuecs);
    void add_bit_field(const char * const value, size_t length,
      const CHARSET_INFO * const valuecs);

    inline size_t get_num_fields() const { return m_row_processing ? m_num_fields : 0; }

  private:
    // how many fields were already stored in the buffer for the row being currently processed (since start_row())
    size_t m_num_fields;
    // true if currently the row is being built (there was start_row() with no closing end_row())
    bool m_row_processing;

    static size_t get_time_size(const MYSQL_TIME * value);
    static void append_time_values(const MYSQL_TIME * value, CodedOutputStream* out_stream);
  };
}


#endif //  _NGS_ROW_BUILDER_H_
