/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#ifndef DD__UDT_PARAMETER_INCLUDED
#define DD__UDT_PARAMETER_INCLUDED

#include <cstddef>  // std::nullptr_t
#include <optional>

#include "my_inttypes.h"
#include "sql/dd/impl/raw/object_keys.h"  // IWYU pragma: keep
#include "sql/dd/types/column.h"
#include "sql/dd/types/entity_object.h"  // dd::Entity_object

class THD;
struct MDL_key;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Item_name_key;
class Primary_id_key;
class UDT_Parameter_impl;
class Void_key;

namespace tables {
class UDT_Parameters;
}

///////////////////////////////////////////////////////////////////////////

class UDT_Parameter : virtual public Entity_object {
 public:
  typedef UDT_Parameter_impl Impl;
  typedef UDT_Parameter Cache_partition;
  typedef tables::UDT_Parameters DD_table;
  typedef Primary_id_key Id_key;
  typedef Item_name_key Name_key;
  typedef Void_key Aux_key;

  enum enum_parameter_mode { PM_IN = 1, PM_OUT, PM_INOUT };

  // We need a set of functions to update a preallocated key.
  virtual bool update_id_key(Id_key *key) const {
    return update_id_key(key, id());
  }

  static bool update_id_key(Id_key *key, Object_id id);

  virtual bool update_name_key(Name_key *key) const {
    return update_name_key(key, method_id(), name());
  }

  static bool update_name_key(Name_key *key, Object_id type_id,
                              const String_type &name);

  virtual bool update_aux_key(Aux_key *) const { return true; }

 public:
  ~UDT_Parameter() override = default;

  /////////////////////////////////////////////////////////////////////////
  // method.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id method_id() const = 0;
  virtual void set_method_id(Object_id method_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // ordinal_position.
  /////////////////////////////////////////////////////////////////////////

  virtual uint ordinal_position() const = 0;
  virtual void set_ordinal_position(uint ordinal_position) = 0;

  /////////////////////////////////////////////////////////////////////////
  // mode
  /////////////////////////////////////////////////////////////////////////

  virtual enum_parameter_mode mode() const = 0;
  virtual void set_mode(enum_parameter_mode mode) = 0;

  /////////////////////////////////////////////////////////////////////////
  // data type.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_column_types data_type() const = 0;
  virtual void set_data_type(enum_column_types type) = 0;

  /////////////////////////////////////////////////////////////////////////
  // is_unsigned.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_unsigned() const = 0;
  virtual void set_is_unsigned(bool is_unsigned) = 0;

  /////////////////////////////////////////////////////////////////////////
  // char_length.
  /////////////////////////////////////////////////////////////////////////

  virtual size_t char_length() const = 0;
  virtual void set_char_length(size_t char_length) = 0;

  /////////////////////////////////////////////////////////////////////////
  // numeric_precision.
  /////////////////////////////////////////////////////////////////////////

  virtual uint numeric_precision() const = 0;
  virtual void set_numeric_precision(uint numeric_precision) = 0;

  /////////////////////////////////////////////////////////////////////////
  // numeric_scale.
  /////////////////////////////////////////////////////////////////////////

  virtual uint numeric_scale() const = 0;
  virtual void set_numeric_scale(uint numeric_scale) = 0;
  virtual void set_numeric_scale_null(bool is_null) = 0;
  virtual bool is_numeric_scale_null() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // datetime_precision.
  /////////////////////////////////////////////////////////////////////////

  virtual uint datetime_precision() const = 0;
  virtual void set_datetime_precision(uint datetime_precision) = 0;
  virtual bool is_datetime_precision_null() const = 0;
  virtual void set_datetime_precision_null(bool is_null) = 0;

  /////////////////////////////////////////////////////////////////////////
  // collation.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id collation_id() const = 0;
  virtual void set_collation_id(Object_id collation_id) = 0;
  virtual bool is_explicit_collation() const = 0;
  virtual void set_is_explicit_collation(bool is_explicit_collation) = 0;

  /////////////////////////////////////////////////////////////////////////
  // is_result.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_result() const = 0;
  virtual void set_is_result(bool is_result) = 0;

  /////////////////////////////////////////////////////////////////////////
  // created
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong created(bool convert_time) const = 0;
  virtual void set_created(ulonglong created) = 0;

  /////////////////////////////////////////////////////////////////////////
  // last_altered
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong last_altered(bool convert_time) const = 0;
  virtual void set_last_altered(ulonglong last_altered) = 0;

  /**
    Allocate a new object and invoke the copy constructor

    @return pointer to dynamically allocated copy
  */
  virtual UDT_Parameter *clone() const = 0;

  /**
    Allocate a new object which can serve as a placeholder for the original
    object in the Dictionary_client's dropped registry. Such object has the
    same keys as the original but has no other info and as result occupies
    less memory.
  */
  virtual UDT_Parameter *clone_dropped_object_placeholder() const = 0;

  static void create_mdl_key(const String_type &schema_name,
                             const String_type &type_name,
                             const String_type &method_name, MDL_key *key);
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__UDT_PARAMETER_INCLUDED
