/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"

#include "dd/impl/collection_item.h"          // dd::Collection_item
#include "dd/impl/os_specific.h"              // DD_HEADER_BEGIN
#include "dd/impl/types/entity_object_impl.h" // dd::Entity_object_impl
#include "dd/types/foreign_key.h"             // dd::Foreign_key
#include "dd/types/object_type.h"             // dd::Object_type

#include <memory>     // std::unique_ptr

DD_HEADER_BEGIN

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Raw_record;
class Table;
class Table_impl;
class Open_dictionary_tables_ctx;
template <typename T> class Collection;

///////////////////////////////////////////////////////////////////////////

class Foreign_key_impl : virtual public Entity_object_impl,
                         virtual public Foreign_key,
                         virtual public Collection_item
{
// Foreign keys not supported in the Global DD yet
/* purecov: begin deadcode */
public:
  typedef Collection<Foreign_key_element> Element_collection;

public:
  Foreign_key_impl();

  virtual ~Foreign_key_impl()
  { }

public:
  virtual const Object_table &object_table() const
  { return Foreign_key::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool restore_children(Open_dictionary_tables_ctx *otx);

  virtual bool store_children(Open_dictionary_tables_ctx *otx);

  virtual bool drop_children(Open_dictionary_tables_ctx *otx);

  virtual bool restore_attributes(const Raw_record &r);

  virtual bool store_attributes(Raw_record *r);

  void serialize(WriterVariant *wv) const;

  void deserialize(const RJ_Document *d);

  void debug_print(std::string &outb) const;

public:
  virtual void drop();

public:
  // Required by Collection_item.
  virtual void set_ordinal_position(uint ordinal_position)
  { }

  // Required by Collection_item.
  virtual uint ordinal_position() const
  { return -1; }

  // Required by Collection_item.
  virtual bool is_hidden() const
  { return false; }

  // Required by Collection_item.
  virtual bool store(Open_dictionary_tables_ctx *otx)
  { return Entity_object_impl::store(otx); }

  // Required by Collection_item.
  virtual bool drop(Open_dictionary_tables_ctx *otx)
  { return Entity_object_impl::drop(otx); }

public:
  /////////////////////////////////////////////////////////////////////////
  // parent table.
  /////////////////////////////////////////////////////////////////////////

  using Foreign_key::table;

  virtual Table &table();

  /* non-virtual */ const Table_impl &table_impl() const
  { return *m_table; }

  /* non-virtual */ Table_impl &table_impl()
  { return *m_table; }

  /////////////////////////////////////////////////////////////////////////
  // unique_constraint
  /////////////////////////////////////////////////////////////////////////

  using Foreign_key::unique_constraint;

  virtual Index &unique_constraint()
  { return *m_unique_constraint; }

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

  virtual const std::string &referenced_table_catalog_name() const
  { return m_referenced_table_catalog_name; }

  virtual void referenced_table_catalog_name(const std::string &name)
  { m_referenced_table_catalog_name= name; }

  /////////////////////////////////////////////////////////////////////////
  // the schema name of the referenced table.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &referenced_table_shema_name() const
  { return m_referenced_table_schema_name; }

  virtual void referenced_table_schema_name(const std::string &name)
  { m_referenced_table_schema_name= name; }

  /////////////////////////////////////////////////////////////////////////
  // the name of the referenced table.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &referenced_table_name() const
  { return m_referenced_table_name; }

  virtual void referenced_table_name(const std::string &name)
  { m_referenced_table_name= name; }

  /////////////////////////////////////////////////////////////////////////
  // Foreign key element collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Foreign_key_element *add_element();

  virtual Foreign_key_element_const_iterator *elements() const;

  virtual Foreign_key_element_iterator *elements();

  Element_collection *element_collection()
  { return m_elements.get(); }

public:
  class Factory : public Collection_item_factory
  {
  public:
    Factory(Table_impl *table)
     :m_table(table)
    { }

    virtual Collection_item *create_item() const;

  private:
    Table_impl *m_table;
  };

private:
  enum_match_option m_match_option;
  enum_rule         m_update_rule;
  enum_rule         m_delete_rule;

  Index *m_unique_constraint;

  std::string m_referenced_table_catalog_name;
  std::string m_referenced_table_schema_name;
  std::string m_referenced_table_name;

  Table_impl *m_table;

  // Collections.

  std::unique_ptr<Element_collection> m_elements;

#ifndef DBUG_OFF
  Foreign_key_impl(const Foreign_key_impl &src,
                   Table_impl *parent, Index *unique_constraint);

public:
  Foreign_key_impl *clone(Table_impl *parent, Index *unique_constraint) const
  {
    return new Foreign_key_impl(*this, parent, unique_constraint);
  }
#endif /* !DBUG_OFF */
/* purecov: end */
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

}

DD_HEADER_END

#endif // DD__FOREIGN_KEY_IMPL_INCLUDED
