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

#ifndef DD__INDEX_IMPL_INCLUDED
#define DD__INDEX_IMPL_INCLUDED

#include <sys/types.h>
#include <memory>
#include <new>
#include <string>

#include "dd/impl/types/entity_object_impl.h" // dd::Entity_object_impl
#include "dd/impl/types/weak_object_impl.h"
#include "dd/object_id.h"
#include "dd/sdi_fwd.h"
#include "dd/types/index.h"                   // dd::Index
#include "dd/types/index_element.h"           // dd::Index_element
#include "dd/types/object_type.h"             // dd::Object_type

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Open_dictionary_tables_ctx;
class Raw_record;
class Table_impl;
class Column;
class Index_element;
class Object_table;
class Properties;
class Sdi_rcontext;
class Sdi_wcontext;
class Table;
class Weak_object;

///////////////////////////////////////////////////////////////////////////

class Index_impl : public Entity_object_impl,
                   public Index
{
public:
  Index_impl();

  Index_impl(Table_impl *table);

  Index_impl(const Index_impl &src, Table_impl *parent);

  virtual ~Index_impl();

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

  void debug_print(String_type &outb) const;

  virtual void set_ordinal_position(uint ordinal_position)
  { m_ordinal_position= ordinal_position; }

  virtual uint ordinal_position() const
  { return m_ordinal_position; }

public:
  /////////////////////////////////////////////////////////////////////////
  // Table.
  /////////////////////////////////////////////////////////////////////////

  virtual const Table &table() const;

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
  // is_hidden.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_hidden() const
  { return m_hidden; }

  virtual void set_hidden(bool hidden)
  { m_hidden= hidden; }

  /////////////////////////////////////////////////////////////////////////
  // comment.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &comment() const
  { return m_comment; }

  virtual void set_comment(const String_type &comment)
  { m_comment= comment; }

  /////////////////////////////////////////////////////////////////////////
  // Options.
  /////////////////////////////////////////////////////////////////////////

  virtual const Properties &options() const
  { return *m_options; }

  virtual Properties &options()
  { return *m_options; }

  virtual bool set_options_raw(const String_type &options_raw);

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  virtual const Properties &se_private_data() const
  { return *m_se_private_data; }

  virtual Properties &se_private_data()
  { return *m_se_private_data; }

  virtual bool set_se_private_data_raw(const String_type &se_private_data_raw);
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

  virtual const String_type &engine() const
  { return m_engine; }

  virtual void set_engine(const String_type &engine)
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

  virtual bool is_visible() const { return m_is_visible; }

  virtual void set_visible(bool is_visible) { m_is_visible= is_visible; }

  /////////////////////////////////////////////////////////////////////////
  // Index-element collection
  /////////////////////////////////////////////////////////////////////////

  virtual Index_element *add_element(Column *c);

  virtual const Index_elements &elements() const
  { return m_elements; }

  virtual bool is_candidate_key() const;

  // Fix "inherits ... via dominance" warnings
  virtual Weak_object_impl *impl()
  { return Weak_object_impl::impl(); }
  virtual const Weak_object_impl *impl() const
  { return Weak_object_impl::impl(); }
  virtual Object_id id() const
  { return Entity_object_impl::id(); }
  virtual bool is_persistent() const
  { return Entity_object_impl::is_persistent(); }
  virtual const String_type &name() const
  { return Entity_object_impl::name(); }
  virtual void set_name(const String_type &name)
  { Entity_object_impl::set_name(name); }

public:
  static Index_impl *restore_item(Table_impl *table)
  {
    return new (std::nothrow) Index_impl(table);
  }

  static Index_impl *clone(const Index_impl &other,
                           Table_impl *table)
  {
    return new (std::nothrow) Index_impl(other, table);
  }

private:
  // Fields.

  bool m_hidden;
  bool m_is_generated;

  uint m_ordinal_position;

  String_type m_comment;
  std::unique_ptr<Properties> m_options;
  std::unique_ptr<Properties> m_se_private_data;

  Index::enum_index_type m_type;
  Index::enum_index_algorithm m_algorithm;
  bool m_is_algorithm_explicit;
  bool m_is_visible;

  String_type m_engine;

  // References to tightly-coupled objects.

  Table_impl *m_table;

  Index_elements m_elements;

  // References to loosely-coupled objects.

  Object_id m_tablespace_id;
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
