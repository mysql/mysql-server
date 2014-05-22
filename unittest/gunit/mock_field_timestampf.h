/* Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef MOCK_FIELD_TIMESTAMPF_H
#define MOCK_FIELD_TIMESTAMPF_H

#include "fake_table.h"
#include "field.h"

class Mock_field_timestampf : public Field_timestampf
{
  uchar null_byte;
  void initialize()
  {
    table = new Fake_TABLE(this);
    EXPECT_FALSE(table == NULL) << "Out of memory";
    ptr= buffer;
    memset(buffer, 0, PACK_LENGTH);
    set_null_ptr(&null_byte, 1);
  }

public:
  uchar buffer[PACK_LENGTH];
  bool store_timestamp_internal_called;
  Mock_field_timestampf(Field::utype utype, int scale) :
    Field_timestampf(static_cast<uchar*>(buffer), // ptr_arg
                     &null_byte, // null_ptr_arg
                     '\0', // null_bit_arg
                     utype,// unireg_check_arg
                     "",   // field_name_arg
                     static_cast<uint8>(scale)), // dec_arg a.k.a. scale.
    null_byte(0),
    store_timestamp_internal_called(false)
  {
    initialize();
  }

  timeval to_timeval()
  {
    timeval tm;
    int warnings= 0;
    get_timestamp(&tm, &warnings);
    EXPECT_EQ(0, warnings);
    return tm;
  }

  /* Averts ASSERT_COLUMN_MARKED_FOR_WRITE assertion. */
  void make_writable() { bitmap_set_bit(table->write_set, field_index); }

  void store_timestamp_internal(const timeval *tm)
  {
    store_timestamp_internal_called= true;
    return Field_timestampf::store_timestamp_internal(tm);
  }

  ~Mock_field_timestampf() { delete table; }
};

#endif // MOCK_FIELD_TIMESTAMPF_H
