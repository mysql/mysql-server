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

#ifndef DD__ABSTRACT_TABLE_IMPL_INCLUDED
#define DD__ABSTRACT_TABLE_IMPL_INCLUDED

#include <stddef.h>
#include <sys/types.h>
#include <memory>   // std::unique_ptr
#include <string>

#include "dd/impl/raw/raw_record.h"
#include "dd/impl/types/column_impl.h"        // dd::Column_impl
#include "dd/impl/types/entity_object_impl.h" // dd::Entity_object_impl
#include "dd/impl/types/weak_object_impl.h"
#include "dd/object_id.h"
#include "dd/properties.h"
#include "dd/sdi_fwd.h"
#include "dd/types/abstract_table.h"          // dd::Abstract_table
#include "dd/types/object_type.h"             // dd::Object_type
#include "my_dbug.h"
#include "my_inttypes.h"

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Column;
class Open_dictionary_tables_ctx;
class Sdi_rcontext;
class Sdi_wcontext;
class Weak_object;

class Abstract_table_impl : public Entity_object_impl,
                            virtual public Abstract_table
{
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
  virtual void debug_print(String_type &outb) const;

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

  virtual const Properties &options() const
  { return *m_options; }

  virtual Properties &options()
  { return *m_options; }

  virtual bool set_options_raw(const String_type &options_raw);

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
  // hidden.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_hidden_type hidden() const
  { return m_hidden; }

  virtual void set_hidden(enum_hidden_type hidden)
  { m_hidden= hidden; }

  /////////////////////////////////////////////////////////////////////////
  // Column collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Column *add_column();

  virtual const Column_collection &columns() const
  { return m_columns; }

  virtual Column_collection *columns()
  { return &m_columns; }

  const Column *get_column(Object_id column_id) const;

  Column *get_column(Object_id column_id);

  const Column *get_column(const String_type name) const;

  Column *get_column(const String_type name);

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

  enum_hidden_type m_hidden;

  std::unique_ptr<Properties> m_options;

  // References to tightly-coupled objects.

  Column_collection m_columns;

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
