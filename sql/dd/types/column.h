/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__COLUMN_INCLUDED
#define DD__COLUMN_INCLUDED

#include "my_global.h"

#include "dd/types/entity_object.h"  // dd::Entity_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Abstract_table;
class Column_type_element;
class Object_table;
class Object_type;
class Properties;
template <typename I> class Iterator;
typedef Iterator<Column_type_element> Column_type_element_iterator;
typedef Iterator<const Column_type_element> Column_type_element_const_iterator;

///////////////////////////////////////////////////////////////////////////

class Column : virtual public Entity_object
{
public:
  static const Object_type &TYPE();
  static const Object_table &OBJECT_TABLE();

public:
  // Redefined enum_field_types here. We can remove some old types ?
  enum enum_column_types
  {
    TYPE_DECIMAL= 1, // This is 1 > than MYSQL_TYPE_DECIMAL
    TYPE_TINY,
    TYPE_SHORT,
    TYPE_LONG,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_NULL,
    TYPE_TIMESTAMP,
    TYPE_LONGLONG,
    TYPE_INT24,
    TYPE_DATE,
    TYPE_TIME,
    TYPE_DATETIME,
    TYPE_YEAR,
    TYPE_NEWDATE,
    TYPE_VARCHAR,
    TYPE_BIT,
    TYPE_TIMESTAMP2,
    TYPE_DATETIME2,
    TYPE_TIME2,
    TYPE_NEWDECIMAL,
    TYPE_ENUM,
    TYPE_SET,
    TYPE_TINY_BLOB,
    TYPE_MEDIUM_BLOB,
    TYPE_LONG_BLOB,
    TYPE_BLOB,
    TYPE_VAR_STRING,
    TYPE_STRING,
    TYPE_GEOMETRY,
    TYPE_JSON
  };

public:
  virtual ~Column()
  { };

  /////////////////////////////////////////////////////////////////////////
  // Table.
  /////////////////////////////////////////////////////////////////////////

  const Abstract_table &table() const
  { return const_cast<Column *> (this)->table(); }

  virtual Abstract_table &table() = 0;

  /////////////////////////////////////////////////////////////////////////
  // collation.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id collation_id() const = 0;
  virtual void set_collation_id(Object_id collation_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // type.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_column_types type() const = 0;
  virtual void set_type(enum_column_types type) = 0;

  /////////////////////////////////////////////////////////////////////////
  // nullable.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_nullable() const = 0;
  virtual void set_nullable(bool nullable) = 0;

  /////////////////////////////////////////////////////////////////////////
  // is_zerofill.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_zerofill() const = 0;
  virtual void set_zerofill(bool zerofill) = 0;

  /////////////////////////////////////////////////////////////////////////
  // is_unsigned.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_unsigned() const = 0;
  virtual void set_unsigned(bool unsigned_flag) = 0;

  /////////////////////////////////////////////////////////////////////////
  // auto increment.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_auto_increment() const = 0;
  virtual void set_auto_increment(bool auto_increment) = 0;

  /////////////////////////////////////////////////////////////////////////
  // ordinal_position
  /////////////////////////////////////////////////////////////////////////

  virtual uint ordinal_position() const = 0;

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

  /////////////////////////////////////////////////////////////////////////
  // has_no_default.
  /////////////////////////////////////////////////////////////////////////

  virtual bool has_no_default() const = 0;
  virtual void set_has_no_default(bool has_explicit_default) = 0;

  /////////////////////////////////////////////////////////////////////////
  // default_value (binary).
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &default_value() const = 0;
  virtual void set_default_value(const std::string &default_value) = 0;
  virtual void set_default_value_null(bool is_null) = 0;
  virtual bool is_default_value_null() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // is virtual ?
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_virtual() const = 0;

  virtual void set_virtual(bool is_virtual) = 0;

  /////////////////////////////////////////////////////////////////////////
  // generation_expression (binary).
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &generation_expression() const = 0;

  virtual void set_generation_expression(const std::string
                                         &generation_expression) = 0;

  virtual bool is_generation_expression_null() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // generation_expression_utf8
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &generation_expression_utf8() const = 0;

  virtual void set_generation_expression_utf8(const std::string
                                              &generation_expression_utf8) = 0;

  virtual bool is_generation_expression_utf8_null() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // default_option.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &default_option() const = 0;
  virtual void set_default_option(const std::string &default_option) = 0;

  /////////////////////////////////////////////////////////////////////////
  // update_option.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &update_option() const = 0;
  virtual void set_update_option(const std::string &update_option) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Comment.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &comment() const = 0;
  virtual void set_comment(const std::string &comment) = 0;

  /////////////////////////////////////////////////////////////////////////
  // hidden.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_hidden() const = 0;
  virtual void set_hidden(bool hidden) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Options.
  /////////////////////////////////////////////////////////////////////////

  const Properties &options() const
  { return const_cast<Column *> (this)->options(); }

  virtual Properties &options() = 0;
  virtual bool set_options_raw(const std::string &options_raw) = 0;

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  const Properties &se_private_data() const
  { return const_cast<Column *> (this)->se_private_data(); }

  virtual Properties &se_private_data() = 0;
  virtual bool set_se_private_data_raw(const std::string &se_private_data_raw) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Enum-elements.
  /////////////////////////////////////////////////////////////////////////

  virtual Column_type_element *add_enum_element() = 0;

  virtual Column_type_element_const_iterator *enum_elements() const = 0;

  virtual Column_type_element_iterator *enum_elements() = 0;

  virtual size_t enum_elements_count() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // Set-elements.
  /////////////////////////////////////////////////////////////////////////

  virtual Column_type_element *add_set_element() = 0;

  virtual Column_type_element_const_iterator *set_elements() const = 0;

  virtual Column_type_element_iterator *set_elements() = 0;

  virtual size_t set_elements_count() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // Drop this column from the collection.
  /////////////////////////////////////////////////////////////////////////

  virtual void drop() = 0; /* purecov: deadcode */
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__COLUMN_INCLUDED
