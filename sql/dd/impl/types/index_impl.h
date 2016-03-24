/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__INDEX_IMPL_INCLUDED
#define DD__INDEX_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/collection_item.h"          // dd::Collection_item
#include "dd/impl/types/entity_object_impl.h" // dd::Entity_object_impl
#include "dd/types/index.h"                   // dd::Index
#include "dd/types/object_type.h"             // dd::Object_type

#include <memory>

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Raw_record;
class Table_impl;
class Open_dictionary_tables_ctx;
template <typename T> class Collection;

///////////////////////////////////////////////////////////////////////////

class Index_impl : public Entity_object_impl,
                   public Index,
                   public Collection_item
{
public:
  typedef Collection<Index_element> Element_collection;

public:
  Index_impl();

  virtual ~Index_impl()
  { }

public:
  virtual const Object_table &object_table() const
  { return Index::OBJECT_TABLE(); }

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

  // Required by Collection_item.
  virtual uint ordinal_position() const
  { return m_ordinal_position; }

public:
  virtual void drop();

public:
  /////////////////////////////////////////////////////////////////////////
  // Table.
  /////////////////////////////////////////////////////////////////////////

  using Index::table;

  virtual Table &table();

  /* non-virtual */ const Table_impl &table_impl() const
  { return *m_table; }

  /* non-virtual */ Table_impl &table_impl()
  { return *m_table; }

  /////////////////////////////////////////////////////////////////////////
  // is_generated
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_generated() const
  { return m_is_generated; }

  virtual void set_generated(bool generated)
  { m_is_generated= generated; }

  /////////////////////////////////////////////////////////////////////////
  // is_hidden. Also required by Collection_item
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_hidden() const
  { return m_hidden; }

  virtual void set_hidden(bool hidden)
  { m_hidden= hidden; }

  /////////////////////////////////////////////////////////////////////////
  // comment.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &comment() const
  { return m_comment; }

  virtual void set_comment(const std::string &comment)
  { m_comment= comment; }

  /////////////////////////////////////////////////////////////////////////
  // Options.
  /////////////////////////////////////////////////////////////////////////

  using Index::options;

  Properties &options()
  { return *m_options; }

  virtual bool set_options_raw(const std::string &options_raw);

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  using Index::se_private_data;

  virtual Properties &se_private_data()
  { return *m_se_private_data; }

  virtual bool set_se_private_data_raw(const std::string &se_private_data_raw);
  virtual void set_se_private_data(const Properties &se_private_data);

  /////////////////////////////////////////////////////////////////////////
  // Tablespace.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id tablespace_id() const
  { return m_tablespace_id; }

  virtual void set_tablespace_id(Object_id tablespace_id)
  { m_tablespace_id= tablespace_id; }

  /////////////////////////////////////////////////////////////////////////
  // Engine.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &engine() const
  { return m_engine; }

  virtual void set_engine(const std::string &engine)
  { m_engine= engine; }

  /////////////////////////////////////////////////////////////////////////
  // Index type.
  /////////////////////////////////////////////////////////////////////////

  virtual Index::enum_index_type type() const
  { return m_type; }

  virtual void set_type(Index::enum_index_type type)
  { m_type= type; }

  /////////////////////////////////////////////////////////////////////////
  // Index algorithm.
  /////////////////////////////////////////////////////////////////////////

  virtual Index::enum_index_algorithm algorithm() const
  { return m_algorithm; }

  virtual void set_algorithm(Index::enum_index_algorithm algorithm)
  { m_algorithm= algorithm; }

  virtual bool is_algorithm_explicit() const
  { return m_is_algorithm_explicit; }

  virtual void set_algorithm_explicit(bool alg_expl)
  { m_is_algorithm_explicit= alg_expl; }

  /////////////////////////////////////////////////////////////////////////
  // Index-element collection
  /////////////////////////////////////////////////////////////////////////

  virtual Index_element *add_element(Column *c);

  virtual Index_element *add_element(const Index_element &e);

  virtual Index_element_const_iterator *elements() const;

  virtual Index_element_iterator *elements();

  virtual Index_element_const_iterator *user_elements() const;

  virtual Index_element_iterator *user_elements();

  virtual uint user_elements_count() const;

  Element_collection *element_collection()
  { return m_elements.get(); }

  void invalidate_user_elements_count_cache()
  {
    m_user_elements_count_cache= static_cast<uint>(-1);
  }

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
    Factory(Table_impl *table)
     :m_table(table)
    { }

    virtual Collection_item *create_item() const;

  private:
    Table_impl *m_table;
  };

private:
  // Fields.

  bool m_hidden;
  bool m_is_generated;

  uint m_ordinal_position;

  std::string m_comment;
  std::unique_ptr<Properties> m_options;
  std::unique_ptr<Properties> m_se_private_data;

  Index::enum_index_type m_type;
  Index::enum_index_algorithm m_algorithm;
  bool m_is_algorithm_explicit;

  std::string m_engine;

  // References to tightly-coupled objects.

  Table_impl *m_table;

  std::unique_ptr<Element_collection> m_elements;

  // References to loosely-coupled objects.

  Object_id m_tablespace_id;

  /** Cached value of user_elements_count() method. */
  mutable uint m_user_elements_count_cache;
  /**
    Value which we use as indication that no value is cached in
    m_user_elements_count_cache member.
  */
  static const uint INVALID_USER_ELEMENTS_COUNT= static_cast<uint>(-1);

  Index_impl(const Index_impl &src, Table_impl *parent);

public:
  Index_impl *clone(Table_impl *parent) const
  {
    return new Index_impl(*this, parent);
  }
};

///////////////////////////////////////////////////////////////////////////

class Index_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Index_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__INDEX_IMPL_INCLUDED
