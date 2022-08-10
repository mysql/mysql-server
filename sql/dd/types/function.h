/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#ifndef DD__FUNCTION_INCLUDED
#define DD__FUNCTION_INCLUDED

#include "my_inttypes.h"
#include "sql/dd/types/column.h"   // dd::Column::enum_column_types
#include "sql/dd/types/routine.h"  // dd::Routine

struct MDL_key;

namespace dd {

class Function_impl;

///////////////////////////////////////////////////////////////////////////

class Function : virtual public Routine {
 public:
  typedef Function_impl Impl;

  bool update_name_key(Name_key *key) const override {
    return update_routine_name_key(key, schema_id(), name());
  }

  static bool update_name_key(Name_key *key, Object_id schema_id,
                              const String_type &name);

 public:
  ~Function() override = default;

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
    Allocate a new object graph and invoke the copy constructor for
    each object. Only used in unit testing.

    @return pointer to dynamically allocated copy
  */
  Function *clone() const override = 0;

  /**
    Allocate a new object which can serve as a placeholder for the original
    object in the Dictionary_client's dropped registry. Such object has the
    same keys as the original but has no other info and as result occupies
    less memory.
  */
  Function *clone_dropped_object_placeholder() const override = 0;

  static void create_mdl_key(const String_type &schema_name,
                             const String_type &name, MDL_key *key) {
    Routine::create_mdl_key(RT_FUNCTION, schema_name, name, key);
  }
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__FUNCTION_INCLUDED
