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

#ifndef DD__TABLESPACE_FILES_IMPL_INCLUDED
#define DD__TABLESPACE_FILES_IMPL_INCLUDED

#include <sys/types.h>
#include <memory>   // std::unique_ptr
#include <new>

#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/types/weak_object_impl.h" // dd::Weak_object_impl
#include "sql/dd/properties.h"
#include "sql/dd/sdi_fwd.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/object_type.h"       // dd::Object_type
#include "sql/dd/types/tablespace_file.h"   // dd::Tablespace_file

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_key;
class Object_table;
class Open_dictionary_tables_ctx;
class Sdi_rcontext;
class Sdi_wcontext;
class Tablespace;
class Tablespace_impl;
class Weak_object;

///////////////////////////////////////////////////////////////////////////

class Tablespace_file_impl : public Weak_object_impl,
                             public Tablespace_file
{
public:
  Tablespace_file_impl();

  Tablespace_file_impl(Tablespace_impl *tablespace);

  Tablespace_file_impl(const Tablespace_file_impl &src,
                       Tablespace_impl *parent);

  virtual ~Tablespace_file_impl()
  { }

public:
  virtual const Object_table &object_table() const
  { return Tablespace_file::OBJECT_TABLE(); }

  virtual bool store(Open_dictionary_tables_ctx *otx);

  virtual bool validate() const;

  virtual bool store_attributes(Raw_record *r);

  virtual bool restore_attributes(const Raw_record &r);

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val);

  virtual void debug_print(String_type &outb) const;

  void set_ordinal_position(uint ordinal_position)
  { m_ordinal_position= ordinal_position; }

public:
  /////////////////////////////////////////////////////////////////////////
  // ordinal_position.
  /////////////////////////////////////////////////////////////////////////

  virtual uint ordinal_position() const
  { return m_ordinal_position; }

  /////////////////////////////////////////////////////////////////////////
  // filename.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &filename() const
  { return m_filename; }

  virtual void set_filename(const String_type &filename)
  { m_filename= filename; }

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  virtual const Properties &se_private_data() const
  { return *m_se_private_data; }

  virtual Properties &se_private_data()
  { return *m_se_private_data; }

  virtual bool set_se_private_data_raw(
    const String_type &se_private_data_raw);

  /////////////////////////////////////////////////////////////////////////
  // tablespace.
  /////////////////////////////////////////////////////////////////////////

  virtual const Tablespace &tablespace() const;

  virtual Tablespace &tablespace();

public:
  static Tablespace_file_impl *restore_item(Tablespace_impl *ts)
  {
    return new (std::nothrow) Tablespace_file_impl(ts);
  }

  static Tablespace_file_impl *clone(const Tablespace_file_impl &other,
                                     Tablespace_impl *ts)
  {
    return new (std::nothrow) Tablespace_file_impl(other, ts);
  }

public:
  virtual Object_key *create_primary_key() const;
  virtual bool has_new_primary_key() const;

private:
  // Fields
  uint m_ordinal_position;

  String_type m_filename;
  std::unique_ptr<Properties> m_se_private_data;

  // References to other objects
  Tablespace_impl *m_tablespace;
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

#endif // DD__TABLESPACE_FILES_IMPL_INCLUDED
