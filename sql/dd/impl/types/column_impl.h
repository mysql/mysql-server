/* Copyright (c) 2014, 2016 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__COLUMN_IMPL_INCLUDED
#define DD__COLUMN_IMPL_INCLUDED

#include "my_global.h"

#include "dd/properties.h"                    // dd::Properties
#include "dd/impl/collection_item.h"          // dd::Collection_item
#include "dd/impl/types/entity_object_impl.h" // dd::Entity_object_impl
#include "dd/types/column.h"                  // dd::Column
#include "dd/types/object_type.h"             // dd::Object_type

#include <memory>   // std::unique_ptr

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Abstract_table_impl;
class Column_type_element;
class Raw_record;
class Open_dictionary_tables_ctx;
template <typename T> class Collection;

///////////////////////////////////////////////////////////////////////////

class Column_impl : public Entity_object_impl,
                    public Column,
                    public Collection_item
{
public:
  typedef Collection<Column_type_element> Column_type_element_collection;

public:
  Column_impl();

  virtual ~Column_impl()
  { }

public:
  virtual const Object_table &object_table() const
  { return Column::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool restore_children(Open_dictionary_tables_ctx *otx);

  virtual bool store_children(Open_dictionary_tables_ctx *otx);

  virtual bool drop_children(Open_dictionary_tables_ctx *otx) const;

  virtual bool restore_attributes(const Raw_record &r);

  virtual bool store_attributes(Raw_record *r);

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val);

  void debug_print(std::string &outb) const;

public:
  // Required by Collection_item.
  virtual bool store(Open_dictionary_tables_ctx *otx)
  { return Entity_object_impl::store(otx); }

  // Required by Collection_item.
  virtual bool drop(Open_dictionary_tables_ctx *otx) const
  { return Entity_object_impl::drop(otx); }

  // Required by Collection_item.
  virtual void set_ordinal_position(uint ordinal_position)
  { m_ordinal_position= ordinal_position; }

  // Required by Collection_item, Column.
  virtual void drop(); /* purecov: deadcode */

