/* Copyright (c) 2014, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DD__TABLESPACE_FILES_IMPL_INCLUDED
#define DD__TABLESPACE_FILES_IMPL_INCLUDED

#include <sys/types.h>
#include <memory>  // std::unique_ptr
#include <new>

#include "sql/dd/impl/properties_impl.h"
#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/types/weak_object_impl.h"  // dd::Weak_object_impl
#include "sql/dd/sdi_fwd.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/tablespace_file.h"  // dd::Tablespace_file

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

class Tablespace_file_impl : public Weak_object_impl, public Tablespace_file {
 public:
  Tablespace_file_impl();

  Tablespace_file_impl(Tablespace_impl *tablespace);

  Tablespace_file_impl(const Tablespace_file_impl &src,
                       Tablespace_impl *parent);

  ~Tablespace_file_impl() override = default;

 public:
  const Object_table &object_table() const override;

  bool store(Open_dictionary_tables_ctx *otx) override;

  bool validate() const override;

  bool store_attributes(Raw_record *r) override;

  bool restore_attributes(const Raw_record &r) override;

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const override;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val) override;

  void debug_print(String_type &outb) const override;

  void set_ordinal_position(uint ordinal_position) {
    m_ordinal_position = ordinal_position;
  }

 public:
  static void register_tables(Open_dictionary_tables_ctx *otx);

  /////////////////////////////////////////////////////////////////////////
  // ordinal_position.
  /////////////////////////////////////////////////////////////////////////

  uint ordinal_position() const override { return m_ordinal_position; }

  /////////////////////////////////////////////////////////////////////////
  // filename.
  /////////////////////////////////////////////////////////////////////////

  const String_type &filename() const override { return m_filename; }

  void set_filename(const String_type &filename) override {
    m_filename = filename;
  }

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  const Properties &se_private_data() const override {
    return m_se_private_data;
  }

  Properties &se_private_data() override { return m_se_private_data; }

  bool set_se_private_data(const String_type &se_private_data_raw) override {
    return m_se_private_data.insert_values(se_private_data_raw);
  }

  /////////////////////////////////////////////////////////////////////////
  // tablespace.
  /////////////////////////////////////////////////////////////////////////

  const Tablespace &tablespace() const override;

  Tablespace &tablespace() override;

 public:
  static Tablespace_file_impl *restore_item(Tablespace_impl *ts) {
    return new (std::nothrow) Tablespace_file_impl(ts);
  }

  static Tablespace_file_impl *clone(const Tablespace_file_impl &other,
                                     Tablespace_impl *ts) {
    return new (std::nothrow) Tablespace_file_impl(other, ts);
  }

 public:
  Object_key *create_primary_key() const override;
  bool has_new_primary_key() const override;

 private:
  // Fields
  uint m_ordinal_position;

  String_type m_filename;
  Properties_impl m_se_private_data;

  // References to other objects
  Tablespace_impl *m_tablespace;
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__TABLESPACE_FILES_IMPL_INCLUDED
