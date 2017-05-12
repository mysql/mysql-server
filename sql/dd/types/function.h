/* Copyright (c) 2017 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef DD__FUNCTION_INCLUDED
#define DD__FUNCTION_INCLUDED

#include "dd/types/column.h"              // dd::Column::enum_column_types
#include "dd/types/routine.h"             // dd::Routine
#include "my_inttypes.h"

namespace dd {

class Object_type;

///////////////////////////////////////////////////////////////////////////

class Function : virtual public Routine
{
public:
  static const Object_type &TYPE();

  virtual bool update_name_key(name_key_type *key) const
  { return update_routine_name_key(key, schema_id(), name()); }

  static bool update_name_key(name_key_type *key,
                              Object_id schema_id,
                              const String_type &name);

public:
  virtual ~Function()
  { };

public:
  /////////////////////////////////////////////////////////////////////////
  // result data type.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_column_types result_data_type() const = 0;
  virtual void set_result_data_type(enum_column_types type) = 0;

  virtual void set_result_data_type_null(bool is_null) = 0;
  virtual bool is_result_data_type_null() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // Result display type
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &result_data_type_utf8() const = 0;

  virtual void set_result_data_type_utf8(
                 const String_type &result_data_type_utf8) = 0;

  /////////////////////////////////////////////////////////////////////////
  // result_is_zerofill.
  /////////////////////////////////////////////////////////////////////////

  virtual bool result_is_zerofill() const = 0;
  virtual void set_result_zerofill(bool zerofill) = 0;

  /////////////////////////////////////////////////////////////////////////
  // result_is_unsigned.
  /////////////////////////////////////////////////////////////////////////

  virtual bool result_is_unsigned() const = 0;
  virtual void set_result_unsigned(bool unsigned_flag) = 0;

  /////////////////////////////////////////////////////////////////////////
  // result_char_length.
  /////////////////////////////////////////////////////////////////////////

  virtual size_t result_char_length() const = 0;
  virtual void set_result_char_length(size_t char_length) = 0;

  /////////////////////////////////////////////////////////////////////////
  // result_numeric_precision.
  /////////////////////////////////////////////////////////////////////////

  virtual uint result_numeric_precision() const = 0;
  virtual void set_result_numeric_precision(uint numeric_precision) = 0;

  /////////////////////////////////////////////////////////////////////////
  // result_numeric_scale.
  /////////////////////////////////////////////////////////////////////////

  virtual uint result_numeric_scale() const = 0;
  virtual void set_result_numeric_scale(uint numeric_scale) = 0;
  virtual void set_result_numeric_scale_null(bool is_null) = 0;
  virtual bool is_result_numeric_scale_null() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // result_datetime_precision.
  /////////////////////////////////////////////////////////////////////////

  virtual uint result_datetime_precision() const = 0;
  virtual void set_result_datetime_precision(uint datetime_precision) = 0;

  /////////////////////////////////////////////////////////////////////////
  // result_collation.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id result_collation_id() const = 0;
  virtual void set_result_collation_id(Object_id collation_id) = 0;

  /**
    Allocate a new object graph and invoke the copy contructor for
    each object. Only used in unit testing.

    @return pointer to dynamically allocated copy
  */
  virtual Function *clone() const = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__FUNCTION_INCLUDED
