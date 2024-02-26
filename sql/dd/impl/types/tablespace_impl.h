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

#ifndef DD__TABLESPACE_IMPL_INCLUDED
#define DD__TABLESPACE_IMPL_INCLUDED

#include <memory>  // std::unique_ptr
#include <new>
#include <string>

#include "sql/dd/impl/properties_impl.h"
#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/types/entity_object_impl.h"  // dd::Entity_object_impl
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/sdi_fwd.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/tablespace.h"       // dd::Tablespace
#include "sql/dd/types/tablespace_file.h"  // dd::Tablespace_file
#include "sql/strfunc.h"

class THD;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_table;
class Open_dictionary_tables_ctx;
class Properties;
class Sdi_rcontext;
class Sdi_wcontext;
class Tablespace_file;
class Weak_object;
class Object_table;

class Tablespace_impl : public Entity_object_impl, public Tablespace {
 public:
  Tablespace_impl();

  ~Tablespace_impl() override;

 public:
  const Object_table &object_table() const override;

  bool validate() const override;

  bool restore_children(Open_dictionary_tables_ctx *otx) override;

  bool store_children(Open_dictionary_tables_ctx *otx) override;

  bool drop_children(Open_dictionary_tables_ctx *otx) const override;

  bool store_attributes(Raw_record *r) override;

  bool restore_attributes(const Raw_record &r) override;

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const override;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val) override;

  void debug_print(String_type &outb) const override;

  bool is_empty(THD *thd, bool *empty) const override;

 public:
  static void register_tables(Open_dictionary_tables_ctx *otx);

  /////////////////////////////////////////////////////////////////////////
  // comment.
  /////////////////////////////////////////////////////////////////////////

  const String_type &comment() const override { return m_comment; }

  void set_comment(const String_type &comment) override { m_comment = comment; }

  /////////////////////////////////////////////////////////////////////////
  // options.
  /////////////////////////////////////////////////////////////////////////

  const Properties &options() const override { return m_options; }

  Properties &options() override { return m_options; }

  bool set_options(const String_type &options_raw) override {
    return m_options.insert_values(options_raw);
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
  // m_engine.
  /////////////////////////////////////////////////////////////////////////

  const String_type &engine() const override { return m_engine; }

  void set_engine(const String_type &engine) override { m_engine = engine; }

  LEX_CSTRING engine_attribute() const override {
    return lex_cstring_handle(m_engine_attribute);
  }
  void set_engine_attribute(LEX_CSTRING a) override {
    m_engine_attribute.assign(a.str, a.length);
  }

  /////////////////////////////////////////////////////////////////////////
  // Tablespace file collection.
  /////////////////////////////////////////////////////////////////////////

  Tablespace_file *add_file() override;

  bool remove_file(String_type data_file) override;

  const Tablespace_file_collection &files() const override { return m_files; }

  // Fix "inherits ... via dominance" warnings
  Entity_object_impl *impl() override { return Entity_object_impl::impl(); }
  const Entity_object_impl *impl() const override {
    return Entity_object_impl::impl();
  }
  Object_id id() const override { return Entity_object_impl::id(); }
  bool is_persistent() const override {
    return Entity_object_impl::is_persistent();
  }
  const String_type &name() const override {
    return Entity_object_impl::name();
  }
  void set_name(const String_type &name) override {
    Entity_object_impl::set_name(name);
  }

 private:
  // Fields

  String_type m_comment;
  Properties_impl m_options;
  Properties_impl m_se_private_data;
  String_type m_engine;
  String_type m_engine_attribute;

  // Collections.

  Tablespace_file_collection m_files;

  Tablespace_impl(const Tablespace_impl &src);

  Tablespace *clone() const override { return new Tablespace_impl(*this); }

  Tablespace *clone_dropped_object_placeholder() const override {
    Tablespace_impl *placeholder = new Tablespace_impl();
    placeholder->set_id(id());
    placeholder->set_name(name());
    return placeholder;
  }
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__TABLESPACE_IMPL_INCLUDED
