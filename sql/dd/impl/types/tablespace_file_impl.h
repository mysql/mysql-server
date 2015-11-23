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

#ifndef DD__TABLESPACE_FILES_IMPL_INCLUDED
#define DD__TABLESPACE_FILES_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/collection_item.h"        // dd::Collection_item
#include "dd/impl/os_specific.h"            // DD_HEADER_BEGIN
#include "dd/impl/types/weak_object_impl.h" // dd::Weak_object_impl
#include "dd/types/object_type.h"           // dd::Object_type
#include "dd/types/tablespace_file.h"       // dd::Tablespace_file

#include <memory>   // std::unique_ptr

DD_HEADER_BEGIN

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Tablespace;
class Tablespace_impl;

///////////////////////////////////////////////////////////////////////////

class Tablespace_file_impl : virtual public Weak_object_impl,
                             virtual public Tablespace_file,
                             virtual public Collection_item
{
public:
  Tablespace_file_impl();

  virtual ~Tablespace_file_impl()
  { }

public:
  virtual const Object_table &object_table() const
  { return Tablespace_file::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool store_attributes(Raw_record *r);

  virtual bool restore_attributes(const Raw_record &r);

  void serialize(WriterVariant *wv) const;

  void deserialize(const RJ_Document *d);

  virtual void debug_print(std::string &outb) const;

public:
  // Required by Collection_item.
  virtual bool store(Open_dictionary_tables_ctx *otx)
  { return Weak_object_impl::store(otx); }

  // Required by Collection_item.
  virtual bool drop(Open_dictionary_tables_ctx *otx)
  { return Weak_object_impl::drop(otx); }

  virtual void drop();

  // Required by Collection_item.
  virtual bool restore_children(Open_dictionary_tables_ctx *otx)
  { return Weak_object_impl::restore_children(otx); }

  // Required by Collection_item.
  virtual bool drop_children(Open_dictionary_tables_ctx *otx)
  { return Weak_object_impl::drop_children(otx); }

  // Required by Collection_item.
  virtual void set_ordinal_position(uint ordinal_position)
  { m_ordinal_position= ordinal_position; }

  // Required by Collection_item.
  virtual bool is_hidden() const
  { return false; }

public:
  /////////////////////////////////////////////////////////////////////////
  // ordinal_position - Also used by Collection_item
  /////////////////////////////////////////////////////////////////////////

  virtual uint ordinal_position() const
  { return m_ordinal_position; }

  /////////////////////////////////////////////////////////////////////////
  // filename.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &filename() const
  { return m_filename; }

  virtual void set_filename(const std::string &filename)
  { m_filename= filename; }

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  using Tablespace_file::se_private_data;

  virtual Properties &se_private_data()
  { return *m_se_private_data; }

  virtual bool set_se_private_data_raw(
    const std::string &se_private_data_raw);

  /////////////////////////////////////////////////////////////////////////
  // tablespace.
  /////////////////////////////////////////////////////////////////////////

  using Tablespace_file::tablespace;

  virtual Tablespace &tablespace();

public:
  class Factory : public Collection_item_factory
  {
  public:
    Factory(Tablespace_impl *ts)
     :m_ts(ts)
    { }

    virtual Collection_item *create_item() const;

  private:
    Tablespace_impl *m_ts;
  };

public:
  virtual Object_key *create_primary_key() const;
  virtual bool has_new_primary_key() const;

private:
  // Fields
  uint m_ordinal_position;

  std::string m_filename;
  std::unique_ptr<Properties> m_se_private_data;

  // References to other objects
  Tablespace_impl *m_tablespace;

#ifndef DBUG_OFF
  Tablespace_file_impl(const Tablespace_file_impl &src,
                       Tablespace_impl *parent);

public:
  Tablespace_file_impl *clone(Tablespace_impl *parent) const
  {
    return new Tablespace_file_impl(*this, parent);
  }
#endif /* !DBUG_OFF */
};

///////////////////////////////////////////////////////////////////////////

class Tablespace_file_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Tablespace_file_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

DD_HEADER_END

#endif // DD__TABLESPACE_FILES_IMPL_INCLUDED
