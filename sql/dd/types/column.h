/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "dd/collection.h"           // dd::Collection
#include "dd/sdi_fwd.h"              // RJ_Document
#include "dd/types/entity_object.h"  // dd::Entity_object
#include "my_inttypes.h"

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Abstract_table;
class Column_impl;
class Column_type_element;
class Object_table;
class Object_type;
class Properties;

///////////////////////////////////////////////////////////////////////////

// Redefined enum_field_types here. We can remove some old types ?
enum class enum_column_types
{
    DECIMAL= 1, // This is 1 > than MYSQL_TYPE_DECIMAL
    TINY,
    SHORT,
    LONG,
    FLOAT,
    DOUBLE,
    TYPE_NULL,
    TIMESTAMP,
    LONGLONG,
    INT24,
    DATE,
    TIME,
    DATETIME,
    YEAR,
    NEWDATE,
    VARCHAR,
    BIT,
    TIMESTAMP2,
    DATETIME2,
    TIME2,
    NEWDECIMAL,
    ENUM,
    SET,
    TINY_BLOB,
    MEDIUM_BLOB,
    LONG_BLOB,
    BLOB,
    VAR_STRING,
    STRING,
    GEOMETRY,
    JSON
  };

class Column : virtual public Entity_object
{
public:
  static const Object_type &TYPE();
  static const Object_table &OBJECT_TABLE();
  typedef Collection<Column_type_element*> Column_type_element_collection;
  typedef Column_impl Impl;

  enum enum_column_key
  {
    CK_NONE= 1,
    CK_PRIMARY,
    CK_UNIQUE,
    CK_MULTIPLE
  };

public:
  virtual ~Column()
  { };

  /////////////////////////////////////////////////////////////////////////
  // Table.
  /////////////////////////////////////////////////////////////////////////

  virtual const Abstract_table &table() const = 0;

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
  virtual void set_datetime_precision_null(bool is_null) = 0;
  virtual bool is_datetime_precision_null() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // has_no_default.
  /////////////////////////////////////////////////////////////////////////

  virtual bool has_no_default() const = 0;
  virtual void set_has_no_default(bool has_explicit_default) = 0;

  /////////////////////////////////////////////////////////////////////////
  // default_value (binary).
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &default_value() const = 0;
  virtual void set_default_value(const String_type &default_value) = 0;
  virtual void set_default_value_null(bool is_null) = 0;
  virtual bool is_default_value_null() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // default_value_utf8
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &default_value_utf8() const = 0;
  virtual void set_default_value_utf8(
                 const String_type &default_value_utf8) = 0;
  virtual void set_default_value_utf8_null(bool is_null) = 0;
  virtual bool is_default_value_utf8_null() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // is virtual ?
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_virtual() const = 0;

  virtual void set_virtual(bool is_virtual) = 0;

  /////////////////////////////////////////////////////////////////////////
  // generation_expression (binary).
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &generation_expression() const = 0;

  virtual void set_generation_expression(const String_type
                                         &generation_expression) = 0;

  virtual bool is_generation_expression_null() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // generation_expression_utf8
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &generation_expression_utf8() const = 0;

  virtual void set_generation_expression_utf8(const String_type
                                              &generation_expression_utf8) = 0;

  virtual bool is_generation_expression_utf8_null() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // default_option.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &default_option() const = 0;
  virtual void set_default_option(const String_type &default_option) = 0;

  /////////////////////////////////////////////////////////////////////////
  // update_option.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &update_option() const = 0;
  virtual void set_update_option(const String_type &update_option) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Comment.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &comment() const = 0;
  virtual void set_comment(const String_type &comment) = 0;

  /////////////////////////////////////////////////////////////////////////
  // hidden.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_hidden() const = 0;
  virtual void set_hidden(bool hidden) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Options.
  /////////////////////////////////////////////////////////////////////////

  virtual const Properties &options() const = 0;

  virtual Properties &options() = 0;
  virtual bool set_options_raw(const String_type &options_raw) = 0;

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  virtual const Properties &se_private_data() const = 0;

  virtual Properties &se_private_data() = 0;
  virtual bool set_se_private_data_raw(const String_type &se_private_data_raw) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Column key type.
  /////////////////////////////////////////////////////////////////////////

  virtual void set_column_key(enum_column_key column_key) = 0;

  virtual enum_column_key column_key() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // Column display type.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &column_type_utf8() const = 0;

  virtual void set_column_type_utf8(const String_type &column_type_utf8) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Elements.
  /////////////////////////////////////////////////////////////////////////

  virtual Column_type_element *add_element() = 0;

  virtual const Column_type_element_collection &elements() const = 0;

  virtual size_t elements_count() const = 0;

  /**
    Converts *this into json.

    Converts all member variables that are to be included in the sdi
    into json by transforming them appropriately and passing them to
    the rapidjson writer provided.

    @param wctx opaque context for data needed by serialization
    @param w rapidjson writer which will perform conversion to json

  */

  virtual void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const = 0;


  /**
    Re-establishes the state of *this by reading sdi information from
    the rapidjson DOM subobject provided.

    Cross-references encountered within this object are tracked in
    sdictx, so that they can be updated when the entire object graph
    has been established.

    @param rctx stores book-keeping information for the
    deserialization process
    @param val subobject of rapidjson DOM containing json
    representation of this object
  */

  virtual bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val) = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__COLUMN_INCLUDED