public:
  /////////////////////////////////////////////////////////////////////////
  // table.
  /////////////////////////////////////////////////////////////////////////

  using Column::table;

  virtual Abstract_table &table();

  /////////////////////////////////////////////////////////////////////////
  // type.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_column_types type() const
  { return m_type; }

  virtual void set_type(enum_column_types type)
  { m_type= type; }

  /////////////////////////////////////////////////////////////////////////
  // collation.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id collation_id() const
  { return m_collation_id; }

  virtual void set_collation_id(Object_id collation_id)
  { m_collation_id= collation_id; }

  /////////////////////////////////////////////////////////////////////////
  // nullable.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_nullable() const
  { return m_is_nullable; }

  virtual void set_nullable(bool nullable)
  { m_is_nullable= nullable; }

  /////////////////////////////////////////////////////////////////////////
  // is_zerofill.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_zerofill() const
  { return m_is_zerofill; }

  virtual void set_zerofill(bool zerofill)
  { m_is_zerofill= zerofill; }

  /////////////////////////////////////////////////////////////////////////
  // is_unsigned.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_unsigned() const
  { return m_is_unsigned; }

  virtual void set_unsigned(bool unsigned_flag)
  { m_is_unsigned= unsigned_flag; }

  /////////////////////////////////////////////////////////////////////////
  // auto increment.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_auto_increment() const
  { return m_is_auto_increment; }

  virtual void set_auto_increment(bool auto_increment)
  { m_is_auto_increment= auto_increment; }

  /////////////////////////////////////////////////////////////////////////
  // ordinal_position. - Also required by Collection_item
  /////////////////////////////////////////////////////////////////////////

  virtual uint ordinal_position() const
  { return m_ordinal_position; }

  /////////////////////////////////////////////////////////////////////////
  // char_length.
  /////////////////////////////////////////////////////////////////////////

  virtual size_t char_length() const
  { return m_char_length; }

  virtual void set_char_length(size_t char_length)
  { m_char_length= char_length; }

  /////////////////////////////////////////////////////////////////////////
  // numeric_precision.
  /////////////////////////////////////////////////////////////////////////

  virtual uint numeric_precision() const
  { return m_numeric_precision; }

  virtual void set_numeric_precision(uint numeric_precision)
  { m_numeric_precision= numeric_precision; }

  /////////////////////////////////////////////////////////////////////////
  // numeric_scale.
  /////////////////////////////////////////////////////////////////////////

  virtual uint numeric_scale() const
  { return m_numeric_scale; }

  virtual void set_numeric_scale(uint numeric_scale)
  {
    m_numeric_scale_null= false;
    m_numeric_scale= numeric_scale;
  }

  virtual void set_numeric_scale_null(bool is_null)
  { m_numeric_scale_null= is_null; }

  virtual bool is_numeric_scale_null() const
  { return m_numeric_scale_null; }

  /////////////////////////////////////////////////////////////////////////
  // datetime_precision.
  /////////////////////////////////////////////////////////////////////////

  virtual uint datetime_precision() const
  { return m_datetime_precision; }

  virtual void set_datetime_precision(uint datetime_precision)
  { m_datetime_precision= datetime_precision; }

  /////////////////////////////////////////////////////////////////////////
  // has_no_default.
  /////////////////////////////////////////////////////////////////////////

  virtual bool has_no_default() const
  { return m_has_no_default; }

  virtual void set_has_no_default(bool has_no_default)
  { m_has_no_default= has_no_default; }

  /////////////////////////////////////////////////////////////////////////
  // default_value (binary).
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &default_value() const
  { return m_default_value; }

  virtual void set_default_value(const std::string &default_value)
  {
    m_default_value_null= false;
    m_default_value= default_value;
  }

  virtual void set_default_value_null(bool is_null)
  { m_default_value_null= is_null; }

  virtual bool is_default_value_null() const
  { return m_default_value_null; }

  /////////////////////////////////////////////////////////////////////////
  // is virtual ?
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_virtual() const
  { return m_is_virtual; }

  virtual void set_virtual(bool is_virtual)
  { m_is_virtual= is_virtual; }

  /////////////////////////////////////////////////////////////////////////
  // generation_expression (binary).
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &generation_expression() const
  { return m_generation_expression; }

  virtual void set_generation_expression(const std::string
                                         &generation_expression)
  { m_generation_expression= generation_expression; }

  virtual bool is_generation_expression_null() const
  { return m_generation_expression.empty(); }

  /////////////////////////////////////////////////////////////////////////
  // generation_expression_utf8
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &generation_expression_utf8() const
  { return m_generation_expression_utf8; }

  virtual void set_generation_expression_utf8(const std::string
                                              &generation_expression_utf8)
  { m_generation_expression_utf8= generation_expression_utf8; }

  virtual bool is_generation_expression_utf8_null() const
  { return m_generation_expression_utf8.empty(); }

  /////////////////////////////////////////////////////////////////////////
  // default_option.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &default_option() const
  { return m_default_option; }

  virtual void set_default_option(const std::string &default_option)
  { m_default_option= default_option; }

  /////////////////////////////////////////////////////////////////////////
  // update_option.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &update_option() const
  { return m_update_option; }

  virtual void set_update_option(const std::string &update_option)
  { m_update_option= update_option; }

  /////////////////////////////////////////////////////////////////////////
  // Comment.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &comment() const
  { return m_comment; }

  virtual void set_comment(const std::string &comment)
  { m_comment= comment; }

  /////////////////////////////////////////////////////////////////////////
  // is_hidden. Also required by Collection_item
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_hidden() const
  { return m_hidden; }

  virtual void set_hidden(bool hidden)
  { m_hidden= hidden; }

  /////////////////////////////////////////////////////////////////////////
  // Options.
  /////////////////////////////////////////////////////////////////////////

  using Column::options;

  virtual Properties &options()
  { return *m_options; }

  virtual bool set_options_raw(const std::string &options_raw);

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  using Column::se_private_data;

  virtual Properties &se_private_data()
  { return *m_se_private_data; }

  virtual bool set_se_private_data_raw(const std::string &se_private_data_raw);

  /////////////////////////////////////////////////////////////////////////
  // Enum-elements.
  /////////////////////////////////////////////////////////////////////////

  virtual Column_type_element *add_enum_element();

  virtual Column_type_element_const_iterator *enum_elements() const;

  virtual Column_type_element_iterator *enum_elements();

  virtual size_t enum_elements_count() const;

  /////////////////////////////////////////////////////////////////////////
  // Set-elements.
  /////////////////////////////////////////////////////////////////////////

  virtual Column_type_element *add_set_element();

  virtual Column_type_element_const_iterator *set_elements() const;

  virtual Column_type_element_iterator *set_elements();

  virtual size_t set_elements_count() const;

  // Fix "inherits ... via dominance" warnings
  virtual Weak_object_impl *impl()
  { return Weak_object_impl::impl(); }
  virtual const Weak_object_impl *impl() const
  { return Weak_object_impl::impl(); }
  virtual Object_id id() const
  { return Entity_object_impl::id(); }
  virtual bool is_persistent() const
  { return Entity_object_impl::is_persistent(); }
  virtual const std::string &name() const
  { return Entity_object_impl::name(); }
  virtual void set_name(const std::string &name)
  { Entity_object_impl::set_name(name); }

public:
  class Factory : public Collection_item_factory
  {
  public:
    Factory(Abstract_table_impl *table)
     :m_table(table)
    { }

    virtual Collection_item *create_item() const;

  private:
    Abstract_table_impl *m_table;
  };

private:
  // Fields.

  enum_column_types m_type;

  bool m_is_nullable;
  bool m_is_zerofill;
  bool m_is_unsigned;
  bool m_is_auto_increment;
  bool m_is_virtual;
  bool m_hidden;

  uint m_ordinal_position;
  size_t m_char_length;
  uint m_numeric_precision;
  uint m_numeric_scale;
  bool m_numeric_scale_null;
  uint m_datetime_precision;

  bool m_has_no_default;

  bool m_default_value_null;
  std::string m_default_value;

  std::string m_default_option;
  std::string m_update_option;
  std::string m_comment;

  std::string m_generation_expression;
  std::string m_generation_expression_utf8;

  std::unique_ptr<Properties> m_options;
  std::unique_ptr<Properties> m_se_private_data;

  // References to tightly-coupled objects.

  Abstract_table_impl *m_table;

  std::unique_ptr<Column_type_element_collection> m_enum_elements;
  std::unique_ptr<Column_type_element_collection> m_set_elements;

  // References to loosely-coupled objects.

  Object_id m_collation_id;

  // TODO-WIKI21 should the columns.name be defined utf8_general_cs ?
  // instead of utf8_general_ci.

  Column_impl(const Column_impl &src, Abstract_table_impl *parent);

public:
  Column_impl *clone(Abstract_table_impl *parent) const
  {
    return new Column_impl(*this, parent);
  }
};

///////////////////////////////////////////////////////////////////////////

class Column_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Column_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__COLUMN_IMPL_INCLUDED
