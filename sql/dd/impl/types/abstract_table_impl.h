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

#ifndef DD__ABSTRACT_TABLE_IMPL_INCLUDED
#define DD__ABSTRACT_TABLE_IMPL_INCLUDED

#include "my_global.h"

#include "dd/types/abstract_table.h"          // dd::Abstract_table
#include "dd/types/object_type.h"             // dd::Object_type
#include "dd/impl/types/entity_object_impl.h" // dd::Entity_object_impl

#include <memory>   // std::unique_ptr

namespace dd {
template <typename T> class Collection;

///////////////////////////////////////////////////////////////////////////

class Abstract_table_impl : public Entity_object_impl,
                            virtual public Abstract_table
{
public:
  typedef Collection<Column> Column_collection;

public:
  virtual bool validate() const;

  virtual bool restore_children(Open_dictionary_tables_ctx *otx);

  virtual bool store_children(Open_dictionary_tables_ctx *otx);

  virtual bool drop_children(Open_dictionary_tables_ctx *otx) const;

  virtual bool restore_attributes(const Raw_record &r);

  virtual bool store_attributes(Raw_record *r);

protected:
  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val);

public:
  virtual void debug_print(std::string &outb) const;

public:
  /////////////////////////////////////////////////////////////////////////
  // schema.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id schema_id() const
  { return m_schema_id; }

  virtual void set_schema_id(Object_id schema_id)
  { m_schema_id= schema_id; }

  /////////////////////////////////////////////////////////////////////////
  // mysql_version_id.
  // Primarily intended for debugging, but can be used as a last-resort
  // version check for SE data and other items, but in general other
  // mechanisms should be preferred.
  /////////////////////////////////////////////////////////////////////////

  virtual uint mysql_version_id() const
  { return m_mysql_version_id; }

  // TODO: Commented out as it is not needed as we either use the value
  // assigned by the constructor, or restore a value from the TABLES
  // table. It may be necessary when implementing upgrade.
  //virtual void set_mysql_version_id(uint mysql_version_id)
  //{ m_mysql_version_id= mysql_version_id; }

  /////////////////////////////////////////////////////////////////////////
  // options.
  /////////////////////////////////////////////////////////////////////////

  using Abstract_table::options;

  virtual Properties &options()
  { return *m_options; }

  virtual bool set_options_raw(const std::string &options_raw);

  /////////////////////////////////////////////////////////////////////////
  // created.
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong created() const
  { return m_created; }

  virtual void set_created(ulonglong created)
  { m_created= created; }

  /////////////////////////////////////////////////////////////////////////
  // last altered.
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong last_altered() const
  { return m_last_altered; }

  virtual void set_last_altered(ulonglong last_altered)
  { m_last_altered= last_altered; }


  /////////////////////////////////////////////////////////////////////////
  // Column collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Column *add_column();

  virtual Column_const_iterator *columns() const;

  virtual Column_iterator *columns();

  virtual Column_const_iterator *user_columns() const;

  virtual Column_iterator *user_columns();

  const Column *get_column(Object_id column_id) const
  { return const_cast<Abstract_table_impl *> (this)->get_column(column_id); }

  Column *get_column(Object_id column_id);

  const Column *get_column(const std::string name) const
  { return const_cast<Abstract_table_impl *> (this)->get_column(name); }

  Column *get_column(const std::string name);

  Column_collection *column_collection()
  { return m_columns.get(); }

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

protected:
  Abstract_table_impl();

  virtual ~Abstract_table_impl()
  { }

private:
  // Fields.

  uint m_mysql_version_id;

  // TODO-POST-MERGE-TO-TRUNK:
  // Add new field m_last_checked_for_upgrade

  ulonglong m_created;
  ulonglong m_last_altered;

  std::unique_ptr<Properties> m_options;

  // References to tightly-coupled objects.

  std::unique_ptr<Column_collection> m_columns;

  // References to other objects.

  Object_id m_schema_id;

protected:
  Abstract_table_impl(const Abstract_table_impl &src);
};

///////////////////////////////////////////////////////////////////////////

class Abstract_table_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  {
    DBUG_ASSERT(false);
    return NULL;
  }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__ABSTRACT_TABLE_IMPL_INCLUDED
