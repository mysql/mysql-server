/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MOCK_FIELD_LONG_INCLUDED
#define MOCK_FIELD_LONG_INCLUDED

#include <cstring>

#include "sql/field.h"

namespace temptable_test {

/** Helper to simplify creating fields. */
class Mock_field_long : public Field_long {
 public:
  /**
    Creates a column.
    @param name The column name.
    @param is_nullable Whether it's nullable.
    @param is_unsigned Whether it's unsigned.
  */
  Mock_field_long(const char *name, bool is_nullable, bool is_unsigned)
      : Field_long(nullptr,                          // ptr_arg
                   8,                                // len_arg
                   is_nullable ? &null_byte : NULL,  // null_ptr_arg
                   is_nullable ? 1 : 0,              // null_bit_arg
                   Field::NONE,                      // auto_flags_arg
                   name,                             // field_name_arg
                   false,                            // zero_arg
                   is_unsigned)                      // unsigned_arg
  {
    ptr = buffer;
    std::memset(buffer, 0, PACK_LENGTH);
    static const char *table_name_buf = "table_name";
    table_name = &table_name_buf;
  }

 private:
  uchar buffer[PACK_LENGTH];
  uchar null_byte = '\0';
};

}  // namespace temptable_test

#endif  // MOCK_FIELD_LONG_INCLUDED
