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

#ifndef DD__FOREIGN_KEY_IMPL_INCLUDED
#define DD__FOREIGN_KEY_IMPL_INCLUDED

#include <sys/types.h>
#include <new>

#include "m_ctype.h"                          // my_strcasecmp
#include "my_sharedlib.h"
#include "sql/dd/impl/types/entity_object_impl.h" // dd::Entity_object_impl
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/sdi_fwd.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/foreign_key.h"         // dd::Foreign_key
#include "sql/dd/types/foreign_key_element.h" // IWYU pragma: keep
#include "sql/dd/types/object_type.h"         // dd::Object_type

extern "C" MYSQL_PLUGIN_IMPORT CHARSET_INFO *system_charset_info;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Index;
class Object_table;
class Open_dictionary_tables_ctx;
class Raw_record;
class Sdi_rcontext;
class Sdi_wcontext;
class Table;
class Table_impl;
class Weak_object;

///////////////////////////////////////////////////////////////////////////

class Foreign_key_impl : public Entity_object_impl,
                         public Foreign_key
{
public:
  Foreign_key_impl();

  Foreign_key_impl(Table_impl *table);

  Foreign_key_impl(const Foreign_key_impl &src,
                   Table_impl *parent);

  virtual ~Foreign_key_impl()
  { }

public:
  virtual const Object_table &object_table() const
  { return Foreign_key::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool store(Open_dictionary_tables_ctx *otx);

  virtual bool restore_children(Open_dictionary_tables_ctx *otx);

  virtual bool store_children(Open_dictionary_tables_ctx *otx);

  virtual bool drop_children(Open_dictionary_tables_ctx *otx) const;

  virtual bool restore_attributes(const Raw_record &r);

  virtual bool store_attributes(Raw_record *r);

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val);

  void debug_print(String_type &outb) const;

public:
  void set_ordinal_position(uint)
  { }

  virtual uint ordinal_position() const
  { return -1; }

public:
  /////////////////////////////////////////////////////////////////////////
  // parent table.
  /////////////////////////////////////////////////////////////////////////

  virtual const Table &table() const;

  virtual Table &table();

  /* non-virtual */ const Table_impl &table_impl() const
  { return *m_table; }

  /* non-virtual */ Table_impl &table_impl()
  { return *m_table; }

  /////////////////////////////////////////////////////////////////////////
  // unique_constraint
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &unique_constraint_name() const
  { return m_unique_constraint_name; }

  virtual void set_unique_constraint_name(const String_type &name)
  { m_unique_constraint_name= name; }

  /////////////////////////////////////////////////////////////////////////
  // match_option.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_match_option match_option() const
  { return m_match_option; }

  virtual void set_match_option(enum_match_option match_option)
  { m_match_option= match_option; }

  /////////////////////////////////////////////////////////////////////////
  // update_rule.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_rule update_rule() const
  { return m_update_rule; }

  virtual void set_update_rule(enum_rule update_rule)
  { m_update_rule= update_rule; }

  /////////////////////////////////////////////////////////////////////////
  // delete_rule.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_rule delete_rule() const
  { return m_delete_rule; }

  virtual void set_delete_rule(enum_rule delete_rule)
  { m_delete_rule= delete_rule; }

  /////////////////////////////////////////////////////////////////////////
  // the catalog name of the referenced table.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &referenced_table_catalog_name() const
  { return m_referenced_table_catalog_name; }

  virtual void set_referenced_table_catalog_name(const String_type &name)
  { m_referenced_table_catalog_name= name; }

  /////////////////////////////////////////////////////////////////////////
  // the schema name of the referenced table.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &referenced_table_schema_name() const
  { return m_referenced_table_schema_name; }

  virtual void set_referenced_table_schema_name(const String_type &name)
  { m_referenced_table_schema_name= name; }

  /////////////////////////////////////////////////////////////////////////
  // the name of the referenced table.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &referenced_table_name() const
  { return m_referenced_table_name; }

  virtual void set_referenced_table_name(const String_type &name)
  { m_referenced_table_name= name; }

  /////////////////////////////////////////////////////////////////////////
  // Foreign key element collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Foreign_key_element *add_element();

  virtual const Foreign_key_elements &elements() const
  { return m_elements; }

  // Fix "inherits ... via dominance" warnings
  virtual Entity_object_impl *impl()
  { return Entity_object_impl::impl(); }
  virtual const Entity_object_impl *impl() const
  { return Entity_object_impl::impl(); }
  virtual Object_id id() const
  { return Entity_object_impl::id(); }
  virtual bool is_persistent() const
  { return Entity_object_impl::is_persistent(); }
  virtual const String_type &name() const
  { return Entity_object_impl::name(); }
  virtual void set_name(const String_type &name)
  { Entity_object_impl::set_name(name); }

public:
  static Foreign_key_impl *restore_item(Table_impl *table)
  {
    return new (std::nothrow) Foreign_key_impl(table);
  }

  static Foreign_key_impl *clone(const Foreign_key_impl &other,
                                 Table_impl *table)
  {
    return new (std::nothrow) Foreign_key_impl(other, table);
  }

private:
  enum_match_option m_match_option;
  enum_rule         m_update_rule;
  enum_rule         m_delete_rule;

  String_type m_unique_constraint_name;

  String_type m_referenced_table_catalog_name;
  String_type m_referenced_table_schema_name;
  String_type m_referenced_table_name;

  Table_impl *m_table;

  // Collections.

  Foreign_key_elements m_elements;

public:
  Foreign_key_impl *clone(Table_impl *parent) const
  {
    return new Foreign_key_impl(*this, parent);
  }
};

///////////////////////////////////////////////////////////////////////////

class Foreign_key_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Foreign_key_impl(); }
};

///////////////////////////////////////////////////////////////////////////

/** Class used to sort Foreign key's by name for the same table. */
struct Foreign_key_order_comparator
{
  bool operator()(const dd::Foreign_key* fk1, const dd::Foreign_key* fk2) const
  {
    return (my_strcasecmp(system_charset_info, fk1->name().c_str(),
                          fk2->name().c_str()) < 0);
  }
};

///////////////////////////////////////////////////////////////////////////
}

#endif // DD__FOREIGN_KEY_IMPL_INCLUDED
